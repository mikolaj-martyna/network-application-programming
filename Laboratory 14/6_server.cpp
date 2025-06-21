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
const string SERVER_CERT_FILE = "server.crt";
const string SERVER_KEY_FILE = "server.key";
const string CA_CERT_FILE = "ca.crt";

void printSSLError() {
    ERR_print_errors_fp(stderr);
}

bool generateCACertificate(const string &caKeyFile, const string &caCertFile) {
    EVP_PKEY *caKey = nullptr;
    EVP_PKEY_CTX *keyCtx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);

    if (!keyCtx) {
        ERR_print_errors_fp(stderr);

        return false;
    }

    if (EVP_PKEY_keygen_init(keyCtx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(keyCtx, 2048) <= 0 ||
        EVP_PKEY_keygen(keyCtx, &caKey) <= 0) {
        EVP_PKEY_CTX_free(keyCtx);
        ERR_print_errors_fp(stderr);

        return false;
    }

    EVP_PKEY_CTX_free(keyCtx);

    X509 *caCert = X509_new();
    X509_set_version(caCert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(caCert), 1);

    X509_gmtime_adj(X509_get_notBefore(caCert), 0);
    X509_gmtime_adj(X509_get_notAfter(caCert), 60 * 60 * 24 * 365 * 5);

    X509_set_pubkey(caCert, caKey);

    X509_NAME *name = X509_get_subject_name(caCert);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *) "PL", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC, (unsigned char *) "Poland", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "L", MBSTRING_ASC, (unsigned char *) "Lublin", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char *) "Echo CA", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *) "Echo CA", -1, -1, 0);

    X509_set_issuer_name(caCert, name);

    if (!X509_sign(caCert, caKey, EVP_sha256())) {
        X509_free(caCert);
        EVP_PKEY_free(caKey);
        ERR_print_errors_fp(stderr);

        return false;
    }

    FILE *keyFile = fopen(caKeyFile.c_str(), "wb");
    if (!keyFile) {
        cerr << "Unable to open CA key file for writing" << endl;

        X509_free(caCert);
        EVP_PKEY_free(caKey);

        return false;
    }

    PEM_write_PrivateKey(keyFile, caKey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(keyFile);

    FILE *certFile = fopen(caCertFile.c_str(), "wb");
    if (!certFile) {
        cerr << "Unable to open CA certificate file for writing" << endl;

        X509_free(caCert);
        EVP_PKEY_free(caKey);

        return false;
    }

    PEM_write_X509(certFile, caCert);
    fclose(certFile);

    X509_free(caCert);
    EVP_PKEY_free(caKey);

    cout << "CA certificate and private key generated successfully" << endl;

    return true;
}

