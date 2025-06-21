#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string>
#include <sstream>

using namespace std;

double calculate(const double num1, const char op, const double num2) {
    switch (op) {
        case '+':
            return num1 + num2;
        case '-':
            return num1 - num2;
        case '*':
            return num1 * num2;
        case '/':
            if (num2 == 0) throw runtime_error("Division by zero");
            return num1 / num2;
        default:
            throw runtime_error("Invalid operator");
    }
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 8004;

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

    cout << "Calculator server is listening on " << serverIp << ":" << serverPort << endl;
    cout << "Format: number operator number (e.g., 5 + 3)" << endl;

    char buffer[1024];
    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        const ssize_t bytesRead = recvfrom(serverSocket, buffer, sizeof(buffer) - 1, 0,
                                          reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressLength);
        if (bytesRead < 0) {
            cerr << "Failed to receive data." << endl;

            break;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
        const int clientPort = ntohs(clientAddress.sin_port);

        buffer[bytesRead] = '\0';
        cout << "Received from " << clientIp << ":" << clientPort << ": " << buffer << endl;

        double num2;
        char op;
        string response;
        istringstream iss(buffer);

        if (double num1; iss >> num1 >> op >> num2) {
            try {
                double result = calculate(num1, op, num2);
                ostringstream oss;
                oss << num1 << " " << op << " " << num2 << " = " << result;
                response = oss.str();
            } catch (const exception& e) {
                response = "Error: " + string(e.what());
            }
        } else {
            response = "Error: Invalid format. Use: <number> <operator> <number>";
        }

        const ssize_t bytesSent = sendto(serverSocket, response.c_str(), response.length(), 0,
                                        reinterpret_cast<const sockaddr*>(&clientAddress), clientAddressLength);
        if (bytesSent < 0) {
            cerr << "Failed to send data." << endl;
        } else {
            cout << "Sent result to " << clientIp << ":" << clientPort << ": " << response << endl;
        }
    }

    close(serverSocket);

    return 0;
}
