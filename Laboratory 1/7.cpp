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

    while (getline(iss, octet, '.')) {
        if (!octet.empty()) {
            octets.push_back(octet);
        }
    }

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
    string ipOrHostname;
    cout << "Enter ip address or hostname: ";
    cin >> ipOrHostname;

    string ip = ipOrHostname;
    if (isIpV4(ipOrHostname)) {
        cout << "ipv4 given." << endl;
    } else {
        const hostent *hostent = gethostbyname(ipOrHostname.c_str());
        auto **addr_list = reinterpret_cast<struct in_addr **>(hostent->h_addr_list);
        ip = inet_ntoa(*addr_list[0]);

        cout << "Hostname given." << endl;
    }
    cout << ip << endl;

    cout << "SCANNED PORTS" << endl;
    int openPorts = 0, closedPorts = 0;
    for (int port = 1; port <= 65535; port++) {
        const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket < 0) {
            cerr << "Failed to create socket." << endl;
            continue;
        }

        sockaddr_in serverAddress;
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(port);

        if (inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr) <= 0) {
            cerr << "Invalid ip address format." << endl;
            close(clientSocket);

            continue;
        }

        if (connect(clientSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
            close(clientSocket);
            closedPorts++;
        } else {
            cout << "Port " << port << " is open." << endl;
            openPorts++;
        }
    }

    cout << openPorts << " ports open, " << closedPorts << " ports closed." << endl;

    return 0;
}
