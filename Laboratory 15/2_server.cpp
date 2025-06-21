#include <iostream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

using namespace std;

struct WeatherData {
    string description;
    double temperature;
    double feelsLike;
    int humidity;
    double windSpeed;
    string cityName;
    bool valid;
};

string fetchWeatherData() {
    const string host = "api.openweathermap.org";
    const string apiKey = "d4af3e33095b8c43f1a6815954face64";
    const string path = "/data/2.5/weather?q=Lublin,pl&units=metric&appid=" + apiKey;

    const hostent *server = gethostbyname(host.c_str());
    if (server == nullptr) {
        cerr << "Failed to resolve hostname: " << host << endl;

        return "";
    }

    const int apiSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (apiSocket < 0) {
        cerr << "Failed to create socket for API request." << endl;

        return "";
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(80);
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(apiSocket, reinterpret_cast<const sockaddr *>(&serverAddr), sizeof(serverAddr)) < 0) {
        cerr << "Failed to connect to API server." << endl;
        close(apiSocket);

        return "";
    }

    const string request = "GET " + path + " HTTP/1.1\r\n"
                           + "Host: " + host + "\r\n"
                           + "Connection: close\r\n\r\n";

    if (send(apiSocket, request.c_str(), request.length(), 0) != static_cast<ssize_t>(request.length())) {
        cerr << "Failed to send API request." << endl;

        close(apiSocket);

        return "";
    }

    string response;
    char buffer[4096];
    ssize_t bytesRead;

    while ((bytesRead = recv(apiSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }

    close(apiSocket);

    if (const size_t jsonStart = response.find("\r\n\r\n"); jsonStart != string::npos) {
        return response.substr(jsonStart + 4);
    }

    return "";
}

string extractJsonValue(const string &json, const string &key) {
    const size_t pos = json.find("\"" + key + "\"");
    if (pos == string::npos) {
        return "";
    }

    size_t valuePos = json.find(":", pos);
    if (valuePos == string::npos) {
        return "";
    }
    valuePos++;

    while (valuePos < json.length() && (json[valuePos] == ' ' || json[valuePos] == '\t')) {
        valuePos++;
    }

    if (json[valuePos] == '"') {
        valuePos++;
        const size_t end = json.find('"', valuePos);
        if (end == string::npos) {
            return "";
        }
        return json.substr(valuePos, end - valuePos);
    }

    if (isdigit(json[valuePos]) || json[valuePos] == '-') {
        const size_t end = json.find_first_of(",}\r\n", valuePos);
        if (end == string::npos) {
            return "";
        }
        return json.substr(valuePos, end - valuePos);
    }

    return "";
}

WeatherData parseWeatherData(const string &json) {
    WeatherData data;
    data.valid = false;

    if (json.empty() || json.find("\"cod\"") == string::npos) {
        return data;
    }

    if (const size_t descPos = json.find("\"description\""); descPos != string::npos) {
        data.description = extractJsonValue(json.substr(descPos), "description");
    }

    try {
        if (const string tempStr = extractJsonValue(json, "temp"); !tempStr.empty()) {
            data.temperature = stod(tempStr);
        }

        if (const string feelsStr = extractJsonValue(json, "feels_like"); !feelsStr.empty()) {
            data.feelsLike = stod(feelsStr);
        }

        if (const string humStr = extractJsonValue(json, "humidity"); !humStr.empty()) {
            data.humidity = stoi(humStr);
        }

        if (const string windStr = extractJsonValue(json, "speed"); !windStr.empty()) {
            data.windSpeed = stod(windStr);
        }

        if (const string nameStr = extractJsonValue(json, "name"); !nameStr.empty()) {
            data.cityName = nameStr;
        } else {
            data.cityName = "Lublin";
        }

        data.valid = true;
    } catch (const exception &e) {
        cerr << "Error parsing weather data: " << e.what() << endl;
        data.valid = false;
    }

    return data;
}

string formatWeatherResponse(const WeatherData &data) {
    if (!data.valid) {
        return "ERROR: Could not retrieve weather data\n";
    }

    string response = "WEATHER_DATA\n";
    response += "City: " + data.cityName + "\n";
    response += "Description: " + data.description + "\n";
    response += "Temperature: " + to_string(data.temperature) + "°C\n";
    response += "Feels like: " + to_string(data.feelsLike) + "°C\n";
    response += "Humidity: " + to_string(data.humidity) + "%\n";
    response += "Wind speed: " + to_string(data.windSpeed) + " m/s\n";
    response += "END\n";

    return response;
}

string processCommand(const string &command) {
    if (command == "GET_WEATHER") {
        const string jsonData = fetchWeatherData();
        const WeatherData weather = parseWeatherData(jsonData);
        return formatWeatherResponse(weather);
    }

    if (command == "HELP") {
        return
                "Available commands:\nGET_WEATHER - Get current weather in Lublin\nHELP - Show this help\nQUIT - Disconnect\n";
    }

    return "ERROR: Unknown command. Type HELP for available commands.\n";
}

int main() {
    const string serverIp = "127.0.0.1";
    constexpr int serverPort = 12346;
    constexpr int maxClients = FD_SETSIZE;

    int clientSockets[maxClients];
    for (int i = 0; i < maxClients; i++) {
        clientSockets[i] = -1;
    }

    const int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        cerr << "Failed to create socket: " << strerror(errno) << endl;

        return 1;
    }

    constexpr int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "Failed to set socket options: " << strerror(errno) << endl;
        close(serverSocket);

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIp.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address format." << endl;
        close(serverSocket);

        return 1;
    }

    if (bind(serverSocket, reinterpret_cast<sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to bind socket: " << strerror(errno) << endl;
        close(serverSocket);

        return 1;
    }

    if (listen(serverSocket, 5) < 0) {
        cerr << "Failed to listen for connections: " << strerror(errno) << endl;
        close(serverSocket);

        return 1;
    }

    cout << "Weather server is listening on " << serverIp << ":" << serverPort << endl;

    fd_set activeFds, readFds;
    FD_ZERO(&activeFds);
    FD_SET(serverSocket, &activeFds);
    int maxFd = serverSocket;

    while (true) {
        readFds = activeFds;

        if (select(maxFd + 1, &readFds, nullptr, nullptr, nullptr) < 0) {
            cerr << "Select error: " << strerror(errno) << endl;
            break;
        }

        if (FD_ISSET(serverSocket, &readFds)) {
            sockaddr_in clientAddress{};
            socklen_t addrLen = sizeof(clientAddress);

            if (const int clientSocket = accept(serverSocket, reinterpret_cast<sockaddr *>(&clientAddress), &addrLen);
                clientSocket < 0) {
                cerr << "Accept error: " << strerror(errno) << endl;
            } else {
                int i;
                for (i = 0; i < maxClients; i++) {
                    if (clientSockets[i] < 0) {
                        clientSockets[i] = clientSocket;
                        break;
                    }
                }

                if (i == maxClients) {
                    cerr << "Too many clients, connection rejected." << endl;
                    close(clientSocket);
                } else {
                    FD_SET(clientSocket, &activeFds);

                    if (clientSocket > maxFd) {
                        maxFd = clientSocket;
                    }

                    char clientIp[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &clientAddress.sin_addr, clientIp, sizeof(clientIp));
                    const int clientPort = ntohs(clientAddress.sin_port);
                    cout << "Client connected: " << clientIp << ":" << clientPort << endl;

                    const string welcomeMsg = "Welcome to Weather Server!\nType HELP for available commands.\n";
                    send(clientSocket, welcomeMsg.c_str(), welcomeMsg.length(), 0);
                }
            }
        }

        for (int i = 0; i < maxClients; i++) {
            if (const int socketFd = clientSockets[i]; socketFd > 0 && FD_ISSET(socketFd, &readFds)) {
                constexpr int bufferSize = 4096;
                char buffer[bufferSize] = {};

                if (const ssize_t bytesRead = read(socketFd, buffer, sizeof(buffer) - 1); bytesRead <= 0) {
                    if (bytesRead == 0) {
                        cout << "Client disconnected." << endl;
                    } else {
                        cerr << "Read error: " << strerror(errno) << endl;
                    }

                    close(socketFd);

                    FD_CLR(socketFd, &activeFds);
                    clientSockets[i] = -1;
                } else {
                    buffer[bytesRead] = '\0';
                    string command = buffer;

                    command.erase(command.find_last_not_of(" \n\r\t") + 1);

                    cout << "Received command: " << command << endl;

                    if (command == "QUIT") {
                        cout << "Client requested to quit." << endl;

                        close(socketFd);

                        FD_CLR(socketFd, &activeFds);
                        clientSockets[i] = -1;
                    } else {
                        const string response = processCommand(command);
                        send(socketFd, response.c_str(), response.length(), 0);
                    }
                }
            }
        }
    }

    close(serverSocket);

    for (int i = 0; i < maxClients; i++) {
        if (clientSockets[i] > 0) {
            close(clientSockets[i]);
        }
    }

    return 0;
}
