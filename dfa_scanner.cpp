#include "dfa_scanner.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <queue>
#include <algorithm>
#include <cctype>

using namespace std;

// Helper function to escape strings for valid JSON
static string jsonEscape(const string& s) {
    ostringstream o;
    for (char c : s) {
        switch (c) {
            case '"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b"; break;
            case '\f': o << "\\f"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) >= 127) {
                    o << "\\u" << hex << setfill('0') << setw(4) << static_cast<int>(static_cast<unsigned char>(c));
                    o << dec;
                } else {
                    o << c;
                }
                break;
        }
    }
    return o.str();
}

static string formatByteChar(uint8_t b) {
    if (b >= 32 && b <= 126 && b != '\\' && b != '"') {
        return string(1, static_cast<char>(b));
    }
    if (b == '\\') return "\\\\";
    if (b == '"') return "\\\"";
    if (b == '\n') return "\\n";
    if (b == '\r') return "\\r";
    if (b == '\t') return "\\t";
    ostringstream oss;
    oss << "\\x" << hex << setfill('0') << setw(2) << static_cast<int>(b);
    return oss.str();
}

static string formatByteHex(uint8_t b) {
    ostringstream oss;
    oss << "0x" << uppercase << hex << setfill('0') << setw(2) << static_cast<int>(b);
    return oss.str();
}

DFAScanner::DFAScanner() {
    signatures.clear();
}

void DFAScanner::addSignature(const string& id, const string& name, const string& pattern) {
    Signature s;
    s.id = id;
    s.name = name;
    s.pattern = pattern;
    signatures.push_back(s);
}

void DFAScanner::clearSignatures() {
    signatures.clear();
}

bool DFAScanner::loadSignaturesFromFile(const string& filepath) {
    ifstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    string line;
    while (getline(file, line)) {
        // Strip trailing \r if any
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) continue;

        size_t firstSep = line.find('|');
        size_t secondSep = line.find('|', firstSep + 1);

        if (firstSep == string::npos || secondSep == string::npos) {
            continue;
        }

        string id = line.substr(0, firstSep);
        string name = line.substr(firstSep + 1, secondSep - firstSep - 1);
        string pattern = line.substr(secondSep + 1);

        addSignature(id, name, pattern);
    }
    file.close();
    return true;
}

// Compute epsilon-closure:
// Starting with initialSet, iteratively apply Deletion transition (pos < len && errors < k):
// (sigIndex, pos, errors) -> (sigIndex, pos + 1, errors + 1)
set<NFAState> DFAScanner::computeEpsilonClosure(const set<NFAState>& initialSet, int k, vector<NFAState>* addedOut) const {
    set<NFAState> visited = initialSet;
    vector<NFAState> frontier(initialSet.begin(), initialSet.end());

    size_t head = 0;
    while (head < frontier.size()) {
        NFAState curr = frontier[head++];

        if (curr.sigIndex < 0 || curr.sigIndex >= static_cast<int>(signatures.size())) {
            continue;
        }

        int patLen = static_cast<int>(signatures[curr.sigIndex].pattern.length());

        if (curr.pos < patLen && curr.errors < k) {
            NFAState nextState{ curr.sigIndex, curr.pos + 1, curr.errors + 1 };
            if (visited.insert(nextState).second) {
                frontier.push_back(nextState);
                if (addedOut) {
                    addedOut->push_back(nextState);
                }
            }
        }
    }

    return visited;
}

// Check which signatures are accepted by the given NFA state set
vector<MatchRecord> DFAScanner::checkAcceptance(const set<NFAState>& stateSet, int k, size_t currentByteOffset) const {
    vector<MatchRecord> accepted;
    map<int, int> minErrorsPerSig;

    for (const auto& st : stateSet) {
        if (st.sigIndex >= 0 && st.sigIndex < static_cast<int>(signatures.size())) {
            int patLen = static_cast<int>(signatures[st.sigIndex].pattern.length());
            if (st.pos == patLen && st.errors <= k) {
                if (minErrorsPerSig.find(st.sigIndex) == minErrorsPerSig.end()) {
                    minErrorsPerSig[st.sigIndex] = st.errors;
                } else {
                    minErrorsPerSig[st.sigIndex] = min(minErrorsPerSig[st.sigIndex], st.errors);
                }
            }
        }
    }

    for (const auto& pair : minErrorsPerSig) {
        const auto& sig = signatures[pair.first];
        MatchRecord rec;
        rec.id = sig.id;
        rec.name = sig.name;
        rec.pattern = sig.pattern;
        rec.minErrors = pair.second;
        rec.byteOffset = currentByteOffset;
        accepted.push_back(rec);
    }

    return accepted;
}

