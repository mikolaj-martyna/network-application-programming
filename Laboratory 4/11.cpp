#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string>
#include <vector>
#include <sstream>

using namespace std;

string checkMessageSyntaxA(const vector<string> &parts) {
    if (parts.size() != 9) {
        return "BAD_SYNTAX";
    }

    if (parts[0] != "zad15odpA" || parts[1] != "ver" || parts[3] != "srcip" ||
        parts[5] != "dstip" || parts[7] != "type") {
        return "BAD_SYNTAX";
    }

    int ver, type;
    try {
        ver = stoi(parts[2]);
        type = stoi(parts[8]);
    } catch (const exception &) {
        return "NIE";
    }

    if (ver == 4 && parts[4] == "127.0.0.1" && parts[6] == "192.168.0.2" && type == 6) {
        return "TAK";
    }

    return "NIE";
}

string checkMessageSyntaxB(const vector<string> &parts) {
    if (parts.size() != 7) {
        return "BAD_SYNTAX";
    }

    if (parts[0] != "zad15odpB" || parts[1] != "srcport" || parts[3] != "dstport" || parts[5] != "data") {
        return "BAD_SYNTAX";
    }

    int srcPort, dstPort;
    try {
        srcPort = stoi(parts[2]);
        dstPort = stoi(parts[4]);
    } catch (const exception &) {
        return "NIE";
    }

    if (srcPort == 2900 && dstPort == 47526 && parts[6] == "network programming is fun") {
        return "TAK";
    }

    return "NIE";
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 8011;

    const int serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create socket." << endl;

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

    if (bind(serverSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to bind socket to address." << endl;

        close(serverSocket);

        return 1;
    }

    cout << "UDP server for is listening on " << serverIp << ":" << serverPort << endl;
    cout << "Expected formats:" << endl;
    cout << "Format A: zad15odpA;ver;4;srcip;127.0.0.1;dstip;192.168.0.2;type;6" << endl;
    cout << "Format B: zad15odpB;srcport;2900;dstport;47526;data;network programming is fun" << endl;

    char buffer[1024];
    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        const ssize_t bytesRead = recvfrom(serverSocket, buffer, sizeof(buffer) - 1, 0,
                                           reinterpret_cast<sockaddr *>(&clientAddress), &clientAddressLength);
        if (bytesRead < 0) {
            cerr << "Failed to receive data." << endl;

            break;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
        const int clientPort = ntohs(clientAddress.sin_port);

        buffer[bytesRead] = '\0';
        cout << "Received from " << clientIp << ":" << clientPort << ": " << buffer << endl;

        vector<string> parts;
        stringstream ss(buffer);
        string part;

        while (getline(ss, part, ';')) {
            parts.push_back(part);
        }

        string response;
        if (!parts.empty()) {
            if (parts[0] == "zad15odpA") {
                response = checkMessageSyntaxA(parts);
            } else if (parts[0] == "zad15odpB") {
                response = checkMessageSyntaxB(parts);
            } else {
                response = "BAD_SYNTAX";
            }
        } else {
            response = "BAD_SYNTAX";
        }

        const ssize_t bytesSent = sendto(serverSocket, response.c_str(), response.length(), 0,
                                         reinterpret_cast<const sockaddr *>(&clientAddress), clientAddressLength);
        if (bytesSent < 0) {
            cerr << "Failed to send data." << endl;
        } else {
            cout << "Sent response to " << clientIp << ":" << clientPort << ": " << response << endl;
        }
    }

    close(serverSocket);

    return 0;
}
