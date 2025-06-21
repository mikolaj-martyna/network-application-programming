#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

vector<char> downloadRange(const string& host, const int port, const string& path, const long startByte, const long endByte) {
    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return {};
    }

    sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = inet_addr(host.c_str());

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect to " << host << ":" << port << endl;

        close(clientSocket);

        return {};
    }

    cout << "Connected to " << host << ":" << port << " for bytes " << startByte << "-" << endByte << endl;

    const string httpRequest = "GET " + path + " HTTP/1.1\r\n"
                         "Host: " + host + "\r\n"
                         "User-Agent: ImageDownloader/1.0\r\n"
                         "Accept: image/jpeg,image/*\r\n"
                         "Range: bytes=" + to_string(startByte) + "-" + to_string(endByte) + "\r\n"
                         "Connection: close\r\n\r\n";

    if (const ssize_t bytesSent = send(clientSocket, httpRequest.c_str(), httpRequest.length(), 0); bytesSent < 0) {
        cerr << "Failed to send HTTP request for range " << startByte << "-" << endByte << endl;

        close(clientSocket);

        return {};
    } else {
        cout << "Sent request for range " << startByte << "-" << endByte << " (" << bytesSent << " bytes)" << endl;
    }

    vector<char> responseData;
    char buffer[4096];
    ssize_t bytesRead;

    while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        responseData.insert(responseData.end(), buffer, buffer + bytesRead);
    }

    if (bytesRead < 0) {
        cerr << "Failed to receive data for range " << startByte << "-" << endByte << endl;

        close(clientSocket);

        return {};
    }

    close(clientSocket);

    string responseStr(responseData.begin(), responseData.end());
    const size_t headerEnd = responseStr.find("\r\n\r\n");
    if (headerEnd == string::npos) {
        cerr << "Invalid HTTP response (no header separator found) for range " << startByte << "-" << endByte << endl;

        return {};
    }

    string headers = responseStr.substr(0, headerEnd);
    cout << "Received response for range " << startByte << "-" << endByte << ":" << endl;

    const size_t statusLineEnd = headers.find("\r\n");
    const string statusLine = headers.substr(0, statusLineEnd);
    cout << statusLine << endl;

    if (statusLine.find("206") == string::npos) {
        cerr << "WARNING: Expected 206 Partial Content, but got: " << statusLine << endl;
    }

    vector bodyData(responseData.begin() + headerEnd + 4, responseData.end());
    cout << "Received " << bodyData.size() << " bytes of image data" << endl;

    return bodyData;
}

long getContentLength(const string& host, const int port, const string& path) {
    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket for HEAD request." << endl;

        return 1;
    }

    sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = inet_addr(host.c_str());

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect to " << host << ":" << port << " for HEAD request" << endl;

        close(clientSocket);

        return 1;
    }

    const string httpRequest = "HEAD " + path + " HTTP/1.1\r\n"
                         "Host: " + host + "\r\n"
                         "Connection: close\r\n\r\n";

    if (const ssize_t bytesSent = send(clientSocket, httpRequest.c_str(), httpRequest.length(), 0); bytesSent < 0) {
        cerr << "Failed to send HEAD request." << endl;

        close(clientSocket);

        return 1;
    }

    vector<char> responseData;
    char buffer[4096];
    ssize_t bytesRead;

    while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        responseData.insert(responseData.end(), buffer, buffer + bytesRead);
    }

    close(clientSocket);

    string responseStr(responseData.begin(), responseData.end());
    const string contentLengthHeader = "Content-Length: ";

    if (size_t pos = responseStr.find(contentLengthHeader); pos != string::npos) {
        pos += contentLengthHeader.length();

        const size_t endPos = responseStr.find("\r\n", pos);
        const string lengthStr = responseStr.substr(pos, endPos - pos);

        return stol(lengthStr);
    }

    cerr << "Content-Length header not found in HEAD response" << endl;

    return 1;
}

int main() {
    const string host = "212.182.24.27";
    constexpr int port = 8080;
    const string path = "/image.jpg";
    const string outputFile = "downloaded_image.jpg";

    cout << "Starting to download image from " << host << ":" << port << path << endl;

    long contentLength = getContentLength(host, port, path);
    if (contentLength <= 0) {
        cerr << "Failed to determine file size. Using GET to check file size instead." << endl;

        if (vector<char> testRange = downloadRange(host, port, path, 0, 0); testRange.empty()) {
            cerr << "Failed to determine file size or download capability. Aborting." << endl;

            return 1;
        }

        contentLength = 1000000;
        cout << "Using estimated file size of " << contentLength << " bytes" << endl;
    } else {
        cout << "Total image size: " << contentLength << " bytes" << endl;
    }

    long partSize = contentLength / 3;
    vector<pair<long, long>> ranges = {
        {0, partSize - 1},
        {partSize, 2 * partSize - 1},
        {2 * partSize, contentLength - 1}
    };

    vector<vector<char>> parts;
    for (const auto&[start, end] : ranges) {
        vector<char> part = downloadRange(host, port, path, start, end);

        if (part.empty()) {
            cerr << "Failed to download part from byte " << start << " to " << end << endl;

            return 1;
        }

        parts.push_back(part);
    }

    ofstream outputFileStream(outputFile, ios::binary);
    if (!outputFileStream) {
        cerr << "Failed to create output file: " << outputFile << endl;

        return 1;
    }

    for (const auto& part : parts) {
        outputFileStream.write(part.data(), part.size());
    }
    outputFileStream.close();

    cout << "Image successfully downloaded and saved to " << outputFile << endl;

    return 0;
}
