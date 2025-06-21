#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <vector>
#include <sstream>

using namespace std;

struct Email {
    int id;
    string subject;
    string from;
    string to;
    string body;
    bool seen;

    Email(const int id, const string &subject, const string &from, const string &to, const string &body)
        : id(id), subject(subject), from(from), to(to), body(body), seen(false) {
    }
};

int main() {
    constexpr int port = 1143;

    int clientSocket;
    sockaddr_in serverAddress{};
    vector<Email> emails;
    string currentMailbox = "";
    bool authenticated = false;

    emails.push_back(Email(1, "Welcome Message", "admin@example.com", "user@example.com",
                           "Welcome to our IMAP server simulation!"));
    emails.push_back(Email(2, "Test Message", "test@example.com", "user@example.com",
                           "This is a test email for the IMAP server."));
    emails.push_back(Email(3, "Unread Message", "noreply@example.com", "user@example.com",
                           "This is an unread message."));

    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Error creating socket" << endl;

        return 1;
    }

    constexpr int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Error setting socket options" << endl;

        close(serverSocket);

        return 1;
    }

    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);

    if (bind(serverSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Error binding socket" << endl;

        close(serverSocket);

        return 1;
    }

    if (listen(serverSocket, 5) < 0) {
        cerr << "Error listening on socket" << endl;

        close(serverSocket);

        return 1;
    }

    cout << "IMAP server running on port " << ntohs(serverAddress.sin_port) << endl;
    cout << "Waiting for connections..." << endl;

    auto sendResponse = [&clientSocket](const string &response) {
        send(clientSocket, response.c_str(), response.length(), 0);
    };

    auto handleCommand = [&emails, &authenticated, &currentMailbox, &sendResponse](const string &command) {
        istringstream iss(command);
        string tag, cmd;
        iss >> tag >> cmd;

        for (auto &c: cmd) {
            c = toupper(c);
        }

        if (cmd == "LOGIN") {
            string username, password;
            iss >> username >> password;

            authenticated = true;
            currentMailbox = "";
            sendResponse("* OK IMAP4rev1 Service Ready\r\n" + tag + " OK LOGIN completed\r\n");
        } else if (!authenticated) {
            sendResponse(tag + " NO Authentication required\r\n");
        } else if (cmd == "LOGOUT") {
            sendResponse("* BYE IMAP4rev1 Server logging out\r\n" + tag + " OK LOGOUT completed\r\n");
            authenticated = false;
        } else if (cmd == "CAPABILITY") {
            sendResponse("* CAPABILITY IMAP4rev1 AUTH=PLAIN\r\n" + tag + " OK CAPABILITY completed\r\n");
        } else if (cmd == "LIST") {
            string reference, mailbox;
            iss >> reference >> mailbox;

            sendResponse("* LIST (\\Noinferiors) \"/\" \"INBOX\"\r\n"
                         "* LIST (\\Noselect) \"/\" \"Drafts\"\r\n"
                         "* LIST (\\Noselect) \"/\" \"Sent\"\r\n"
                         + tag + " OK LIST completed\r\n");
        } else if (cmd == "SELECT") {
            string mailbox;
            iss >> mailbox;

            if (mailbox.front() == '"' && mailbox.back() == '"') {
                mailbox = mailbox.substr(1, mailbox.length() - 2);
            }

            currentMailbox = mailbox;
            int unseenCount = 0;

            for (const auto &email: emails) {
                if (!email.seen) {
                    unseenCount++;
                }
            }

            sendResponse("* " + to_string(emails.size()) + " EXISTS\r\n"
                         "* " + to_string(unseenCount) + " RECENT\r\n"
                         "* OK [UNSEEN " + to_string(unseenCount) + "]\r\n"
                         "* OK [UIDVALIDITY 1234567890]\r\n"
                         "* FLAGS (\\Answered \\Flagged \\Deleted \\Seen \\Draft)\r\n"
                         + tag + " OK [READ-WRITE] SELECT completed\r\n");
        } else if (cmd == "SEARCH") {
            string criteria;
            iss >> criteria;

            if (criteria == "UNSEEN") {
                stringstream searchResult;
                searchResult << "* SEARCH";

                for (size_t i = 0; i < emails.size(); ++i) {
                    if (!emails[i].seen) {
                        searchResult << " " << (i + 1);
                    }
                }
                searchResult << "\r\n";

                sendResponse(searchResult.str() + tag + " OK SEARCH completed\r\n");
            } else {
                sendResponse("* SEARCH\r\n" + tag + " OK SEARCH completed\r\n");
            }
        } else if (cmd == "FETCH") {
            int messageId;
            string fetchType;
            iss >> messageId >> fetchType;

            if (messageId > 0 && messageId <= static_cast<int>(emails.size())) {
                const Email &email = emails[messageId - 1];

                if (fetchType == "BODY[]" || fetchType == "BODY[TEXT]") {
                    sendResponse(
                        "* " + to_string(messageId) + " FETCH (BODY[] {" + to_string(email.body.length() + 100) +
                        "}\r\n"
                        "From: " + email.from + "\r\n"
                        "To: " + email.to + "\r\n"
                        "Subject: " + email.subject + "\r\n\r\n"
                        + email.body + ")\r\n"
                        + tag + " OK FETCH completed\r\n");
                } else {
                    sendResponse(tag + " BAD Invalid fetch parameters\r\n");
                }
            } else {
                sendResponse(tag + " NO Message not found\r\n");
            }
        } else if (cmd == "STORE") {
            int messageId;
            string mode, flagsStr;
            iss >> messageId >> mode;

            getline(iss, flagsStr);

            if (messageId > 0 && messageId <= static_cast<int>(emails.size())) {
                if (mode == "+FLAGS" && flagsStr.find("\\Seen") != string::npos) {
                    emails[messageId - 1].seen = true;
                    sendResponse("* " + to_string(messageId) + " FETCH (FLAGS (\\Seen))\r\n"
                                 + tag + " OK STORE completed\r\n");
                } else if (mode == "+FLAGS" && flagsStr.find("\\Deleted") != string::npos) {
                    sendResponse("* " + to_string(messageId) + " FETCH (FLAGS (\\Deleted))\r\n"
                                 + tag + " OK STORE completed\r\n");
                } else {
                    sendResponse(tag + " OK STORE completed\r\n");
                }
            } else {
                sendResponse(tag + " NO Message not found\r\n");
            }
        } else if (cmd == "EXPUNGE") {
            sendResponse("* 1 EXPUNGE\r\n" + tag + " OK EXPUNGE completed\r\n");
        } else {
            sendResponse(tag + " BAD Command not recognized\r\n");
        }
    };

    while (true) {
        sockaddr_in clientAddress;
        socklen_t clientAddressLength = sizeof(clientAddress);

        clientSocket = accept(serverSocket, reinterpret_cast<struct sockaddr *>(&clientAddress), &clientAddressLength);
        if (clientSocket < 0) {
            cerr << "Error accepting connection" << endl;
            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, INET_ADDRSTRLEN);
        const int clientPort = ntohs(clientAddress.sin_port);

        cout << "Client connected from " << clientIp << ":" << clientPort << endl;

        sendResponse("* OK IMAP4rev1 Server ready\r\n");

        char buffer[4096];
        while (true) {
            memset(buffer, 0, sizeof(buffer));

            if (const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0); bytesRead <= 0) {
                break;
            }

            string command(buffer);
            cout << "Received: " << command;

            handleCommand(command);
        }

        cout << "Client disconnected" << endl;
        close(clientSocket);
    }

    close(serverSocket);

    return 0;
}
