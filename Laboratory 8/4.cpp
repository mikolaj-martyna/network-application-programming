#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <regex>
#include <vector>
#include <sstream>

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

    char buffer[8192] = {};

    ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to receive server greeting." << endl;

        close(clientSocket);

        return 1;
    }

    string loginCommand = "a1 LOGIN pasinf2017@infumcs.edu P4SInf2017\r\n";
    send(clientSocket, loginCommand.c_str(), loginCommand.length(), 0);
    memset(buffer, 0, sizeof(buffer));

    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to login." << endl;

        close(clientSocket);

        return 1;
    }

    if (string response(buffer); response.find("a1 OK") == string::npos) {
        cerr << "Login failed." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Successfully logged in." << endl;

    string selectCommand = "a2 SELECT INBOX\r\n";
    send(clientSocket, selectCommand.c_str(), selectCommand.length(), 0);
    memset(buffer, 0, sizeof(buffer));

    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to select INBOX." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Selected INBOX." << endl;

    string searchCommand = "a3 SEARCH UNSEEN\r\n";
    send(clientSocket, searchCommand.c_str(), searchCommand.length(), 0);
    memset(buffer, 0, sizeof(buffer));

    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to search for unread messages." << endl;

        close(clientSocket);

        return 1;
    }

    string searchResponse(buffer);
    regex searchRegex("\\* SEARCH((?:\\s+\\d+)*)");

    vector<int> unreadMessages;

    if (smatch searchMatches; regex_search(searchResponse, searchMatches, searchRegex) && searchMatches.size() > 1) {
        istringstream iss(searchMatches[1].str());
        int id;

        while (iss >> id) {
            unreadMessages.push_back(id);
        }
    }

    if (unreadMessages.empty()) {
        cout << "No unread messages found." << endl;
    } else {
        cout << "Found " << unreadMessages.size() << " unread message(s)." << endl;

        for (int messageId : unreadMessages) {
            cout << "\n--- Message ID: " << messageId << " ---\n" << endl;

            string fetchCommand = "a4 FETCH " + to_string(messageId) + " BODY[TEXT]\r\n";
            send(clientSocket, fetchCommand.c_str(), fetchCommand.length(), 0);
            memset(buffer, 0, sizeof(buffer));

            bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead <= 0) {
                cerr << "Failed to fetch message " << messageId << endl;
                continue;
            }

            cout << "Message content: " << endl;
            cout << buffer << endl;

            string storeCommand = "a5 STORE " + to_string(messageId) + " +FLAGS (\\Seen)\r\n";
            send(clientSocket, storeCommand.c_str(), storeCommand.length(), 0);
            memset(buffer, 0, sizeof(buffer));

            bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytesRead <= 0) {
                cerr << "Failed to mark message " << messageId << " as read." << endl;
                continue;
            }

            cout << "Message " << messageId << " marked as read." << endl;
        }
    }

    string logoutCommand = "a6 LOGOUT\r\n";
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