// Compute the raw NFA state transitions on byteVal (before substring reseed and epsilon closure)
set<NFAState> DFAScanner::stepNFARaw(const set<NFAState>& currentStates, uint8_t byteVal, int k) const {
    set<NFAState> nextStates;

    for (const auto& st : currentStates) {
        if (st.sigIndex < 0 || st.sigIndex >= static_cast<int>(signatures.size())) {
            continue;
        }

        const string& pat = signatures[st.sigIndex].pattern;
        int patLen = static_cast<int>(pat.length());

        // Rule 1: Exact Match
        if (st.pos < patLen && static_cast<uint8_t>(pat[st.pos]) == byteVal) {
            nextStates.insert({ st.sigIndex, st.pos + 1, st.errors });
        }

        // Rule 2: Substitution
        if (st.pos < patLen && static_cast<uint8_t>(pat[st.pos]) != byteVal && st.errors < k) {
            nextStates.insert({ st.sigIndex, st.pos + 1, st.errors + 1 });
        }

        // Rule 3: Insertion (byte is consumed, position retained)
        if (st.errors < k) {
            nextStates.insert({ st.sigIndex, st.pos, st.errors + 1 });
        }
    }

    return nextStates;
}

// -------------------------------------------------------------
// Method 1: Deterministic Exact Substring Matcher (k = 0)
// Using multi-pattern Aho-Corasick automaton
// -------------------------------------------------------------
ExactMatchResult DFAScanner::runExactMatching(const vector<uint8_t>& inputBytes) const {
    ExactMatchResult result;
    result.matchedCount = 0;
    result.statesGenerated = 0;
    result.transitionsComputed = 0;
    result.verdict = "CLEAN";

    if (signatures.empty()) {
        result.executionTimeUs = 0.0;
        return result;
    }

    auto startTime = chrono::high_resolution_clock::now();

    struct ACTNode {
        array<int, 256> next;
        int fail;
        vector<int> output;
        ACTNode() {
            next.fill(0);
            fail = 0;
        }
    };

    vector<ACTNode> trie;
    trie.emplace_back(); // State 0 (root)

    // Build trie
    for (size_t sigIdx = 0; sigIdx < signatures.size(); sigIdx++) {
        const string& pat = signatures[sigIdx].pattern;
        int currState = 0;
        for (char c : pat) {
            uint8_t b = static_cast<uint8_t>(c);
            if (trie[currState].next[b] == 0) {
                int newState = static_cast<int>(trie.size());
                trie.emplace_back();
                trie[currState].next[b] = newState;
            }
            currState = trie[currState].next[b];
        }
        trie[currState].output.push_back(static_cast<int>(sigIdx));
    }

    // Build failure links & complete transition table (DFA style)
    queue<int> q;
    for (int b = 0; b < 256; b++) {
        int nextSt = trie[0].next[b];
        if (nextSt != 0) {
            trie[nextSt].fail = 0;
            q.push(nextSt);
        }
    }

    int transComputed = 0;
    while (!q.empty()) {
        int r = q.front();
        q.pop();

        for (int b = 0; b < 256; b++) {
            int u = trie[r].next[b];
            if (u != 0) {
                trie[u].fail = trie[trie[r].fail].next[b];
                for (int outSig : trie[trie[u].fail].output) {
                    trie[u].output.push_back(outSig);
                }
                q.push(u);
                transComputed++;
            } else {
                trie[r].next[b] = trie[trie[r].fail].next[b];
            }
        }
    }

    result.statesGenerated = static_cast<int>(trie.size());
    result.transitionsComputed = transComputed + static_cast<int>(signatures.size());

    // Scan input bytes
    int currentState = 0;
    map<int, bool> alreadyReported;

    for (size_t offset = 0; offset < inputBytes.size(); offset++) {
        uint8_t b = inputBytes[offset];
        currentState = trie[currentState].next[b];

        if (!trie[currentState].output.empty()) {
            for (int sigIdx : trie[currentState].output) {
                if (!alreadyReported[sigIdx]) {
                    alreadyReported[sigIdx] = true;
                    MatchRecord rec;
                    rec.id = signatures[sigIdx].id;
                    rec.name = signatures[sigIdx].name;
                    rec.pattern = signatures[sigIdx].pattern;
                    rec.minErrors = 0;
                    rec.byteOffset = offset;
                    result.matchedSignatures.push_back(rec);
                }
            }
        }
    }

    auto endTime = chrono::high_resolution_clock::now();
    result.executionTimeUs = chrono::duration<double, micro>(endTime - startTime).count();
    result.matchedCount = static_cast<int>(result.matchedSignatures.size());
    result.verdict = (result.matchedCount > 0) ? "MATCHED" : "CLEAN";

    return result;
}

