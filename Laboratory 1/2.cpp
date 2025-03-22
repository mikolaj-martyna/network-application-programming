#include <cstring>
#include <iostream>
#include <fstream>

using namespace std;

bool isImageFile(istream& file) {
    char header[8];
    file.seekg(0);
    file.read(header, sizeof(header));

    const char* imageSignatures[] = {
        "\x89\x50\x4E\x47",    // PNG
        "\xFF\xD8\xFF",        // JPG
        "\x42\x4D",            // BMP
        "\x47\x49\x46\x38",    // GIF
        "\x49\x49\x2A\x00",    // TIFF LE
        "\x4D\x4D\x00\x2A",    // TIFF BE
        "\x00\x00\x01\x00"     // ICO
    };

    const auto len = file.gcount();
    file.seekg(0);

    for (const char* sig : imageSignatures) {
        if (const size_t sigLen = strlen(sig); len >= sigLen && memcmp(header, sig, sigLen) == 0) {
            return true;
        }
    }

    return false;
}

int main() {
    // Ask user for file details
    string inputFileName;
    cout << "Please enter the input file name: ";
    cin >> inputFileName;

    string outputFileName = "lab1zad2.png";

    // Open the streams
    ifstream inputFile{inputFileName};
    ofstream outputFile{outputFileName, ios::out | ios::trunc | ios::binary};

    // Check if streams opened successfully
    if (!inputFile) {
        cerr << "Error opening input file '" << inputFileName << "'." << endl;

        if (errno == EACCES) {
            cerr << "Permission denied." << endl;
        } else if (errno == ENOENT) {
            cerr << "File not found." << endl;
        }

        return 1;
    }

    if (!outputFile) {
        cerr << "Error opening output file '" << outputFileName << "'." << endl;

        if (errno == EACCES) {
            cerr << "Permission denied or parent directory not writable." << endl;
        } else if (errno == ENOTDIR) {
            cerr << "Invalid path component." << endl;
        }
        
        return 1;
    }

    if (!isImageFile(inputFile)) {
        cerr << "The input file is not a valid image." << endl;
        inputFile.close();
        outputFile.close();

        return 1;
    }

    // Copy the contents
    try {
        static constexpr int BUFFER_SIZE = 4096;
        char buffer[BUFFER_SIZE];

        while (inputFile.read(buffer, BUFFER_SIZE)) {
            outputFile.write(buffer, BUFFER_SIZE);
        }

        outputFile.write(buffer, inputFile.gcount());
    } catch (const exception& e) {
        cerr << "Error during file copy: " << e.what() << endl;
        return 1;
    }

    // Close the streams
    inputFile.close();
    outputFile.close();

    return 0;
}
