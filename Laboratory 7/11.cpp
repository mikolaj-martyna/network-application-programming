#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sstream>
#include <vector>
#include <fstream>
#include <regex>

using namespace std;

string base64Decode(const string &encoded_string) {
    static const string base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    vector<unsigned char> decoded;
    int val = 0;
    int valb = -8;

    for (const char c: encoded_string) {
        if (c == '=') {
            break;
        }

        if (const size_t pos = base64_chars.find(c); pos != string::npos) {
            val = (val << 6) + static_cast<int>(pos);
            valb += 6;

            if (valb >= 0) {
                decoded.push_back(static_cast<unsigned char>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
    }

    return string(decoded.begin(), decoded.end());
}

struct Attachment {
    string filename;
    string content_type;
    string encoding;
    string data;
};

vector<Attachment> parseEmail(const string &email) {
    vector<Attachment> attachments;

    regex boundary_regex("boundary=\"([^\"]+)\"");

    if (smatch boundary_match; regex_search(email, boundary_match, boundary_regex) && boundary_match.size() > 1) {
        string boundary = "--" + boundary_match[1].str();

        size_t pos = 0;
        size_t next_pos;

        while ((next_pos = email.find(boundary, pos)) != string::npos) {
            pos = next_pos + boundary.length();

            size_t part_end = email.find(boundary, pos);
            if (part_end == string::npos) {
                break;
            }

            string part = email.substr(pos, part_end - pos);

            regex content_disp_regex("Content-Disposition:\\s*attachment;\\s*filename=\"([^\"]+)\"", regex::icase);

            if (smatch filename_match; regex_search(part, filename_match, content_disp_regex) && filename_match.size() >
                                       1) {
                Attachment att;
                att.filename = filename_match[1].str();

                regex content_type_regex("Content-Type:\\s*([^;\\r\\n]+)", regex::icase);

                if (smatch content_type_match; regex_search(part, content_type_match, content_type_regex) &&
                                               content_type_match.size() > 1) {
                    att.content_type = content_type_match[1].str();
                }

                regex encoding_regex("Content-Transfer-Encoding:\\s*([^\\r\\n]+)", regex::icase);

                if (smatch encoding_match; regex_search(part, encoding_match, encoding_regex) && encoding_match.size() >
                                           1) {
                    att.encoding = encoding_match[1].str();
                }

                if (size_t data_start = part.find("\r\n\r\n"); data_start != string::npos) {
                    data_start += 4;
                    att.data = part.substr(data_start);

                    if (att.encoding == "base64") {
                        string clean_data;
                        for (char c: att.data) {
                            if (c != '\r' && c != '\n') {
                                clean_data += c;
                            }
                        }
                        att.data = clean_data;
                    }

                    attachments.push_back(att);
                }
            }
        }
    }

    return attachments;
}

int main() {
    const string hostname = "interia.pl";
    constexpr int port = 110;

    const hostent *hostent = gethostbyname(hostname.c_str());
    if (hostent == nullptr) {
        cerr << "Failed to resolve hostname." << endl;

        return 1;
    }

    auto **addr_list = reinterpret_cast<struct in_addr **>(hostent->h_addr_list);
    const string ip = inet_ntoa(*addr_list[0]);

    const int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        cerr << "Failed to create socket." << endl;

        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &serverAddress.sin_addr) <= 0) {
        cerr << "Invalid IP address format." << endl;

        close(clientSocket);

        return 1;
    }

    if (connect(clientSocket, reinterpret_cast<const sockaddr *>(&serverAddress), sizeof(serverAddress)) < 0) {
        cerr << "Failed to connect." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Connected to POP3 server." << endl;

    char buffer[16384] = {};
    if (recv(clientSocket, buffer, sizeof(buffer) - 1, 0) < 0) {
        cerr << "Failed to receive welcome message." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Server: " << buffer;

    const string username = "pas2017@interia.pl";
    const string password = "P4SInf2017";

    const string userCmd = "USER " + username + "\r\n";
    if (send(clientSocket, userCmd.c_str(), userCmd.length(), 0) < 0) {
        cerr << "Failed to send USER command." << endl;

        close(clientSocket);

        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (recv(clientSocket, buffer, sizeof(buffer) - 1, 0) < 0) {
        cerr << "Failed to receive response to USER command." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Server: " << buffer;
    if (strncmp(buffer, "+OK", 3) != 0) {
        cerr << "Username not accepted." << endl;

        close(clientSocket);

        return 1;
    }

    const string passCmd = "PASS " + password + "\r\n";
    if (send(clientSocket, passCmd.c_str(), passCmd.length(), 0) < 0) {
        cerr << "Failed to send PASS command." << endl;

        close(clientSocket);

        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (recv(clientSocket, buffer, sizeof(buffer) - 1, 0) < 0) {
        cerr << "Failed to receive response to PASS command." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Server: " << buffer;
    if (strncmp(buffer, "+OK", 3) != 0) {
        cerr << "Authentication failed." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Successfully logged in." << endl;

    const string statCmd = "STAT\r\n";
    if (send(clientSocket, statCmd.c_str(), statCmd.length(), 0) < 0) {
        cerr << "Failed to send STAT command." << endl;

        close(clientSocket);

        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (recv(clientSocket, buffer, sizeof(buffer) - 1, 0) < 0) {
        cerr << "Failed to receive response to STAT command." << endl;

        close(clientSocket);

        return 1;
    }

    cout << "Server: " << buffer;

    const char *token = strtok(buffer, " ");
    int numMessages = 0;
    if (token != nullptr && strcmp(token, "+OK") == 0) {
        token = strtok(nullptr, " ");

        if (token != nullptr) {
            numMessages = atoi(token);
        }
    }

    if (numMessages == 0) {
        cout << "No messages in the mailbox." << endl;

        const string quitCmd = "QUIT\r\n";

        send(clientSocket, quitCmd.c_str(), quitCmd.length(), 0);
        close(clientSocket);

        return 0;
    }

    cout << "\nRetrieving all messages in the mailbox..." << endl;

    for (int i = 1; i <= numMessages; ++i) {
        string retrCmd = "RETR " + to_string(i) + "\r\n";

        if (send(clientSocket, retrCmd.c_str(), retrCmd.length(), 0) < 0) {
            cerr << "Failed to send RETR command for message #" << i << endl;
            continue;
        }

        memset(buffer, 0, sizeof(buffer));
        if (recv(clientSocket, buffer, sizeof(buffer) - 1, 0) < 0) {
            cerr << "Failed to receive response to RETR command for message #" << i << endl;
            continue;
        }

        cout << "\n===================================================" << endl;
        cout << "Message #" << i << ":" << endl;
        cout << "===================================================" << endl;

        string emailContent(buffer);

        if (vector<Attachment> attachments = parseEmail(emailContent); !attachments.empty()) {
            cout << "Found " << attachments.size() << " attachment(s) in this message." << endl;

            for (const auto &[filename, content_type, encoding, data]: attachments) {
                cout << "Attachment: " << filename << " (" << content_type << ")" << endl;

                if (content_type.find("image/") != string::npos) {
                    cout << "Detected image attachment. Saving to disk..." << endl;

                    string fileData;
                    if (encoding == "base64") {
                        fileData = base64Decode(data);
                    } else {
                        fileData = data;
                    }

                    if (ofstream outFile(filename, ios::binary); outFile.is_open()) {
                        outFile.write(fileData.c_str(), fileData.size());
                        outFile.close();
                        cout << "Successfully saved image to: " << filename << endl;
                    } else {
                        cerr << "Failed to save attachment: " << filename << endl;
                    }
                } else {
                    cout << "Skipping non-image attachment." << endl;
                }
            }
        } else {
            cout << "No attachments found in this message." << endl;
        }

        cout << "Email content summary:" << endl;
        if (size_t headerEnd = emailContent.find("\r\n\r\n"); headerEnd != string::npos) {
            cout << emailContent.substr(0, headerEnd) << endl;
            cout << "[...Content truncated...]" << endl;
        } else {
            cout << emailContent.substr(0, 200) << "..." << endl;
        }
    }

    const string quitCmd = "QUIT\r\n";
    if (send(clientSocket, quitCmd.c_str(), quitCmd.length(), 0) < 0) {
        cerr << "Failed to send QUIT command." << endl;

        close(clientSocket);

        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    if (recv(clientSocket, buffer, sizeof(buffer) - 1, 0) < 0) {
        cerr << "Failed to receive response to QUIT command." << endl;
    }

    cout << "Server: " << buffer;
    cout << "Session closed." << endl;

    close(clientSocket);

    return 0;
}
