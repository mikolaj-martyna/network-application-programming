#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <thread>
#include <mutex>
#include <vector>

using namespace std;

mutex logMutex;

string getCurrentTimestamp() {
    const auto now = chrono::system_clock::now();
    const auto now_time_t = chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
    localtime_r(&now_time_t, &tm_now);

    std::ostringstream timestamp;
    timestamp << std::put_time(&tm_now, "%d-%m-%Y %H:%M:%S");

    return timestamp.str();
}

void logEvent(ofstream &logFile, const string &event) {
    lock_guard lock(logMutex);

    if (logFile.is_open()) {
        logFile << getCurrentTimestamp() << " - " << event << endl;
    }
}

void handleClient(const int clientSocket, const string& clientIp, const int clientPort, ofstream &logFile) {
    logEvent(logFile, string("Client connected: ") + clientIp + ":" + to_string(clientPort));

    char buffer[1024] = {};
    const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead < 0) {
        cerr << "Failed to receive data from client." << endl;
        logEvent(logFile, string("Failed to receive data from client: ") + clientIp + ":" + to_string(clientPort));

        close(clientSocket);

        return;
    }

    if (bytesRead == 0) {
        cout << "Client disconnected." << endl;
        logEvent(logFile, string("Client disconnected: ") + clientIp + ":" + to_string(clientPort));

        close(clientSocket);

        return;
    }

    buffer[bytesRead] = '\0';
    cout << "Received " << bytesRead << " bytes from client: " << buffer << endl;
    logEvent(logFile,
             string("Received ") + to_string(bytesRead) + " bytes from " + clientIp + ":" + to_string(clientPort) +
             ": " + buffer);

    if (ssize_t bytesSent = 0; (bytesSent = send(clientSocket, buffer, bytesRead, 0)) < 0) {
        cerr << "Failed to send data to client." << endl;
        logEvent(logFile, string("Failed to send echo response to ") + clientIp + ":" + to_string(clientPort));
    } else {
        cout << "Echoed " << bytesSent << " bytes back to client." << endl;
        logEvent(logFile,
                 string("Echoed ") + to_string(bytesSent) + " bytes back to " + clientIp + ":" + to_string(
                     clientPort));
    }

    close(clientSocket);
    logEvent(logFile, string("Closed connection with ") + clientIp + ":" + to_string(clientPort));
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 8002;

    ofstream logFile("server_log.txt", ios::app);
    if (!logFile.is_open()) {
        cerr << "Failed to open log file." << endl;
    } else {
        logEvent(logFile, "Server started");
    }

    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create socket." << endl;
        logEvent(logFile, "Failed to create socket");

        return 1;
    }
    logEvent(logFile, "Socket created successfully");

    constexpr int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Failed to set socket options." << endl;
        logEvent(logFile, "Failed to set socket options");

        close(serverSocket);

        return 1;
    }
    logEvent(logFile, "Socket options set successfully");

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address format." << endl;
        logEvent(logFile, "Invalid IP address format");

        close(serverSocket);

        return 1;
    }
    logEvent(logFile, "IP address converted successfully");

    if (bind(serverSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to bind socket to address." << endl;
        logEvent(logFile, "Failed to bind socket to address");

        close(serverSocket);

        return 1;
    }
    logEvent(logFile, "Socket bound to address successfully");

    if (listen(serverSocket, 10) < 0) { // Increased backlog to 10
        cerr << "Failed to listen for connections." << endl;
        logEvent(logFile, "Failed to listen for connections");

        close(serverSocket);

        return 1;
    }
    logEvent(logFile, "Server listening on " + serverIp + ":" + to_string(serverPort));
    cout << "Echo server is listening on " << serverIp << ":" << serverPort << endl;

    vector<thread> clientThreads;

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);
        const int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress),
                                        &clientAddressLength);

        if (clientSocket < 0) {
            cerr << "Failed to accept connection." << endl;
            logEvent(logFile, "Failed to accept connection");
            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
        const int clientPort = ntohs(clientAddress.sin_port);
        cout << "Client connected: " << clientIp << ":" << clientPort << endl;

        clientThreads.emplace_back(handleClient, clientSocket, string(clientIp), clientPort, ref(logFile));
        clientThreads.back().detach();
    }

    close(serverSocket);
    logEvent(logFile, "Server shutting down");
    logFile.close();

    return 0;
}
