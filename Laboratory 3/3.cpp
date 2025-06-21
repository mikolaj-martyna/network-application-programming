#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

using namespace std;

int main() {
    const string ip = "127.0.0.1";
    constexpr int port = 2900;

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid ip address format." << endl;

        close(clientSocket);

        return 1;
    }
    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect." << endl;

        return 1;
    }

    cout << "Connected." << endl;

    string message = "";
    while (true) {
        cin >> message;
        if (const ssize_t bytesSent = send(clientSocket, message.c_str(), message.length(), 0); bytesSent < 0) {
            cerr << "Failed to send message." << endl;

            break;
        } else {
            cout << "Sent " << bytesSent << " bytes." << endl;
            message = "";
        }

        char buffer[4096] = {};
        const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead < 0) {
            cerr << "Failed to receive data." << endl;

            break;
        }

        if (bytesRead == 0) {
            cout << "Connection closed by server." << endl;

            break;
        }

        cout << "Received data:\n" << buffer << endl;
    }

    close(clientSocket);

    return 0;
}
