#include <iostream>
#include <fstream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <filesystem>
#include <string>
#include <vector>

using namespace std;
namespace fs = filesystem;

constexpr int BUFFER_SIZE = 4096;
constexpr int PORT = 8080;
constexpr auto SERVER_IP = "127.0.0.1";

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

Protocol parseResponse(const string &message) {
    if (message.empty()) {
        return {CommandType::ERROR, "Empty response"};
    }

    if (message.substr(0, 2) == "OK") {
        return {CommandType::OK, message.length() > 3 ? message.substr(3) : ""};
    }

    if (message.substr(0, 5) == "ERROR") {
        return {CommandType::ERROR, message.length() > 6 ? message.substr(6) : ""};
    }

    return {CommandType::UNKNOWN, message};
}

bool sendCommand(const int socket, const string &command) {
    return send(socket, command.c_str(), command.length(), 0) > 0;
}

Protocol receiveResponse(const int socket) {
    char buffer[BUFFER_SIZE] = {};
    const ssize_t bytesRead = recv(socket, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead <= 0) {
        return {CommandType::ERROR, "Connection closed"};
    }

    buffer[bytesRead] = '\0';

    return parseResponse(buffer);
}

bool isImageFile(const string &filePath) {
    const string extension = filePath.substr(filePath.find_last_of('.') + 1);

    return extension == "jpg" || extension == "jpeg" || extension == "png" ||
           extension == "gif" || extension == "bmp";
}

int main() {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <image_file_path>" << endl;

        return 1;
    }

    const string filePath = argv[1];

    if (!fs::exists(filePath)) {
        cerr << "File not found: " << filePath << endl;

        return 1;
    }

    if (!isImageFile(filePath)) {
        cerr << "Only image files are supported." << endl;

        return 1;
    }

    ifstream inputFile(filePath, ios::binary);
    if (!inputFile) {
        cerr << "Failed to open file: " << filePath << endl;

        return 1;
    }

    const size_t fileSize = fs::file_size(filePath);
    const string fileName = fs::path(filePath).filename().string();

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address format." << endl;

        close(clientSocket);

        return 1;
    }

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect to server." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Connected to server at " << SERVER_IP << ":" << PORT << endl;

    if (!sendCommand(clientSocket, "HELLO")) {
        cerr << "Failed to send HELLO command." << endl;

        close(clientSocket);

        return 1;
    }

    Protocol response = receiveResponse(clientSocket);
    if (response.type != CommandType::OK) {
        cerr << "Server error: " << response.payload << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Server response: " << response.payload << endl;

    if (!sendCommand(clientSocket, "SEND_FILE")) {
        cerr << "Failed to send SEND_FILE command." << endl;

        close(clientSocket);

        return 1;
    }

    response = receiveResponse(clientSocket);
    if (response.type != CommandType::OK) {
        cerr << "Server error: " << response.payload << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Server ready to receive file info." << endl;

    if (const string fileInfo = "FILE_INFO " + fileName + " " + to_string(fileSize); !sendCommand(clientSocket, fileInfo)) {
        cerr << "Failed to send file info." << endl;

        close(clientSocket);

        return 1;
    }

    response = receiveResponse(clientSocket);
    if (response.type != CommandType::OK) {
        cerr << "Server error: " << response.payload << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Sending file: " << fileName << " (" << fileSize << " bytes)" << endl;

    vector<char> buffer(BUFFER_SIZE);
    size_t totalSent = 0;

    while (inputFile && totalSent < fileSize) {
        inputFile.read(buffer.data(), BUFFER_SIZE - 5);
        const size_t bytesRead = inputFile.gcount();

        if (bytesRead == 0) {
            break;
        }

        vector<char> sendBuffer(bytesRead + 5);
        const string prefix = "DATA ";
        memcpy(sendBuffer.data(), prefix.c_str(), 5);
        memcpy(sendBuffer.data() + 5, buffer.data(), bytesRead);

        if (send(clientSocket, sendBuffer.data(), bytesRead + 5, 0) <= 0) {
            cerr << "Failed to send data." << endl;

            close(clientSocket);

            return 1;
        }

        response = receiveResponse(clientSocket);
        if (response.type != CommandType::OK) {
            cerr << "Server error: " << response.payload << endl;

            close(clientSocket);

            return 1;
        }

        totalSent += bytesRead;
        cout << "\rProgress: " << (totalSent * 100 / fileSize) << "%" << flush;
    }

    cout << endl;
    inputFile.close();

    if (!sendCommand(clientSocket, "END")) {
        cerr << "Failed to send END command." << endl;

        close(clientSocket);

        return 1;
    }

    response = receiveResponse(clientSocket);
    if (response.type != CommandType::OK) {
        cerr << "File transfer failed: " << response.payload << endl;

        close(clientSocket);

        return 1;
    }

    cout << "File transfer complete: " << response.payload << endl;
    close(clientSocket);

    return 0;
}
