#include <iostream>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <regex>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509.h>

using namespace std;

const string IRC_SERVER = "chat.freenode.net";
constexpr int IRC_PORT = 7000;
const string NICKNAME = "WeatherBot";
const string USERNAME = "weatherbot";
const string REALNAME = "Weather Information Bot";
const string CHANNEL = "#weatherchannel";

const string API_KEY = "d4af3e33095b8c43f1a6815954face64";
const string WEATHER_API_HOST = "api.openweathermap.org";
const string WEATHER_API_PATH = "/data/2.5/weather";

float kelvinToCelsius(const float kelvin) {
    return kelvin - 273.15f;
}

int verifyCertificate(int preverify_ok, X509_STORE_CTX *ctx) {
    if (!preverify_ok) {
        X509 *cert = X509_STORE_CTX_get_current_cert(ctx);
        int depth = X509_STORE_CTX_get_error_depth(ctx);
        int err = X509_STORE_CTX_get_error(ctx);

        char subjectName[256];
        X509_NAME_oneline(X509_get_subject_name(cert), subjectName, sizeof(subjectName));

        cerr << "Certificate verification error at depth: " << depth << endl;
        cerr << "  Subject: " << subjectName << endl;
        cerr << "  Error: " << X509_verify_cert_error_string(err) << endl;

        return 0;
    }

    if (X509_STORE_CTX_get_error_depth(ctx) == 0) {
        X509 *cert = X509_STORE_CTX_get_current_cert(ctx);

        char issuerName[256];
        X509_NAME_oneline(X509_get_issuer_name(cert), issuerName, sizeof(issuerName));

        string issuer(issuerName);
        if (issuer.find("Let's Encrypt Authority X3") == string::npos) {
            cerr << "Certificate not issued by Let's Encrypt Authority X3!" << endl;
            cerr << "Issuer: " << issuerName << endl;

            return 0;
        }

        cout << "Certificate issued by: " << issuerName << endl;
    }

    return 1;
}

void parseIRCMessage(const string &message, string &prefix, string &command, vector<string> &params) {
    prefix = "";
    command = "";
    params.clear();

    size_t pos = 0;
    size_t nextPos = 0;

    if (message[0] == ':') {
        nextPos = message.find(' ', 1);

        if (nextPos != string::npos) {
            prefix = message.substr(1, nextPos - 1);
            pos = nextPos + 1;
        }
    }

    nextPos = message.find(' ', pos);
    if (nextPos != string::npos) {
        command = message.substr(pos, nextPos - pos);
        pos = nextPos + 1;
    } else {
        command = message.substr(pos);

        return;
    }

    while (pos < message.length()) {
        if (message[pos] == ':') {
            params.push_back(message.substr(pos + 1));
            break;
        }

        nextPos = message.find(' ', pos);
        if (nextPos != string::npos) {
            params.push_back(message.substr(pos, nextPos - pos));
            pos = nextPos + 1;
        } else {
            params.push_back(message.substr(pos));
            break;
        }
    }
}

SSL *createSSLConnection(const string &hostname, const int port) {
    SSL_load_error_strings();
    OpenSSL_add_ssl_algorithms();

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        cerr << "Failed to create socket" << endl;

        EVP_cleanup();

        return nullptr;
    }

    const hostent *server = gethostbyname(hostname.c_str());
    if (server == nullptr) {
        cerr << "Failed to resolve hostname: " << hostname << endl;

        close(sockfd);
        EVP_cleanup();

        return nullptr;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sockfd, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) < 0) {
        cerr << "Failed to connect to " << hostname << ":" << port << endl;

        close(sockfd);
        EVP_cleanup();

        return nullptr;
    }

    cout << "Connected to " << hostname << ":" << port << endl;

    SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx) {
        cerr << "Failed to create SSL context" << endl;

        ERR_print_errors_fp(stderr);
        close(sockfd);
        EVP_cleanup();

        return nullptr;
    }

    if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
        cerr << "Failed to load trusted CA certificates" << endl;

        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        close(sockfd);
        EVP_cleanup();

        return nullptr;
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, verifyCertificate);

    SSL *ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);
    SSL_set_tlsext_host_name(ssl, hostname.c_str());

    if (SSL_connect(ssl) != 1) {
        cerr << "SSL handshake failed" << endl;

        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sockfd);
        EVP_cleanup();

        return nullptr;
    }

    cout << "SSL connection established with " << SSL_get_cipher(ssl) << " cipher" << endl;

    X509 *cert = SSL_get_peer_certificate(ssl);
    if (!cert) {
        cerr << "No certificate presented by the server" << endl;

        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sockfd);
        EVP_cleanup();

        return nullptr;
    }

    char subject[256];
    X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof(subject));
    cout << "Server certificate subject: " << subject << endl;

    char issuer[256];
    X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof(issuer));
    cout << "Server certificate issuer: " << issuer << endl;

    if (const long verify_result = SSL_get_verify_result(ssl); verify_result != X509_V_OK) {
        cerr << "Certificate verification failed: " << X509_verify_cert_error_string(verify_result) << endl;

        X509_free(cert);
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        close(sockfd);
        EVP_cleanup();

        return nullptr;
    }

    cout << "Server certificate verified successfully" << endl;
    X509_free(cert);

    return ssl;
}

bool sendIRCMessage(SSL *ssl, const string &message) {
    cout << "Sending: " << message;

    string fullMessage = message;
    if (fullMessage.find("\r\n") != fullMessage.length() - 2) {
        fullMessage += "\r\n";
    }

    if (const int ret = SSL_write(ssl, fullMessage.c_str(), fullMessage.length()); ret <= 0) {
        cerr << "Failed to send message" << endl;

        ERR_print_errors_fp(stderr);

        return false;
    }

    return true;
}