// -------------------------------------------------------------
// Method 2: Direct NFA Simulation byte-by-byte
// -------------------------------------------------------------
DirectNFAResult DFAScanner::runDirectNFASimulation(const vector<uint8_t>& inputBytes, int k) const {
    DirectNFAResult result;
    result.matchedCount = 0;
    result.stateEvaluations = 0;
    result.verdict = "CLEAN";

    if (signatures.empty()) {
        result.executionTimeUs = 0.0;
        return result;
    }

    auto startTime = chrono::high_resolution_clock::now();

    // Initial NFA state set: base start states for all signatures
    set<NFAState> activeStates;
    for (size_t i = 0; i < signatures.size(); i++) {
        activeStates.insert({ static_cast<int>(i), 0, 0 });
    }
    activeStates = computeEpsilonClosure(activeStates, k);

    map<int, MatchRecord> bestMatches;

    // Check acceptance in initial state (e.g. empty patterns or k >= length)
    auto initialAccept = checkAcceptance(activeStates, k, 0);
    for (const auto& m : initialAccept) {
        int idx = -1;
        for (size_t i = 0; i < signatures.size(); i++) {
            if (signatures[i].id == m.id) { idx = static_cast<int>(i); break; }
        }
        if (idx != -1 && bestMatches.find(idx) == bestMatches.end()) {
            bestMatches[idx] = m;
        }
    }

    for (size_t offset = 0; offset < inputBytes.size(); offset++) {
        uint8_t b = inputBytes[offset];
        result.stateEvaluations += activeStates.size();

        // 1. Step transitions on byte
        set<NFAState> rawNext = stepNFARaw(activeStates, b, k);

        // 2. Reseed substring start states
        for (size_t i = 0; i < signatures.size(); i++) {
            rawNext.insert({ static_cast<int>(i), 0, 0 });
        }

        // 3. Compute epsilon closure
        activeStates = computeEpsilonClosure(rawNext, k);

        // 4. Check acceptance
        auto matches = checkAcceptance(activeStates, k, offset);
        for (const auto& m : matches) {
            int idx = -1;
            for (size_t i = 0; i < signatures.size(); i++) {
                if (signatures[i].id == m.id) { idx = static_cast<int>(i); break; }
            }
            if (idx != -1) {
                if (bestMatches.find(idx) == bestMatches.end()) {
                    bestMatches[idx] = m;
                } else if (m.minErrors < bestMatches[idx].minErrors) {
                    bestMatches[idx] = m;
                }
            }
        }
    }

    auto endTime = chrono::high_resolution_clock::now();
    result.executionTimeUs = chrono::duration<double, micro>(endTime - startTime).count();

    for (const auto& pair : bestMatches) {
        result.matchedSignatures.push_back(pair.second);
    }
    result.matchedCount = static_cast<int>(result.matchedSignatures.size());
    result.verdict = (result.matchedCount > 0) ? "MATCHED" : "CLEAN";

    return result;
}

