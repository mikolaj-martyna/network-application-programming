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
    const string packetHex =
            "45 00 00 4e f7 fa 40 00 38 06 9d 33 d4 b6 18 1b "
            "c0 a8 00 02 0b 54 b9 a6 fb f9 3c 57 c1 0a 06 c1 "
            "80 18 00 e3 ce 9c 00 00 01 01 08 0a 03 a6 eb 01 "
            "00 0b f8 e5 6e 65 74 77 6f 72 6b 20 70 72 6f 67 "
            "72 61 6d 6d 69 6e 67 20 69 73 20 66 75 6e";

    const vector<uint8_t> packet = parseHexDatagram(packetHex);

    const int ipVersion = (packet[0] >> 4) & 0xF;
    const int ipHeaderLength = (packet[0] & 0xF) * 4;

    stringstream srcIpStream;
    srcIpStream << static_cast<int>(packet[12]) << "." << static_cast<int>(packet[13]) << "."
               << static_cast<int>(packet[14]) << "." << static_cast<int>(packet[15]);
    const string srcIp = srcIpStream.str();

    stringstream dstIpStream;
    dstIpStream << static_cast<int>(packet[16]) << "." << static_cast<int>(packet[17]) << "."
               << static_cast<int>(packet[18]) << "." << static_cast<int>(packet[19]);
    const string dstIp = dstIpStream.str();

    const int protocol = packet[9];

    uint16_t srcPort = 0, dstPort = 0;
    string data;
    int dataOffset = 0;

    if (protocol == 6) {
        srcPort = (packet[ipHeaderLength] << 8) | packet[ipHeaderLength + 1];
        dstPort = (packet[ipHeaderLength + 2] << 8) | packet[ipHeaderLength + 3];

        const int tcpHeaderLength = ((packet[ipHeaderLength + 12] >> 4) & 0xF) * 4;

        dataOffset = ipHeaderLength + tcpHeaderLength;
    } else if (protocol == 17) {
        srcPort = (packet[ipHeaderLength] << 8) | packet[ipHeaderLength + 1];
        dstPort = (packet[ipHeaderLength + 2] << 8) | packet[ipHeaderLength + 3];

        dataOffset = ipHeaderLength + 8;
    }

    for (size_t i = dataOffset; i < packet.size(); ++i) {
        data += static_cast<char>(packet[i]);
    }

    cout << "IP Version: " << ipVersion << endl;
    cout << "Source IP: " << srcIp << endl;
    cout << "Destination IP: " << dstIp << endl;
    cout << "Protocol: " << protocol << endl;
    cout << "Source Port: " << srcPort << endl;
    cout << "Destination Port: " << dstPort << endl;
    cout << "Data: " << data << endl;
    cout << "Data size: " << data.length() << " bytes" << endl;

    const string messageA = "zad15odpA;ver;" + to_string(ipVersion) +
                          ";srcip;" + srcIp +
                          ";dstip;" + dstIp +
                          ";type;" + to_string(protocol);

    cout << "Message A to send: " << messageA << endl;

    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 2911;

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

    if (!sendUdpMessage(clientSocket, messageA, serverAddress)) {
        cerr << "Failed to send message A." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Message A sent, waiting for response..." << endl;

    constexpr size_t bufferSize = 1024;
    char responseBuffer[bufferSize] = {};

    if (!receiveUdpResponse(clientSocket, responseBuffer, bufferSize, serverAddress)) {
        cerr << "Failed to receive response for message A." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Server response for A: " << responseBuffer << endl;

    if (strcmp(responseBuffer, "TAK") == 0) {
        const string messageB = "zad15odpB;srcport;" + to_string(srcPort) +
                              ";dstport;" + to_string(dstPort) +
                              ";data;" + data;

        cout << "Message B to send: " << messageB << endl;

        if (!sendUdpMessage(clientSocket, messageB, serverAddress)) {
            cerr << "Failed to send message B." << endl;

            close(clientSocket);

            return 1;
        }
        cout << "Message B sent, waiting for response..." << endl;

        if (!receiveUdpResponse(clientSocket, responseBuffer, bufferSize, serverAddress)) {
            cerr << "Failed to receive response for message B." << endl;

            close(clientSocket);

            return 1;
        }
        cout << "Server response for B: " << responseBuffer << endl;
    }

    close(clientSocket);

    return 0;
}
