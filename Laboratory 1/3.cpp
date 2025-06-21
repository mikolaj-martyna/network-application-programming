#include <iostream>
#include <sstream>
#include <vector>

using namespace std;

int main() {
    string ip;
    cout << "Please enter the ip address to check: ";
    cin >> ip;

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

    if (invalid) {
        cout << "The ip address is invalid." << endl;
    } else {
        cout << "The ip address is valid." << endl;
    }

    return 0;
}
