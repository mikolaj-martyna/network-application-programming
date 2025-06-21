#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

using namespace std;

const string SERVER_IP = "127.0.0.1";
constexpr int SERVER_PORT = 8443;
const string CLIENT_CERT_FILE = "client.crt";
const string CLIENT_KEY_FILE = "client.key";
const string CA_CERT_FILE = "ca.crt";
const string CA_KEY_FILE = "ca.key";

bool generateClientCertificate() {
    FILE *caKeyFilePtr = fopen(CA_KEY_FILE.c_str(), "rb");
    if (!caKeyFilePtr) {
        cerr << "Unable to open CA key file. Make sure the server has generated the CA first." << endl;

        return false;
    }

    EVP_PKEY *caKey = PEM_read_PrivateKey(caKeyFilePtr, nullptr, nullptr, nullptr);
    fclose(caKeyFilePtr);

    if (!caKey) {
        ERR_print_errors_fp(stderr);

        return false;
    }

    FILE *caCertFilePtr = fopen(CA_CERT_FILE.c_str(), "rb");
    if (!caCertFilePtr) {
        cerr << "Unable to open CA certificate file. Make sure the server has generated the CA first." << endl;

        EVP_PKEY_free(caKey);

        return false;
    }

    X509 *caCert = PEM_read_X509(caCertFilePtr, nullptr, nullptr, nullptr);
    fclose(caCertFilePtr);

    if (!caCert) {
        EVP_PKEY_free(caKey);
        ERR_print_errors_fp(stderr);

        return false;
    }

    EVP_PKEY *clientKey = nullptr;
    EVP_PKEY_CTX *keyCtx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);

    if (!keyCtx ||
        EVP_PKEY_keygen_init(keyCtx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(keyCtx, 2048) <= 0 ||
        EVP_PKEY_keygen(keyCtx, &clientKey) <= 0) {

        if (keyCtx) EVP_PKEY_CTX_free(keyCtx);

        EVP_PKEY_free(caKey);
        X509_free(caCert);
        ERR_print_errors_fp(stderr);

        return false;
    }

    EVP_PKEY_CTX_free(keyCtx);

    X509_REQ *req = X509_REQ_new();
    X509_REQ_set_pubkey(req, clientKey);

    X509_NAME *name = X509_REQ_get_subject_name(req);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *)"PL", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC, (unsigned char *)"Poland", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "L", MBSTRING_ASC, (unsigned char *)"Lublin", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char *)"Echo Client", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *)"client", -1, -1, 0);

    if (!X509_REQ_sign(req, clientKey, EVP_sha256())) {
        X509_REQ_free(req);
        EVP_PKEY_free(clientKey);
        EVP_PKEY_free(caKey);
        X509_free(caCert);
        ERR_print_errors_fp(stderr);

        return false;
    }

    X509 *clientCert = X509_new();
    X509_set_version(clientCert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(clientCert), 3);

    X509_gmtime_adj(X509_get_notBefore(clientCert), 0);
    X509_gmtime_adj(X509_get_notAfter(clientCert), 60 * 60 * 24 * 365);

    X509_set_pubkey(clientCert, clientKey);
    X509_set_subject_name(clientCert, X509_REQ_get_subject_name(req));
    X509_set_issuer_name(clientCert, X509_get_subject_name(caCert));

    if (!X509_sign(clientCert, caKey, EVP_sha256())) {
        X509_free(clientCert);
        X509_REQ_free(req);
        EVP_PKEY_free(clientKey);
        EVP_PKEY_free(caKey);
        X509_free(caCert);
        ERR_print_errors_fp(stderr);

        return false;
    }

    X509_REQ_free(req);

    FILE *keyFile = fopen(CLIENT_KEY_FILE.c_str(), "wb");
    if (!keyFile) {
        cerr << "Unable to open client key file for writing" << endl;

        X509_free(clientCert);
        EVP_PKEY_free(clientKey);
        EVP_PKEY_free(caKey);
        X509_free(caCert);

        return false;
    }

    PEM_write_PrivateKey(keyFile, clientKey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(keyFile);

    FILE *certFile = fopen(CLIENT_CERT_FILE.c_str(), "wb");
    if (!certFile) {
        cerr << "Unable to open client certificate file for writing" << endl;

        X509_free(clientCert);
        EVP_PKEY_free(clientKey);
        EVP_PKEY_free(caKey);
        X509_free(caCert);

        return false;
    }

    PEM_write_X509(certFile, clientCert);
    fclose(certFile);

    X509_free(clientCert);
    EVP_PKEY_free(clientKey);
    EVP_PKEY_free(caKey);
    X509_free(caCert);

    cout << "Client certificate and private key generated successfully" << endl;

    return true;
}