// -------------------------------------------------------------
// Method 3: Lazy DFA Execution with on-demand subset construction
// -------------------------------------------------------------
LazyDFAResult DFAScanner::runLazyDFA(const vector<uint8_t>& inputBytes, int k, int maxTrace) const {
    LazyDFAResult result;
    result.matchedCount = 0;
    result.dfaStatesGenerated = 0;
    result.transitionsComputed = 0;
    result.cacheHits = 0;
    result.cacheHitRate = 0.0;
    result.verdict = "CLEAN";

    if (signatures.empty()) {
        result.executionTimeUs = 0.0;
        return result;
    }

    auto startTime = chrono::high_resolution_clock::now();

    map<set<NFAState>, int> stateRegistry;
    vector<set<NFAState>> dfaStates;
    vector<array<int, 256>> transition;

    // Helper to create a DFA state
    auto registerDFAState = [&](const set<NFAState>& nfaSubset) -> int {
        int newId = static_cast<int>(dfaStates.size());
        stateRegistry[nfaSubset] = newId;
        dfaStates.push_back(nfaSubset);
        array<int, 256> row;
        row.fill(-1); // -1 indicates uncomputed transition
        transition.push_back(row);
        return newId;
    };

    // State q0: base start states for all signatures + epsilon closure
    set<NFAState> q0Seed;
    for (size_t i = 0; i < signatures.size(); i++) {
        q0Seed.insert({ static_cast<int>(i), 0, 0 });
    }
    set<NFAState> q0 = computeEpsilonClosure(q0Seed, k);
    registerDFAState(q0);

    int currentDFAState = 0;
    map<int, MatchRecord> bestMatches;

    // Check acceptance in initial state
    auto initialAccept = checkAcceptance(q0, k, 0);
    for (const auto& m : initialAccept) {
        int idx = -1;
        for (size_t i = 0; i < signatures.size(); i++) {
            if (signatures[i].id == m.id) { idx = static_cast<int>(i); break; }
        }
        if (idx != -1 && bestMatches.find(idx) == bestMatches.end()) {
            bestMatches[idx] = m;
        }
    }

    for (size_t offset = 0; offset < inputBytes.size(); offset++) {
        uint8_t b = inputBytes[offset];
        int fromState = currentDFAState;
        int toState = -1;
        string cacheStatus;
        vector<string> rawStStrings;
        vector<string> epsAddedStrings;

        int cachedNext = transition[fromState][b];

        if (cachedNext != -1) {
            // CACHE HIT
            result.cacheHits++;
            cacheStatus = "HIT";
            toState = cachedNext;

            // If we are recording trace, compute raw and epsilon closure components for visibility
            if (static_cast<int>(result.trace.size()) < maxTrace) {
                set<NFAState> rawNext = stepNFARaw(dfaStates[fromState], b, k);
                for (size_t i = 0; i < signatures.size(); i++) {
                    rawNext.insert({ static_cast<int>(i), 0, 0 });
                }
                for (const auto& st : rawNext) {
                    rawStStrings.push_back(st.toString());
                }

                vector<NFAState> added;
                computeEpsilonClosure(rawNext, k, &added);
                for (const auto& st : added) {
                    epsAddedStrings.push_back(st.toString());
                }
            }
        } else {
            // CACHE MISS: Compute transition on demand
            result.transitionsComputed++;
            cacheStatus = "COMPUTED";

            set<NFAState> rawNext = stepNFARaw(dfaStates[fromState], b, k);
            for (size_t i = 0; i < signatures.size(); i++) {
                rawNext.insert({ static_cast<int>(i), 0, 0 });
            }

            if (static_cast<int>(result.trace.size()) < maxTrace) {
                for (const auto& st : rawNext) {
                    rawStStrings.push_back(st.toString());
                }
            }

            vector<NFAState> added;
            set<NFAState> fullNext = computeEpsilonClosure(rawNext, k, &added);

            if (static_cast<int>(result.trace.size()) < maxTrace) {
                for (const auto& st : added) {
                    epsAddedStrings.push_back(st.toString());
                }
            }

            auto it = stateRegistry.find(fullNext);
            if (it != stateRegistry.end()) {
                toState = it->second;
            } else {
                toState = registerDFAState(fullNext);
            }

            // Cache computed transition
            transition[fromState][b] = toState;
        }

        currentDFAState = toState;

        // Check acceptance for the destination state
        auto stepAccepted = checkAcceptance(dfaStates[toState], k, offset);
        for (const auto& m : stepAccepted) {
            int idx = -1;
            for (size_t i = 0; i < signatures.size(); i++) {
                if (signatures[i].id == m.id) { idx = static_cast<int>(i); break; }
            }
            if (idx != -1) {
                if (bestMatches.find(idx) == bestMatches.end()) {
                    bestMatches[idx] = m;
                } else if (m.minErrors < bestMatches[idx].minErrors) {
                    bestMatches[idx] = m;
                }
            }
        }

        // Record trace step if within budget
        if (static_cast<int>(result.trace.size()) < maxTrace) {
            StepTrace step;
            step.stepIndex = static_cast<int>(offset);
            step.byteChar = formatByteChar(b);
            step.byteHex = formatByteHex(b);
            step.byteAscii = static_cast<int>(b);
            step.fromDFAState = fromState;
            step.toDFAState = toState;
            step.cacheStatus = cacheStatus;
            step.rawNFASubset = rawStStrings;
            step.epsilonClosureAdded = epsAddedStrings;
            for (const auto& st : dfaStates[toState]) {
                step.fullNFASubset.push_back(st.toString());
            }
            step.acceptedSignatures = stepAccepted;
            result.trace.push_back(step);
        }
    }

    auto endTime = chrono::high_resolution_clock::now();
    result.executionTimeUs = chrono::duration<double, micro>(endTime - startTime).count();

    result.dfaStatesGenerated = static_cast<int>(dfaStates.size());
    int totalQueries = result.cacheHits + result.transitionsComputed;
    result.cacheHitRate = (totalQueries > 0) ? (100.0 * result.cacheHits / totalQueries) : 0.0;

    for (const auto& pair : bestMatches) {
        result.matchedSignatures.push_back(pair.second);
    }
    result.matchedCount = static_cast<int>(result.matchedSignatures.size());
    result.verdict = (result.matchedCount > 0) ? "MATCHED" : "CLEAN";

    // Dump state registry (dfaStates)
    for (size_t s = 0; s < dfaStates.size(); s++) {
        DFAStateDump dump;
        dump.dfaStateId = static_cast<int>(s);
        dump.acceptedSignatures = checkAcceptance(dfaStates[s], k, 0);
        dump.isAccepting = !dump.acceptedSignatures.empty();
        for (const auto& st : dfaStates[s]) {
            dump.nfaSubset.push_back(st.toString());
        }
        result.stateRegistryDump.push_back(dump);
    }

    // Dump active computed transitions
    for (size_t s = 0; s < transition.size(); s++) {
        for (int b = 0; b < 256; b++) {
            if (transition[s][b] != -1) {
                ActiveTransitionDump trans;
                trans.fromState = static_cast<int>(s);
                trans.byteAscii = b;
                trans.byteChar = formatByteChar(static_cast<uint8_t>(b));
                trans.toState = transition[s][b];
                result.activeTransitionsDump.push_back(trans);
            }
        }
    }

    return result;
}

