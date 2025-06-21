#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <map>
#include <cstring>
#include <filesystem>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>

using namespace std;
namespace fs = std::filesystem;

const string SERVER_ROOT = "/home/enix/Studies/Programowanie Aplikacji Sieciowych/Laboratory 9";
const string SERVER_NAME = "PAS/2017 HTTP Server";
constexpr int PORT = 8080;

struct HttpRequest {
    string method;
    string path;
    string version;
    map<string, string> headers;
};

struct HttpResponse {
    int statusCode;
    string statusMessage;
    map<string, string> headers;
    string body;
};

string readFileContent(const string& filePath) {
    ifstream file(filePath, ios::binary);
    if (!file.is_open()) {
        return "";
    }

    ostringstream content;
    content << file.rdbuf();

    return content.str();
}

string getMimeType(const string& path) {
    const size_t dotPos = path.find_last_of('.');
    if (dotPos == string::npos) {
        return "application/octet-stream";
    }

    const string ext = path.substr(dotPos + 1);
    if (ext == "html" || ext == "htm") return "text/html";
    if (ext == "css") return "text/css";
    if (ext == "js") return "application/javascript";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "png") return "image/png";
    if (ext == "gif") return "image/gif";
    if (ext == "txt") return "text/plain";

    return "application/octet-stream";
}

HttpRequest parseRequest(const string& requestStr) {
    HttpRequest request;
    istringstream iss(requestStr);
    string line;

    getline(iss, line);
    istringstream requestLineStream(line);
    requestLineStream >> request.method >> request.path >> request.version;

    while (getline(iss, line) && line != "\r") {
        if (size_t colonPos = line.find(':'); colonPos != string::npos) {
            string key = line.substr(0, colonPos);
            string value = line.substr(colonPos + 1);

            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of("\r\n") + 1);

            request.headers[key] = value;
        }
    }

    return request;
}

HttpResponse handleRequest(const HttpRequest& request) {
    HttpResponse response;
    response.headers["Server"] = SERVER_NAME;
    response.headers["Connection"] = "close";

    string filePath = request.path;
    if (filePath == "/") {
        filePath = "/index.html";
    }

    const string absolutePath = SERVER_ROOT + filePath;

    if (absolutePath.find("..") != string::npos) {
        response.statusCode = 400;
        response.statusMessage = "Bad Request";
        response.headers["Content-Type"] = "text/html";
        response.body = readFileContent(SERVER_ROOT + "/400.html");

        return response;
    }

    if (!fs::exists(absolutePath) || fs::is_directory(absolutePath)) {
        response.statusCode = 404;
        response.statusMessage = "Not Found";
        response.headers["Content-Type"] = "text/html";
        response.body = readFileContent(SERVER_ROOT + "/404.html");

        return response;
    }

    response.body = readFileContent(absolutePath);
    if (response.body.empty()) {
        response.statusCode = 500;
        response.statusMessage = "Internal Server Error";
        response.headers["Content-Type"] = "text/plain";
        response.body = "Failed to read file";

        return response;
    }

    response.statusCode = 200;
    response.statusMessage = "OK";
    response.headers["Content-Type"] = getMimeType(absolutePath);
    response.headers["Content-Length"] = to_string(response.body.size());

    return response;
}

string buildResponseString(const HttpResponse& response) {
    ostringstream oss;
    oss << "HTTP/1.1 " << response.statusCode << " " << response.statusMessage << "\r\n";

    for (const auto& [key, value] : response.headers) {
        oss << key << ": " << value << "\r\n";
    }

    oss << "\r\n" << response.body;

    return oss.str();
}

void handleClient(const int clientSocket) {
    char buffer[4096] = {0};
    const ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (bytesRead <= 0) {
        close(clientSocket);

        return;
    }

    buffer[bytesRead] = '\0';
    const string requestStr(buffer);

    const HttpRequest request = parseRequest(requestStr);
    const HttpResponse response = handleRequest(request);

    const string responseStr = buildResponseString(response);
    send(clientSocket, responseStr.c_str(), responseStr.size(), 0);

    close(clientSocket);
    cout << request.method << " " << request.path << " - " << response.statusCode << endl;
}

int main() {
    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    constexpr int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Failed to set socket options." << endl;

        return 1;
    }

    sockaddr_in serverAddress = {};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons(PORT);

    if (bind(serverSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to bind socket." << endl;

        return 1;
    }

    if (listen(serverSocket, 10) < 0) {
        cerr << "Failed to listen on socket." << endl;

        return 1;
    }

    cout << "HTTP Server running at http://127.0.0.1:" << PORT << endl;
    cout << "Serving files from: " << SERVER_ROOT << endl;

    while (true) {
        sockaddr_in clientAddress = {};
        socklen_t clientAddressLen = sizeof(clientAddress);

        int clientSocket = accept(serverSocket, reinterpret_cast<struct sockaddr *>(&clientAddress), &clientAddressLen);
        if (clientSocket < 0) {
            cerr << "Failed to accept connection." << endl;

            continue;
        }

        char clientIp[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddress.sin_addr), clientIp, INET_ADDRSTRLEN);
        cout << "Connection accepted from " << clientIp << endl;

        thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }

    close(serverSocket);
    return 0;
}
