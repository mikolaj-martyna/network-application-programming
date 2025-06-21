#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <regex>
#include <vector>

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

    string loginCommand = "a1 LOGIN pasinf2017@infumcs.edu P4SInf2017\r\n";
    send(clientSocket, loginCommand.c_str(), loginCommand.length(), 0);
    memset(buffer, 0, sizeof(buffer));

    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to login." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Login response: " << buffer;

    if (string response(buffer); response.find("a1 OK") == string::npos) {
        cerr << "Login failed." << endl;

        close(clientSocket);

        return 1;
    }

    // List all mailboxes
    string listCommand = "a2 LIST \"\" *\r\n";
    send(clientSocket, listCommand.c_str(), listCommand.length(), 0);
    memset(buffer, 0, sizeof(buffer));
    bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead <= 0) {
        cerr << "Failed to list mailboxes." << endl;
        close(clientSocket);
        return 1;
    }

    string listResponse(buffer);
    regex mailboxRegex("\\* LIST \\([^)]*\\) \".\" \"([^\"]+)\"");

    vector<string> mailboxes;
    auto searchStart(listResponse.cbegin());
    smatch matches;

    while (regex_search(searchStart, listResponse.cend(), matches, mailboxRegex)) {
        mailboxes.push_back(matches[1].str());
        searchStart = matches.suffix().first;
    }

    cout << "Found " << mailboxes.size() << " mailboxes." << endl;

    int totalMessages = 0;

    for (const auto& mailbox : mailboxes) {
        if (mailbox == "INBOX/Trash" || mailbox.find('#') != string::npos) {
            continue;
        }

        string selectCommand = "a3 SELECT \"" + mailbox + "\"\r\n";
        send(clientSocket, selectCommand.c_str(), selectCommand.length(), 0);
        memset(buffer, 0, sizeof(buffer));

        bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            cerr << "Failed to select mailbox: " << mailbox << endl;
            continue;
        }

        string selectResponse(buffer);
        regex existsRegex("\\* (\\d+) EXISTS");

        if (smatch existsMatches; regex_search(selectResponse, existsMatches, existsRegex) && existsMatches.size() > 1) {
            int messages = stoi(existsMatches[1].str());
            cout << "Mailbox " << mailbox << ": " << messages << " messages" << endl;
            totalMessages += messages;
        } else {
            cout << "Could not determine the number of messages in " << mailbox << endl;
        }
    }

    cout << "Total messages across all mailboxes: " << totalMessages << endl;

    string logoutCommand = "a4 LOGOUT\r\n";
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
