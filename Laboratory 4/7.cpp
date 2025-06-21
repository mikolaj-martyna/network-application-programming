#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string>

using namespace std;

string prepareMessage(const string &originalMessage) {
    constexpr size_t maxLength = 20;

    if (originalMessage.length() == maxLength) {
        return originalMessage;
    }

    if (originalMessage.length() < maxLength) {
        string paddedMessage = originalMessage;
        paddedMessage.append(maxLength - originalMessage.length(), ' ');

        return paddedMessage;
    }

    return originalMessage.substr(0, maxLength);
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 8007;
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
    cout << "Server will handle messages with max length of " << messageLength << " characters" << endl;

    char buffer[messageLength + 1];
    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        const ssize_t bytesRead = recvfrom(serverSocket, buffer, messageLength, 0,
                                          reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressLength);
        if (bytesRead < 0) {
            cerr << "Failed to receive data." << endl;

            break;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
        const int clientPort = ntohs(clientAddress.sin_port);

        buffer[bytesRead] = '\0';
        cout << "Received from " << clientIp << ":" << clientPort << ": '" << buffer << "'" << endl;

        string responseMsg = prepareMessage(buffer);

        const ssize_t bytesSent = sendto(serverSocket, responseMsg.c_str(), responseMsg.length(), 0,
                                        reinterpret_cast<const sockaddr*>(&clientAddress), clientAddressLength);
        if (bytesSent < 0) {
            cerr << "Failed to send data." << endl;
        } else {
            cout << "Sent to " << clientIp << ":" << clientPort << ": '" << responseMsg << "'" << endl;
        }
    }

    close(serverSocket);

    return 0;
}
