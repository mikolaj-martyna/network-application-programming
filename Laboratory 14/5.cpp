#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

using namespace std;

bool generateSelfSignedCertificate(const string &certFile, const string &keyFile) {
    EVP_PKEY *pkey = nullptr;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);

    if (!ctx) {
        ERR_print_errors_fp(stderr);

        return false;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        ERR_print_errors_fp(stderr);

        return false;
    }

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        ERR_print_errors_fp(stderr);

        return false;
    }

    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        ERR_print_errors_fp(stderr);

        return false;
    }

    EVP_PKEY_CTX_free(ctx);

    X509 *x509 = X509_new();

    X509_set_version(x509, 2);

    ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

    X509_gmtime_adj(X509_get_notBefore(x509), 0);
    X509_gmtime_adj(X509_get_notAfter(x509), 60 * 60 * 24 * 365);

    X509_set_pubkey(x509, pkey);

    X509_NAME *name = X509_get_subject_name(x509);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *)"PL", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC, (unsigned char *)"Poland", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "L", MBSTRING_ASC, (unsigned char *)"Lublin", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char *)"Echo Server", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *)"localhost", -1, -1, 0);

    X509_set_issuer_name(x509, name);

    if (X509_sign(x509, pkey, EVP_sha256()) <= 0) {
        X509_free(x509);
        EVP_PKEY_free(pkey);
        ERR_print_errors_fp(stderr);

        return false;
    }

    FILE *certFilePtr = fopen(certFile.c_str(), "wb");
    if (!certFilePtr) {
        cerr << "Unable to open certificate file for writing" << endl;

        X509_free(x509);
        EVP_PKEY_free(pkey);

        return false;
    }

    PEM_write_X509(certFilePtr, x509);
    fclose(certFilePtr);

    FILE *keyFilePtr = fopen(keyFile.c_str(), "wb");
    if (!keyFilePtr) {
        cerr << "Unable to open key file for writing" << endl;

        X509_free(x509);
        EVP_PKEY_free(pkey);

        return false;
    }

    PEM_write_PrivateKey(keyFilePtr, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(keyFilePtr);

    X509_free(x509);
    EVP_PKEY_free(pkey);

    cout << "Self-signed certificate and private key generated successfully" << endl;

    return true;
}

SSL_CTX* initServerContext(const string &certFile, const string &keyFile) {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
    if (!ctx) {
        ERR_print_errors_fp(stderr);

        return nullptr;
    }

    if (SSL_CTX_use_certificate_file(ctx, certFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(ctx);
        ERR_print_errors_fp(stderr);

        return nullptr;
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, keyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(ctx);
        ERR_print_errors_fp(stderr);

        return nullptr;
    }

    if (!SSL_CTX_check_private_key(ctx)) {
        cerr << "Private key does not match the certificate" << endl;

        SSL_CTX_free(ctx);
        ERR_print_errors_fp(stderr);

        return nullptr;
    }

    return ctx;
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 8443;

    const string certFile = "server.crt";
    const string keyFile = "server.key";

    if (!generateSelfSignedCertificate(certFile, keyFile)) {
        cerr << "Failed to generate self-signed certificate" << endl;

        return 1;
    }

    SSL_CTX *ctx = initServerContext(certFile, keyFile);
    if (!ctx) {
        cerr << "Failed to initialize SSL context" << endl;

        return 1;
    }

    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create socket." << endl;

        SSL_CTX_free(ctx);

        return 1;
    }

    constexpr int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Failed to set socket options." << endl;

        close(serverSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address format." << endl;

        close(serverSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to bind socket to address." << endl;

        close(serverSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    if (listen(serverSocket, 5) < 0) {
        cerr << "Failed to listen for connections." << endl;

        close(serverSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    cout << "Secure echo server is listening on " << serverIp << ":" << serverPort << endl;
    cout << "Using self-signed certificate: " << certFile << endl;
    cout << "Using private key: " << keyFile << endl;

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);
        const int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressLength);

        if (clientSocket < 0) {
            cerr << "Failed to accept connection." << endl;
            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
        const int clientPort = ntohs(clientAddress.sin_port);
        cout << "Client connected: " << clientIp << ":" << clientPort << endl;

        SSL *ssl = SSL_new(ctx);
        if (!ssl) {
            cerr << "Failed to create SSL structure" << endl;

            close(clientSocket);
            continue;
        }

        SSL_set_fd(ssl, clientSocket);

        if (SSL_accept(ssl) <= 0) {
            cerr << "SSL handshake failed" << endl;

            ERR_print_errors_fp(stderr);
            SSL_free(ssl);
            close(clientSocket);
            continue;
        }

        cout << "SSL connection established using " << SSL_get_cipher(ssl) << endl;

        char buffer[1024] = {};
        const int bytesRead = SSL_read(ssl, buffer, sizeof(buffer) - 1);

        if (bytesRead <= 0) {
            if (const int error = SSL_get_error(ssl, bytesRead); error == SSL_ERROR_ZERO_RETURN) {
                cout << "SSL connection closed by client" << endl;
            } else {
                cerr << "SSL read failed: " << error << endl;
                ERR_print_errors_fp(stderr);
            }

            SSL_free(ssl);
            close(clientSocket);
            continue;
        }

        buffer[bytesRead] = '\0';
        cout << "Received " << bytesRead << " bytes from client: " << buffer << endl;

        if (const int bytesSent = SSL_write(ssl, buffer, bytesRead); bytesSent <= 0) {
            cerr << "Failed to send data to client." << endl;
            ERR_print_errors_fp(stderr);
        } else {
            cout << "Echoed " << bytesSent << " bytes back to client." << endl;
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(clientSocket);
    }

    close(serverSocket);
    SSL_CTX_free(ctx);
    EVP_cleanup();

    return 0;
}
