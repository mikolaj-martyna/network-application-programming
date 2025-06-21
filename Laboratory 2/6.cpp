#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

using namespace std;

int main() {
    const string ip = "127.0.0.1";
    constexpr int port = 2902;

    const int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
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

    string firstNum;
    cin >> firstNum;

    string userOp;
    cin >> userOp;

    string secondNum;
    cin >> secondNum;

    if (const ssize_t bytesSent = sendto(clientSocket, firstNum.c_str(), firstNum.length(), 0,
                                         reinterpret_cast<const sockaddr *>(&serverAddress),
                                         sizeof(serverAddress)); bytesSent < 0) {
        cerr << "Failed to send first number." << endl;

        close(clientSocket);

        return 1;
    } else {
        cout << "Sent " << bytesSent << " bytes of first number." << endl;
    }

    if (const ssize_t bytesSent = sendto(clientSocket, userOp.c_str(), userOp.length(), 0,
                                         reinterpret_cast<const sockaddr *>(&serverAddress),
                                         sizeof(serverAddress)); bytesSent < 0) {
        cerr << "Failed to send operator." << endl;

        close(clientSocket);

        return 1;
    } else {
        cout << "Sent " << bytesSent << " bytes of operator." << endl;
    }

    if (const ssize_t bytesSent = sendto(clientSocket, secondNum.c_str(), secondNum.length(), 0,
                                         reinterpret_cast<const sockaddr *>(&serverAddress),
                                         sizeof(serverAddress)); bytesSent < 0) {
        cerr << "Failed to send second number." << endl;

        close(clientSocket);

        return 1;
    } else {
        cout << "Sent " << bytesSent << " bytes of second number." << endl;
    }

    char buffer[4096] = {};
    sockaddr_in senderAddress;
    socklen_t senderAddressLength = sizeof(senderAddress);
    if (const ssize_t bytesRead = recvfrom(clientSocket, buffer, sizeof(buffer) - 1, 0,
                                           reinterpret_cast<sockaddr *>(&senderAddress),
                                           &senderAddressLength); bytesRead < 0) {
        cerr << "Failed to receive result." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Received result:\n" << buffer << endl;

    char senderIp[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(senderAddress.sin_addr), senderIp, INET_ADDRSTRLEN);

    cout << "From: " << senderIp << ":" << ntohs(senderAddress.sin_port) << endl;

    close(clientSocket);

    return 0;
}
