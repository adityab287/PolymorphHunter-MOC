#!/usr/bin/env python3
"""
PolyMorph Hunter - Local API Bridge Server
Zero-bloat local HTTP server using Python standard library.
Connects the browser research workbench to the compiled C++17 automata engine.
"""

import http.server
import socketserver
import json
import subprocess
import os
import sys
import tempfile
import time
from pathlib import Path

PORT = 8080
BASE_DIR = Path(__file__).resolve().parent
SCANNER_EXE = BASE_DIR / "scanner.exe"

def ensure_compiled():
    """Ensure scanner.exe is compiled and up-to-date with C++ sources."""
    cpp_files = [BASE_DIR / "main.cpp", BASE_DIR / "dfa_scanner.cpp", BASE_DIR / "dfa_scanner.h"]
    need_compile = not SCANNER_EXE.exists()

    if not need_compile:
        exe_time = SCANNER_EXE.stat().st_mtime
        for f in cpp_files:
            if f.exists() and f.stat().st_mtime > exe_time:
                need_compile = True
                break

    if need_compile:
        print("[Bridge] Compiling C++17 engine (scanner.exe)...")
        cmd = ["g++", "-std=c++17", "-O2", str(BASE_DIR / "main.cpp"), str(BASE_DIR / "dfa_scanner.cpp"), "-o", str(SCANNER_EXE)]
        res = subprocess.run(cmd, cwd=str(BASE_DIR), capture_output=True, text=True)
        if res.returncode != 0:
            print("[Bridge] ERROR compiling C++ engine:\n", res.stderr)
            raise RuntimeError(f"C++ Compilation failed: {res.stderr}")
        print("[Bridge] scanner.exe compiled successfully.")

def read_text_safe(filepath, default=""):
    try:
        p = Path(filepath)
        if p.exists():
            with open(p, "r", encoding="utf-8", errors="replace") as f:
                return f.read()
    except Exception as e:
        print(f"[Bridge] Warning reading {filepath}: {e}")
    return default

def parse_signatures_file(filepath):
    sigs = []
    content = read_text_safe(filepath)
    for line in content.splitlines():
        line = line.strip()
        if not line:
            continue
        parts = line.split("|")
        if len(parts) >= 3:
            sigs.append({
                "id": parts[0].strip(),
                "name": parts[1].strip(),
                "pattern": parts[2].strip()
            })
    return sigs

def run_scanner_process(args, input_data=None):
    """Executes scanner.exe with specified arguments and returns parsed JSON or raises."""
    ensure_compiled()
    cmd = [str(SCANNER_EXE), "--json"] + args
    
    start_t = time.perf_counter()
    proc = subprocess.run(
        cmd,
        cwd=str(BASE_DIR),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace"
    )
    elapsed_ms = (time.perf_counter() - start_t) * 1000.0

    if proc.returncode != 0:
        err_msg = proc.stderr.strip() or proc.stdout.strip()
        raise RuntimeError(f"Engine process returned code {proc.returncode}: {err_msg}")

    out_text = proc.stdout.strip()
    # Handle possible leading logs by finding the first '{'
    idx = out_text.find("{")
    if idx == -1:
        raise ValueError(f"No JSON object found in engine output: {out_text}")
    
    json_str = out_text[idx:]
    data = json.loads(json_str)
    data["bridgeElapsedMs"] = round(elapsed_ms, 2)
    return data

class PolyMorphRequestHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(BASE_DIR), **kwargs)

    def end_headers(self):
        # Enable CORS for local workbench testing
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        super().end_headers()

    def do_OPTIONS(self):
        self.send_response(200)
        self.end_headers()

    def do_GET(self):
        if self.path == "/api/presets" or self.path == "/api/Presets":
            self.handle_get_presets()
        else:
            # Serve index.html by default on root
            if self.path == "/" or self.path == "":
                self.path = "/index.html"
            super().do_GET()

    def do_POST(self):
        if self.path == "/api/scan":
            self.handle_post_scan()
        elif self.path == "/api/run-test-suite":
            self.handle_post_run_test_suite()
        else:
            self.send_error(404, "Endpoint not found")

    def read_json_body(self):
        content_len = int(self.headers.get("Content-Length", 0))
        if content_len == 0:
            return {}
        body = self.rfile.read(content_len).decode("utf-8", errors="replace")
        return json.loads(body)

    def send_json_response(self, data, status=200):
        body = json.dumps(data, indent=2).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def handle_get_presets(self):
        signatures = parse_signatures_file(BASE_DIR / "signatures.txt")
        clean_text = read_text_safe(BASE_DIR / "clean.txt")
        infected_text = read_text_safe(BASE_DIR / "infected.txt")

        test_presets = [
            {
                "id": "cat_exact",
                "name": "CAT Suite 1: Exact Match (CAT vs CAT, k=1)",
                "pattern": "CAT",
                "inputText": "CAT",
                "k": 1,
                "description": "Baseline exact match (0 edits). Verifies standard transition traversal."
            },
            {
                "id": "cat_sub",
                "name": "CAT Suite 2: Substitution Mutation (CAT vs CUT, k=1)",
                "pattern": "CAT",
                "inputText": "CUT",
                "k": 1,
                "description": "Single-character substitution (A -> U). Exact fails; Levenshtein NFA/DFA matches."
            },
            {
                "id": "cat_del",
                "name": "CAT Suite 3: Deletion Mutation (CAT vs CT, k=1)",
                "pattern": "CAT",
                "inputText": "CT",
                "k": 1,
                "description": "Character deletion (A omitted). Verified via epsilon-closure transitions without byte consumption."
            },
            {
                "id": "cat_ins",
                "name": "CAT Suite 4: Insertion Mutation (CAT vs CAAT, k=1)",
                "pattern": "CAT",
                "inputText": "CAAT",
                "k": 1,
                "description": "Character insertion (duplicate A consumed). Verified via insertion transition rule."
            },
            {
                "id": "cat_disjoint",
                "name": "CAT Suite 5: Disjoint Negative Control (CAT vs DOG, k=1)",
                "pattern": "CAT",
                "inputText": "DOG",
                "k": 1,
                "description": "Completely disjoint string (edit distance > k). Must return CLEAN."
            },
            {
                "id": "cat_cache_loop",
                "name": "CAT Suite 6: Cache Reuse Benchmark (CAT vs CATCATCAT, k=1)",
                "pattern": "CAT",
                "inputText": "CATCATCAT",
                "k": 1,
                "description": "Repeated pattern stream. Demonstrates transition caching and cache hit rate spikes."
            },
            {
                "id": "clean_file",
                "name": "Workspace File: clean.txt",
                "pattern": "SUSPICIOUS_PAYLOAD",
                "inputText": clean_text,
                "k": 1,
                "description": "Benign workspace file containing standard non-malicious text."
            },
            {
                "id": "infected_file",
                "name": "Workspace File: infected.txt",
                "pattern": "SUSPICIOUS_PAYLOAD",
                "inputText": infected_text,
                "k": 1,
                "description": "Infected workspace file containing the exact SUSPICIOUS_PAYLOAD signature."
            },
            {
                "id": "polymorphic_payload",
                "name": "Polymorphic Mutation: SUSP1CIOUS_PAYLOAD (k=1)",
                "pattern": "SUSPICIOUS_PAYLOAD",
                "inputText": "SUSP1CIOUS_PAYLOAD",
                "k": 1,
                "description": "Evasion payload where 'I' is obfuscated to '1'. Exact matching misses; Lazy DFA detects."
            },
            {
                "id": "dangerous_cmd_mutation",
                "name": "Polymorphic Mutation: DANG3R_COMMAND (k=1)",
                "pattern": "DANGER_COMMAND",
                "inputText": "EXEC DANG3R_COMMAND NOW",
                "k": 1,
                "description": "Obfuscated malicious command embedded inside a command shell string."
            }
        ]

        payload = {
            "signatures": signatures,
            "cleanText": clean_text,
            "infectedText": infected_text,
            "testPresets": test_presets
        }
        self.send_json_response(payload)

    def handle_post_scan(self):
        try:
            req = self.read_json_body()
            k = int(req.get("k", 1))
            trials = int(req.get("trials", 1))
            max_trace = int(req.get("maxTrace", 200))
            input_text = req.get("inputText", "")
            signatures = req.get("signatures", [])
            use_default_sigs = req.get("useDefaultSigs", False)

            # Build CLI arguments
            cli_args = ["--k", str(k), "--trials", str(trials), "--max-trace", str(max_trace)]

            # If custom signatures provided, write a temporary signature file
            temp_sig_path = None
            if signatures and len(signatures) > 0:
                with tempfile.NamedTemporaryFile("w", delete=False, suffix=".txt", encoding="utf-8") as f:
                    temp_sig_path = f.name
                    for s in signatures:
                        sid = s.get("id", "SIG").strip()
                        sname = s.get("name", "Signature").strip()
                        spat = s.get("pattern", "").strip()
                        if spat:
                            f.write(f"{sid}|{sname}|{spat}\n")
                cli_args += ["--sig-file", temp_sig_path]
            elif not use_default_sigs:
                cli_args += ["--sig-file", str(BASE_DIR / "signatures.txt")]

            # If input text provided, pass as input text or write temporary file if large
            temp_input_path = None
            if len(input_text) > 500:
                with tempfile.NamedTemporaryFile("w", delete=False, suffix=".bin", encoding="utf-8") as f:
                    temp_input_path = f.name
                    f.write(input_text)
                cli_args += ["--input-file", temp_input_path]
            else:
                cli_args += ["--input-text", input_text]

            # Execute scanner
            try:
                result = run_scanner_process(cli_args)
                self.send_json_response(result)
            finally:
                if temp_sig_path and os.path.exists(temp_sig_path):
                    try: os.remove(temp_sig_path)
                    except: pass
                if temp_input_path and os.path.exists(temp_input_path):
                    try: os.remove(temp_input_path)
                    except: pass

        except Exception as e:
            print(f"[Bridge] Scan error: {e}")
            self.send_json_response({"error": str(e)}, status=500)

    def handle_post_run_test_suite(self):
        """Runs the 8 formal functional test cases from Section 6.1 through real scanner.exe."""
        try:
            tests = [
                {
                    "caseId": 1,
                    "name": "Exact Match",
                    "pattern": "CAT",
                    "input": "CAT",
                    "k": 1,
                    "expectedVerdict": "MATCHED",
                    "expectedMinErrors": 0,
                    "ruleTested": "Exact byte match (pos -> pos+1)"
                },
                {
                    "caseId": 2,
                    "name": "Substitution Mutation",
                    "pattern": "CAT",
                    "input": "CUT",
                    "k": 1,
                    "expectedVerdict": "MATCHED",
                    "expectedMinErrors": 1,
                    "ruleTested": "Substitution (inputByte != pattern[pos], errors+1)"
                },
                {
                    "caseId": 3,
                    "name": "Deletion Mutation",
                    "pattern": "CAT",
                    "input": "CT",
                    "k": 1,
                    "expectedVerdict": "MATCHED",
                    "expectedMinErrors": 1,
                    "ruleTested": "Deletion (epsilon-transition without consuming input)"
                },
                {
                    "caseId": 4,
                    "name": "Insertion Mutation",
                    "pattern": "CAT",
                    "input": "CAAT",
                    "k": 1,
                    "expectedVerdict": "MATCHED",
                    "expectedMinErrors": 1,
                    "ruleTested": "Insertion (byte consumed without advancing pattern pos)"
                },
                {
                    "caseId": 5,
                    "name": "Disjoint Negative Control",
                    "pattern": "CAT",
                    "input": "DOG",
                    "k": 1,
                    "expectedVerdict": "CLEAN",
                    "expectedMinErrors": None,
                    "ruleTested": "Negative rejection when edit distance > k"
                },
                {
                    "caseId": 6,
                    "name": "Edit Budget Exceeded (k=0)",
                    "pattern": "CAT",
                    "input": "CUT",
                    "k": 0,
                    "expectedVerdict": "CLEAN",
                    "expectedMinErrors": None,
                    "ruleTested": "Strict zero-budget constraint enforcement"
                },
                {
                    "caseId": 7,
                    "name": "Cache Transition Reuse",
                    "pattern": "CAT",
                    "input": "CATCATCAT",
                    "k": 1,
                    "expectedVerdict": "MATCHED",
                    "expectedMinErrors": 0,
                    "ruleTested": "Deterministic state ID reuse & cache hit verification"
                },
                {
                    "caseId": 8,
                    "name": "Polymorphic Malware Evasion Detection",
                    "pattern": "SUSPICIOUS_PAYLOAD",
                    "input": "SUSP1CIOUS_PAYLOAD",
                    "k": 1,
                    "expectedVerdict": "MATCHED",
                    "expectedMinErrors": 1,
                    "ruleTested": "Real-world evasion attack: Exact fails, Lazy DFA detects"
                }
            ]

            results = []
            all_passed = True

            for tc in tests:
                args = [
                    "--k", str(tc["k"]),
                    "--no-default-sigs",
                    "--custom-sig", f"TC{tc['caseId']}|{tc['name']}|{tc['pattern']}",
                    "--input-text", tc["input"],
                    "--trials", "1",
                    "--max-trace", "50"
                ]

                res = run_scanner_process(args)

                dfa_verdict = res["lazyDFA"]["verdict"]
                exact_verdict = res["comparison"]["exact"]["verdict"]
                nfa_verdict = res["comparison"]["directNFA"]["verdict"]

                matched_sigs = res.get("matchedSignatures", [])
                actual_min_errors = matched_sigs[0]["minErrors"] if matched_sigs else None

                # Validation logic per test case
                passed = (dfa_verdict == tc["expectedVerdict"])
                if tc["expectedMinErrors"] is not None and passed:
                    passed = (actual_min_errors == tc["expectedMinErrors"])

                if tc["caseId"] == 7:
                    # Test case 7 must specifically verify cacheHits > 0
                    cache_hits = res["lazyDFA"]["cacheHits"]
                    passed = passed and (cache_hits > 0)
                elif tc["caseId"] == 8:
                    # Test case 8 must prove Exact Match missed (CLEAN) while Lazy DFA detected (MATCHED)
                    passed = passed and (exact_verdict == "CLEAN") and (dfa_verdict == "MATCHED")

                if not passed:
                    all_passed = False

                results.append({
                    "caseId": tc["caseId"],
                    "name": tc["name"],
                    "pattern": tc["pattern"],
                    "input": tc["input"],
                    "k": tc["k"],
                    "ruleTested": tc["ruleTested"],
                    "expectedVerdict": tc["expectedVerdict"],
                    "expectedMinErrors": tc["expectedMinErrors"],
                    "actualMinErrors": actual_min_errors,
                    "dfaVerdict": dfa_verdict,
                    "exactVerdict": exact_verdict,
                    "nfaVerdict": nfa_verdict,
                    "cacheHits": res["lazyDFA"]["cacheHits"],
                    "cacheHitRate": res["lazyDFA"]["cacheHitRate"],
                    "transitionsComputed": res["lazyDFA"]["transitionsComputed"],
                    "dfaStatesGenerated": res["lazyDFA"]["dfaStatesGenerated"],
                    "dfaTimeUs": res["lazyDFA"]["executionTimeUs"],
                    "exactTimeUs": res["comparison"]["exact"]["executionTimeUs"],
                    "nfaTimeUs": res["comparison"]["directNFA"]["executionTimeUs"],
                    "passed": passed
                })

            summary = {
                "allPassed": all_passed,
                "totalTests": len(tests),
                "passedCount": sum(1 for r in results if r["passed"]),
                "failedCount": sum(1 for r in results if not r["passed"]),
                "testResults": results
            }
            self.send_json_response(summary)

        except Exception as e:
            print(f"[Bridge] Test suite error: {e}")
            self.send_json_response({"error": str(e)}, status=500)

def run_server():
    ensure_compiled()
    # Allow port reuse
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("", PORT), PolyMorphRequestHandler) as httpd:
        print(f"==============================================================")
        print(f"  PolyMorph Hunter Bridge Server Online")
        print(f"  Serving http://localhost:{PORT}")
        print(f"==============================================================")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nShutting down server.")

if __name__ == "__main__":
    run_server()
