#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <fstream>
#include <sstream>

using namespace std;

constexpr size_t BUFFER_SIZE = 1024;

string base64Encode(const vector<unsigned char> &input) {
    static const string base64Chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string output;

    int val = 0, valb = -6;
    for (const unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;

        while (valb >= 0) {
            output.push_back(base64Chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        output.push_back(base64Chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (output.size() % 4) {
        output.push_back('=');
    }

    return output;
}

string encodeString(const string &str) {
    vector<unsigned char> data;

    for (const char c : str) {
        data.push_back(static_cast<unsigned char>(c));
    }

    return base64Encode(data);
}

string readFileToBase64(const string &filePath) {
    ifstream file(filePath, ios::binary);
    if (!file) {
        cerr << "Failed to open file: " << filePath << endl;

        return "";
    }

    vector<unsigned char> fileData;
    char byte;
    while (file.get(byte)) {
        fileData.push_back(static_cast<unsigned char>(byte));
    }

    file.close();

    return base64Encode(fileData);
}

string generateBoundary() {
    return "------------BOUNDARY_STRING_" + to_string(time(nullptr));
}

bool sendCommand(const int sock, const string &command, char *buffer, const size_t bufferSize) {
    cout << "C: " << command;

    if (send(sock, command.c_str(), command.length(), 0) < 0) {
        cerr << "Failed to send command." << endl;

        return false;
    }

    memset(buffer, 0, bufferSize);
    if (const ssize_t bytesReceived = recv(sock, buffer, bufferSize - 1, 0); bytesReceived < 0) {
        cerr << "Failed to receive response." << endl;

        return false;
    }

    cout << "S: " << buffer;

    const int responseCode = stoi(string(buffer, 3));

    return (responseCode >= 200 && responseCode < 400);
}

string getInput(const string &prompt) {
    string input;
    cout << prompt;
    getline(cin, input);

    return input;
}

vector<string> getRecipients() {
    vector<string> recipients;
    string input;

    cout << "Enter recipients (comma-separated email addresses): ";
    getline(cin, input);

    stringstream ss(input);
    string email;

    while (getline(ss, email, ',')) {
        if (const size_t start = email.find_first_not_of(" \t"); start != string::npos) {
            const size_t end = email.find_last_not_of(" \t");
            email = email.substr(start, end - start + 1);

            if (!email.empty()) {
                recipients.push_back(email);
            }
        }
    }

    return recipients;
}

string getMessageBody() {
    cout << "Enter message body (end with a line containing only '.'):" << endl;
    string line;
    string body;

    while (getline(cin, line)) {
        if (line == ".") {
            break;
        }

        body += line + "\n";
    }

    return body;
}

string getFilename(const string &path) {
    size_t lastSlash = path.find_last_of("/\\");
    if (lastSlash != string::npos) {
        return path.substr(lastSlash + 1);
    }

    return path;
}

int main() {
    const string server = "smtp.interia.pl";
    constexpr int port = 587;

    const string sender = getInput("Enter sender email: ");
    const string password = getInput("Enter password: ");

    const vector<string> recipients = getRecipients();
    if (recipients.empty()) {
        cerr << "No valid recipients provided." << endl;

        return 1;
    }

    const string subject = getInput("Enter subject: ");
    const string body = getMessageBody();
    const string attachmentPath = getInput("Enter attachment file path: ");

    string attachmentName = getFilename(attachmentPath);

    cout << "Reading attachment file..." << endl;
    const string base64Attachment = readFileToBase64(attachmentPath);
    if (base64Attachment.empty()) {
        cerr << "Failed to read attachment file. Aborting." << endl;

        return 1;
    }

    cout << "Attachment file read successfully." << endl;

    const string boundary = generateBoundary();
    const string encodedUsername = encodeString(sender);
    const string encodedPassword = encodeString(password);

    cout << "Connecting to " << server << ":" << port << "..." << endl;

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    const hostent *host = gethostbyname(server.c_str());
    if (!host) {
        cerr << "Failed to resolve hostname." << endl;

        close(clientSocket);

        return 1;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    memcpy(&serverAddr.sin_addr, host->h_addr, host->h_length);

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddr), sizeof(serverAddr)) < 0) {
        cerr << "Failed to connect to server." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Connected to " << server << ":" << port << endl;

    char buffer[BUFFER_SIZE] = {};

    if (const ssize_t bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0); bytesReceived < 0) {
        cerr << "Failed to receive greeting." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "S: " << buffer;

    if (!sendCommand(clientSocket, "EHLO client.example.com\r\n", buffer, BUFFER_SIZE)) {
        close(clientSocket);

        return 1;
    }

    if (!sendCommand(clientSocket, "STARTTLS\r\n", buffer, BUFFER_SIZE)) {
        close(clientSocket);

        return 1;
    }

    if (!sendCommand(clientSocket, "EHLO client.example.com\r\n", buffer, BUFFER_SIZE)) {
        close(clientSocket);

        return 1;
    }

    if (!sendCommand(clientSocket, "AUTH LOGIN\r\n", buffer, BUFFER_SIZE)) {
        close(clientSocket);

        return 1;
    }

    if (!sendCommand(clientSocket, encodedUsername + "\r\n", buffer, BUFFER_SIZE)) {
        close(clientSocket);

        return 1;
    }

    if (!sendCommand(clientSocket, encodedPassword + "\r\n", buffer, BUFFER_SIZE)) {
        cerr << "Authentication failed. Check your email and password." << endl;

        close(clientSocket);

        return 1;
    }

    if (const string mailFrom = "MAIL FROM:<" + sender + ">\r\n"; !sendCommand(
        clientSocket, mailFrom, buffer, BUFFER_SIZE)) {
        close(clientSocket);

        return 1;
    }

    for (const auto &recipient : recipients) {
        if (const string rcptTo = "RCPT TO:<" + recipient + ">\r\n"; !
            sendCommand(clientSocket, rcptTo, buffer, BUFFER_SIZE)) {
            close(clientSocket);

            return 1;
        }
    }

    if (!sendCommand(clientSocket, "DATA\r\n", buffer, BUFFER_SIZE)) {
        close(clientSocket);

        return 1;
    }

    string message = "From: " + sender + "\r\n";
    message += "To: ";

    for (size_t i = 0; i < recipients.size(); ++i) {
        message += recipients[i];
        if (i < recipients.size() - 1) {
            message += ", ";
        }
    }
    message += "\r\n";

    message += "Subject: " + subject + "\r\n";
    message += "MIME-Version: 1.0\r\n";
    message += "Content-Type: multipart/mixed; boundary=\"" + boundary + "\"\r\n";
    message += "\r\n";

    message += "--" + boundary + "\r\n";
    message += "Content-Type: text/plain; charset=utf-8\r\n";
    message += "Content-Transfer-Encoding: 7bit\r\n";
    message += "\r\n";
    message += body + "\r\n";
    message += "\r\n";

    message += "--" + boundary + "\r\n";
    message += "Content-Type: text/plain; charset=utf-8; name=\"" + attachmentName + "\"\r\n";
    message += "Content-Transfer-Encoding: base64\r\n";
    message += "Content-Disposition: attachment; filename=\"" + attachmentName + "\"\r\n";
    message += "\r\n";

    for (size_t i = 0; i < base64Attachment.length(); i += 76) {
        message += base64Attachment.substr(i, 76) + "\r\n";
    }

    message += "\r\n";
    message += "--" + boundary + "--\r\n";
    message += ".\r\n";

    if (!sendCommand(clientSocket, message, buffer, BUFFER_SIZE)) {
        close(clientSocket);

        return 1;
    }

    sendCommand(clientSocket, "QUIT\r\n", buffer, BUFFER_SIZE);

    close(clientSocket);

    cout << "Email with attachment sent successfully to " << recipients.size() << " recipient(s)!" << endl;

    return 0;
}
