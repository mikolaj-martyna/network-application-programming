#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <filesystem>
#include <sstream>

using namespace std;
namespace fs = std::filesystem;

int main() {
    string serverIp = "127.0.0.1";
    int serverPort = 2904;
    string imagesDirectory = "./";

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Failed to set socket options." << endl;

        close(serverSocket);

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid address." << endl;

        close(serverSocket);

        return 1;
    }

    if (bind(serverSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to bind socket." << endl;

        close(serverSocket);

        return 1;
    }

    if (listen(serverSocket, 5) < 0) {
        cerr << "Failed to listen for connections." << endl;

        close(serverSocket);

        return 1;
    }

    cout << "Server listening on " << serverIp << ":" << serverPort << "." << endl;
    cout << "Serving images from directory: " << imagesDirectory << endl;

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress), &clientAddressLength);
        if (clientSocket < 0) {
            cerr << "Failed to accept connection." << endl;
            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
        cout << "New connection from " << clientIp << ":" << ntohs(clientAddress.sin_port) << "." << endl;

        char buffer[1024] = {0};
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            cerr << "Failed to read from client or connection closed." << endl;

            close(clientSocket);
            continue;
        }

        string request(buffer, bytesRead);
        cout << "Received request: " << request << endl;

        if (request.compare(0, 10, "GET_IMAGE ") == 0) {
            string imageName = request.substr(10);
            if (imageName.size() >= 2 && imageName.substr(imageName.size() - 2) == "\r\n") {
                imageName = imageName.substr(0, imageName.size() - 2);
            }

            string imagePath = imagesDirectory + imageName;
            cout << "Requested image: " << imagePath << endl;

            ifstream imageFile(imagePath, ios::binary | ios::ate);
            if (!imageFile) {
                cerr << "Failed to open image file: " << imagePath << "." << endl;

                string errorResponse = "ERROR File not found\r\n";
                send(clientSocket, errorResponse.c_str(), errorResponse.length(), 0);

                close(clientSocket);
                continue;
            }

            size_t fileSize = imageFile.tellg();
            imageFile.seekg(0, ios::beg);

            vector<char> imageData(fileSize);
            imageFile.read(imageData.data(), fileSize);
            imageFile.close();

            string header = "SIZE " + to_string(fileSize) + " NAME " + imageName + "\r\n";
            cout << "Sending header: " << header << endl;

            if (send(clientSocket, header.c_str(), header.length(), 0) < 0) {
                cerr << "Failed to send header." << endl;

                close(clientSocket);
                continue;
            }

            size_t sentBytes = 0;
            while (sentBytes < fileSize) {
                size_t chunkSize = min(static_cast<size_t>(4096), fileSize - sentBytes);
                ssize_t sent = send(clientSocket, imageData.data() + sentBytes, chunkSize, 0);

                if (sent <= 0) {
                    cerr << "Failed to send image data." << endl;
                    break;
                }

                sentBytes += sent;
                cout << "Sent " << sentBytes << " of " << fileSize << " bytes ("
                        << (static_cast<double>(sentBytes) / fileSize * 100) << "%)." << endl;
            }

            cout << "Image transfer complete." << endl;
        } else if (request == "GET_IMAGE\r\n") {
            string defaultImage = "test.jpg";
            string imagePath = imagesDirectory + defaultImage;

            ifstream imageFile(imagePath, ios::binary | ios::ate);
            if (!imageFile) {
                cerr << "Failed to open image file: " << imagePath << "." << endl;

                string errorResponse = "ERROR Default file not found\r\n";
                send(clientSocket, errorResponse.c_str(), errorResponse.length(), 0);

                close(clientSocket);
                continue;
            }

            size_t fileSize = imageFile.tellg();
            imageFile.seekg(0, ios::beg);

            vector<char> imageData(fileSize);
            imageFile.read(imageData.data(), fileSize);
            imageFile.close();

            string header = "SIZE " + to_string(fileSize) + " NAME " + defaultImage + "\r\n";
            cout << "Sending header: " << header << endl;

            if (send(clientSocket, header.c_str(), header.length(), 0) < 0) {
                cerr << "Failed to send header." << endl;

                close(clientSocket);
                continue;
            }

            size_t sentBytes = 0;
            while (sentBytes < fileSize) {
                size_t chunkSize = min(static_cast<size_t>(4096), fileSize - sentBytes);
                ssize_t sent = send(clientSocket, imageData.data() + sentBytes, chunkSize, 0);

                if (sent <= 0) {
                    cerr << "Failed to send image data." << endl;
                    break;
                }

                sentBytes += sent;
                cout << "Sent " << sentBytes << " of " << fileSize << " bytes ("
                        << (static_cast<double>(sentBytes) / fileSize * 100) << "%)." << endl;
            }

            cout << "Image transfer complete." << endl;
        } else if (request == "LIST_FILES\r\n") {
            cout << "Client requested file list" << endl;

            vector<string> fileList;
            for (const auto &entry: fs::directory_iterator(imagesDirectory)) {
                if (entry.is_regular_file()) {
                    if (string filename = entry.path().filename().string();
                        filename.ends_with(".jpg") || filename.ends_with(".jpeg") ||
                        filename.ends_with(".png") || filename.ends_with(".gif") ||
                        filename.ends_with(".bmp")) {
                        fileList.push_back(filename);
                    }
                }
            }

            stringstream response;
            response << "FILES " << fileList.size() << "\r\n";
            for (const auto &file: fileList) {
                response << file << "\r\n";
            }

            string responseStr = response.str();
            cout << "Sending file list with " << fileList.size() << " files" << endl;

            if (send(clientSocket, responseStr.c_str(), responseStr.length(), 0) < 0) {
                cerr << "Failed to send file list." << endl;
            }
        } else {
            string errorResponse = "ERROR Unknown command\r\n";
            send(clientSocket, errorResponse.c_str(), errorResponse.length(), 0);
            cout << "Sent error response for invalid command." << endl;
        }

        close(clientSocket);

        cout << "Connection closed." << endl;
    }

    close(serverSocket);

    return 0;
}