bool generateServerCertificate(const string &caKeyFile, const string &caCertFile,
                               const string &serverKeyFile, const string &serverCertFile) {
    FILE *caKeyFilePtr = fopen(caKeyFile.c_str(), "rb");
    if (!caKeyFilePtr) {
        cerr << "Unable to open CA key file" << endl;

        return false;
    }

    EVP_PKEY *caKey = PEM_read_PrivateKey(caKeyFilePtr, nullptr, nullptr, nullptr);
    fclose(caKeyFilePtr);

    if (!caKey) {
        ERR_print_errors_fp(stderr);

        return false;
    }

    FILE *caCertFilePtr = fopen(caCertFile.c_str(), "rb");
    if (!caCertFilePtr) {
        cerr << "Unable to open CA certificate file" << endl;

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

    EVP_PKEY *serverKey = nullptr;
    EVP_PKEY_CTX *keyCtx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);

    if (!keyCtx ||
        EVP_PKEY_keygen_init(keyCtx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_keygen_bits(keyCtx, 2048) <= 0 ||
        EVP_PKEY_keygen(keyCtx, &serverKey) <= 0) {
        EVP_PKEY_CTX_free(keyCtx);
        EVP_PKEY_free(caKey);
        X509_free(caCert);
        ERR_print_errors_fp(stderr);

        return false;
    }

    EVP_PKEY_CTX_free(keyCtx);

    X509_REQ *req = X509_REQ_new();
    X509_REQ_set_pubkey(req, serverKey);

    X509_NAME *name = X509_REQ_get_subject_name(req);
    X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *) "PL", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC, (unsigned char *) "Poland", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "L", MBSTRING_ASC, (unsigned char *) "Lublin", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char *) "Echo Server", -1, -1, 0);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *) "localhost", -1, -1, 0);

    if (!X509_REQ_sign(req, serverKey, EVP_sha256())) {
        X509_REQ_free(req);
        EVP_PKEY_free(serverKey);
        EVP_PKEY_free(caKey);
        X509_free(caCert);
        ERR_print_errors_fp(stderr);

        return false;
    }

    X509 *serverCert = X509_new();
    X509_set_version(serverCert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(serverCert), 2);

    X509_gmtime_adj(X509_get_notBefore(serverCert), 0);
    X509_gmtime_adj(X509_get_notAfter(serverCert), 60 * 60 * 24 * 365);

    X509_set_pubkey(serverCert, serverKey);
    X509_set_subject_name(serverCert, X509_REQ_get_subject_name(req));
    X509_set_issuer_name(serverCert, X509_get_subject_name(caCert));

    if (!X509_sign(serverCert, caKey, EVP_sha256())) {
        X509_free(serverCert);
        X509_REQ_free(req);
        EVP_PKEY_free(serverKey);
        EVP_PKEY_free(caKey);
        X509_free(caCert);
        ERR_print_errors_fp(stderr);

        return false;
    }

    X509_REQ_free(req);

    FILE *keyFile = fopen(serverKeyFile.c_str(), "wb");
    if (!keyFile) {
        cerr << "Unable to open server key file for writing" << endl;

        X509_free(serverCert);
        EVP_PKEY_free(serverKey);
        EVP_PKEY_free(caKey);
        X509_free(caCert);

        return false;
    }

    PEM_write_PrivateKey(keyFile, serverKey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(keyFile);

    FILE *certFile = fopen(serverCertFile.c_str(), "wb");
    if (!certFile) {
        cerr << "Unable to open server certificate file for writing" << endl;

        X509_free(serverCert);
        EVP_PKEY_free(serverKey);
        EVP_PKEY_free(caKey);
        X509_free(caCert);

        return false;
    }

    PEM_write_X509(certFile, serverCert);
    fclose(certFile);

    X509_free(serverCert);
    EVP_PKEY_free(serverKey);
    EVP_PKEY_free(caKey);
    X509_free(caCert);

    cout << "Server certificate and private key generated successfully" << endl;

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

SSL_CTX *initServerContext(const string &certFile, const string &keyFile, const string &caFile) {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
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

    if (SSL_CTX_load_verify_locations(ctx, caFile.c_str(), nullptr) <= 0) {
        cerr << "Failed to load CA certificate" << endl;

        SSL_CTX_free(ctx);
        ERR_print_errors_fp(stderr);

        return nullptr;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, verifyCertificate);
    SSL_CTX_set_verify_depth(ctx, 1);

    return ctx;
}

int main() {
    const string CA_KEY_FILE = "ca.key";

    if (!generateCACertificate(CA_KEY_FILE, CA_CERT_FILE)) {
        cerr << "Failed to generate CA certificate" << endl;

        return 1;
    }

    if (!generateServerCertificate(CA_KEY_FILE, CA_CERT_FILE, SERVER_KEY_FILE, SERVER_CERT_FILE)) {
        cerr << "Failed to generate server certificate" << endl;

        return 1;
    }

    SSL_CTX *ctx = initServerContext(SERVER_CERT_FILE, SERVER_KEY_FILE, CA_CERT_FILE);
    if (!ctx) {
        cerr << "Failed to initialize SSL context" << endl;

        return 1;
    }

    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create socket" << endl;

        SSL_CTX_free(ctx);

        return 1;
    }

    constexpr int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Failed to set socket options" << endl;

        close(serverSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address format" << endl;

        close(serverSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    if (bind(serverSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to bind socket to address" << endl;

        close(serverSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    if (listen(serverSocket, 5) < 0) {
        cerr << "Failed to listen for connections" << endl;

        close(serverSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    cout << "Secure echo server is listening on " << SERVER_IP << ":" << SERVER_PORT << endl;
    cout << "Server is using certificate: " << SERVER_CERT_FILE << endl;
    cout << "Server is verifying clients against CA certificate: " << CA_CERT_FILE << endl;

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        cout << "Waiting for connection..." << endl;

        const int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress),
                                        &clientAddressLength);
        if (clientSocket < 0) {
            cerr << "Failed to accept connection" << endl;
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

        X509 *cert = SSL_get_peer_certificate(ssl);
        if (!cert) {
            cerr << "No client certificate presented" << endl;

            SSL_free(ssl);
            close(clientSocket);
            continue;
        }

        char subjectName[256];
        X509_NAME_oneline(X509_get_subject_name(cert), subjectName, sizeof(subjectName));
        cout << "Client certificate subject: " << subjectName << endl;

        X509_free(cert);

        char buffer[1024];
        while (true) {
            const int bytesRead = SSL_read(ssl, buffer, sizeof(buffer) - 1);

            if (bytesRead <= 0) {
                if (SSL_get_error(ssl, bytesRead) == SSL_ERROR_ZERO_RETURN) {
                    cout << "Client closed connection" << endl;
                } else {
                    cerr << "Error reading data: " << SSL_get_error(ssl, bytesRead) << endl;

                    ERR_print_errors_fp(stderr);
                }
                break;
            }

            buffer[bytesRead] = '\0';
            cout << "Received: " << buffer << endl;

            if (SSL_write(ssl, buffer, bytesRead) <= 0) {
                cerr << "Error writing data" << endl;

                ERR_print_errors_fp(stderr);
                break;
            }
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(clientSocket);

        cout << "Connection closed" << endl;
    }

    close(serverSocket);
    SSL_CTX_free(ctx);
    EVP_cleanup();

    return 0;
}
