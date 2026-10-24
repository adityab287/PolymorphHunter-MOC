#ifndef DFA_SCANNER_H
#define DFA_SCANNER_H

#include <string>
#include <vector>
#include <array>
#include <set>
#include <map>
#include <chrono>
#include <cstdint>
#include <sstream>

struct Signature {
    std::string id;
    std::string name;
    std::string pattern;
};

// Levenshtein epsilon-NFA State tuple (sigIndex, pos, errors)
struct NFAState {
    int sigIndex;
    int pos;
    int errors;

    bool operator<(const NFAState& other) const {
        if (sigIndex != other.sigIndex) return sigIndex < other.sigIndex;
        if (pos != other.pos) return pos < other.pos;
        return errors < other.errors;
    }

    bool operator==(const NFAState& other) const {
        return sigIndex == other.sigIndex && pos == other.pos && errors == other.errors;
    }

    bool operator!=(const NFAState& other) const {
        return !(*this == other);
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << "(" << sigIndex << "," << pos << "," << errors << ")";
        return oss.str();
    }
};

struct MatchRecord {
    std::string id;
    std::string name;
    std::string pattern;
    int minErrors;
    size_t byteOffset;
};

struct StepTrace {
    int stepIndex;
    std::string byteChar;
    std::string byteHex;
    int byteAscii;
    int fromDFAState;
    int toDFAState;
    std::string cacheStatus; // "HIT" or "COMPUTED"
    std::vector<std::string> rawNFASubset;
    std::vector<std::string> epsilonClosureAdded;
    std::vector<std::string> fullNFASubset;
    std::vector<MatchRecord> acceptedSignatures;
};

struct ExactMatchResult {
    std::string verdict;
    int matchedCount;
    std::vector<MatchRecord> matchedSignatures;
    int statesGenerated;
    int transitionsComputed;
    double executionTimeUs;
};

struct DirectNFAResult {
    std::string verdict;
    int matchedCount;
    std::vector<MatchRecord> matchedSignatures;
    long long stateEvaluations;
    double executionTimeUs;
};

struct DFAStateDump {
    int dfaStateId;
    bool isAccepting;
    std::vector<MatchRecord> acceptedSignatures;
    std::vector<std::string> nfaSubset;
};

struct ActiveTransitionDump {
    int fromState;
    std::string byteChar;
    int byteAscii;
    int toState;
};

struct LazyDFAResult {
    std::string verdict;
    int matchedCount;
    std::vector<MatchRecord> matchedSignatures;
    int dfaStatesGenerated;
    int transitionsComputed; // Cache misses
    int cacheHits;
    double cacheHitRate;
    double executionTimeUs;
    std::vector<StepTrace> trace;
    std::vector<DFAStateDump> stateRegistryDump;
    std::vector<ActiveTransitionDump> activeTransitionsDump;
};

struct ScanComparisonReport {
    std::string verdict;
    std::vector<MatchRecord> matchedSignatures;
    int k;
    size_t bytesScanned;
    int trials;
    ExactMatchResult exact;
    DirectNFAResult directNFA;
    LazyDFAResult lazyDFA;
};

class DFAScanner {
private:
    std::vector<Signature> signatures;

    // Helper: Compute epsilon-closure given an active set of NFA states
    // Adds all states reachable via zero or more Deletion transitions while pos < pattern.length() and errors < k.
    // Also outputs the newly added states in addedOut if provided.
    std::set<NFAState> computeEpsilonClosure(const std::set<NFAState>& initialSet, int k, std::vector<NFAState>* addedOut = nullptr) const;

    // Helper: Check which signatures are accepted by an NFA state set
    std::vector<MatchRecord> checkAcceptance(const std::set<NFAState>& stateSet, int k, size_t currentByteOffset) const;

    // Helper: Step NFA on a single byte, before reseeding start states and epsilon closure
    std::set<NFAState> stepNFARaw(const std::set<NFAState>& currentStates, uint8_t byteVal, int k) const;

public:
    DFAScanner();

    void addSignature(const std::string& id, const std::string& name, const std::string& pattern);
    void clearSignatures();
    const std::vector<Signature>& getSignatures() const { return signatures; }

    bool loadSignaturesFromFile(const std::string& filepath);

    // Three matching engines
    ExactMatchResult runExactMatching(const std::vector<uint8_t>& inputBytes) const;
    DirectNFAResult runDirectNFASimulation(const std::vector<uint8_t>& inputBytes, int k) const;
    LazyDFAResult runLazyDFA(const std::vector<uint8_t>& inputBytes, int k, int maxTrace = 200) const;

    // Full comparison driver
    ScanComparisonReport runFullComparison(const std::vector<uint8_t>& inputBytes, int k, int trials = 1, int maxTrace = 200) const;

    // Serialization helper
    std::string reportToJSON(const ScanComparisonReport& report) const;

    // Terminal display helpers
    void displaySignatures() const;
    void printReportText(const ScanComparisonReport& report) const;
};

#endif // DFA_SCANNER_H
