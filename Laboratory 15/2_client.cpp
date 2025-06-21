#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

void showHelp() {
    cout << "Weather Client Commands:" << endl;
    cout << "  weather  - Get current weather for Lublin" << endl;
    cout << "  help     - Show this help" << endl;
    cout << "  quit     - Exit the program" << endl;
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 12346;
    constexpr int bufferSize = 4096;

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
        cerr << "Failed to connect to weather server." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Connected to weather server at " << serverIp << ":" << serverPort << endl;

    char buffer[bufferSize] = {};
    if (const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0); bytesRead > 0) {
        buffer[bytesRead] = '\0';
        cout << buffer;
    }

    showHelp();

    while (true) {
        cout << "\nEnter command: ";

        string input;
        cin >> input;

        if (input == "quit") {
            if (const auto quitCmd = "QUIT"; send(clientSocket, quitCmd, strlen(quitCmd), 0) < 0) {
                cerr << "Failed to send quit command." << endl;
            }

            cout << "Disconnecting from weather server..." << endl;
            break;
        }

        if (input == "help") {
            showHelp();
            continue;
        }

        if (input == "weather") {
            if (const auto cmd = "GET_WEATHER"; send(clientSocket, cmd, strlen(cmd), 0) < 0) {
                cerr << "Failed to send command to server." << endl;
                break;
            }

            memset(buffer, 0, sizeof(buffer));
            const ssize_t msgBytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

            if (msgBytes < 0) {
                cerr << "Failed to receive data from server." << endl;
                break;
            }

            if (msgBytes == 0) {
                cout << "Server closed the connection." << endl;
                break;
            }

            buffer[msgBytes] = '\0';
            cout << "\n--- Weather Information ---\n" << buffer;
        } else {
            cout << "Unknown command. Type 'help' for available commands." << endl;
        }
    }

    close(clientSocket);

    return 0;
}
