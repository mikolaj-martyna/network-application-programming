#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>

using namespace std;

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 2907;

    const int clientSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid server IP address format." << endl;

        close(clientSocket);

        return 1;
    }

    string hostnameToResolve;
    cout << "Enter hostname to resolve: ";
    cin >> hostnameToResolve;

    if (sendto(clientSocket, hostnameToResolve.c_str(), hostnameToResolve.length(), 0,
               reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to send data to server." << endl;

        close(clientSocket);

        return 1;
    }
    cout << "Sent hostname to server: " << hostnameToResolve << endl;

    char buffer[1024] = {};
    sockaddr_in fromAddress{};
    socklen_t fromLength = sizeof(fromAddress);
    const ssize_t bytesRead = recvfrom(clientSocket, buffer, sizeof(buffer) - 1, 0,
                                       reinterpret_cast<sockaddr *>(&fromAddress), &fromLength);

    if (bytesRead < 0) {
        cerr << "Failed to receive data from server." << endl;

        close(clientSocket);

        return 1;
    }

    buffer[bytesRead] = '\0';
    cout << "Received IP address from server: " << buffer << endl;

    close(clientSocket);

    return 0;
}
