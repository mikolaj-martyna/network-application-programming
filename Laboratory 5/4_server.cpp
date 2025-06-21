#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <vector>

using namespace std;
using namespace std::chrono;

constexpr int PACKET_SIZE = 1024;
constexpr int NUM_PACKETS = 1000;

void handleTcpConnection(const int clientSocket) {
    cout << "TCP connection established." << endl;

    char buffer[PACKET_SIZE] = {};
    int packetsReceived = 0;
    const auto startTime = high_resolution_clock::now();

    while (packetsReceived < NUM_PACKETS) {
        memset(buffer, 0, sizeof(buffer));

        if (const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0); bytesRead <= 0) {
            break;
        }

        packetsReceived++;

        send(clientSocket, "ACK", 3, 0);
    }

    const auto endTime = high_resolution_clock::now();
    const auto duration = duration_cast<microseconds>(endTime - startTime);

    cout << "TCP: Received " << packetsReceived << " packets in "
         << duration.count() / 1000.0 << " ms." << endl;
    cout << "TCP: Average time per packet: " << duration.count() / static_cast<double>(packetsReceived) << " us." << endl;
}

void runUdpServer(const int port) {
    const int udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket < 0) {
        cerr << "Failed to create UDP socket." << endl;

        return;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(udpSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to bind UDP socket." << endl;

        close(udpSocket);

        return;
    }

    cout << "UDP server started on port " << port << "." << endl;

    char buffer[PACKET_SIZE] = {};
    sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    int packetsReceived = 0;

    const auto startTime = high_resolution_clock::now();

    while (packetsReceived < NUM_PACKETS) {
        memset(buffer, 0, sizeof(buffer));

        const ssize_t bytesRead = recvfrom(udpSocket, buffer, sizeof(buffer), 0,
                                   reinterpret_cast<sockaddr *>(&clientAddress), &clientAddressLength);
        if (bytesRead <= 0) {
            continue;
        }

        packetsReceived++;

        sendto(udpSocket, "ACK", 3, 0,
              reinterpret_cast<const sockaddr *>(&clientAddress), clientAddressLength);
    }

    const auto endTime = high_resolution_clock::now();
    const auto duration = duration_cast<microseconds>(endTime - startTime);

    cout << "UDP: Received " << packetsReceived << " packets in "
         << duration.count() / 1000.0 << " ms." << endl;
    cout << "UDP: Average time per packet: " << duration.count() / static_cast<double>(packetsReceived) << " us." << endl;

    close(udpSocket);
}

void runTcpServer(const int port) {
    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create TCP socket." << endl;

        return;
    }

    constexpr int enable = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        cerr << "Failed to set socket options." << endl;

        close(serverSocket);

        return;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to bind TCP socket." << endl;

        close(serverSocket);

        return;
    }

    if (listen(serverSocket, 1) < 0) {
        cerr << "Failed to listen for connections." << endl;

        close(serverSocket);

        return;
    }

    cout << "TCP server started on port " << port << ". Waiting for connection..." << endl;

    sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    const int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress), &clientAddressLength);

    if (clientSocket < 0) {
        cerr << "Failed to accept connection." << endl;

        close(serverSocket);

        return;
    }

    handleTcpConnection(clientSocket);

    close(clientSocket);
    close(serverSocket);
}

int main() {
    constexpr int tcpPort = 8888;
    constexpr int udpPort = 8889;

    cout << "Starting transmission test server..." << endl;
    cout << "TCP port: " << tcpPort << ", UDP port: " << udpPort << endl;
    cout << "Waiting for " << NUM_PACKETS << " packets of " << PACKET_SIZE << " bytes each." << endl;

    runTcpServer(tcpPort);
    runUdpServer(udpPort);

    cout << "Tests completed." << endl;

    return 0;
}
