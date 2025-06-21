#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <vector>
#include <algorithm>
#include <map>

using namespace std;

constexpr int DEFAULT_PORT = 2525;
constexpr size_t BUFFER_SIZE = 1024;

struct ClientState {
    bool greeted = false;
    bool authenticated = false;
    bool hasMailFrom = false;
    bool inData = false;
    string mailFrom;
    vector<string> recipients;
    string messageData;

    void reset() {
        hasMailFrom = false;
        recipients.clear();
        messageData.clear();
        inData = false;
    }
};

map<int, ClientState> clientStates;

void sendResponse(const int clientSocket, const string& responseCode, const string& message) {
    const string response = responseCode + " " + message + "\r\n";
    send(clientSocket, response.c_str(), response.length(), 0);

    cout << "S: " << response;
}

void handleEHLO(const int clientSocket, const string& domain) {
    clientStates[clientSocket].greeted = true;

    sendResponse(clientSocket, "250", "Hello " + domain);
    sendResponse(clientSocket, "250", "STARTTLS");
    sendResponse(clientSocket, "250", "AUTH LOGIN PLAIN");
    sendResponse(clientSocket, "250", "HELP");
    sendResponse(clientSocket, "250", "SIZE 35882577");
    sendResponse(clientSocket, "250", "OK");
}

void handleHELO(const int clientSocket, const string& domain) {
    clientStates[clientSocket].greeted = true;
    sendResponse(clientSocket, "250", "Hello " + domain);
}

void handleMAIL(const int clientSocket, const string& command) {
    auto& state = clientStates[clientSocket];

    if (!state.greeted) {
        sendResponse(clientSocket, "503", "Error: send HELO/EHLO first");
        return;
    }

    const size_t start = command.find('<');
    const size_t end = command.find('>');
    if (start == string::npos || end == string::npos || start >= end) {
        sendResponse(clientSocket, "501", "Syntax error in parameters");
        return;
    }

    state.mailFrom = command.substr(start + 1, end - start - 1);
    state.hasMailFrom = true;
    state.recipients.clear();

    sendResponse(clientSocket, "250", "OK");
}

void handleRCPT(const int clientSocket, const string& command) {
    auto& state = clientStates[clientSocket];

    if (!state.hasMailFrom) {
        sendResponse(clientSocket, "503", "Error: need MAIL command");
        return;
    }

    const size_t start = command.find('<');
    const size_t end = command.find('>');
    if (start == string::npos || end == string::npos || start >= end) {
        sendResponse(clientSocket, "501", "Syntax error in parameters");
        return;
    }

    const string recipient = command.substr(start + 1, end - start - 1);
    state.recipients.push_back(recipient);

    sendResponse(clientSocket, "250", "OK");
}

void handleDATA(const int clientSocket) {
    auto& state = clientStates[clientSocket];

    if (state.recipients.empty()) {
        sendResponse(clientSocket, "503", "Error: need RCPT command");
        return;
    }

    state.inData = true;
    sendResponse(clientSocket, "354", "End data with <CR><LF>.<CR><LF>");
}

void handleAUTH(const int clientSocket, const string& command) {
    string authType;
    if (const size_t pos = command.find(' '); pos != string::npos && pos + 1 < command.length()) {
        authType = command.substr(pos + 1);
    }

    if (authType == "LOGIN") {
        sendResponse(clientSocket, "334", "VXNlcm5hbWU6");
    } else {
        clientStates[clientSocket].authenticated = true;
        sendResponse(clientSocket, "235", "2.7.0 Authentication successful");
    }
}

void handleDataContent(int clientSocket, const string& line) {
    auto& state = clientStates[clientSocket];

    if (line == ".") {
        state.inData = false;

        cout << "Received mail from: " << state.mailFrom << endl;
        cout << "Recipients: ";
        for (const auto& recipient : state.recipients) {
            cout << recipient << " ";
        }
        cout << endl;
        cout << "Message size: " << state.messageData.length() << " bytes" << endl;

        sendResponse(clientSocket, "250", "OK: message queued");

        state.reset();
    } else {
        if (!line.empty() && line[0] == '.') {
            state.messageData += line.substr(1);
        } else {
            state.messageData += line;
        }
        state.messageData += "\n";
    }
}

