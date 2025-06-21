#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

using namespace std;

int main() {
    const string hostname = "ntp.task.gda.pl";
    constexpr int port = 13;

    const hostent *hostent = gethostbyname(hostname.c_str());
    if (hostent == nullptr) {
        cerr << "Failed to resolve hostname." << endl;

        return 1;
    }

    auto **addr_list = reinterpret_cast<struct in_addr **>(hostent->h_addr_list);
    const string ip = inet_ntoa(*addr_list[0]);

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;
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

    char buffer[4096] = {};
    if (const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0); bytesRead < 0) {
        cerr << "Failed to receive data." << endl;
    } else if (bytesRead == 0) {
        cout << "Connection closed by server." << endl;
    } else {
        cout << "Received data:\n" << buffer << endl;
    }

    close(clientSocket);

    return 0;
}
