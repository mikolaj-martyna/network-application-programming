#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <regex>

using namespace std;

int main() {
    const string hostname = "212.182.24.27";
    constexpr int port = 143;

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, hostname.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address format." << endl;

        close(clientSocket);

        return 1;
    }

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Connected to IMAP server at " << hostname << ":" << port << endl;

    char buffer[4096] = {};

    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to receive server greeting." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Server greeting: " << buffer;

    const string loginCommand = "a1 LOGIN pasinf2017@infumcs.edu P4SInf2017\r\n";
    send(clientSocket, loginCommand.c_str(), loginCommand.length(), 0);
    memset(buffer, 0, sizeof(buffer));

    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to login." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Login response: " << buffer;

    if (const string response(buffer); response.find("a1 OK") == string::npos) {
        cerr << "Login failed." << endl;

        close(clientSocket);

        return 1;
    }

    const string selectCommand = "a2 SELECT INBOX\r\n";
    send(clientSocket, selectCommand.c_str(), selectCommand.length(), 0);
    memset(buffer, 0, sizeof(buffer));

    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to select INBOX." << endl;

        close(clientSocket);

        return 1;
    }

    const string selectResponse(buffer);
    const regex existsRegex("\\* (\\d+) EXISTS");

    if (smatch matches; regex_search(selectResponse, matches, existsRegex) && matches.size() > 1) {
        cout << "Number of messages in INBOX: " << matches[1].str() << endl;
    } else {
        cout << "Could not determine the number of messages in INBOX." << endl;
    }

    const string logoutCommand = "a3 LOGOUT\r\n";
    send(clientSocket, logoutCommand.c_str(), logoutCommand.length(), 0);
    memset(buffer, 0, sizeof(buffer));

    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to receive logout response." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Logout response: " << buffer;

    close(clientSocket);

    return 0;
}
