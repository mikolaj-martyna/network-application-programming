#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>

using namespace std;

int main() {
    const string serverIp = "212.182.24.27";
    constexpr int port = 2912;

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid ip address format." << endl;

        close(clientSocket);

        return 1;
    }

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect to server." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Connected to number guessing server." << endl;

    int userGuess;
    char buffer[4096] = {};

    while (true) {
        cout << "Enter your guess (or type a negative number to quit): ";
        cin >> userGuess;

        if (userGuess < 0) {
            cout << "Exiting program." << endl;
            break;
        }

        const string guessStr = to_string(userGuess) + "\n";

        if (send(clientSocket, guessStr.c_str(), guessStr.length(), 0) < 0) {
            cerr << "Failed to send data." << endl;
            break;
        }

        memset(buffer, 0, sizeof(buffer));
        if (const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0); bytesRead < 0) {
            cerr << "Failed to receive data." << endl;
        } else if (bytesRead == 0) {
            cout << "Connection closed by server." << endl;
            break;
        } else {
            cout << "Server response: " << buffer << endl;
        }
    }

    close(clientSocket);

    return 0;
}
