#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string>

using namespace std;

string resolveHostname(const string& ipAddress) {
    sockaddr_in sa{};
    char hostname[NI_MAXHOST] = {};
    
    if (inet_pton(AF_INET, ipAddress.c_str(), &sa.sin_addr) <= 0) {
        return "Error: Invalid IP address format";
    }

    sa.sin_family = AF_INET;
    sa.sin_port = 0;
    
    const int result = getnameinfo(reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa),
                            hostname, NI_MAXHOST, nullptr, 0, NI_NAMEREQD);
    if (result != 0) {
        return "Error: " + string(gai_strerror(result));
    }

    return hostname;
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 8005;
    
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

    cout << "IP to Hostname resolver server is listening on " << serverIp << ":" << serverPort << endl;
    
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
        cout << "Received IP from " << clientIp << ":" << clientPort << ": " << buffer << endl;
        
        string hostname = resolveHostname(buffer);
        
        const ssize_t bytesSent = sendto(serverSocket, hostname.c_str(), hostname.length(), 0,
                                        reinterpret_cast<const sockaddr*>(&clientAddress), clientAddressLength);
        if (bytesSent < 0) {
            cerr << "Failed to send data." << endl;
        } else {
            cout << "Sent hostname to " << clientIp << ":" << clientPort << ": " << hostname << endl;
        }
    }

    close(serverSocket);

    return 0;
}
