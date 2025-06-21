#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

using namespace std;

int main() {
    const string hostname = "interia.pl";
    constexpr int port = 110;

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

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address format." << endl;

        close(clientSocket);

        return 1;
    }

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Connected to POP3 server." << endl;

    char buffer[4096] = {};
    if (recv(clientSocket, buffer, sizeof(buffer) - 1, 0) < 0) {
        cerr << "Failed to receive welcome message." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Server: " << buffer;

    const string username = "pas2017@interia.pl";
    const string password = "P4SInf2017";

    const string userCmd = "USER " + username + "\r\n";
    if (send(clientSocket, userCmd.c_str(), userCmd.length(), 0) < 0) {
        cerr << "Failed to send USER command." << endl;

        close(clientSocket);

        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (recv(clientSocket, buffer, sizeof(buffer) - 1, 0) < 0) {
        cerr << "Failed to receive response to USER command." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Server: " << buffer;
    if (strncmp(buffer, "+OK", 3) != 0) {
        cerr << "Username not accepted." << endl;

        close(clientSocket);

        return 1;
    }

    const string passCmd = "PASS " + password + "\r\n";
    if (send(clientSocket, passCmd.c_str(), passCmd.length(), 0) < 0) {
        cerr << "Failed to send PASS command." << endl;

        close(clientSocket);

        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (recv(clientSocket, buffer, sizeof(buffer) - 1, 0) < 0) {
        cerr << "Failed to receive response to PASS command." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Server: " << buffer;
    if (strncmp(buffer, "+OK", 3) != 0) {
        cerr << "Authentication failed." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Successfully logged in." << endl;

    const string statCmd = "STAT\r\n";
    if (send(clientSocket, statCmd.c_str(), statCmd.length(), 0) < 0) {
        cerr << "Failed to send STAT command." << endl;

        close(clientSocket);

        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (recv(clientSocket, buffer, sizeof(buffer) - 1, 0) < 0) {
        cerr << "Failed to receive response to STAT command." << endl;

        close(clientSocket);

        return 1;
    }

    int totalSize = 0;
    if (const char* token = strtok(buffer, " "); token != nullptr && strcmp(token, "+OK") == 0) {
        token = strtok(nullptr, " ");

        if (token != nullptr) {
            totalSize = atoi(token);
        }
    }

    cout << "Total size of all messages in the mailbox: " << totalSize << " bytes." << endl;

    const string quitCmd = "QUIT\r\n";
    if (send(clientSocket, quitCmd.c_str(), quitCmd.length(), 0) < 0) {
        cerr << "Failed to send QUIT command." << endl;

        close(clientSocket);

        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (recv(clientSocket, buffer, sizeof(buffer) - 1, 0) < 0) {
        cerr << "Failed to receive response to QUIT command." << endl;
    }

    cout << "Server: " << buffer;
    cout << "Session closed." << endl;

    close(clientSocket);

    return 0;
}
