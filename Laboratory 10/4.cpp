#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <vector>
#include <string>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <sstream>
#include <iomanip>
#include <signal.h>

using namespace std;

volatile sig_atomic_t running = 1;

void handleSignal(int signal) {
    running = 0;
}

string computeSHA1(const string& input) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), hash);

    string binaryResult(reinterpret_cast<char*>(hash), SHA_DIGEST_LENGTH);

    return binaryResult;
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

string computeAcceptKey(const string& clientKey) {
    const string magicString = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const string concatenated = clientKey + magicString;
    const string sha1Hash = computeSHA1(concatenated);

    vector<uint8_t> binaryData;
    for (size_t i = 0; i < sha1Hash.length(); i += 2) {
        string byteString = sha1Hash.substr(i, 2);
        uint8_t byte = strtol(byteString.c_str(), nullptr, 16);

        binaryData.push_back(byte);
    }

    string base64Key = base64Encode(binaryData);

    return base64Key;
}

string createHandshakeResponse(const string& acceptKey) {
    stringstream ss;

    ss << "HTTP/1.1 101 Switching Protocols\r\n";
    ss << "Upgrade: websocket\r\n";
    ss << "Connection: Upgrade\r\n";
    ss << "Sec-WebSocket-Accept: " << acceptKey << "\r\n";
    ss << "\r\n";

    return ss.str();
}

string extractWebSocketKey(const string& request) {
    const string keyPrefix = "Sec-WebSocket-Key: ";
    size_t start = request.find(keyPrefix);

    if (start == string::npos) {
        return "";
    }

    start += keyPrefix.length();
    const size_t end = request.find("\r\n", start);

    if (end == string::npos) {
        return "";
    }

    return request.substr(start, end - start);
}

vector<uint8_t> createWebSocketFrame(const string& message, bool masked = false) {
    vector<uint8_t> frame;

    frame.push_back(0x81);

    if (const size_t length = message.length(); length <= 125) {
        frame.push_back(masked ? 0x80 | length : length);
    } else if (length <= 65535) {
        frame.push_back(masked ? 0x80 | 126 : 126);
        frame.push_back((length >> 8) & 0xFF);
        frame.push_back(length & 0xFF);
    } else {
        frame.push_back(masked ? 0x80 | 127 : 127);

        for (int i = 7; i >= 0; --i) {
            frame.push_back((length >> (i * 8)) & 0xFF);
        }
    }

    if (masked) {
        constexpr uint8_t maskingKey[4] = {0x12, 0x34, 0x56, 0x78};

        for (int i = 0; i < 4; ++i) {
            frame.push_back(maskingKey[i]);
        }

        for (size_t i = 0; i < message.length(); ++i) {
            frame.push_back(message[i] ^ maskingKey[i % 4]);
        }
    } else {
        for (const char c : message) {
            frame.push_back(c);
        }
    }

    return frame;
}

string decodeWebSocketFrame(const uint8_t* buffer, size_t length) {
    if (length < 2) return "";

    const uint8_t opcode = buffer[0] & 0x0F;

    if (opcode == 0x8) {
        return "CLOSE_CONNECTION";
    }

    if (opcode != 0x1 && opcode != 0x2) return "";

    const bool masked = (buffer[1] & 0x80) != 0;
    const uint8_t payloadLen = buffer[1] & 0x7F;

    size_t payloadStart = 2;
    uint64_t actualPayloadLen = payloadLen;

    if (payloadLen == 126) {
        if (length < 4) return "";

        actualPayloadLen = buffer[2] << 8 | buffer[3];
        payloadStart += 2;
    } else if (payloadLen == 127) {
        if (length < 10) return "";

        actualPayloadLen = 0;

        for (int i = 0; i < 8; ++i) {
            actualPayloadLen = actualPayloadLen << 8 | buffer[i + 2];
        }

        payloadStart += 8;
    }

    uint8_t maskingKey[4] = {0};
    if (masked) {
        if (payloadStart + 4 > length) return "";

        for (int i = 0; i < 4; ++i) {
            maskingKey[i] = buffer[payloadStart + i];
        }

        payloadStart += 4;
    }

    if (payloadStart + actualPayloadLen > length) {
        return "";
    }

    string payload;
    for (size_t i = 0; i < actualPayloadLen; ++i) {
        uint8_t byte = buffer[payloadStart + i];

        if (masked) {
            byte ^= maskingKey[i % 4];
        }

        payload += byte;
    }

    return payload;
}

