#include <iostream>
#include <netdb.h>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;

bool isIpV4(const string &ip) {
    istringstream iss(ip);
    vector<string> octets;
    string octet;
    bool invalid = false;

    while (getline(iss, octet, '.')) {
        if (!octet.empty()) {
            octets.push_back(octet);
        }
    }

    for (string &octet: octets) {
        for (const char &c: octet) {
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

string getServiceName(const int port) {
    if (const servent *service = getservbyport(htons(port), "tcp"); service != nullptr) {
        return string(service->s_name);
    }

    return "unknown";
}

int main() {
    string ipOrHostname;
    cout << "Enter ip address or hostname: ";
    cin >> ipOrHostname;

    string ip = ipOrHostname;
    if (isIpV4(ipOrHostname)) {
        cout << "ipv4 given." << endl;
    } else {
        const hostent *hostent = gethostbyname(ipOrHostname.c_str());
        if (hostent == nullptr) {
            cerr << "Failed to resolve hostname." << endl;

            return 1;
        }

        auto **addr_list = reinterpret_cast<struct in_addr **>(hostent->h_addr_list);
        ip = inet_ntoa(*addr_list[0]);

        cout << "Hostname given." << endl;
    }
    cout << "IP: " << ip << endl;

    unsigned short port;
    cout << "Enter port number: ";
    cin >> port;

    if (port < 0 || port > 65535) {
        cerr << "Invalid port number." << endl;

        return 1;
    }

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid ip address format." << endl;
        close(clientSocket);

        return 1;
    }

    if (connect(clientSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Connection failed. Port " << port << " is closed." << endl;
        close(clientSocket);

        return 1;
    }

    cout << "Connected successfully! Port " << port << " is open." << endl;

    const string serviceName = getServiceName(port);
    cout << "Service running on port " << port << ": " << serviceName << endl;

    close(clientSocket);

    return 0;
}
