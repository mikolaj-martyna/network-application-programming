#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <string>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>

using namespace std;

struct CacheMetadata {
    string etag;
    string lastModified;
};

struct HttpResponse {
    string headers;
    vector<char> body;
    int statusCode;
};

HttpResponse downloadRange(const string &host, const int port, const string &path, const long startByte,
                           const long endByte, const CacheMetadata &metadata = {}) {
    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return HttpResponse{};
    }

    sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = inet_addr(host.c_str());

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect to " << host << ":" << port << endl;

        close(clientSocket);

        return HttpResponse{};
    }

    cout << "Connected to " << host << ":" << port << " for bytes " << startByte << "-" << endByte << endl;

    string httpRequest = "GET " + path + " HTTP/1.1\r\n"
                         "Host: " + host + "\r\n"
                         "User-Agent: ImageDownloader/1.0\r\n"
                         "Accept: image/jpeg,image/*\r\n";

    if (startByte > 0 || endByte > 0) {
        httpRequest += "Range: bytes=" + to_string(startByte) + "-" + to_string(endByte) + "\r\n";
    }

    if (startByte == 0 && endByte == 0) {
        if (!metadata.etag.empty()) {
            httpRequest += "If-None-Match: " + metadata.etag + "\r\n";
        }
        if (!metadata.lastModified.empty()) {
            httpRequest += "If-Modified-Since: " + metadata.lastModified + "\r\n";
        }
    }

    httpRequest += "Connection: close\r\n\r\n";

    if (const ssize_t bytesSent = send(clientSocket, httpRequest.c_str(), httpRequest.length(), 0); bytesSent < 0) {
        cerr << "Failed to send HTTP request for range " << startByte << "-" << endByte << endl;

        close(clientSocket);

        return HttpResponse{};
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

        return HttpResponse{};
    }

    close(clientSocket);

    HttpResponse response;
    string responseStr(responseData.begin(), responseData.end());
    const size_t headerEnd = responseStr.find("\r\n\r\n");

    if (headerEnd == string::npos) {
        cerr << "Invalid HTTP response (no header separator found) for range " << startByte << "-" << endByte << endl;

        return HttpResponse{};
    }

    response.headers = responseStr.substr(0, headerEnd);

    const size_t statusLineEnd = response.headers.find("\r\n");
    const string statusLine = response.headers.substr(0, statusLineEnd);
    cout << "Received response for range " << startByte << "-" << endByte << ":" << endl;
    cout << statusLine << endl;

    if (const size_t statusCodePos = statusLine.find(" "); statusCodePos != string::npos) {
        const string statusCodeStr = statusLine.substr(statusCodePos + 1, 3);
        response.statusCode = stoi(statusCodeStr);
    } else {
        response.statusCode = 0;
    }

    if (response.statusCode == 304) {
        cout << "Resource not modified (304 response)" << endl;

        return response;
    }

    if (startByte > 0 || endByte > 0) {
        if (response.statusCode != 206) {
            cerr << "WARNING: Expected 206 Partial Content, but got: " << statusLine << endl;
        }
    } else if (response.statusCode != 200) {
        cerr << "WARNING: Expected 200 OK, but got: " << statusLine << endl;
    }

    response.body = vector<char>(responseData.begin() + headerEnd + 4, responseData.end());
    cout << "Received " << response.body.size() << " bytes of data" << endl;

    return response;
}

long getContentLength(const string &host, const int port, const string &path) {
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

CacheMetadata readCacheMetadata(const string &metadataFile) {
    CacheMetadata metadata;

    if (ifstream file(metadataFile); file.is_open()) {
        string line;

        while (getline(file, line)) {
            if (line.empty()) continue;

            if (size_t separator = line.find(": "); separator != string::npos) {
                string key = line.substr(0, separator);
                string value = line.substr(separator + 2);

                if (key == "ETag") {
                    metadata.etag = value;
                } else if (key == "Last-Modified") {
                    metadata.lastModified = value;
                }
            }
        }

        file.close();
    }

    return metadata;
}

void saveCacheMetadata(const string &metadataFile, const CacheMetadata &metadata) {
    if (ofstream file(metadataFile); file.is_open()) {
        if (!metadata.etag.empty()) {
            file << "ETag: " << metadata.etag << endl;
        }

        if (!metadata.lastModified.empty()) {
            file << "Last-Modified: " << metadata.lastModified << endl;
        }

        file.close();
    } else {
        cerr << "Failed to save cache metadata" << endl;
    }
}

string extractHeaderValue(const string &responseStr, const string &headerName) {
    const string header = headerName + ": ";

    if (size_t pos = responseStr.find(header); pos != string::npos) {
        pos += header.length();

        if (const size_t endPos = responseStr.find("\r\n", pos); endPos != string::npos) {
            return responseStr.substr(pos, endPos - pos);
        }
    }

    return "";
}

bool checkIfModified(const string &host, const int port, const string &path, const CacheMetadata &metadata) {
    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket for conditional request." << endl;

        return true;
    }

    sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = inet_addr(host.c_str());

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect to " << host << ":" << port << " for conditional request" << endl;

        close(clientSocket);

        return true;
    }

    string httpRequest = "HEAD " + path + " HTTP/1.1\r\n"
                         "Host: " + host + "\r\n";

    if (!metadata.etag.empty()) {
        httpRequest += "If-None-Match: " + metadata.etag + "\r\n";
    }

    if (!metadata.lastModified.empty()) {
        httpRequest += "If-Modified-Since: " + metadata.lastModified + "\r\n";
    }

    httpRequest += "Connection: close\r\n\r\n";

    if (const ssize_t bytesSent = send(clientSocket, httpRequest.c_str(), httpRequest.length(), 0); bytesSent < 0) {
        cerr << "Failed to send conditional request." << endl;

        close(clientSocket);

        return true;
    }

    vector<char> responseData;
    char buffer[4096];
    ssize_t bytesRead;

    while ((bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0)) > 0) {
        responseData.insert(responseData.end(), buffer, buffer + bytesRead);
    }

    close(clientSocket);

    if (responseData.empty()) {
        cerr << "No response received from server" << endl;
        return true;
    }

    if (const string responseStr(responseData.begin(), responseData.end());
        responseStr.find("HTTP/1.1 304") != string::npos ||
        responseStr.find("HTTP/1.0 304") != string::npos) {
        cout << "Resource not modified since last download" << endl;
        return false;
    }

    cout << "Resource has been modified since last download" << endl;
    return true;
}

