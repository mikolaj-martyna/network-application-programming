#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

using namespace std;

int main() {
    const string ip = "127.0.0.1";
    constexpr int port = 2901;

    const int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid ip address format." << endl;
        close(clientSocket);
    }
    string message = "";
    while (true) {
        cin >> message;

        if (const ssize_t bytesSent = sendto(clientSocket, message.c_str(), message.length(), 0,
                                       reinterpret_cast<const sockaddr *>(&serverAddress),
                                       sizeof(serverAddress)); bytesSent < 0) {
            cerr << "Failed to send message." << endl;
            break;
        } else {
            cout << "Sent " << bytesSent << " bytes." << endl;
        }

        char buffer[4096] = {};
        sockaddr_in senderAddress;
        socklen_t senderAddressLength = sizeof(senderAddress);
        if (const ssize_t bytesRead = recvfrom(clientSocket, buffer, sizeof(buffer) - 1, 0,
                                         reinterpret_cast<sockaddr *>(&senderAddress),
                                         &senderAddressLength); bytesRead < 0) {
            cerr << "Failed to receive data." << endl;
            break;
        }
        cout << "Received data:\n" << buffer << endl;

        char senderIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(senderAddress.sin_addr), senderIp, INET_ADDRSTRLEN);

        cout << "From: " << senderIp << ":" << ntohs(senderAddress.sin_port) << endl;
    }

    close(clientSocket);

    return 0;
}
