#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <iostream>

using namespace std;

int main() {
    sockaddr_in socketAddress{};

    string ip;
    cout << "Enter ip address: ";
    cin >> ip;

    socketAddress.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &socketAddress.sin_addr);

    if (char hostname[253]; getnameinfo(reinterpret_cast<sockaddr *>(&socketAddress), sizeof(socketAddress), hostname,
                                         sizeof(hostname), nullptr, 0, 0) == 0) {
        cout << "Hostname: " << hostname << endl;
    } else {
        cout << "Could not resolve hostname." << endl;
    }

    return 0;
}
