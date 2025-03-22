#include <iostream>
#include <netdb.h>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

bool isIpV4(const string& ip) {
    istringstream iss(ip);
    vector<string> octets;
    string octet;
    bool invalid = false;

    // Split on dot
    while (getline(iss, octet, '.')) {
        if (!octet.empty()) {
            octets.push_back(octet);
        }
    }

    // Check if each octet is valid
    for (string& octet : octets) {
        for (const char& c : octet) {
            if (c < '0' || c > '9') {
                invalid = true;
            }
        }

        if (invalid || stoi(octet) > 255 || stoi(octet) < 0) {
            invalid = true;
            break;
        }
    }

    return !invalid;
}

int main() {
    // Get ip or hostname
    string ipOrHostname;
    cout << "Enter ip address or hostname: ";
    cin >> ipOrHostname;

    // Check whether ipv4 or hostname
    string ip = ipOrHostname;
    if (isIpV4(ipOrHostname)) {
        cout << "ipv4 given." << endl;
    } else {
        const hostent *hostent = gethostbyname(ipOrHostname.c_str());
        auto **addr_list = reinterpret_cast<struct in_addr **>(hostent->h_addr_list);
        ip = inet_ntoa(*addr_list[0]);

        cout << "Hostname given." << endl;
    }
    cout << ip;

    // Get port
    short port;
    cout << "Enter port number: ";
    cin >> port;

    // Check if valid
    if (port < 0 || port > 65535) {
        cerr << "Invalid port number." << endl;
        return 1;
    }

    // Create socket
    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;
        return 1;
    }

    // Set up server address
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    // Convert ip to binary format
    if (inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid ip address format." << endl;
        close(clientSocket);

        return 1;
    }

    // Connect to server
    if (connect(clientSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Connection failed." << endl;
        close(clientSocket);

        return 1;
    }

    cout << "Connected successfully!" << endl;

    return 0;
}
