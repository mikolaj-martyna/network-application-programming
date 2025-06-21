#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 12345;

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address format." << endl;

        close(clientSocket);

        return 1;
    }

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect to server." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Connected to echo server at " << serverIp << ":" << serverPort << endl;

    while (true) {
        constexpr int bufferSize = 1024;
        cout << "Enter message (type 'quit' to exit): ";

        string input;
        cin >> input;

        if (input == "quit") {
            cout << "Disconnecting from server..." << endl;
            break;
        }

        if (send(clientSocket, input.c_str(), input.length(), 0) < 0) {
            cerr << "Failed to send data to server." << endl;
            break;
        }

        char buffer[bufferSize] = {};
        const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead < 0) {
            cerr << "Failed to receive data from server." << endl;
            break;
        }

        if (bytesRead == 0) {
            cout << "Server closed the connection." << endl;
            break;
        }

        buffer[bytesRead] = '\0';
        cout << "Server response: " << buffer << endl;
    }

    close(clientSocket);

    return 0;
}
