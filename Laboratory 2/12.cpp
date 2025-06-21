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

bool sendAll(const int socket, const char *buffer, const size_t length) {
    size_t totalSent = 0;

    while (totalSent < length) {
        const ssize_t sent = send(socket, buffer + totalSent, length - totalSent, 0);

        if (sent < 0) {
            cerr << "Error during send: " << strerror(errno) << endl;

            return false;
        }

        totalSent += sent;
        cout << "Sent " << sent << " bytes, total sent: " << totalSent << "/" << length << endl;
    }

    return true;
}

bool recvAll(const int socket, char *buffer, const size_t length) {
    size_t totalReceived = 0;

    while (totalReceived < length) {
        const ssize_t received = recv(socket, buffer + totalReceived, length - totalReceived, 0);

        if (received < 0) {
            cerr << "Error during receive: " << strerror(errno) << endl;

            return false;
        }

        if (received == 0) {
            cerr << "Connection closed by server before receiving all data." << endl;

            return false;
        }

        totalReceived += received;
        cout << "Received " << received << " bytes, total received: " << totalReceived << "/" << length << endl;
    }

    return true;
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 2908;
    constexpr size_t messageLength = 20;

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid ip address format." << endl;

        close(clientSocket);

        return 1;
    }

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Connected to server at " << serverIp << ":" << serverPort << endl;

    string userMessage;
    cout << "Enter message to send (will be adjusted to 20 characters): ";
    getline(cin, userMessage);

    const string messageToSend = prepareMessage(userMessage);

    cout << "Original message: '" << userMessage << "' (length: " << userMessage.length() << ")" << endl;
    cout << "Prepared message: '" << messageToSend << "' (length: " << messageToSend.length() << ")" << endl;

    if (!sendAll(clientSocket, messageToSend.c_str(), messageLength)) {
        cerr << "Failed to send complete message." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Successfully sent all " << messageLength << " bytes." << endl;

    char buffer[messageLength + 1] = {};

    if (!recvAll(clientSocket, buffer, messageLength)) {
        cerr << "Failed to receive complete response." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Successfully received all " << messageLength << " bytes." << endl;

    buffer[messageLength] = '\0';
    cout << "Received data: '" << buffer << "'" << endl;

    close(clientSocket);

    return 0;
}
