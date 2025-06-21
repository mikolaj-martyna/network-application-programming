#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sstream>
#include <vector>
#include <thread>
#include <mutex>
#include <map>

using namespace std;

struct Email {
    string from;
    string to;
    string subject;
    string body;
    vector<pair<string, string> > attachments;
};

string base64Encode(const string &input) {
    static const string base64Chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    string encoded;
    int val = 0;
    int valb = -6;

    for (const unsigned char c: input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64Chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        encoded.push_back(base64Chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (encoded.size() % 4 != 0) {
        encoded.push_back('=');
    }

    return encoded;
}

void handleClient(int clientSocket, map<string, string> &users, map<string, vector<Email> > &userEmails,
                  mutex &emailsMutex) {
    char buffer[4096] = {0};
    string currentUser;
    bool isAuthenticated = false;
    string authenticationStage;

    send(clientSocket, "+OK POP3 server ready\r\n", 23, 0);

    while (true) {
        memset(buffer, 0, sizeof(buffer));

        if (const ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0); bytesReceived <= 0) {
            break;
        }

        string command(buffer);
        if (!command.empty() && command.back() == '\n') {
            command.pop_back();
        }
        if (!command.empty() && command.back() == '\r') {
            command.pop_back();
        }

        cout << "Received command: " << command << endl;

        if (command.substr(0, 4) == "USER") {
            currentUser = command.substr(5);
            authenticationStage = "USER";
            send(clientSocket, "+OK User accepted\r\n", 19, 0);
        } else if (command.substr(0, 4) == "PASS") {
            if (authenticationStage != "USER") {
                send(clientSocket, "-ERR Send USER first\r\n", 22, 0);
            } else {
                if (const string password = command.substr(5);
                    users.find(currentUser) != users.end() && users[currentUser] == password) {
                    isAuthenticated = true;
                    send(clientSocket, "+OK Login successful\r\n", 23, 0);
                } else {
                    send(clientSocket, "-ERR Login failed\r\n", 19, 0);
                }
            }
        } else if (command == "QUIT") {
            send(clientSocket, "+OK POP3 server signing off\r\n", 30, 0);
            break;
        } else if (!isAuthenticated) {
            send(clientSocket, "-ERR Not authenticated\r\n", 23, 0);
        } else if (command == "STAT") {
            int numEmails = 0;
            int totalSize = 0;

            emailsMutex.lock();
            if (userEmails.find(currentUser) != userEmails.end()) {
                numEmails = userEmails[currentUser].size();

                for (const auto &email: userEmails[currentUser]) {
                    totalSize += email.body.length();
                }
            }
            emailsMutex.unlock();

            const string response = "+OK " + to_string(numEmails) + " " + to_string(totalSize) + "\r\n";
            send(clientSocket, response.c_str(), response.length(), 0);
        } else if (command.substr(0, 4) == "LIST") {
            emailsMutex.lock();
            int numEmails = 0;

            if (userEmails.find(currentUser) != userEmails.end()) {
                numEmails = userEmails[currentUser].size();
            }

            string response = "+OK " + to_string(numEmails) + " messages\r\n";

            if (userEmails.find(currentUser) != userEmails.end()) {
                for (size_t i = 0; i < userEmails[currentUser].size(); i++) {
                    response += to_string(i + 1) + " " + to_string(userEmails[currentUser][i].body.length()) + "\r\n";
                }
            }

            response += ".\r\n";
            emailsMutex.unlock();

            send(clientSocket, response.c_str(), response.length(), 0);
        } else if (command.substr(0, 4) == "RETR") {
            const int emailIndex = stoi(command.substr(5)) - 1;

            emailsMutex.lock();
            if (userEmails.find(currentUser) != userEmails.end() && emailIndex >= 0 &&
                emailIndex < static_cast<int>(userEmails[currentUser].size())) {
                const auto &[from, to, subject, body, attachments] = userEmails[currentUser][emailIndex];
                const string boundary = "boundary_" + to_string(rand());
                stringstream response;

                response << "+OK message follows\r\n";
                response << "From: " << from << "\r\n";
                response << "To: " << to << "\r\n";
                response << "Subject: " << subject << "\r\n";

                if (!attachments.empty()) {
                    response << "MIME-Version: 1.0\r\n";
                    response << "Content-Type: multipart/mixed; boundary=\"" << boundary << "\"\r\n";
                    response << "\r\n";
                    response << "This is a multi-part message in MIME format.\r\n";
                    response << "--" << boundary << "\r\n";
                    response << "Content-Type: text/plain; charset=utf-8\r\n";
                    response << "Content-Transfer-Encoding: 8bit\r\n\r\n";
                    response << body << "\r\n\r\n";

                    for (const auto &[filename, contentType]: attachments) {
                        response << "--" << boundary << "\r\n";
                        response << "Content-Type: " << contentType << "\r\n";
                        response << "Content-Disposition: attachment; filename=\"" << filename << "\"\r\n";
                        response << "Content-Transfer-Encoding: base64\r\n\r\n";

                        string dummyImageData = "This is a simulated image content for testing attachments.";
                        string base64Data = base64Encode(dummyImageData);

                        for (size_t i = 0; i < base64Data.length(); i += 76) {
                            response << base64Data.substr(i, 76) << "\r\n";
                        }
                        response << "\r\n";
                    }

                    response << "--" << boundary << "--\r\n";
                } else {
                    response << "\r\n" << body << "\r\n";
                }

                response << ".\r\n";

                const string responseStr = response.str();
                send(clientSocket, responseStr.c_str(), responseStr.length(), 0);
            } else {
                send(clientSocket, "-ERR No such message\r\n", 22, 0);
            }
            emailsMutex.unlock();
        } else if (command.substr(0, 4) == "DELE") {
            send(clientSocket, "+OK Message deleted\r\n", 22, 0);
        } else if (command == "RSET") {
            send(clientSocket, "+OK\r\n", 5, 0);
        } else if (command == "NOOP") {
            send(clientSocket, "+OK\r\n", 5, 0);
        } else {
            send(clientSocket, "-ERR Command not implemented\r\n", 30, 0);
        }
    }

    close(clientSocket);
    cout << "Client disconnected" << endl;
}

int main() {
    const string ipAddress = "127.0.0.1";
    constexpr int port = 8110;  // Changed from 110 to 8110 (non-privileged port)

    mutex emailsMutex;

    map<string, string> users;
    map<string, vector<Email> > userEmails;

    users["pas2017@interia.pl"] = "P4SInf2017";

    Email email1;
    email1.from = "sender@example.com";
    email1.to = "pas2017@interia.pl";
    email1.subject = "Test email with attachment";
    email1.body = "This is a test email with an image attachment.";
    email1.attachments.push_back(make_pair("test_image.jpg", "image/jpeg"));

    Email email2;
    email2.from = "another@example.com";
    email2.to = "pas2017@interia.pl";
    email2.subject = "Second test email";
    email2.body = "This is another test email without attachments.";

    Email email3;
    email3.from = "third@example.com";
    email3.to = "pas2017@interia.pl";
    email3.subject = "Email with multiple attachments";
    email3.body = "This email contains multiple image attachments.";
    email3.attachments.push_back(make_pair("image1.png", "image/png"));
    email3.attachments.push_back(make_pair("image2.gif", "image/gif"));

    vector emails = {email1, email2, email3};
    userEmails["pas2017@interia.pl"] = emails;

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Failed to set socket options." << endl;

        close(serverSocket);

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, ipAddress.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address." << endl;

        close(serverSocket);

        return 1;
    }

    if (bind(serverSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to bind socket." << endl;

        close(serverSocket);

        return 1;
    }

    if (listen(serverSocket, 5) < 0) {
        cerr << "Failed to listen on socket." << endl;

        close(serverSocket);

        return 1;
    }

    cout << "POP3 server started on " << ipAddress << ":" << port << endl;

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientAddressLength = sizeof(clientAddress);

        const int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress), &clientAddressLength);
        if (clientSocket < 0) {
            cerr << "Failed to accept connection." << endl;
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddress.sin_addr), clientIP, INET_ADDRSTRLEN);
        cout << "New connection from " << clientIP << ":" << ntohs(clientAddress.sin_port) << endl;

        thread clientThread(handleClient, clientSocket, ref(users), ref(userEmails), ref(emailsMutex));
        clientThread.detach();
    }

    close(serverSocket);

    return 0;
}
