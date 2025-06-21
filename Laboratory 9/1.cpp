#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fstream>
#include <string>

using namespace std;

int main() {
    const string host = "httpbin.org";
    constexpr int port = 80;
    const string path = "/html";
    const string outputFile = "httpbin.html";

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    hostent *server = gethostbyname(host.c_str());
    if (server == nullptr) {
        cerr << "Error: Could not resolve hostname " << host << endl;

        close(clientSocket);

        return 1;
    }

    sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    memcpy(&serverAddress.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect to " << host << ":" << port << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Connected to " << host << ":" << port << endl;

    string httpRequest = "GET " + path + " HTTP/1.1\r\n"
                        "Host: " + host + "\r\n"
                        "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9_3) AppleWebKit/537.75.14 (KHTML, like Gecko) Version/7.0.3 Safari/7046A194A\r\n"
                        "Accept: text/html,application/xhtml+xml,application/xml\r\n"
                        "Connection: close\r\n\r\n";

    if (const ssize_t bytesSent = send(clientSocket, httpRequest.c_str(), httpRequest.length(), 0); bytesSent < 0) {
        cerr << "Failed to send HTTP request." << endl;

        close(clientSocket);

        return 1;
    } else {
        cout << "Sent " << bytesSent << " bytes." << endl;
    }

    string response;
    char buffer[4096];
    ssize_t bytesRead;

    while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }

    if (bytesRead < 0) {
        cerr << "Failed to receive data." << endl;

        close(clientSocket);

        return 1;
    }

    close(clientSocket);

    size_t headerEnd = response.find("\r\n\r\n");
    if (headerEnd == string::npos) {
        cerr << "Invalid HTTP response (no header separator found)." << endl;

        return 1;
    }

    string headers = response.substr(0, headerEnd);
    string body = response.substr(headerEnd + 4);

    cout << "HTTP response received. Headers size: " << headers.size()
         << " bytes, Body size: " << body.size() << " bytes." << endl;

    ofstream htmlFile(outputFile);
    if (!htmlFile) {
        cerr << "Failed to create output file: " << outputFile << endl;

        return 1;
    }

    htmlFile << body;
    htmlFile.close();

    cout << "HTML content saved to " << outputFile << endl;

    return 0;
}
