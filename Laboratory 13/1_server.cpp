#include <iostream>
#include <fstream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <filesystem>
#include <string>

using namespace std;
namespace fs = filesystem;

constexpr int BUFFER_SIZE = 4096;
constexpr int PORT = 8080;

enum class CommandType {
    HELLO,
    SEND_FILE,
    FILE_INFO,
    DATA,
    END,
    OK,
    ERROR,
    UNKNOWN
};

struct Protocol {
    CommandType type;
    string payload;
};

Protocol parseCommand(const string &message) {
    if (message.empty()) {
        return {CommandType::ERROR, "Empty command"};
    }

    if (message.substr(0, 5) == "HELLO") {
        return {CommandType::HELLO, ""};
    }

    if (message.substr(0, 9) == "SEND_FILE") {
        return {CommandType::SEND_FILE, ""};
    }

    if (message.substr(0, 9) == "FILE_INFO") {
        if (const size_t separator = message.find(' ', 10); separator != string::npos) {
            return {CommandType::FILE_INFO, message.substr(10)};
        }

        return {CommandType::ERROR, "Invalid FILE_INFO format"};
    }

    if (message.substr(0, 4) == "DATA") {
        return {CommandType::DATA, message.substr(5)};
    }

    if (message.substr(0, 3) == "END") {
        return {CommandType::END, ""};
    }

    return {CommandType::UNKNOWN, message};
}

Protocol parseDataCommand(const char* buffer, size_t length) {
    if (length < 5 || strncmp(buffer, "DATA ", 5) != 0) {
        return {CommandType::ERROR, "Invalid DATA command format"};
    }

    return {CommandType::DATA, string(buffer + 5, length - 5)};
}

string createResponse(const CommandType type, const string &message = "") {
    switch (type) {
        case CommandType::OK:
            return "OK" + (message.empty() ? "" : " " + message);
        case CommandType::ERROR:
            return "ERROR" + (message.empty() ? "" : " " + message);
        default:
            return "ERROR Unknown response type";
    }
}

int main() {
    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    constexpr int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Failed to set socket options." << endl;

        close(serverSocket);

        return 1;
    }

    if (bind(serverSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to bind socket." << endl;

        close(serverSocket);

        return 1;
    }

    if (listen(serverSocket, 5) < 0) {
        cerr << "Failed to listen on socket." << endl;

        close(serverSocket);

        return 1;
    }

    cout << "Server started on port " << PORT << endl;

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        const int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress),
                                        &clientAddressLength);
        if (clientSocket < 0) {
            cerr << "Failed to accept connection." << endl;
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, INET_ADDRSTRLEN);
        cout << "Client connected: " << clientIP << endl;

        bool isReceivingFile = false;
        ofstream outputFile;
        string fileName;
        size_t fileSize = 0;
        size_t receivedBytes = 0;

        while (true) {
            char buffer[BUFFER_SIZE] = {};
            const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);

            if (bytesRead <= 0) {
                cout << "Client disconnected." << endl;

                if (isReceivingFile && outputFile.is_open()) {
                    outputFile.close();

                    if (receivedBytes < fileSize) {
                        fs::remove(fileName);
                    }
                }

                break;
            }

            Protocol command;
            if (bytesRead >= 5 && strncmp(buffer, "DATA ", 5) == 0) {
                command = parseDataCommand(buffer, bytesRead);
            } else {
                buffer[bytesRead] = '\0';
                command = parseCommand(buffer);
            }

            string response;

            switch (command.type) {
                case CommandType::HELLO:
                    response = createResponse(CommandType::OK, "Ready for file transfer");
                    break;

                case CommandType::SEND_FILE:
                    if (isReceivingFile) {
                        response = createResponse(CommandType::ERROR, "Already receiving a file");
                    } else {
                        isReceivingFile = true;
                        response = createResponse(CommandType::OK, "Ready for file info");
                    }
                    break;

                case CommandType::FILE_INFO:
                    if (!isReceivingFile) {
                        response = createResponse(CommandType::ERROR, "Not in file transfer mode");
                        break;
                    }

                    try {
                        const size_t separator = command.payload.find(' ');
                        if (separator == string::npos) {
                            response = createResponse(CommandType::ERROR, "Invalid file info format");
                            isReceivingFile = false;
                            break;
                        }

                        fileName = command.payload.substr(0, separator);
                        fileSize = stoull(command.payload.substr(separator + 1));

                        if (const string extension = fileName.substr(fileName.find_last_of('.') + 1);
                            extension != "jpg" && extension != "jpeg" && extension != "png" && extension != "gif" &&
                            extension != "bmp") {
                            response = createResponse(CommandType::ERROR, "Only image files are supported");
                            isReceivingFile = false;
                            break;
                        }

                        outputFile.open(fileName, ios::binary);
                        if (!outputFile) {
                            response = createResponse(CommandType::ERROR, "Failed to create output file");
                            isReceivingFile = false;
                            break;
                        }

                        receivedBytes = 0;
                        response = createResponse(CommandType::OK, "Ready for data");
                    } catch (const exception &e) {
                        response = createResponse(CommandType::ERROR, "Invalid file info: " + string(e.what()));
                        isReceivingFile = false;

                        if (outputFile.is_open()) {
                            outputFile.close();
                        }
                    }
                    break;

                case CommandType::DATA:
                    if (!isReceivingFile || !outputFile.is_open()) {
                        response = createResponse(CommandType::ERROR, "Not ready for data");
                        break;
                    }

                    outputFile.write(command.payload.data(), command.payload.length());
                    receivedBytes += command.payload.length();
                    response = createResponse(CommandType::OK, "Data received");
                    break;

                case CommandType::END:
                    if (!isReceivingFile) {
                        response = createResponse(CommandType::ERROR, "Not in file transfer mode");
                        break;
                    }

                    if (outputFile.is_open()) {
                        outputFile.close();
                    }

                    if (receivedBytes == fileSize) {
                        response = createResponse(CommandType::OK, "File received successfully");
                        cout << "File received: " << fileName << " (" << fileSize << " bytes)" << endl;
                    } else {
                        response = createResponse(CommandType::ERROR, "File size mismatch");
                        fs::remove(fileName);
                        cout << "File transfer failed: size mismatch: received "
                             << receivedBytes << " of " << fileSize << " bytes" << endl;
                    }

                    isReceivingFile = false;
                    break;

                case CommandType::ERROR:
                case CommandType::UNKNOWN:
                default:
                    response = createResponse(CommandType::ERROR, "Invalid command");
                    break;
            }

            send(clientSocket, response.c_str(), response.length(), 0);
        }

        close(clientSocket);
    }

    close(serverSocket);
    return 0;
}
