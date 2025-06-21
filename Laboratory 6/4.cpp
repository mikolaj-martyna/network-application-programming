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
    for (char c : str) {
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

int main() {
    const string server = "smtp.interia.pl";
    constexpr int port = 587;

    const string sender = "pas2017@interia.pl";
    const string password = "P4SInf2017";
    const string recipient = "pasinf2017@interia.pl";
    const string subject = "Email with attachment";
    const string body = "Email with attachment";

    const string attachmentPath = "test.txt";
    const string attachmentName = "test.txt";

    const string boundary = generateBoundary();
    const string encodedUsername = encodeString(sender);
    const string encodedPassword = encodeString(password);

    const string base64Attachment = readFileToBase64(attachmentPath);
    if (base64Attachment.empty()) {
        cerr << "Failed to read attachment." << endl;

        return 1;
    }

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
        close(clientSocket);

        return 1;
    }

    if (const string mailFrom = "MAIL FROM:<" + sender + ">\r\n"; !sendCommand(
        clientSocket, mailFrom, buffer, BUFFER_SIZE)) {
        close(clientSocket);

        return 1;
    }

    if (const string rcptTo = "RCPT TO:<" + recipient + ">\r\n"; !
        sendCommand(clientSocket, rcptTo, buffer, BUFFER_SIZE)) {
        close(clientSocket);

        return 1;
    }

    if (!sendCommand(clientSocket, "DATA\r\n", buffer, BUFFER_SIZE)) {
        close(clientSocket);

        return 1;
    }

    string message = "From: " + sender + "\r\n";
    message += "To: " + recipient + "\r\n";
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

    cout << "Email with attachment sent successfully!" << endl;

    return 0;
}
