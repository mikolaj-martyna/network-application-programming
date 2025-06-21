#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string>

using namespace std;

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 8002;

    const int serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
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

    cout << "UDP Echo server is running on " << serverIp << ":" << serverPort << endl;

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);
        char buffer[1024] = {};

        const ssize_t bytesRead = recvfrom(serverSocket, buffer, sizeof(buffer) - 1, 0,
                                          reinterpret_cast<sockaddr*>(&clientAddress),
                                          &clientAddressLength);

        if (bytesRead < 0) {
            cerr << "Failed to receive data from client." << endl;
            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
        const int clientPort = ntohs(clientAddress.sin_port);

        buffer[bytesRead] = '\0';
        cout << "Received " << bytesRead << " bytes from " << clientIp << ":" << clientPort
             << ": " << buffer << endl;

        if (const ssize_t bytesSent = sendto(serverSocket, buffer, bytesRead, 0,
                                           reinterpret_cast<sockaddr*>(&clientAddress),
                                           clientAddressLength); bytesSent < 0) {
            cerr << "Failed to send data to client." << endl;
        } else {
            cout << "Echoed " << bytesSent << " bytes back to " << clientIp << ":" << clientPort << endl;
        }
    }

    close(serverSocket);
    return 0;
}
