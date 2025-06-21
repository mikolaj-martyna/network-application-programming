#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctime>

using namespace std;

bool isNumeric(const char* str) {
    if (str == nullptr || *str == '\0') {
        return false;
    }

    while (*str && isspace(*str)) {
        ++str;
    }

    if (*str == '+' || *str == '-') {
        ++str;
    }

    bool hasDigit = false;
    while (*str && *str != '\n' && *str != '\r') {
        if (!isdigit(*str)) {
            return false;
        }
        hasDigit = true;
        ++str;
    }

    return hasDigit;
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int port = 2912;

    srand(static_cast<unsigned int>(time(nullptr)));
    const int secretNumber = rand() % 100 + 1;

    cout << "Secret number: " << secretNumber << endl;

    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    constexpr int enable = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        cerr << "Failed to set socket options." << endl;

        close(serverSocket);

        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid ip address format." << endl;

        close(serverSocket);

        return 1;
    }

    if (bind(serverSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to bind socket." << endl;

        close(serverSocket);

        return 1;
    }

    if (listen(serverSocket, 1) < 0) {
        cerr << "Failed to listen for connections." << endl;

        close(serverSocket);

        return 1;
    }

    cout << "Server is listening on " << serverIp << ":" << port << endl;
    cout << "Waiting for connection..." << endl;

    sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    const int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress), &clientAddressLength);

    if (clientSocket < 0) {
        cerr << "Failed to accept connection." << endl;

        close(serverSocket);

        return 1;
    }

    char clientIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
    cout << "Connected with client: " << clientIp << endl;

    char buffer[4096] = {};
    bool guessedCorrectly = false;

    while (!guessedCorrectly) {
        memset(buffer, 0, sizeof(buffer));

        if (const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0); bytesRead < 0) {
            cerr << "Failed to receive data." << endl;
            break;
        } else {
            if (bytesRead == 0) {
                cout << "Connection closed by client." << endl;
                break;
            }
            string response;

            if (!isNumeric(buffer)) {
                response = "Error: Please send a valid number!\n";
            } else {
                const int guess = atoi(buffer);
                cout << "Client guessed: " << guess << endl;

                if (guess < secretNumber) {
                    response = "Your guess is too small. Try a larger number!\n";
                } else if (guess > secretNumber) {
                    response = "Your guess is too large. Try a smaller number!\n";
                } else {
                    response = "Correct! You've guessed the number!\n";
                    guessedCorrectly = true;
                }
            }

            if (send(clientSocket, response.c_str(), response.length(), 0) < 0) {
                cerr << "Failed to send response." << endl;
                break;
            }
        }
    }

    cout << "Closing connection." << endl;
    close(clientSocket);
    close(serverSocket);

    return 0;
}
