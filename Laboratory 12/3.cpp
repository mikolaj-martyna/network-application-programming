#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string>
#include <random>
#include <sstream>
#include <thread>
#include <vector>
#include <mutex>

using namespace std;

mutex consoleMutex;

random_device rd;
mt19937 gen(rd());
uniform_int_distribution<> distrib(1, 100);
mutex randomMutex;

int generateRandomNumber() {
    lock_guard lock(randomMutex);

    return distrib(gen);
}

void handleClient(const int clientSocket, const string& clientIp, const int clientPort) {
    const int secretNumber = generateRandomNumber();

    {
        lock_guard lock(consoleMutex);
        cout << "Client connected: " << clientIp << ":" << clientPort << " (secret number: " << secretNumber << ")" << endl;
    }

    bool guessedCorrectly = false;

    while (!guessedCorrectly) {
        char buffer[1024] = {};
        const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead < 0) {
            lock_guard lock(consoleMutex);
            cerr << "Failed to receive data from client " << clientIp << ":" << clientPort << endl;
            break;
        }

        if (bytesRead == 0) {
            lock_guard lock(consoleMutex);
            cout << "Client disconnected: " << clientIp << ":" << clientPort << endl;
            break;
        }

        buffer[bytesRead] = '\0';

        lock_guard lock(consoleMutex);
        cout << "Received " << bytesRead << " bytes from client " << clientIp << ":" << clientPort << ": " << buffer << endl;

        string responseMsg;
        istringstream iss(buffer);

        if (int guess; iss >> guess) {
            if (guess < secretNumber) {
                responseMsg = "Too low.";
            } else if (guess > secretNumber) {
                responseMsg = "Too high.";
            } else {
                responseMsg = "Correct.";
                guessedCorrectly = true;
            }
        } else {
            responseMsg = "Error: Please send a valid number.";
        }

        if (const ssize_t bytesSent = send(clientSocket, responseMsg.c_str(), responseMsg.length(), 0); bytesSent < 0) {
            lock_guard lock(consoleMutex);
            cerr << "Failed to send data to client " << clientIp << ":" << clientPort << endl;
            break;
        }

        {
            lock_guard lock(consoleMutex);
            cout << "Sent response to client " << clientIp << ":" << clientPort << ": " << responseMsg << endl;
        }
    }

    close(clientSocket);
    {
        lock_guard<mutex> lock(consoleMutex);
        cout << "Connection closed with client: " << clientIp << ":" << clientPort << endl;
    }
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 8003;

    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    constexpr int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Failed to set socket options." << endl;

        close(serverSocket);

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address format." << endl;

        close(serverSocket);

        return 1;
    }

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to bind socket to address." << endl;

        close(serverSocket);

        return 1;
    }

    if (listen(serverSocket, 10) < 0) {
        cerr << "Failed to listen for connections." << endl;

        close(serverSocket);

        return 1;
    }
    cout << "Number guessing game server is listening on " << serverIp << ":" << serverPort << endl;
    cout << "Each client will receive a unique number to guess." << endl;

    vector<thread> clientThreads;

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

        clientThreads.emplace_back(handleClient, clientSocket, string(clientIp), clientPort);
        clientThreads.back().detach();
    }

    close(serverSocket);

    return 0;
}
