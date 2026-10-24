# PolyMorph Hunter
**Visual & Experimental Analysis of Lazy Determinization in Approximate Pattern Matching**

Traditional exact-matching signature engines fail when polymorphic malware mutates by even a single byte. PolyMorph Hunter is a full-stack academic research instrument built to solve this evasion tactic using Automata Theory. By modeling signatures as Levenshtein $\epsilon$-NFAs, the engine tolerates a configurable budget of substitutions, insertions, and deletions ($k$). To overcome the exponential memory explosion of standard DFAs, this project implements **Lazy Subset Construction**, determinizing and caching states strictly on-demand.

## Theoretical Foundation
* **Levenshtein $\epsilon$-NFA:** Nondeterministic states are represented as tuples `(sigIndex, position, errors)`.
* **$\epsilon$-Closure for Deletions:** Deletions are mathematically modeled as $\epsilon$-transitions, advancing the pattern pointer without consuming a file byte.
* **Lazy Determinization:** Transitions initialize as `-1` (uncomputed). The engine calculates NFA subsets and closures on the fly during a cache miss, then caches the deterministic state ID for $O(1)$ future lookups.

## The 3-Way Experimental Framework
The C++17 kernel evaluates three distinct computational paradigms on identical input streams:
1. **Method 1: Exact Matching:** Deterministic AC-Trie simulation ($O(N)$ speed, but 0% tolerance for mutated variants).
2. **Method 2: Direct NFA Simulation:** Evaluates all active branches per byte (Catches mutations, but highly CPU-intensive).
3. **Method 3: Lazy DFA (Ours):** On-demand subset determinization (Catches mutations, avoids state-explosion, achieves high throughput via cache reuse).

## Interface & Telemetry
The frontend acts as an interactive editorial workbench rather than a black-box scanner:
* **Real-Time Telemetry:** Live C++ counters for DFA States Generated, Transitions Computed (Cache Misses), and Cached Transitions Reused (Cache Hits).
* **Deterministic Execution Trace:** A byte-by-byte scrubber exposing step transition metadata, separated $\epsilon$-closure additions, and the canonical deterministic subset.
* **Visual Automaton State Graph:** Dynamic rendering of the generated DFA states and accepting nodes.
* **Automated Verification:** A built-in 8-case suite verifying constraints (e.g., negative controls, exact matches, cache reuse, and real-world polymorphic evasion).

## Tech Stack
* **Kernel:** C++17 (Standard Template Library).
* **Local Bridge:** Python / Node.js API.
* **Frontend:** HTML5, CSS3, JavaScript (Vanilla DOM manipulation, zero-bloat architecture).

## Authors & Institution
* **Aditya Ajit Borge**
* **Meeth S. Bafna**
* **Arin Kulkarni**

*B.Tech Computer Science and Engineering (Artificial Intelligence and Data Engineering)*  
*Vellore Institute of Technology | Course: Models of Computation*
