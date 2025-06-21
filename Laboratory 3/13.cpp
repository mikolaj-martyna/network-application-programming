#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <vector>
#include <sstream>
#include <string>

using namespace std;

vector<uint8_t> parseHexDatagram(const string &hexString) {
    vector<uint8_t> datagram;
    stringstream ss(hexString);
    string hexByte;

    while (ss >> hexByte) {
        uint8_t byte = stoi(hexByte, nullptr, 16);
        datagram.push_back(byte);
    }

    return datagram;
}

bool sendUdpMessage(const int socket, const string &message, const sockaddr_in &serverAddress) {
    if (sendto(socket, message.c_str(), message.length(), 0,
               reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Error during send: " << strerror(errno) << endl;

        return false;
    }

    cout << "Successfully sent " << message.length() << " bytes." << endl;

    return true;
}

bool receiveUdpResponse(const int socket, char *buffer, const size_t bufferSize, sockaddr_in &serverAddress) {
    socklen_t serverLen = sizeof(serverAddress);
    const ssize_t received = recvfrom(socket, buffer, bufferSize - 1, 0,
                                      reinterpret_cast<sockaddr *>(&serverAddress), &serverLen);

    if (received < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            cerr << "Timeout - no response from server" << endl;
        } else {
            cerr << "Error during receive: " << strerror(errno) << endl;
        }

        return false;
    }

    buffer[received] = '\0';
    cout << "Received " << received << " bytes." << endl;

    return true;
}

int main() {
    const string hexDatagram =
            "ed 74 0b 55 00 24 ef fd 70 72 6f 67 72 61 6d 6d 69 6e 67 20 69 6e 20 70 79 74 68 6f 6e 20 69 73 20 66 75 6e";

    const vector<uint8_t> datagram = parseHexDatagram(hexDatagram);

    const uint16_t sourcePort = (datagram[0] << 8) | datagram[1];
    const uint16_t destPort = (datagram[2] << 8) | datagram[3];

    string data;
    for (size_t i = 8; i < datagram.size(); i++) {
        data += static_cast<char>(datagram[i]);
    }

    cout << "Source port: " << sourcePort << endl;
    cout << "Destination port: " << destPort << endl;
    cout << "Data: " << data << endl;
    cout << "Data size: " << data.length() << " bytes" << endl;

    // zad14odp for task number 13 is due to the task's requirements
    const string messageToSend = "zad14odp;src;" + to_string(sourcePort) +
                                 ";dst;" + to_string(destPort) +
                                 ";data;" + data;

    cout << "Message to send: " << messageToSend << endl;

    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 2910;

    const int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    timeval tv{};
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        cerr << "Failed to set socket timeout." << endl;

        close(clientSocket);

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address format." << endl;

        close(clientSocket);

        return 1;
    }

    if (!sendUdpMessage(clientSocket, messageToSend, serverAddress)) {
        cerr << "Failed to send message." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Message sent, waiting for response..." << endl;

    constexpr size_t bufferSize = 1024;
    char responseBuffer[bufferSize] = {};

    if (!receiveUdpResponse(clientSocket, responseBuffer, bufferSize, serverAddress)) {
        cerr << "Failed to receive response." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Server response: " << responseBuffer << endl;

    close(clientSocket);

    return 0;
}