// -------------------------------------------------------------
// Full Comparison Driver
// -------------------------------------------------------------
ScanComparisonReport DFAScanner::runFullComparison(const vector<uint8_t>& inputBytes, int k, int trials, int maxTrace) const {
    ScanComparisonReport report;
    report.k = k;
    report.bytesScanned = inputBytes.size();
    report.trials = max(1, trials);

    // Run Method 1: Exact
    double exactTotalTime = 0.0;
    ExactMatchResult exactRes;
    for (int t = 0; t < report.trials; t++) {
        exactRes = runExactMatching(inputBytes);
        exactTotalTime += exactRes.executionTimeUs;
    }
    exactRes.executionTimeUs = exactTotalTime / report.trials;
    report.exact = exactRes;

    // Run Method 2: Direct NFA
    double nfaTotalTime = 0.0;
    DirectNFAResult nfaRes;
    for (int t = 0; t < report.trials; t++) {
        nfaRes = runDirectNFASimulation(inputBytes, k);
        nfaTotalTime += nfaRes.executionTimeUs;
    }
    nfaRes.executionTimeUs = nfaTotalTime / report.trials;
    report.directNFA = nfaRes;

    // Run Method 3: Lazy DFA
    double dfaTotalTime = 0.0;
    LazyDFAResult dfaRes;
    for (int t = 0; t < report.trials; t++) {
        dfaRes = runLazyDFA(inputBytes, k, maxTrace);
        dfaTotalTime += dfaRes.executionTimeUs;
    }
    dfaRes.executionTimeUs = dfaTotalTime / report.trials;
    report.lazyDFA = dfaRes;

    // Overall verdict and matched signatures follow the approximate engine (Lazy DFA)
    report.verdict = dfaRes.verdict;
    report.matchedSignatures = dfaRes.matchedSignatures;

    return report;
}

