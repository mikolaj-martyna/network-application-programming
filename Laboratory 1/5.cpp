#include <iostream>
#include <netdb.h>
#include <arpa/inet.h>

using namespace std;

int main() {
    string hostname;
    cout << "Enter hostname: ";
    cin >> hostname;

    const hostent *hostent = gethostbyname(hostname.c_str());
    auto **addr_list = reinterpret_cast<struct in_addr **>(hostent->h_addr_list);

    cout << "Main ip address of the host: " << inet_ntoa(*addr_list[0]) << endl;

    return 0;
}