string createWeatherRequest(const string &city) {
    string request = "GET " + WEATHER_API_PATH + "?q=" + city + "&appid=" + API_KEY + " HTTP/1.1\r\n";
    request += "Host: " + WEATHER_API_HOST + "\r\n";
    request += "User-Agent: WeatherBot/1.0\r\n";
    request += "Accept: application/json\r\n";
    request += "Connection: close\r\n";
    request += "\r\n";

    return request;
}

string parseWeatherResponse(const string &response) {
    const size_t bodyPos = response.find("\r\n\r\n");
    if (bodyPos == string::npos) {
        return "Error: Invalid response format";
    }

    string body = response.substr(bodyPos + 4);

    if (body.find("\"cod\":\"404\"") != string::npos) {
        return "Error: City not found";
    }

    string temp;
    if (const size_t tempPos = body.find("\"temp\":"); tempPos != string::npos) {
        const size_t startPos = tempPos + 7;

        if (const size_t endPos = body.find(',', startPos); endPos != string::npos) {
            temp = body.substr(startPos, endPos - startPos);
        }
    }

    string description;
    if (const size_t descPos = body.find("\"description\":\""); descPos != string::npos) {
        const size_t startPos = descPos + 15;

        if (const size_t endPos = body.find('"', startPos); endPos != string::npos) {
            description = body.substr(startPos, endPos - startPos);
        }
    }

    string cityName;
    if (const size_t namePos = body.find("\"name\":\""); namePos != string::npos) {
        const size_t startPos = namePos + 8;

        if (const size_t endPos = body.find('"', startPos); endPos != string::npos) {
            cityName = body.substr(startPos, endPos - startPos);
        }
    }

    string weatherInfo;
    if (!temp.empty() && !description.empty() && !cityName.empty()) {
        const float tempC = kelvinToCelsius(stof(temp));
        weatherInfo = "Weather in " + cityName + ": " + description + ", Temperature: " +
                      to_string(static_cast<int>(tempC)) + "°C";
    } else {
        weatherInfo = "Failed to parse weather information";
    }

    return weatherInfo;
}

string getWeatherInfo(const string &city) {
    SSL *apiSsl = createSSLConnection(WEATHER_API_HOST, 443);
    if (!apiSsl) {
        return "Error: Could not connect to weather service";
    }

    if (const string request = createWeatherRequest(city); !sendIRCMessage(apiSsl, request)) {
        SSL_free(apiSsl);

        return "Error: Failed to send weather request";
    }

    string response;
    char buffer[4096];
    int bytesRead;

    while ((bytesRead = SSL_read(apiSsl, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }

    SSL_free(apiSsl);

    return parseWeatherResponse(response);
}

void processPrivateMessage(SSL *ssl, const string &sender, const string &target, const string &message) {
    const regex weatherPattern("pogoda\\s+([a-zA-Z]+)");

    if (smatch matches; regex_search(message, matches, weatherPattern) && matches.size() > 1) {
        const string city = matches[1].str();
        cout << "Weather request for city: " << city << endl;

        const string weatherInfo = getWeatherInfo(city);

        const string recipient = target[0] == '#' ? target : sender;
        const string response = "PRIVMSG " + recipient + " :" + weatherInfo;
        sendIRCMessage(ssl, response);
    }
}

int main() {
    SSL *ssl = createSSLConnection(IRC_SERVER, IRC_PORT);
    if (!ssl) {
        cerr << "Failed to connect to IRC server" << endl;

        return 1;
    }

    sendIRCMessage(ssl, "NICK " + NICKNAME);
    sendIRCMessage(ssl, "USER " + USERNAME + " 0 * :" + REALNAME);

    char buffer[4096];
    bool registered = false;

    pollfd fds[1];
    fds[0].events = POLLIN;
    fds[0].fd = SSL_get_fd(ssl);

    while (true) {
        const int pollResult = poll(fds, 1, 1000);

        if (pollResult < 0) {
            cerr << "Poll failed" << endl;
            break;
        }

        if (pollResult == 0) {
            continue;
        }

        const int bytesRead = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        if (bytesRead <= 0) {
            if (const int sslError = SSL_get_error(ssl, bytesRead); sslError == SSL_ERROR_ZERO_RETURN) {
                cout << "Connection closed by server" << endl;
            } else {
                cerr << "SSL read error: " << sslError << endl;
                ERR_print_errors_fp(stderr);
            }
            break;
        }

        buffer[bytesRead] = '\0';
        string response(buffer);

        size_t pos = 0;
        size_t nextPos;

        while ((nextPos = response.find("\r\n", pos)) != string::npos) {
            string line = response.substr(pos, nextPos - pos);
            cout << "Received: " << line << endl;

            string prefix, command;
            vector<string> params;
            parseIRCMessage(line, prefix, command, params);

            if (command == "PING") {
                sendIRCMessage(ssl, "PONG :" + params.back());
            } else if (command == "001" && !registered) {
                registered = true;
                sendIRCMessage(ssl, "JOIN " + CHANNEL);
            } else if (command == "PRIVMSG" && params.size() >= 2) {
                string sender = prefix.substr(0, prefix.find('!'));
                string target = params[0];
                string message = params[1];

                processPrivateMessage(ssl, sender, target, message);
            }

            pos = nextPos + 2;
        }
    }

    SSL_free(ssl);

    EVP_cleanup();

    return 0;
}