int main() {
    const string host = "212.182.24.27";
    constexpr int port = 8080;
    const string path = "/image.jpg";
    const string outputFile = "downloaded_image.jpg";
    const string metadataFile = "image_cache_metadata.txt";

    cout << "Starting to check image from " << host << ":" << port << path << endl;

    CacheMetadata metadata = readCacheMetadata(metadataFile);
    cout << "Checking if image has been modified since last download..." << endl;

    if (bool fileExists = filesystem::exists(outputFile);
        fileExists && (!metadata.etag.empty() || !metadata.lastModified.empty())) {
        cout << "Local file exists. Checking if server version has changed..." << endl;

        auto [headers, body, statusCode] = downloadRange(host, port, path, 0, 0, metadata);

        if (statusCode == 304) {
            cout << "Server confirmed image hasn't changed (304 Not Modified)." << endl;
            cout << "Using existing cached image: " << outputFile << endl;

            return 0;
        }

        cout << "Server indicates the image has changed. Downloading new version..." << endl;

        if (!body.empty()) {
            if (ofstream outputFileStream(outputFile, ios::binary); outputFileStream) {
                outputFileStream.write(body.data(), body.size());
                outputFileStream.close();

                cout << "Image successfully downloaded and saved to " << outputFile << endl;

                metadata.etag = extractHeaderValue(headers, "ETag");
                metadata.lastModified = extractHeaderValue(headers, "Last-Modified");

                saveCacheMetadata(metadataFile, metadata);

                return 0;
            }

            cerr << "Failed to save the downloaded image" << endl;
        }
    } else {
        cout << "No cache metadata found or local file doesn't exist. Downloading image..." << endl;
    }

    long contentLength = getContentLength(host, port, path);
    if (contentLength <= 0) {
        cerr << "Failed to determine file size. Using full GET request instead." << endl;

        auto [headers, body, statusCode] = downloadRange(host, port, path, 0, 0);
        if (statusCode == 200 && !body.empty()) {
            ofstream outputFileStream(outputFile, ios::binary);
            if (!outputFileStream) {
                cerr << "Failed to create output file: " << outputFile << endl;
                return 1;
            }

            outputFileStream.write(body.data(), body.size());
            outputFileStream.close();

            cout << "Image successfully downloaded and saved to " << outputFile << endl;

            metadata.etag = extractHeaderValue(headers, "ETag");
            metadata.lastModified = extractHeaderValue(headers, "Last-Modified");
            saveCacheMetadata(metadataFile, metadata);

            return 0;
        }

        cerr << "Failed to download the image. Aborting." << endl;
        return 1;
    }

    cout << "Total image size: " << contentLength << " bytes" << endl;
    cout << "Downloading in multiple parts..." << endl;

    long partSize = contentLength / 3;
    vector<pair<long, long> > ranges = {
        {0, partSize - 1},
        {partSize, 2 * partSize - 1},
        {2 * partSize, contentLength - 1}
    };

    vector<HttpResponse> responses;

    for (const auto &[start, end]: ranges) {
        HttpResponse response = downloadRange(host, port, path, start, end);

        if (response.body.empty()) {
            cerr << "Failed to download part from byte " << start << " to " << end << endl;

            return 1;
        }

        responses.push_back(std::move(response));
    }

    ofstream outputFileStream(outputFile, ios::binary);
    if (!outputFileStream) {
        cerr << "Failed to create output file: " << outputFile << endl;

        return 1;
    }

    for (const auto &response: responses) {
        outputFileStream.write(response.body.data(), response.body.size());
    }
    outputFileStream.close();

    cout << "Image successfully downloaded and saved to " << outputFile << endl;

    metadata.etag = extractHeaderValue(responses.back().headers, "ETag");
    metadata.lastModified = extractHeaderValue(responses.back().headers, "Last-Modified");

    saveCacheMetadata(metadataFile, metadata);

    return 0;
}
