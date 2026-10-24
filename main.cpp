#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>

#include "dfa_scanner.h"

using namespace std;

int main(int argc, char* argv[]) {
    bool jsonMode = false;
    int k = 1;
    int trials = 1;
    int maxTrace = 200;
    string sigFile = "signatures.txt";
    string inputFile = "";
    string inputText = "";
    bool hasInputText = false;
    vector<string> customSigs;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--json") {
            jsonMode = true;
        } else if (arg == "--k" && i + 1 < argc) {
            k = atoi(argv[++i]);
        } else if (arg == "--trials" && i + 1 < argc) {
            trials = atoi(argv[++i]);
        } else if (arg == "--max-trace" && i + 1 < argc) {
            maxTrace = atoi(argv[++i]);
        } else if (arg == "--sig-file" && i + 1 < argc) {
            sigFile = argv[++i];
        } else if (arg == "--no-default-sigs") {
            sigFile = "";
        } else if (arg == "--input-file" && i + 1 < argc) {
            inputFile = argv[++i];
        } else if (arg == "--input-text" && i + 1 < argc) {
            inputText = argv[++i];
            hasInputText = true;
        } else if (arg == "--custom-sig" && i + 1 < argc) {
            customSigs.push_back(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            cout << "PolyMorph Hunter - Automata-based Approximate Signature Scanner\n";
            cout << "Usage:\n";
            cout << "  scanner.exe [options]\n\n";
            cout << "Options:\n";
            cout << "  --json                     Output result strictly in JSON format\n";
            cout << "  --k <int>                  Edit distance budget (default: 1)\n";
            cout << "  --sig-file <path>          Path to signature database (default: signatures.txt)\n";
            cout << "  --custom-sig \"ID|N|P\"      Add single custom signature\n";
            cout << "  --input-file <path>        Path to input file to scan\n";
            cout << "  --input-text \"<string>\"    Inline input text to scan\n";
            cout << "  --trials <int>             Number of benchmark trials (default: 1)\n";
            cout << "  --max-trace <int>          Maximum trace steps to record (default: 200)\n";
            return 0;
        }
    }

    DFAScanner scanner;

    // Load signatures
    if (!sigFile.empty() && sigFile != "none") {
        scanner.loadSignaturesFromFile(sigFile);
    }

    // Add any custom signatures passed via CLI
    for (const string& cs : customSigs) {
        size_t firstSep = cs.find('|');
        size_t secondSep = cs.find('|', firstSep + 1);
        if (firstSep != string::npos && secondSep != string::npos) {
            string id = cs.substr(0, firstSep);
            string name = cs.substr(firstSep + 1, secondSep - firstSep - 1);
            string pattern = cs.substr(secondSep + 1);
            scanner.addSignature(id, name, pattern);
        } else if (firstSep != string::npos) {
            string id = cs.substr(0, firstSep);
            string pattern = cs.substr(firstSep + 1);
            scanner.addSignature(id, id, pattern);
        } else {
            scanner.addSignature("CUSTOM", "Custom Signature", cs);
        }
    }

    // Check if signatures were loaded
    if (scanner.getSignatures().empty()) {
        if (jsonMode) {
            cout << "{\"error\": \"No signatures loaded. Check signatures.txt or pass --custom-sig.\"}\n";
        } else {
            cerr << "ERROR: No signatures loaded. Please verify " << sigFile << " or use --custom-sig.\n";
        }
        return 1;
    }

    // Prepare input bytes
    vector<uint8_t> inputBytes;

    if (hasInputText) {
        inputBytes.assign(inputText.begin(), inputText.end());
    } else if (!inputFile.empty()) {
        ifstream file(inputFile, ios::binary);
        if (!file.is_open()) {
            if (jsonMode) {
                cout << "{\"error\": \"Could not open input file: " << inputFile << "\"}\n";
            } else {
                cerr << "ERROR: Could not open input file: " << inputFile << "\n";
            }
            return 1;
        }
        char ch;
        while (file.get(ch)) {
            inputBytes.push_back(static_cast<uint8_t>(ch));
        }
        file.close();
    } else {
        // Interactive terminal mode if no input specified
        if (jsonMode) {
            cout << "{\"error\": \"No input provided. Specify --input-file or --input-text.\"}\n";
            return 1;
        }

        cout << "==============================================================\n";
        cout << "         POLYMORPH HUNTER - AUTOMATA ENGINE (CLI MODE)        \n";
        cout << "==============================================================\n";
        scanner.displaySignatures();
        cout << "\nEnter file name to scan (or press Enter to scan 'infected.txt'): ";
        string fn;
        getline(cin, fn);
        if (fn.empty()) {
            fn = "infected.txt";
        }

        ifstream file(fn, ios::binary);
        if (!file.is_open()) {
            cerr << "ERROR: Could not open file: " << fn << "\n";
            return 1;
        }
        char ch;
        while (file.get(ch)) {
            inputBytes.push_back(static_cast<uint8_t>(ch));
        }
        file.close();
        cout << "\nScanning " << fn << " (" << inputBytes.size() << " bytes) with k = " << k << "...\n";
    }

    // Execute full 3-way comparison
    ScanComparisonReport report = scanner.runFullComparison(inputBytes, k, trials, maxTrace);

    if (jsonMode) {
        cout << scanner.reportToJSON(report) << endl;
    } else {
        scanner.printReportText(report);
    }

    return 0;
}
