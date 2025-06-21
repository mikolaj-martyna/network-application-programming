#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <vector>

using namespace std;
using namespace std::chrono;

constexpr int PACKET_SIZE = 1024;
constexpr int NUM_PACKETS = 1000;

void runTcpClient(const string &serverIp, const int port) {
    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create TCP socket." << endl;
        return;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid ip address format." << endl;

        close(clientSocket);

        return;
    }

    cout << "Connecting to TCP server at " << serverIp << ":" << port << "..." << endl;

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect to TCP server." << endl;

        close(clientSocket);

        return;
    }

    cout << "Connected to TCP server. Sending " << NUM_PACKETS << " packets..." << endl;

    const vector data(PACKET_SIZE, 'A');
    char ackBuffer[16] = {};
    int packetsSent = 0;
    int packetsAcknowledged = 0;

    const auto startTime = high_resolution_clock::now();

    for (int i = 0; i < NUM_PACKETS; i++) {
        if (send(clientSocket, data.data(), PACKET_SIZE, 0) < 0) {
            cerr << "Failed to send TCP packet." << endl;
            break;
        }

        packetsSent++;

        if (recv(clientSocket, ackBuffer, sizeof(ackBuffer), 0) > 0) {
            packetsAcknowledged++;
        }
    }

    const auto endTime = high_resolution_clock::now();
    const auto duration = duration_cast<microseconds>(endTime - startTime);

    cout << "TCP: Sent " << packetsSent << " packets, acknowledged " << packetsAcknowledged
         << " in " << duration.count() / 1000.0 << " ms." << endl;
    cout << "TCP: Average round-trip time per packet: " << duration.count() / static_cast<double>(packetsSent) << " us." << endl;

    close(clientSocket);
}

void runUdpClient(const string &serverIp, const int port) {
    const int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create UDP socket." << endl;

        return;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid ip address format." << endl;

        close(clientSocket);

        return;
    }

    cout << "Preparing to send UDP packets to " << serverIp << ":" << port << "..." << endl;

    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        cerr << "Failed to set socket timeout." << endl;

        close(clientSocket);

        return;
    }

    const vector data(PACKET_SIZE, 'B');
    char ackBuffer[16] = {};
    int packetsSent = 0;
    int packetsAcknowledged = 0;

    sockaddr_in responseAddress;
    socklen_t responseAddressLength = sizeof(responseAddress);

    const auto startTime = high_resolution_clock::now();

    for (int i = 0; i < NUM_PACKETS; i++) {
        if (sendto(clientSocket, data.data(), PACKET_SIZE, 0,
                  reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
            cerr << "Failed to send UDP packet." << endl;
            continue;
        }

        packetsSent++;

        if (recvfrom(clientSocket, ackBuffer, sizeof(ackBuffer), 0,
                    reinterpret_cast<sockaddr *>(&responseAddress), &responseAddressLength) > 0) {
            packetsAcknowledged++;
        }
    }

    const auto endTime = high_resolution_clock::now();
    const auto duration = duration_cast<microseconds>(endTime - startTime);

    cout << "UDP: Sent " << packetsSent << " packets, acknowledged " << packetsAcknowledged
         << " in " << duration.count() / 1000.0 << " ms." << endl;
    cout << "UDP: Average round-trip time per packet: " << duration.count() / static_cast<double>(packetsSent) << " us." << endl;

    close(clientSocket);
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int tcpPort = 8888;
    constexpr int udpPort = 8889;

    cout << "Starting transmission test client..." << endl;
    cout << "Server IP: " << serverIp << endl;
    cout << "TCP port: " << tcpPort << ", UDP port: " << udpPort << endl;
    cout << "Will send " << NUM_PACKETS << " packets of " << PACKET_SIZE << " bytes each." << endl;

    cout << "\n--- TCP Transmission Test ---" << endl;
    runTcpClient(serverIp, tcpPort);

    this_thread::sleep_for(seconds(1));

    cout << "\n--- UDP Transmission Test ---" << endl;
    runUdpClient(serverIp, udpPort);

    cout << "\nTests completed." << endl;

    return 0;
}
