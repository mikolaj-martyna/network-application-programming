#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sstream>
#include <iomanip>
#include <string>
#include <random>
#include <vector>

using namespace std;

void printSslError(const char* message) {
    cerr << message << ": " << ERR_error_string(ERR_get_error(), nullptr) << endl;
}

string base64Encode(const vector<unsigned char> &input) {
    static const string base64Chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string output;

    int val = 0, valb = -6;
    for (const unsigned char c: input) {
        val = (val << 8) + c;
        valb += 8;

        while (valb >= 0) {
            output.push_back(base64Chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        output.push_back(base64Chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (output.size() % 4) {
        output.push_back('=');
    }

    return output;
}

string generateWebSocketKey() {
    vector<unsigned char> randomData(16);
    random_device rd;

    for (int i = 0; i < 16; i++) {
        randomData[i] = static_cast<unsigned char>(rd() % 256);
    }

    return base64Encode(randomData);
}

string createHandshakeRequest(const string& host, const string& resource, const string& key) {
    stringstream ss;

    ss << "GET " << resource << " HTTP/1.1\r\n";
    ss << "Host: " << host << "\r\n";
    ss << "Upgrade: websocket\r\n";
    ss << "Connection: Upgrade\r\n";
    ss << "Sec-WebSocket-Key: " << key << "\r\n";
    ss << "Sec-WebSocket-Version: 13\r\n";
    ss << "Origin: https://" << host << "\r\n";
    ss << "\r\n";

    return ss.str();
}

vector<uint8_t> createWebSocketFrame(const string& message) {
    vector<uint8_t> frame;

    frame.push_back(0x81);
    frame.push_back(0x80 | message.length());

    uint8_t maskingKey[4];
    random_device rd;
    for (int i = 0; i < 4; ++i) {
        maskingKey[i] = static_cast<uint8_t>(rd() % 256);
        frame.push_back(maskingKey[i]);
    }

    for (size_t i = 0; i < message.length(); ++i) {
        frame.push_back(message[i] ^ maskingKey[i % 4]);
    }

    return frame;
}

int main() {
    const string hostname = "echo.websocket.org";
    constexpr int port = 443;
    const string resource = "/";
    const string message = "Hello Websocket!";

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    ERR_load_crypto_strings();

    SSL_CTX* ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx) {
        printSslError("Error creating SSL context");

        return 1;
    }

    const hostent *hostinfo = gethostbyname(hostname.c_str());
    if (hostinfo == nullptr) {
        cerr << "Failed to resolve hostname." << endl;

        SSL_CTX_free(ctx);

        return 1;
    }

    auto **addrList = reinterpret_cast<struct in_addr **>(hostinfo->h_addr_list);
    const string ip = inet_ntoa(*addrList[0]);

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        SSL_CTX_free(ctx);

        return 1;
    }

    sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address format." << endl;

        close(clientSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect." << endl;

        close(clientSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    cout << "Connected to " << hostname << " (" << ip << "):" << port << endl;

    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        printSslError("Error creating SSL structure");

        close(clientSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    SSL_set_fd(ssl, clientSocket);
    SSL_set_tlsext_host_name(ssl, hostname.c_str());

    if (SSL_connect(ssl) != 1) {
        printSslError("SSL connection failed");

        SSL_free(ssl);
        close(clientSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    cout << "SSL connection established using: " << SSL_get_cipher(ssl) << endl;

    const string webSocketKey = generateWebSocketKey();
    cout << "Generated WebSocket key: " << webSocketKey << endl;

    const string request = createHandshakeRequest(hostname, resource, webSocketKey);

    if (SSL_write(ssl, request.c_str(), request.length()) <= 0) {
        printSslError("Failed to send handshake request");

        SSL_free(ssl);
        close(clientSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    cout << "WebSocket handshake request sent" << endl;

    char buffer[4096] = {0};
    const int bytesReceived = SSL_read(ssl, buffer, sizeof(buffer) - 1);

    if (bytesReceived <= 0) {
        printSslError("Failed to receive handshake response");

        SSL_free(ssl);
        close(clientSocket);
        SSL_CTX_free(ctx);

        return 1;
    }

    cout << "Received response (" << bytesReceived << " bytes):" << endl;
    cout << string(buffer, bytesReceived) << endl;

    if (const string response(buffer, bytesReceived); response.find("HTTP/1.1 101") != string::npos &&
                                                      (response.find("upgrade: websocket") != string::npos || response.find("Upgrade: websocket") != string::npos) &&
                                                      (response.find("connection: Upgrade") != string::npos || response.find("Connection: Upgrade") != string::npos)) {
        cout << "WebSocket handshake successful!" << endl;

        const vector<uint8_t> frame = createWebSocketFrame(message);

        if (SSL_write(ssl, frame.data(), frame.size()) <= 0) {
            printSslError("Failed to send WebSocket message");

            SSL_free(ssl);
            close(clientSocket);
            SSL_CTX_free(ctx);

            return 1;
        }

        cout << "Sent message: " << message << endl;

        memset(buffer, 0, sizeof(buffer));

        if (const int msgBytesReceived = SSL_read(ssl, buffer, sizeof(buffer) - 1); msgBytesReceived <= 0) {
            printSslError("Failed to receive response");
        } else {
            cout << "Received data (" << msgBytesReceived << " bytes)" << endl;

            if (msgBytesReceived > 2) {
                uint8_t payloadLen = buffer[1] & 0x7F;
                int payloadOffset = 2;

                if (payloadLen == 126) {
                    payloadLen = (buffer[2] << 8) | buffer[3];
                    payloadOffset = 4;
                } else if (payloadLen == 127) {
                    payloadOffset = 10;
                }

                const string receivedMessage(buffer + payloadOffset, min(msgBytesReceived - payloadOffset, static_cast<int>(payloadLen)));
                cout << "Received response: " << receivedMessage << endl;
            }
        }
    } else {
        cerr << "WebSocket handshake failed!" << endl;
    }

    SSL_free(ssl);
    close(clientSocket);
    SSL_CTX_free(ctx);

    return 0;
}