void handleSTARTTLS(const int clientSocket) {
    sendResponse(clientSocket, "220", "Ready to start TLS");
}

void handleQUIT(const int clientSocket) {
    sendResponse(clientSocket, "221", "Bye");

    clientStates.erase(clientSocket);
}

void handleRSET(const int clientSocket) {
    clientStates[clientSocket].reset();
    sendResponse(clientSocket, "250", "OK");
}

void handleNOOP(const int clientSocket) {
    sendResponse(clientSocket, "250", "OK");
}

void handleCommand(const int clientSocket, const string& command) {
    cout << "C: " << command << endl;

    if (const auto& state = clientStates[clientSocket]; state.inData) {
        handleDataContent(clientSocket, command);
        return;
    }

    string cmd;
    string args;

    if (const size_t spacePos = command.find(' '); spacePos != string::npos) {
        cmd = command.substr(0, spacePos);
        args = command.substr(spacePos + 1);
    } else {
        cmd = command;
    }

    ranges::transform(cmd, cmd.begin(), ::toupper);

    if (cmd == "EHLO" || cmd == "HELO") {
        if (cmd == "EHLO") {
            handleEHLO(clientSocket, args);
        } else {
            handleHELO(clientSocket, args);
        }
    } else if (cmd == "MAIL") {
        handleMAIL(clientSocket, args);
    } else if (cmd == "RCPT") {
        handleRCPT(clientSocket, args);
    } else if (cmd == "DATA") {
        handleDATA(clientSocket);
    } else if (cmd == "AUTH") {
        handleAUTH(clientSocket, command);
    } else if (cmd == "STARTTLS") {
        handleSTARTTLS(clientSocket);
    } else if (cmd == "QUIT") {
        handleQUIT(clientSocket);
    } else if (cmd == "RSET") {
        handleRSET(clientSocket);
    } else if (cmd == "NOOP") {
        handleNOOP(clientSocket);
    } else {
        sendResponse(clientSocket, "502", "Command not implemented");
    }
}

int main() {
    int port = DEFAULT_PORT;

    if (argc > 1) {
        port = stoi(argv[1]);
    }

    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    constexpr int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Failed to set socket options." << endl;

        close(serverSocket);

        return 1;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr) <= 0) {
        cerr << "Invalid address." << endl;

        close(serverSocket);

        return 1;
    }

    if (bind(serverSocket, reinterpret_cast<const sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        cerr << "Failed to bind socket." << endl;

        close(serverSocket);

        return 1;
    }

    if (listen(serverSocket, 10) < 0) {
        cerr << "Failed to listen." << endl;

        close(serverSocket);

        return 1;
    }

    cout << "SMTP server started on 127.0.0.1:" << port << endl;

    while (true) {
        sockaddr_in clientAddr = {};
        socklen_t addrLen = sizeof(clientAddr);

        const int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
        if (clientSocket < 0) {
            cerr << "Failed to accept connection." << endl;
            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, INET_ADDRSTRLEN);
        cout << "Client connected: " << clientIp << endl;

        clientStates[clientSocket] = ClientState();

        sendResponse(clientSocket, "220", "SMTP Server Ready");

        char buffer[BUFFER_SIZE];
        string command;
        bool clientConnected = true;

        while (clientConnected) {
            memset(buffer, 0, BUFFER_SIZE);

            if (const ssize_t bytesRead = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0); bytesRead <= 0) {
                cout << "Client disconnected" << endl;
                clientConnected = false;
                break;
            }

            string data(buffer);

            size_t pos = 0;
            while ((pos = data.find("\r\n", pos)) != string::npos) {
                if (string cmd = data.substr(0, pos); !cmd.empty()) {
                    handleCommand(clientSocket, cmd);
                }

                data = data.substr(pos + 2);
                pos = 0;
            }

            if (!clientStates.contains(clientSocket)) {
                clientConnected = false;
            }
        }

        close(clientSocket);
    }

    close(serverSocket);

    return 0;
}