// -------------------------------------------------------------
// Convert report to strictly valid, well-formed JSON
// -------------------------------------------------------------
string DFAScanner::reportToJSON(const ScanComparisonReport& report) const {
    ostringstream oss;
    oss << "{\n";
    oss << "  \"verdict\": \"" << report.verdict << "\",\n";
    oss << "  \"config\": {\n";
    oss << "    \"k\": " << report.k << ",\n";
    oss << "    \"bytesScanned\": " << report.bytesScanned << ",\n";
    oss << "    \"trials\": " << report.trials << "\n";
    oss << "  },\n";

    // matchedSignatures
    oss << "  \"matchedSignatures\": [\n";
    for (size_t i = 0; i < report.matchedSignatures.size(); i++) {
        const auto& m = report.matchedSignatures[i];
        oss << "    {\n";
        oss << "      \"id\": \"" << jsonEscape(m.id) << "\",\n";
        oss << "      \"name\": \"" << jsonEscape(m.name) << "\",\n";
        oss << "      \"pattern\": \"" << jsonEscape(m.pattern) << "\",\n";
        oss << "      \"minErrors\": " << m.minErrors << ",\n";
        oss << "      \"byteOffset\": " << m.byteOffset << "\n";
        oss << "    }" << (i + 1 < report.matchedSignatures.size() ? "," : "") << "\n";
    }
    oss << "  ],\n";

    // lazyDFA summary
    const auto& ldfa = report.lazyDFA;
    oss << "  \"lazyDFA\": {\n";
    oss << "    \"verdict\": \"" << ldfa.verdict << "\",\n";
    oss << "    \"matchedCount\": " << ldfa.matchedCount << ",\n";
    oss << "    \"dfaStatesGenerated\": " << ldfa.dfaStatesGenerated << ",\n";
    oss << "    \"transitionsComputed\": " << ldfa.transitionsComputed << ",\n";
    oss << "    \"cacheHits\": " << ldfa.cacheHits << ",\n";
    oss << "    \"cacheHitRate\": " << fixed << setprecision(2) << ldfa.cacheHitRate << ",\n";
    oss << "    \"executionTimeUs\": " << fixed << setprecision(2) << ldfa.executionTimeUs << ",\n";

    // stateRegistryDump
    oss << "    \"stateRegistryDump\": [\n";
    for (size_t i = 0; i < ldfa.stateRegistryDump.size(); i++) {
        const auto& sd = ldfa.stateRegistryDump[i];
        oss << "      {\n";
        oss << "        \"dfaStateId\": " << sd.dfaStateId << ",\n";
        oss << "        \"isAccepting\": " << (sd.isAccepting ? "true" : "false") << ",\n";
        oss << "        \"acceptedSignatures\": [";
        for (size_t a = 0; a < sd.acceptedSignatures.size(); a++) {
            oss << "\"" << jsonEscape(sd.acceptedSignatures[a].id) << "\"" << (a + 1 < sd.acceptedSignatures.size() ? ", " : "");
        }
        oss << "],\n";
        oss << "        \"nfaSubset\": [";
        for (size_t n = 0; n < sd.nfaSubset.size(); n++) {
            oss << "\"" << jsonEscape(sd.nfaSubset[n]) << "\"" << (n + 1 < sd.nfaSubset.size() ? ", " : "");
        }
        oss << "]\n";
        oss << "      }" << (i + 1 < ldfa.stateRegistryDump.size() ? "," : "") << "\n";
    }
    oss << "    ],\n";

    // activeTransitionsDump
    oss << "    \"activeTransitionsDump\": [\n";
    for (size_t i = 0; i < ldfa.activeTransitionsDump.size(); i++) {
        const auto& td = ldfa.activeTransitionsDump[i];
        oss << "      {\n";
        oss << "        \"fromState\": " << td.fromState << ",\n";
        oss << "        \"byteChar\": \"" << jsonEscape(td.byteChar) << "\",\n";
        oss << "        \"byteAscii\": " << td.byteAscii << ",\n";
        oss << "        \"toState\": " << td.toState << "\n";
        oss << "      }" << (i + 1 < ldfa.activeTransitionsDump.size() ? "," : "") << "\n";
    }
    oss << "    ]\n";
    oss << "  },\n";

    // trace
    oss << "  \"trace\": [\n";
    for (size_t i = 0; i < ldfa.trace.size(); i++) {
        const auto& tr = ldfa.trace[i];
        oss << "    {\n";
        oss << "      \"stepIndex\": " << tr.stepIndex << ",\n";
        oss << "      \"byteChar\": \"" << jsonEscape(tr.byteChar) << "\",\n";
        oss << "      \"byteHex\": \"" << jsonEscape(tr.byteHex) << "\",\n";
        oss << "      \"byteAscii\": " << tr.byteAscii << ",\n";
        oss << "      \"fromDFAState\": " << tr.fromDFAState << ",\n";
        oss << "      \"toDFAState\": " << tr.toDFAState << ",\n";
        oss << "      \"cacheStatus\": \"" << tr.cacheStatus << "\",\n";

        oss << "      \"rawNFASubset\": [";
        for (size_t r = 0; r < tr.rawNFASubset.size(); r++) {
            oss << "\"" << jsonEscape(tr.rawNFASubset[r]) << "\"" << (r + 1 < tr.rawNFASubset.size() ? ", " : "");
        }
        oss << "],\n";

        oss << "      \"epsilonClosureAdded\": [";
        for (size_t e = 0; e < tr.epsilonClosureAdded.size(); e++) {
            oss << "\"" << jsonEscape(tr.epsilonClosureAdded[e]) << "\"" << (e + 1 < tr.epsilonClosureAdded.size() ? ", " : "");
        }
        oss << "],\n";

        oss << "      \"fullNFASubset\": [";
        for (size_t f = 0; f < tr.fullNFASubset.size(); f++) {
            oss << "\"" << jsonEscape(tr.fullNFASubset[f]) << "\"" << (f + 1 < tr.fullNFASubset.size() ? ", " : "");
        }
        oss << "],\n";

        oss << "      \"acceptedSignatures\": [";
        for (size_t a = 0; a < tr.acceptedSignatures.size(); a++) {
            oss << "{\"id\": \"" << jsonEscape(tr.acceptedSignatures[a].id) << "\", \"minErrors\": " << tr.acceptedSignatures[a].minErrors << "}"
                << (a + 1 < tr.acceptedSignatures.size() ? ", " : "");
        }
        oss << "]\n";

        oss << "    }" << (i + 1 < ldfa.trace.size() ? "," : "") << "\n";
    }
    oss << "  ],\n";

    // comparison
    oss << "  \"comparison\": {\n";
    // exact
    oss << "    \"exact\": {\n";
    oss << "      \"verdict\": \"" << report.exact.verdict << "\",\n";
    oss << "      \"matchedCount\": " << report.exact.matchedCount << ",\n";
    oss << "      \"statesGenerated\": " << report.exact.statesGenerated << ",\n";
    oss << "      \"transitionsComputed\": " << report.exact.transitionsComputed << ",\n";
    oss << "      \"executionTimeUs\": " << fixed << setprecision(2) << report.exact.executionTimeUs << "\n";
    oss << "    },\n";

    // directNFA
    oss << "    \"directNFA\": {\n";
    oss << "      \"verdict\": \"" << report.directNFA.verdict << "\",\n";
    oss << "      \"matchedCount\": " << report.directNFA.matchedCount << ",\n";
    oss << "      \"stateEvaluations\": " << report.directNFA.stateEvaluations << ",\n";
    oss << "      \"executionTimeUs\": " << fixed << setprecision(2) << report.directNFA.executionTimeUs << "\n";
    oss << "    },\n";

    // lazyDFA
    oss << "    \"lazyDFA\": {\n";
    oss << "      \"verdict\": \"" << ldfa.verdict << "\",\n";
    oss << "      \"matchedCount\": " << ldfa.matchedCount << ",\n";
    oss << "      \"dfaStatesGenerated\": " << ldfa.dfaStatesGenerated << ",\n";
    oss << "      \"transitionsComputed\": " << ldfa.transitionsComputed << ",\n";
    oss << "      \"cacheHits\": " << ldfa.cacheHits << ",\n";
    oss << "      \"executionTimeUs\": " << fixed << setprecision(2) << ldfa.executionTimeUs << "\n";
    oss << "    }\n";
    oss << "  }\n";
    oss << "}\n";

    return oss.str();
}