int verifyCertificate(int preverify_ok, X509_STORE_CTX *ctx) {
    if (!preverify_ok) {
        X509 *cert = X509_STORE_CTX_get_current_cert(ctx);
        int depth = X509_STORE_CTX_get_error_depth(ctx);
        int err = X509_STORE_CTX_get_error(ctx);

        char subjectName[256];
        X509_NAME_oneline(X509_get_subject_name(cert), subjectName, sizeof(subjectName));

        cerr << "Certificate verification error (depth=" << depth << "): "
             << X509_verify_cert_error_string(err) << endl;
        cerr << "Subject: " << subjectName << endl;

        return 0;
    }

    return 1;
}

SSL_CTX* initClientContext() {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        ERR_print_errors_fp(stderr);

        return nullptr;
    }

    if (SSL_CTX_use_certificate_file(ctx, CLIENT_CERT_FILE.c_str(), SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(ctx);
        ERR_print_errors_fp(stderr);

        return nullptr;
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, CLIENT_KEY_FILE.c_str(), SSL_FILETYPE_PEM) <= 0) {
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

    if (SSL_CTX_load_verify_locations(ctx, CA_CERT_FILE.c_str(), nullptr) <= 0) {
        cerr << "Failed to load CA certificate" << endl;

        SSL_CTX_free(ctx);
        ERR_print_errors_fp(stderr);

        return nullptr;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verifyCertificate);
    SSL_CTX_set_verify_depth(ctx, 1);

    return ctx;
}

int main() {
    if (!generateClientCertificate()) {
        cerr << "Failed to generate client certificate" << endl;

        return 1;
    }

    SSL_CTX *ctx = initClientContext();
    if (!ctx) {
        cerr << "Failed to initialize SSL context" << endl;

        return 1;
    }

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket" << endl;

        SSL_CTX_free(ctx);

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address format" << endl;

        close(clientSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    cout << "Connecting to server at " << SERVER_IP << ":" << SERVER_PORT << "..." << endl;
    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect to server" << endl;

        close(clientSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    cout << "Connected to server" << endl;

    SSL *ssl = SSL_new(ctx);
    if (!ssl) {
        cerr << "Failed to create SSL structure" << endl;

        close(clientSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    SSL_set_fd(ssl, clientSocket);

    if (SSL_connect(ssl) <= 0) {
        cerr << "SSL handshake failed" << endl;

        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(clientSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    cout << "SSL connection established using " << SSL_get_cipher(ssl) << endl;

    X509 *cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        cerr << "No certificate presented by the server" << endl;

        SSL_free(ssl);
        close(clientSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    char subjectName[256];
    X509_NAME_oneline(X509_get_subject_name(cert), subjectName, sizeof(subjectName));
    cout << "Server certificate subject: " << subjectName << endl;

    char issuerName[256];
    X509_NAME_oneline(X509_get_issuer_name(cert), issuerName, sizeof(issuerName));
    cout << "Server certificate issuer: " << issuerName << endl;

    X509_free(cert);

    cout << "Server identity verified successfully" << endl;
    cout << "Type messages to send to the server (type 'quit' to exit):" << endl;

    string input;
    while (true) {
        cout << "> ";
        getline(cin, input);

        if (input == "quit") {
            break;
        }

        if (SSL_write(ssl, input.c_str(), input.length()) <= 0) {
            cerr << "Failed to send data to server" << endl;

            ERR_print_errors_fp(stderr);
            break;
        }

        char buffer[1024] = {};
        const int bytesRead = SSL_read(ssl, buffer, sizeof(buffer) - 1);

        if (bytesRead <= 0) {
            if (const int error = SSL_get_error(ssl, bytesRead); error == SSL_ERROR_ZERO_RETURN) {
                cout << "Server closed connection" << endl;
            } else {
                cerr << "Error receiving data: " << error << endl;

                ERR_print_errors_fp(stderr);
            }
            break;
        }

        buffer[bytesRead] = '\0';
        cout << "Server response: " << buffer << endl;
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(clientSocket);
    SSL_CTX_free(ctx);
    EVP_cleanup();

    cout << "Connection closed" << endl;

    return 0;
}
