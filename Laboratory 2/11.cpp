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

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 2908;

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

    const ssize_t bytesSent = send(clientSocket, messageToSend.c_str(), messageToSend.length(), 0);
    if (bytesSent < 0) {
        cerr << "Failed to send message." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Sent " << bytesSent << " bytes." << endl;

    char buffer[21] = {};
    const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead < 0) {
        cerr << "Failed to receive data." << endl;

        close(clientSocket);

        return 1;
    }

    if (bytesRead == 0) {
        cout << "Connection closed by server." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Received " << bytesRead << " bytes." << endl;

    buffer[bytesRead] = '\0';
    cout << "Received data: '" << buffer << "'" << endl;

    close(clientSocket);

    return 0;
}
