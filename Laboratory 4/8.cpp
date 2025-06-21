#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string>

using namespace std;

string prepareMessage(const string &originalMessage) {
    constexpr size_t requiredLength = 20;

    if (originalMessage.length() == requiredLength) {
        return originalMessage;
    }

    if (originalMessage.length() < requiredLength) {
        string paddedMessage = originalMessage;
        paddedMessage.append(requiredLength - originalMessage.length(), ' ');

        return paddedMessage;
    }

    return originalMessage.substr(0, requiredLength);
}

bool sendAll(const int socket, const char *buffer, const size_t length, const sockaddr_in &clientAddress) {
    size_t totalSent = 0;

    while (totalSent < length) {
        constexpr socklen_t clientAddressLength = sizeof(clientAddress);
        const ssize_t sent = sendto(socket, buffer + totalSent, length - totalSent, 0,
                                  reinterpret_cast<const sockaddr *>(&clientAddress), clientAddressLength);

        if (sent < 0) {
            cerr << "Error during send: " << strerror(errno) << endl;

            return false;
        }

        totalSent += sent;
        cout << "Sent " << sent << " bytes, total sent: " << totalSent << "/" << length << endl;
    }

    return true;
}

bool recvAll(const int socket, char *buffer, const size_t length, sockaddr_in &clientAddress) {
    size_t totalReceived = 0;

    while (totalReceived < length) {
        socklen_t clientAddressLength = sizeof(clientAddress);
        const ssize_t received = recvfrom(socket, buffer + totalReceived, length - totalReceived, 0,
                                        reinterpret_cast<sockaddr *>(&clientAddress), &clientAddressLength);

        if (received < 0) {
            cerr << "Error during receive: " << strerror(errno) << endl;

            return false;
        }

        if (received == 0) {
            cerr << "Connection closed by client before receiving all data." << endl;

            return false;
        }

        totalReceived += received;
        cout << "Received " << received << " bytes, total received: " << totalReceived << "/" << length << endl;
    }

    return true;
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 8008;
    constexpr size_t messageLength = 20;

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

    cout << "UDP server is listening on " << serverIp << ":" << serverPort << endl;
    cout << "Server will handle messages with required length of " << messageLength << " characters" << endl;

    char buffer[messageLength + 1];
    while (true) {
        sockaddr_in clientAddress{};

        cout << "Waiting for client message..." << endl;
        if (!recvAll(serverSocket, buffer, messageLength, clientAddress)) {
            cerr << "Failed to receive complete message." << endl;

            continue;
        }

        buffer[messageLength] = '\0';

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
        const int clientPort = ntohs(clientAddress.sin_port);

        cout << "Received complete message from " << clientIp << ":" << clientPort << ": '" << buffer << "'" << endl;

        string response = prepareMessage(buffer);
        cout << "Sending response: '" << response << "'" << endl;

        if (!sendAll(serverSocket, response.c_str(), messageLength, clientAddress)) {
            cerr << "Failed to send complete response." << endl;

            continue;
        }

        cout << "Successfully sent complete response to " << clientIp << ":" << clientPort << endl;
    }

    close(serverSocket);

    return 0;
}