void DFAScanner::displaySignatures() const {
    cout << "Loaded Signatures (" << signatures.size() << "):\n";
    for (const auto& s : signatures) {
        cout << "  " << s.id << " | " << s.name << " | " << s.pattern << "\n";
    }
}

void DFAScanner::printReportText(const ScanComparisonReport& report) const {
    cout << "==============================================================\n";
    cout << "           POLYMORPH HUNTER - SCAN BENCHMARK REPORT           \n";
    cout << "==============================================================\n";
    cout << "Overall Verdict       : " << report.verdict << "\n";
    cout << "Edit Distance Budget k: " << report.k << "\n";
    cout << "Bytes Scanned         : " << report.bytesScanned << " bytes\n";
    cout << "Benchmark Trials      : " << report.trials << "\n";
    cout << "--------------------------------------------------------------\n";

    if (report.matchedSignatures.empty()) {
        cout << "Matches Found: None (Input is CLEAN within budget k=" << report.k << ")\n";
    } else {
        cout << "Matches Found (" << report.matchedSignatures.size() << "):\n";
        for (const auto& m : report.matchedSignatures) {
            cout << "  * [" << m.id << "] \"" << m.name << "\" (Pattern: " << m.pattern 
                 << ", Min Errors: " << m.minErrors << ", at Byte Offset: " << m.byteOffset << ")\n";
        }
    }

    cout << "\n--------------------------------------------------------------\n";
    cout << "METHOD 1: EXACT MATCHING (Deterministic Multi-Pattern DFA)\n";
    cout << "  Verdict            : " << report.exact.verdict << "\n";
    cout << "  Matches            : " << report.exact.matchedCount << "\n";
    cout << "  States Generated   : " << report.exact.statesGenerated << "\n";
    cout << "  Transitions Built  : " << report.exact.transitionsComputed << "\n";
    cout << "  Execution Time     : " << fixed << setprecision(2) << report.exact.executionTimeUs << " us\n";

    cout << "\nMETHOD 2: DIRECT NFA SIMULATION (Levenshtein e-NFA)\n";
    cout << "  Verdict            : " << report.directNFA.verdict << "\n";
    cout << "  Matches            : " << report.directNFA.matchedCount << "\n";
    cout << "  State Evaluations  : " << report.directNFA.stateEvaluations << "\n";
    cout << "  Execution Time     : " << fixed << setprecision(2) << report.directNFA.executionTimeUs << " us\n";

    cout << "\nMETHOD 3: LAZY DETERMINISTIC FINITE AUTOMATON (Lazy DFA)\n";
    cout << "  Verdict            : " << report.lazyDFA.verdict << "\n";
    cout << "  Matches            : " << report.lazyDFA.matchedCount << "\n";
    cout << "  DFA States Created : " << report.lazyDFA.dfaStatesGenerated << "\n";
    cout << "  Transitions Built  : " << report.lazyDFA.transitionsComputed << " (Cache Misses)\n";
    cout << "  Transitions Reused : " << report.lazyDFA.cacheHits << " (Cache Hits)\n";
    cout << "  Cache Hit Rate     : " << fixed << setprecision(2) << report.lazyDFA.cacheHitRate << " %\n";
    cout << "  Execution Time     : " << fixed << setprecision(2) << report.lazyDFA.executionTimeUs << " us\n";
    cout << "==============================================================\n";
}
