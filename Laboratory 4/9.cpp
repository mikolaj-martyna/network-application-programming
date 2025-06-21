#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string>
#include <vector>
#include <sstream>

using namespace std;

string checkMessageSyntax(const string &message) {
    vector<string> parts;
    stringstream ss(message);
    string part;

    while (getline(ss, part, ';')) {
        parts.push_back(part);
    }
    
    if (parts.size() != 7) {
        return "BAD_SYNTAX";
    }
    
    if (parts[0] != "zad13odp" || parts[1] != "src" || parts[3] != "dst" || parts[5] != "data") {
        return "BAD_SYNTAX";
    }
    
    int srcPort, dstPort;
    try {
        srcPort = stoi(parts[2]);
        dstPort = stoi(parts[4]);
    } catch (const exception &) {
        return "BAD_SYNTAX";
    }
    
    if (srcPort == 2900 && dstPort == 35211 && parts[6] == "programming in python is fun") {
        return "TAK";
    }

    return "NIE";
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 8009;
    
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
    
    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to bind socket to address." << endl;

        close(serverSocket);

        return 1;
    }

    cout << "UDP server for is listening on " << serverIp << ":" << serverPort << endl;
    cout << "Expected format: zad13odp;src;2900;dst;35211;data;programming in python is fun" << endl;

    char buffer[1024];
    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);
        
        const ssize_t bytesRead = recvfrom(serverSocket, buffer, sizeof(buffer) - 1, 0,
                                          reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressLength);
        if (bytesRead < 0) {
            cerr << "Failed to receive data." << endl;

            continue;
        }
        
        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
        const int clientPort = ntohs(clientAddress.sin_port);
        
        buffer[bytesRead] = '\0';
        cout << "Received from " << clientIp << ":" << clientPort << ": " << buffer << endl;
        
        string response = checkMessageSyntax(buffer);
        
        const ssize_t bytesSent = sendto(serverSocket, response.c_str(), response.length(), 0,
                                        reinterpret_cast<const sockaddr*>(&clientAddress), clientAddressLength);
        if (bytesSent < 0) {
            cerr << "Failed to send data." << endl;
        } else {
            cout << "Sent response to " << clientIp << ":" << clientPort << ": " << response << endl;
        }
    }

    close(serverSocket);

    return 0;
}
