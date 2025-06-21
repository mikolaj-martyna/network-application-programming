#include <iostream>
#include <fstream>

using namespace std;

int main() {
    string inputFileName;
    cout << "Please enter the input file name: ";
    cin >> inputFileName;

    string outputFileName = "lab1zad1.txt";

    ifstream inputFile{inputFileName};
    ofstream outputFile{outputFileName, ios::out | ios::trunc | ios::binary};

    if (!inputFile) {
        cerr << "Error opening input file '" << inputFileName << "'" << endl;

        if (errno == EACCES) {
            cerr << "Permission denied." << endl;
        } else if (errno == ENOENT) {
            cerr << "File not found." << endl;
        }

        return 1;
    }

    if (!outputFile) {
        cerr << "Error opening output file '" << outputFileName << "'" << endl;

        if (errno == EACCES) {
            cerr << "Permission denied or parent directory not writable." << endl;
        } else if (errno == ENOTDIR) {
            cerr << "Invalid path component." << endl;
        }
        
        return 1;
    }

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

    inputFile.close();
    outputFile.close();

    return 0;
}