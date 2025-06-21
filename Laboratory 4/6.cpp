#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string>

using namespace std;

string resolveIpAddress(const string& hostname) {
    addrinfo hints{}, *result = nullptr;
    char ipAddress[INET_ADDRSTRLEN] = {};
    
    hints.ai_family = AF_INET;      
    hints.ai_socktype = SOCK_DGRAM;

    if (const int status = getaddrinfo(hostname.c_str(), nullptr, &hints, &result); status != 0) {
        return "Error: " + string(gai_strerror(status));
    }
    
    void* addr;
    if (result->ai_family == AF_INET) { 
        auto* ipv4 = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
        addr = &(ipv4->sin_addr);
    } else {
        freeaddrinfo(result);

        return "Error: Not an IPv4 address";
    }

    inet_ntop(AF_INET, addr, ipAddress, INET_ADDRSTRLEN);
    freeaddrinfo(result);

    return ipAddress;
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 8006;
    
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

    cout << "Hostname to IP resolver server is listening on " << serverIp << ":" << serverPort << endl;

    char buffer[1024];
    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        const ssize_t bytesRead = recvfrom(serverSocket, buffer, sizeof(buffer) - 1, 0,
                                          reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressLength);
        if (bytesRead < 0) {
            cerr << "Failed to receive data." << endl;

            break;
        }
        
        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
        const int clientPort = ntohs(clientAddress.sin_port);
        
        buffer[bytesRead] = '\0';
        cout << "Received hostname from " << clientIp << ":" << clientPort << ": " << buffer << endl;
        
        string ipAddress = resolveIpAddress(buffer);
        
        const ssize_t bytesSent = sendto(serverSocket, ipAddress.c_str(), ipAddress.length(), 0,
                                        reinterpret_cast<const sockaddr*>(&clientAddress), clientAddressLength);
        if (bytesSent < 0) {
            cerr << "Failed to send data." << endl;
        } else {
            cout << "Sent IP address to " << clientIp << ":" << clientPort << ": " << ipAddress << endl;
        }
    }

    close(serverSocket);

    return 0;
}
