#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

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

    const string loginCommand = "a1 LOGIN pasinf2017@infumcs.edu P4SInf2017\r\n";
    send(clientSocket, loginCommand.c_str(), loginCommand.length(), 0);
    memset(buffer, 0, sizeof(buffer));

    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to login." << endl;

        close(clientSocket);

        return 1;
    }

    string response(buffer);
    if (response.find("a1 OK") == string::npos) {
        cerr << "Login failed." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Successfully logged in." << endl;

    const string selectCommand = "a2 SELECT INBOX\r\n";
    send(clientSocket, selectCommand.c_str(), selectCommand.length(), 0);
    memset(buffer, 0, sizeof(buffer));

    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to select INBOX." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Selected INBOX." << endl;

    int messageId;
    cout << "Enter the message ID to delete: ";
    cin >> messageId;

    if (messageId <= 0) {
        cerr << "Invalid message ID." << endl;

        close(clientSocket);

        return 1;
    }

    const string storeCommand = "a3 STORE " + to_string(messageId) + " +FLAGS (\\Deleted)\r\n";
    send(clientSocket, storeCommand.c_str(), storeCommand.length(), 0);
    memset(buffer, 0, sizeof(buffer));

    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to mark message for deletion." << endl;

        close(clientSocket);

        return 1;
    }

    response = string(buffer);
    if (response.find("a3 OK") == string::npos) {
        cerr << "Failed to mark message for deletion. Message ID may not exist." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Message " << messageId << " marked for deletion." << endl;

    const string expungeCommand = "a4 EXPUNGE\r\n";
    send(clientSocket, expungeCommand.c_str(), expungeCommand.length(), 0);
    memset(buffer, 0, sizeof(buffer));

    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to expunge messages." << endl;

        close(clientSocket);

        return 1;
    }

    response = string(buffer);
    if (response.find("a4 OK") == string::npos) {
        cerr << "Failed to expunge messages." << endl;
    } else {
        cout << "Message physically deleted from the server." << endl;
    }

    const string logoutCommand = "a5 LOGOUT\r\n";
    send(clientSocket, logoutCommand.c_str(), logoutCommand.length(), 0);
    memset(buffer, 0, sizeof(buffer));

    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to receive logout response." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Logged out." << endl;

    close(clientSocket);

    return 0;
}
