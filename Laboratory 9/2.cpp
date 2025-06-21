#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

int main() {
    const string host = "httpbin.org";
    constexpr int port = 80;
    const string path = "/image/png";
    const string outputFile = "httpbin.png";

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
                         "Accept: image/png,image/*,*/*\r\n"
                         "Connection: close\r\n\r\n";

    if (const ssize_t bytesSent = send(clientSocket, httpRequest.c_str(), httpRequest.length(), 0); bytesSent < 0) {
        cerr << "Failed to send HTTP request." << endl;

        close(clientSocket);

        return 1;
    } else {
        cout << "Sent " << bytesSent << " bytes." << endl;
    }

    vector<char> responseData;
    char buffer[4096];
    ssize_t bytesRead;

    while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        responseData.insert(responseData.end(), buffer, buffer + bytesRead);
    }

    if (bytesRead < 0) {
        cerr << "Failed to receive data." << endl;

        close(clientSocket);

        return 1;
    }

    close(clientSocket);

    string response(responseData.begin(), responseData.end());

    size_t headerEnd = response.find("\r\n\r\n");
    if (headerEnd == string::npos) {
        cerr << "Invalid HTTP response (no header separator found)." << endl;

        return 1;
    }

    string headers = response.substr(0, headerEnd);
    cout << "HTTP response received. Headers size: " << headers.size() << " bytes, ";
    cout << "Body size: " << (responseData.size() - (headerEnd + 4)) << " bytes." << endl;

    cout << "Response headers:" << endl;
    size_t endOfLine = 0;
    size_t startOfLine = 0;
    for (int i = 0; i < 3 && startOfLine < headerEnd; i++) {
        endOfLine = headers.find("\r\n", startOfLine);

        if (endOfLine == string::npos) break;

        cout << headers.substr(startOfLine, endOfLine - startOfLine) << endl;
        startOfLine = endOfLine + 2;
    }

    ofstream imageFile(outputFile, ios::binary);
    if (!imageFile) {
        cerr << "Failed to create output file: " << outputFile << endl;

        return 1;
    }

    imageFile.write(&responseData[headerEnd + 4], responseData.size() - (headerEnd + 4));
    imageFile.close();

    cout << "PNG image saved to " << outputFile << endl;

    return 0;
}