int main() {
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    const string serverAddress = "127.0.0.1";
    constexpr int serverPort = 8080;

    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create server socket." << endl;

        return 1;
    }

    constexpr int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Failed to set socket options." << endl;

        close(serverSocket);

        return 1;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverAddress.c_str(), &address.sin_addr) <= 0) {
        cerr << "Invalid address format." << endl;

        close(serverSocket);

        return 1;
    }

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        cerr << "Failed to bind socket to address." << endl;

        close(serverSocket);

        return 1;
    }

    if (listen(serverSocket, 3) < 0) {
        cerr << "Failed to listen for connections." << endl;

        close(serverSocket);

        return 1;
    }

    cout << "WebSocket server started on " << serverAddress << ":" << serverPort << endl;
    cout << "Press Ctrl+C to stop the server" << endl;

    while (running) {
        sockaddr_in clientAddress;
        socklen_t clientAddressLen = sizeof(clientAddress);
        const int clientSocket = accept(serverSocket, reinterpret_cast<struct sockaddr*>(&clientAddress), &clientAddressLen);

        if (clientSocket < 0) {
            if (running) {
                cerr << "Failed to accept connection." << endl;
            }
            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddress.sin_addr), clientIp, INET_ADDRSTRLEN);

        cout << "New connection from " << clientIp << ":" << ntohs(clientAddress.sin_port) << endl;

        char buffer[4096] = {0};
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead <= 0) {
            cerr << "Failed to read handshake request." << endl;

            close(clientSocket);

            continue;
        }

        if (string request(buffer); request.find("Upgrade: websocket") != string::npos &&
                                    request.find("Connection: Upgrade") != string::npos) {

            string webSocketKey = extractWebSocketKey(request);
            if (webSocketKey.empty()) {
                cerr << "Invalid WebSocket request (missing key)." << endl;

                close(clientSocket);

                continue;
            }

            string acceptKey = computeAcceptKey(webSocketKey);

            string response = createHandshakeResponse(acceptKey);
            if (send(clientSocket, response.c_str(), response.length(), 0) < 0) {
                cerr << "Failed to send handshake response." << endl;

                close(clientSocket);

                continue;
            }

            cout << "WebSocket handshake successful with " << clientIp << endl;

            bool clientConnected = true;
            while (running && clientConnected) {
                memset(buffer, 0, sizeof(buffer));
                bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

                if (bytesRead <= 0) {
                    cout << "Connection closed by client " << clientIp << endl;

                    clientConnected = false;

                    break;
                }

                string message = decodeWebSocketFrame(reinterpret_cast<const uint8_t*>(buffer), bytesRead);

                if (message == "CLOSE_CONNECTION") {
                    cout << "Received close frame from " << clientIp << endl;

                    clientConnected = false;

                    break;
                }

                if (!message.empty()) {
                    cout << "Received message from " << clientIp << ": " << message << endl;

                    vector<uint8_t> frame = createWebSocketFrame(message);
                    if (send(clientSocket, frame.data(), frame.size(), 0) < 0) {
                        cerr << "Failed to send message back to client." << endl;
                        clientConnected = false;
                        break;
                    }

                    cout << "Sent message back to " << clientIp << endl;
                }
            }
        } else {
            cerr << "Not a WebSocket upgrade request." << endl;

            string httpResponse = "HTTP/1.1 400 Bad Request\r\n"
                                 "Content-Type: text/plain\r\n"
                                 "Connection: close\r\n"
                                 "\r\n"
                                 "This server only accepts WebSocket connections.";

            send(clientSocket, httpResponse.c_str(), httpResponse.length(), 0);
        }

        close(clientSocket);
    }

    close(serverSocket);

    cout << "Server shutdown complete." << endl;

    return 0;
}
