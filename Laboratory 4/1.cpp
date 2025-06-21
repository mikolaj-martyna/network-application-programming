#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <ctime>
#include <string>

using namespace std;

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 8001;
    
    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
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
    
    if (listen(serverSocket, 1) < 0) {
        cerr << "Failed to listen for connections." << endl;

        close(serverSocket);

        return 1;
    }
    cout << "Server is listening on " << serverIp << ":" << serverPort << endl;

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);
        const int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressLength);

        if (clientSocket < 0) {
            cerr << "Failed to accept connection." << endl;
            break;
        }
        
        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
        const int clientPort = ntohs(clientAddress.sin_port);
        cout << "Client connected: " << clientIp << ":" << clientPort << endl;
        
        char buffer[1024] = {};
        const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead < 0) {
            cerr << "Failed to receive data from client." << endl;

            close(clientSocket);

            continue;
        }

        if (bytesRead == 0) {
            cout << "Client disconnected." << endl;

            close(clientSocket);

            continue;
        }

        buffer[bytesRead] = '\0';
        cout << "Received message from client: " << buffer << endl;
        
        time_t now = time(nullptr);
        string dateTime = ctime(&now);
        
        if (!dateTime.empty() && dateTime[dateTime.length() - 1] == '\n') {
            dateTime.erase(dateTime.length() - 1);
        }

        if (const ssize_t bytesSent = send(clientSocket, dateTime.c_str(), dateTime.length(), 0); bytesSent < 0) {
            cerr << "Failed to send data to client." << endl;
        } else {
            cout << "Sent date and time to client: " << dateTime << endl;
        }

        close(clientSocket);
    }

    close(serverSocket);

    return 0;
}
