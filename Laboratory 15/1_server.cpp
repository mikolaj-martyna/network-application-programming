#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <errno.h>

using namespace std;

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 12345;
    constexpr int maxClients = FD_SETSIZE;

    int clientSockets[maxClients];
    for (int i = 0; i < maxClients; i++) {
        clientSockets[i] = -1;
    }

    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create socket: " << strerror(errno) << endl;

        return 1;
    }

    constexpr int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Failed to set socket options: " << strerror(errno) << endl;

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
        cerr << "Failed to bind socket: " << strerror(errno) << endl;

        close(serverSocket);

        return 1;
    }

    if (listen(serverSocket, 5) < 0) {
        cerr << "Failed to listen for connections: " << strerror(errno) << endl;

        close(serverSocket);

        return 1;
    }

    cout << "Echo server is listening on " << serverIp << ":" << serverPort << endl;

    fd_set activeFds, readFds;
    FD_ZERO(&activeFds);
    FD_SET(serverSocket, &activeFds);
    int maxFd = serverSocket;

    while (true) {
        readFds = activeFds;

        if (select(maxFd + 1, &readFds, nullptr, nullptr, nullptr) < 0) {
            cerr << "Select error: " << strerror(errno) << endl;
            break;
        }

        if (FD_ISSET(serverSocket, &readFds)) {
            sockaddr_in clientAddress{};
            socklen_t addrLen = sizeof(clientAddress);

            if (const int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &addrLen); clientSocket < 0) {
                cerr << "Accept error: " << strerror(errno) << endl;
            } else {
                int i;
                for (i = 0; i < maxClients; i++) {
                    if (clientSockets[i] < 0) {
                        clientSockets[i] = clientSocket;
                        break;
                    }
                }

                if (i == maxClients) {
                    cerr << "Too many clients, connection rejected." << endl;

                    close(clientSocket);
                } else {
                    FD_SET(clientSocket, &activeFds);

                    if (clientSocket > maxFd) {
                        maxFd = clientSocket;
                    }

                    char clientIp[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, sizeof(clientIp));
                    const int clientPort = ntohs(clientAddress.sin_port);
                    cout << "Client connected: " << clientIp << ":" << clientPort << endl;
                }
            }
        }

        for (int i = 0; i < maxClients; i++) {
            if (const int socketFd = clientSockets[i]; socketFd > 0 && FD_ISSET(socketFd, &readFds)) {
                constexpr int bufferSize = 1024;
                char buffer[bufferSize] = {};

                if (const ssize_t bytesRead = read(socketFd, buffer, sizeof(buffer) - 1); bytesRead <= 0) {
                    if (bytesRead == 0) {
                        cout << "Client disconnected." << endl;
                    } else {
                        cerr << "Read error: " << strerror(errno) << endl;
                    }

                    close(socketFd);

                    FD_CLR(socketFd, &activeFds);
                    clientSockets[i] = -1;
                } else {
                    buffer[bytesRead] = '\0';
                    cout << "Received " << bytesRead << " bytes from client: " << buffer << endl;

                    if (const ssize_t bytesSent = write(socketFd, buffer, bytesRead); bytesSent < 0) {
                        cerr << "Write error: " << strerror(errno) << endl;
                    } else {
                        cout << "Echoed " << bytesSent << " bytes back to client." << endl;
                    }
                }
            }
        }
    }

    close(serverSocket);

    for (int i = 0; i < maxClients; i++) {
        if (clientSockets[i] > 0) {
            close(clientSockets[i]);
        }
    }

    return 0;
}
