#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <iomanip>
#include <limits>

using namespace std;

string urlEncode(const string &value) {
    ostringstream escaped;
    escaped.fill('0');
    escaped << hex;

    for (const char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else if (c == ' ') {
            escaped << '+';
        } else {
            escaped << '%' << setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        }
    }

    return escaped.str();
}

int main() {
    const string host = "httpbin.org";
    constexpr int port = 80;
    const string path = "/post";
    const string outputFile = "httpbin_response.json";

    multimap<string, string> formData;
    string input;
    char choice;

    cout << "=== Pizza Order Form ===\n" << endl;

    cout << "Customer name: ";
    getline(cin, input);
    formData.insert({"custname", urlEncode(input)});

    cout << "Telephone: ";
    getline(cin, input);
    formData.insert({"custtel", urlEncode(input)});

    cout << "E-mail address: ";
    getline(cin, input);
    formData.insert({"custemail", urlEncode(input)});

    cout << "\n=== Pizza Size ===\n";
    cout << "Select size (s: Small, m: Medium, l: Large): ";
    cin >> choice;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');

    switch(tolower(choice)) {
        case 's':
            formData.insert({"size", "small"});
            break;
        case 'm':
            formData.insert({"size", "medium"});
            break;
        case 'l':
            formData.insert({"size", "large"});
            break;
        default:
            formData.insert({"size", "medium"});
            cout << "Invalid choice. Setting to medium." << endl;
    }

    cout << "\n=== Pizza Toppings ===\n";
    cout << "Would you like bacon? (y/n): ";
    cin >> choice;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    if (tolower(choice) == 'y') {
        formData.insert({"topping", "bacon"});
    }

    cout << "Would you like extra cheese? (y/n): ";
    cin >> choice;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    if (tolower(choice) == 'y') {
        formData.insert({"topping", "cheese"});
    }

    cout << "Would you like onion? (y/n): ";
    cin >> choice;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    if (tolower(choice) == 'y') {
        formData.insert({"topping", "onion"});
    }

    cout << "Would you like mushroom? (y/n): ";
    cin >> choice;
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    if (tolower(choice) == 'y') {
        formData.insert({"topping", "mushroom"});
    }

    cout << "\nPreferred delivery time (HH:MM): ";
    getline(cin, input);
    formData.insert({"delivery", urlEncode(input)});

    cout << "Delivery instructions: ";
    getline(cin, input);
    formData.insert({"comments", urlEncode(input)});

    cout << "\nForm completed. Sending to server..." << endl;

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

    string postData;
    bool isFirst = true;

    for (const auto&[key, val] : formData) {
        if (!isFirst) {
            postData += "&";
        }

        postData += key + "=" + val;
        isFirst = false;
    }

    string httpRequest = "POST " + path + " HTTP/1.1\r\n"
                        "Host: " + host + "\r\n"
                        "User-Agent: Mozilla/5.0\r\n"
                        "Accept: application/json\r\n"
                        "Content-Type: application/x-www-form-urlencoded\r\n"
                        "Content-Length: " + to_string(postData.length()) + "\r\n"
                        "Connection: close\r\n\r\n" +
                        postData;

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

    ofstream jsonFile(outputFile);
    if (!jsonFile) {
        cerr << "Failed to create output file: " << outputFile << endl;

        return 1;
    }

    jsonFile << body;
    jsonFile.close();

    cout << "Response content saved to " << outputFile << endl;

    cout << "\n--- Response from server ---\n" << body << endl;

    return 0;
}
