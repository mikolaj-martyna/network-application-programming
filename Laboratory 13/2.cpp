#include <iostream>
#include <fstream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstream>
#include <vector>

using namespace std;

void receiveFileList(int socket) {
    char buffer[4096] = {};
    string response;
    bool headerReceived = false;
    size_t fileCount = 0;

    while (!headerReceived) {
        ssize_t bytesRead = recv(socket, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead <= 0) {
            cerr << "Failed to receive response or connection closed." << endl;
            return;
        }

        response.append(buffer, bytesRead);

        if (size_t endPos = response.find("\r\n"); endPos != string::npos) {
            string header = response.substr(0, endPos);
            istringstream headerStream(header);

            string prefix;
            headerStream >> prefix >> fileCount;

            if (prefix != "FILES" || headerStream.fail()) {
                cerr << "Invalid file list format." << endl;
                return;
            }

            headerReceived = true;
            response = response.substr(endPos + 2);
        }
    }

    cout << "Available files (" << fileCount << "):" << endl;

    istringstream fileStream(response);
    string line;
    size_t filesProcessed = 0;

    while (filesProcessed < fileCount && getline(fileStream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (!line.empty()) {
            cout << " - " << line << endl;
            filesProcessed++;
        }
    }

    while (filesProcessed < fileCount) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytesRead = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            cerr << "Failed to receive complete file list." << endl;
            return;
        }

        string chunk(buffer, bytesRead);
        istringstream chunkStream(chunk);

        while (filesProcessed < fileCount && getline(chunkStream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (!line.empty()) {
                cout << " - " << line << endl;
                filesProcessed++;
            }
        }
    }
}

void downloadImage(int socket, const string &imageName) {
    string request;
    if (imageName.empty()) {
        request = "GET_IMAGE\r\n";
    } else {
        request = "GET_IMAGE " + imageName + "\r\n";
    }

    if (send(socket, request.c_str(), request.length(), 0) < 0) {
        cerr << "Failed to send request." << endl;
        return;
    }

    char buffer[4096] = {};
    string header;
    bool headerReceived = false;

    while (!headerReceived) {
        const ssize_t bytesRead = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            cerr << "Failed to receive header or connection closed." << endl;
            return;
        }

        header.append(buffer, bytesRead);

        if (size_t headerEndPos = header.find("\r\n"); headerEndPos != string::npos) {
            headerReceived = true;
            header = header.substr(0, headerEndPos);
        }
    }

    if (header.find("ERROR") == 0) {
        cerr << "Server returned error: " << header << endl;
        return;
    }

    istringstream headerStream(header);
    size_t fileSize;
    string fileName;
    string prefix;

    headerStream >> prefix >> fileSize >> prefix >> fileName;

    if (prefix != "SIZE" || headerStream.fail()) {
        cerr << "Invalid header format." << endl;
        return;
    }

    cout << "Receiving file: " << fileName << ", size: " << fileSize << " bytes" << endl;

    vector<char> imageData;
    imageData.reserve(fileSize);

    size_t receivedTotal = 0;
    size_t remainingInBuffer = header.size() - (header.find("\r\n") + 2);

    if (remainingInBuffer > 0) {
        imageData.insert(imageData.end(), buffer + bytesRead - remainingInBuffer, buffer + bytesRead);
        receivedTotal += remainingInBuffer;
    }

    while (receivedTotal < fileSize) {
        memset(buffer, 0, sizeof(buffer));
        const ssize_t bytesRead = recv(socket, buffer, sizeof(buffer), 0);

        if (bytesRead <= 0) {
            cerr << "Failed to receive image data or connection closed." << endl;
            return;
        }

        imageData.insert(imageData.end(), buffer, buffer + bytesRead);
        receivedTotal += bytesRead;
        cout << "Received " << receivedTotal << " of " << fileSize << " bytes ("
                << (static_cast<double>(receivedTotal) / fileSize * 100) << "%)" << endl;
    }

    ofstream imageFile(fileName, ios::binary);
    if (!imageFile) {
        cerr << "Failed to create output file." << endl;
        return;
    }

    imageFile.write(imageData.data(), imageData.size());
    cout << "Image saved to " << fileName << endl;
    imageFile.close();
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 2904;

    int option;
    string imageName;

    cout << "Choose an option:" << endl;
    cout << "1. List available files" << endl;
    cout << "2. Download default image (test.jpg)" << endl;
    cout << "3. Download specific image" << endl;
    cout << "Option: ";
    cin >> option;
    cin.ignore();

    if (option == 3) {
        cout << "Enter image name: ";
        getline(cin, imageName);
    }

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address format." << endl;

        close(clientSocket);

        return 1;
    }

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect to server." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Connected to server " << serverIp << ":" << serverPort << endl;

    switch (option) {
        case 1: {
            const string request = "LIST_FILES\r\n";
            if (send(clientSocket, request.c_str(), request.length(), 0) < 0) {
                cerr << "Failed to send request." << endl;

                close(clientSocket);

                return 1;
            }

            receiveFileList(clientSocket);
            break;
        }
        case 2:
            downloadImage(clientSocket, "");
            break;
        case 3:
            downloadImage(clientSocket, imageName);
            break;
        default:
            cout << "Invalid option." << endl;
            break;
    }

    close(clientSocket);

    return 0;
}
