#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fstream>
#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

using namespace std;

int verifyCertificate(int preverifyOk, X509_STORE_CTX *ctx) {
    if (!preverifyOk) {
        X509 *cert = X509_STORE_CTX_get_current_cert(ctx);
        int depth = X509_STORE_CTX_get_error_depth(ctx);
        int err = X509_STORE_CTX_get_error(ctx);

        char subjectName[256];
        X509_NAME_oneline(X509_get_subject_name(cert), subjectName, sizeof(subjectName));

        cerr << "Certificate verification error at depth: " << depth << endl;
        cerr << "  Subject: " << subjectName << endl;
        cerr << "  Error: " << X509_verify_cert_error_string(err) << endl;

        return 0;
    }

    return 1;
}

int main() {
    const string host = "httpbin.org";
    constexpr int port = 443;
    const string path = "/html";
    const string outputFile = "httpbin.html";

    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        EVP_cleanup();

        return 1;
    }

    hostent *server = gethostbyname(host.c_str());
    if (server == nullptr) {
        cerr << "Error: Could not resolve hostname " << host << endl;

        close(clientSocket);
        EVP_cleanup();

        return 1;
    }

    sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    memcpy(&serverAddress.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect to " << host << ":" << port << endl;

        close(clientSocket);
        EVP_cleanup();

        return 1;
    }

    cout << "Connected to " << host << ":" << port << endl;

    SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx) {
        cerr << "Failed to create SSL context." << endl;

        ERR_print_errors_fp(stderr);
        close(clientSocket);
        EVP_cleanup();

        return 1;
    }

    if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
        cerr << "Failed to set default verify paths." << endl;

        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        close(clientSocket);
        EVP_cleanup();

        return 1;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verifyCertificate);

    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        cerr << "Failed to create SSL connection." << endl;

        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        close(clientSocket);
        EVP_cleanup();

        return 1;
    }

    SSL_set_tlsext_host_name(ssl, host.c_str());

    SSL_set_fd(ssl, clientSocket);

    if (SSL_connect(ssl) != 1) {
        cerr << "SSL connection failed." << endl;

        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(clientSocket);
        EVP_cleanup();

        return 1;
    }

    cout << "SSL connection established using: " << SSL_get_cipher(ssl) << endl;

    X509 *cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        cerr << "Error: No certificate presented by the server." << endl;

        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(clientSocket);
        EVP_cleanup();

        return 1;
    }

    char subjectName[256];
    X509_NAME_oneline(X509_get_subject_name(cert), subjectName, sizeof(subjectName));
    cout << "Server certificate subject: " << subjectName << endl;

    char issuerName[256];
    X509_NAME_oneline(X509_get_issuer_name(cert), issuerName, sizeof(issuerName));
    cout << "Server certificate issuer: " << issuerName << endl;

    long verifyResult = SSL_get_verify_result(ssl);
    if (verifyResult != X509_V_OK) {
        cerr << "Server certificate verification failed: "
                << X509_verify_cert_error_string(verifyResult) << endl;

        X509_free(cert);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(clientSocket);
        EVP_cleanup();

        return 1;
    }

    cout << "Server certificate verified successfully." << endl;
    X509_free(cert);

    string httpRequest = "GET " + path + " HTTP/1.1\r\n"
                         "Host: " + host + "\r\n"
                         "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9_3) AppleWebKit/537.75.14 (KHTML, like Gecko) Version/7.0.3 Safari/7046A194A\r\n"
                         "Accept: text/html,application/xhtml+xml,application/xml\r\n"
                         "Connection: close\r\n\r\n";

    if (SSL_write(ssl, httpRequest.c_str(), httpRequest.length()) <= 0) {
        cerr << "Failed to send HTTP request." << endl;

        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(clientSocket);
        EVP_cleanup();

        return 1;
    }

    cout << "HTTP request sent." << endl;

    string response;
    char buffer[4096];
    int bytesRead;

    while ((bytesRead = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }

    if (bytesRead < 0) {
        cerr << "Failed to receive data." << endl;

        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(clientSocket);
        EVP_cleanup();

        return 1;
    }

    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(clientSocket);
    EVP_cleanup();

    size_t headerEnd = response.find("\r\n\r\n");
    if (headerEnd == string::npos) {
        cerr << "Invalid HTTP response (no header separator found)." << endl;

        return 1;
    }

    string headers = response.substr(0, headerEnd);
    string body = response.substr(headerEnd + 4);

    cout << "HTTP response received. Headers size: " << headers.size()
            << " bytes, Body size: " << body.size() << " bytes." << endl;

    ofstream htmlFile(outputFile);
    if (!htmlFile) {
        cerr << "Failed to create output file: " << outputFile << endl;

        return 1;
    }

    htmlFile << body;
    htmlFile.close();

    cout << "HTML content saved to " << outputFile << endl;

    return 0;
}
