#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <vector>
#include <chrono>
#include <thread>

using namespace std;

bool knockUdpPort(const string &serverIp, const int port) {
    const int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket < 0) {
        cerr << "Failed to create UDP socket." << endl;

        return false;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid ip address format." << endl;

        close(udpSocket);

        return false;
    }

    timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        cerr << "Failed to set socket timeout." << endl;

        close(udpSocket);

        return false;
    }

    const string message = "PING";
    if (sendto(udpSocket, message.c_str(), message.length(), 0,
              reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to send UDP message to port " << port << endl;

        close(udpSocket);

        return false;
    }

    char buffer[4096] = {};
    sockaddr_in responseAddress;
    socklen_t responseAddressLength = sizeof(responseAddress);

    const ssize_t bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer) - 1, 0,
                             reinterpret_cast<sockaddr *>(&responseAddress), &responseAddressLength);

    close(udpSocket);

    if (bytesRead < 0) {
        return false;
    }

    return string(buffer, bytesRead) == "PONG";
}

bool connectToTcpServer(const string &serverIp, const int port, string &response) {
    const int tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSocket < 0) {
        cerr << "Failed to create TCP socket." << endl;
        return false;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid ip address format." << endl;

        close(tcpSocket);

        return false;
    }

    timeval timeout;
    timeout.tv_sec = 3;
    timeout.tv_usec = 0;
    if (setsockopt(tcpSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        cerr << "Failed to set socket timeout." << endl;

        close(tcpSocket);

        return false;
    }

    if (connect(tcpSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect to TCP server on port " << port << endl;

        close(tcpSocket);

        return false;
    }

    cout << "Connected to TCP server on port " << port << endl;

    char buffer[4096] = {};
    const ssize_t bytesRead = recv(tcpSocket, buffer, sizeof(buffer) - 1, 0);

    close(tcpSocket);

    if (bytesRead < 0) {
        cerr << "Failed to receive data from TCP server." << endl;

        return false;
    }

    if (bytesRead == 0) {
        cerr << "Connection closed by TCP server." << endl;

        return false;
    }

    response = string(buffer, bytesRead);

    return true;
}

int main() {
    const string serverIp = "212.182.24.27";
    constexpr int tcpPort = 2913;

    cout << "Starting port-knocking client for " << serverIp << endl;
    cout << "Looking for UDP ports ending with 666 that respond with PONG..." << endl;

    vector<int> knockSequence;

    constexpr int startPort = 1000;
    constexpr int endPort = 65535;
    constexpr int suffixToFind = 666;

    for (int port = startPort; port <= endPort; port++) {
        if (port % 1000 == suffixToFind) {
            cout << "Trying UDP port " << port << "..." << flush;

            if (knockUdpPort(serverIp, port)) {
                cout << " [FOUND]" << endl;
                knockSequence.push_back(port);
            } else {
                cout << " [not part of sequence]" << endl;
            }

            this_thread::sleep_for(chrono::milliseconds(100));
        }
    }

    cout << "Discovered knock sequence: ";
    for (size_t i = 0; i < knockSequence.size(); i++) {
        cout << knockSequence[i];
        if (i < knockSequence.size() - 1) {
            cout << ", ";
        }
    }
    cout << endl;

    cout << "Applying the knock sequence..." << endl;
    for (const int port : knockSequence) {
        cout << "Knocking on UDP port " << port << endl;
        knockUdpPort(serverIp, port);

        this_thread::sleep_for(chrono::milliseconds(500));
    }

    cout << "Waiting for the TCP port to open..." << endl;
    this_thread::sleep_for(chrono::seconds(2));

    if (string response; connectToTcpServer(serverIp, tcpPort, response)) {
        cout << "Successfully connected to hidden TCP service!" << endl;
        cout << "Server response: " << response << endl;
    } else {
        cerr << "Failed to connect to the hidden TCP service." << endl;
    }

    return 0;
}
