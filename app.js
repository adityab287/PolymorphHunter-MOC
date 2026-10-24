/**
 * PolyMorph Hunter — Editorial Lab Workbench Application Logic
 * Strict Zero-Hardcoding: All data dynamically computed by C++17 engine.
 */

// Global Application State
const state = {
  signatures: [],
  testPresets: [],
  scanResult: null,
  verificationSuiteResult: null,
  currentStepIndex: 0,
  isPlaying: false,
  playTimer: null,
  graphTransform: { x: 40, y: 30, scale: 1.0 },
  isDraggingGraph: false,
  dragStart: { x: 0, y: 0 }
};

// Dynamic API base: Use http://localhost:8080 if loaded via file:// or alternative port (e.g. Live Server)
const API_BASE = (window.location.protocol === 'file:' || (window.location.port !== '8080'))
  ? 'http://localhost:8080'
  : '';

// DOM References
const elements = {
  // Masthead / Model Strip
  engineStatusTag: document.getElementById('engine-status-tag'),
  bridgeStatusTag: document.getElementById('bridge-status-tag'),
  modelQCount: document.getElementById('model-q-count'),
  modelKVal: document.getElementById('model-k-val'),

  // Config & Signatures
  signaturesTbody: document.getElementById('signatures-tbody'),
  btnAddSignature: document.getElementById('btn-add-signature'),
  btnResetSignatures: document.getElementById('btn-reset-signatures'),
  presetSelect: document.getElementById('preset-select'),
  inputTextStream: document.getElementById('input-text-stream'),
  streamByteCount: document.getElementById('stream-byte-count'),
  budgetKSelect: document.getElementById('budget-k-select'),
  trialsCountSelect: document.getElementById('trials-count-select'),
  btnRunScan: document.getElementById('btn-run-scan'),
  btnRunTable61: document.getElementById('btn-run-table61'),

  // Verdict & Telemetry
  scanStatusIndicator: document.getElementById('scan-status-indicator'),
  verdictBanner: document.getElementById('verdict-banner'),
  verdictTag: document.getElementById('verdict-tag'),
  verdictStatusText: document.getElementById('verdict-status-text'),
  verdictDetails: document.getElementById('verdict-details'),
  metricStatesGen: document.getElementById('metric-states-gen'),
  metricTransComp: document.getElementById('metric-trans-comp'),
  metricCacheHits: document.getElementById('metric-cache-hits'),
  metricHitRate: document.getElementById('metric-hit-rate'),
  metricHitRateSub: document.getElementById('metric-hit-rate-sub'),
  metricExecTime: document.getElementById('metric-exec-time'),
  metricThroughput: document.getElementById('metric-throughput'),
  matchesManifest: document.getElementById('matches-manifest'),

  // Trace Scrubber
  byteTape: document.getElementById('byte-tape'),
  scrubFirst: document.getElementById('scrub-first'),
  scrubPrev: document.getElementById('scrub-prev'),
  scrubPlay: document.getElementById('scrub-play'),
  scrubNext: document.getElementById('scrub-next'),
  scrubLast: document.getElementById('scrub-last'),
  playbackSpeed: document.getElementById('playback-speed'),
  stepCounterTag: document.getElementById('step-counter-tag'),

  // Step Inspector
  stepCacheStatusBadge: document.getElementById('step-cache-status-badge'),
  stepByteIdx: document.getElementById('step-byte-idx'),
  stepByteChar: document.getElementById('step-byte-char'),
  stepByteHex: document.getElementById('step-byte-hex'),
  stepDfaTransition: document.getElementById('step-dfa-transition'),
  stepAcceptedSigs: document.getElementById('step-accepted-sigs'),
  stepRawNfaStates: document.getElementById('step-raw-nfa-states'),
  stepEpsAddedStates: document.getElementById('step-eps-added-states'),
  stepCanonicalId: document.getElementById('step-canonical-id'),
  stepFullSubsetStates: document.getElementById('step-full-subset-states'),

  // Automaton Graph
  automatonSvg: document.getElementById('automaton-svg'),
  svgGraphLayer: document.getElementById('svg-graph-layer'),
  btnGraphResetZoom: document.getElementById('btn-graph-reset-zoom'),
  btnExportDot: document.getElementById('btn-export-dot'),

  // Comparison Table & Bars
  cmpExactVerdict: document.getElementById('cmp-exact-verdict'),
  cmpNfaVerdict: document.getElementById('cmp-nfa-verdict'),
  cmpDfaVerdict: document.getElementById('cmp-dfa-verdict'),
  cmpExactMatches: document.getElementById('cmp-exact-matches'),
  cmpNfaMatches: document.getElementById('cmp-nfa-matches'),
  cmpDfaMatches: document.getElementById('cmp-dfa-matches'),
  cmpExactStates: document.getElementById('cmp-exact-states'),
  cmpNfaEvals: document.getElementById('cmp-nfa-evals'),
  cmpDfaStates: document.getElementById('cmp-dfa-states'),
  cmpExactTrans: document.getElementById('cmp-exact-trans'),
  cmpDfaTrans: document.getElementById('cmp-dfa-trans'),
  cmpDfaHits: document.getElementById('cmp-dfa-hits'),
  cmpExactTime: document.getElementById('cmp-exact-time'),
  cmpNfaTime: document.getElementById('cmp-nfa-time'),
  cmpDfaTime: document.getElementById('cmp-dfa-time'),
  barFillExact: document.getElementById('bar-fill-exact'),
  barFillNfa: document.getElementById('bar-fill-nfa'),
  barFillDfa: document.getElementById('bar-fill-dfa'),
  barValExact: document.getElementById('bar-val-exact'),
  barValNfa: document.getElementById('bar-val-nfa'),
  barValDfa: document.getElementById('bar-val-dfa'),

  // Table 6.1 Suite
  suiteSummaryBadge: document.getElementById('suite-summary-badge'),
  btnReRunSuite: document.getElementById('btn-re-run-suite'),
  verificationSuiteTbody: document.getElementById('verification-suite-tbody'),

  // DOT Modal
  dotModalBackdrop: document.getElementById('dot-modal-backdrop'),
  btnCloseDotModal: document.getElementById('btn-close-dot-modal'),
  btnDismissDot: document.getElementById('btn-dismiss-dot'),
  btnCopyDot: document.getElementById('btn-copy-dot'),
  dotCodeArea: document.getElementById('dot-code-area')
};

// =========================================================================
// Initialization & Preset Loading
// =========================================================================

async function initializeApp() {
  try {
    elements.scanStatusIndicator.textContent = 'Loading presets...';
    const res = await fetch(`${API_BASE}/api/presets`);
    if (!res.ok) throw new Error(`HTTP error ${res.status}`);
    const data = await res.json();

    state.signatures = data.signatures || [];
    state.testPresets = data.testPresets || [];

    renderSignaturesTable();
    populatePresetsDropdown();

    // Default to CAT exact match preset
    if (state.testPresets.length > 0) {
      applyPreset(state.testPresets[0]);
    }

    elements.scanStatusIndicator.textContent = 'Engine Ready';
    elements.scanStatusIndicator.className = 'badge badge-success';

    // Auto-run initial scan and table 6.1 verification
    await executeScan();
    await executeVerificationSuite();

  } catch (err) {
    console.error('Initialization error:', err);
    elements.scanStatusIndicator.textContent = 'Bridge Offline';
    elements.scanStatusIndicator.className = 'badge badge-danger';
    elements.verdictBanner.className = 'verdict-banner verdict-matched';
    elements.verdictTag.textContent = 'BRIDGE ERROR';
    elements.verdictStatusText.textContent = 'CANNOT CONNECT TO LOCAL BRIDGE';
    elements.verdictDetails.textContent = `Ensure server.py is running on port 8080: ${err.message}`;
  }
}

// Populate Presets Dropdown
function populatePresetsDropdown() {
  elements.presetSelect.innerHTML = '<option value="" disabled selected>-- Choose Test / File Preset --</option>';
  state.testPresets.forEach((p, idx) => {
    const opt = document.createElement('option');
    opt.value = idx;
    opt.textContent = p.name;
    elements.presetSelect.appendChild(opt);
  });
}

function applyPreset(preset) {
  if (!preset) return;
  elements.inputTextStream.value = preset.inputText;
  elements.budgetKSelect.value = String(preset.k !== undefined ? preset.k : 1);
  updateByteCount();

  // If this preset has a dedicated pattern, check if signatures should adjust
  if (preset.id.startsWith('cat_')) {
    state.signatures = [
      { id: 'TEST_CAT', name: 'CAT Test Pattern', pattern: 'CAT' }
    ];
    renderSignaturesTable();
  }
}

// =========================================================================
// Signature Table Management
// =========================================================================

function renderSignaturesTable() {
  elements.signaturesTbody.innerHTML = '';
  if (state.signatures.length === 0) {
    elements.signaturesTbody.innerHTML = '<tr><td colspan="5" class="empty-hint" style="text-align: center; padding: 12px;">No signatures configured. Click "+ Add Signature" or "Reload".</td></tr>';
    return;
  }

  state.signatures.forEach((sig, idx) => {
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td><input type="text" class="form-input code-font select-xs sig-id-input" value="${escapeHtml(sig.id)}" data-idx="${idx}"></td>
      <td><input type="text" class="form-input select-xs sig-name-input" value="${escapeHtml(sig.name)}" data-idx="${idx}"></td>
      <td><input type="text" class="form-input code-font select-xs sig-pattern-input" value="${escapeHtml(sig.pattern)}" data-idx="${idx}"></td>
      <td class="code-font" style="text-align: center;">${sig.pattern.length}</td>
      <td style="text-align: center;">
        <button class="btn btn-xs btn-danger btn-remove-sig" data-idx="${idx}" title="Delete signature">&times;</button>
      </td>
    `;
    elements.signaturesTbody.appendChild(tr);
  });

  // Attach event listeners for inputs
  document.querySelectorAll('.sig-id-input').forEach(inp => {
    inp.addEventListener('input', e => {
      const idx = parseInt(e.target.dataset.idx, 10);
      state.signatures[idx].id = e.target.value.trim();
    });
  });

  document.querySelectorAll('.sig-name-input').forEach(inp => {
    inp.addEventListener('input', e => {
      const idx = parseInt(e.target.dataset.idx, 10);
      state.signatures[idx].name = e.target.value.trim();
    });
  });

  document.querySelectorAll('.sig-pattern-input').forEach(inp => {
    inp.addEventListener('input', e => {
      const idx = parseInt(e.target.dataset.idx, 10);
      state.signatures[idx].pattern = e.target.value;
      renderSignaturesTable(); // re-render to update length
    });
  });

  document.querySelectorAll('.btn-remove-sig').forEach(btn => {
    btn.addEventListener('click', e => {
      const idx = parseInt(e.target.dataset.idx, 10);
      state.signatures.splice(idx, 1);
      renderSignaturesTable();
    });
  });
}

function updateByteCount() {
  const text = elements.inputTextStream.value;
  elements.streamByteCount.textContent = `${text.length} bytes`;
}

// =========================================================================
// Scan Execution via Local Bridge & C++ Engine
// =========================================================================

async function executeScan() {
  stopPlayback();
  const k = parseInt(elements.budgetKSelect.value, 10);
  const trials = parseInt(elements.trialsCountSelect.value, 10);
  const inputText = elements.inputTextStream.value;

  elements.btnRunScan.disabled = true;
  elements.btnRunScan.querySelector('.btn-text').textContent = 'Computing Determinization...';
  elements.scanStatusIndicator.textContent = 'Scanning C++ Engine...';
  elements.scanStatusIndicator.className = 'badge badge-neutral';

  try {
    const payload = {
      k: k,
      trials: trials,
      inputText: inputText,
      signatures: state.signatures,
      maxTrace: 200
    };

    const res = await fetch(`${API_BASE}/api/scan`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });

    if (!res.ok) {
      const errData = await res.json().catch(() => ({}));
      throw new Error(errData.error || `HTTP ${res.status}`);
    }

    const data = await res.json();
    state.scanResult = data;

    renderScanResults(data);
    elements.scanStatusIndicator.textContent = 'Execution Complete';
    elements.scanStatusIndicator.className = 'badge badge-success';

  } catch (err) {
    console.error('Scan execution error:', err);
    elements.scanStatusIndicator.textContent = 'Scan Failed';
    elements.scanStatusIndicator.className = 'badge badge-danger';
    alert(`Engine Execution Error: ${err.message}`);
  } finally {
    elements.btnRunScan.disabled = false;
    elements.btnRunScan.querySelector('.btn-text').textContent = 'Execute Scan & Determinization';
  }
}

// Render Engine Results
function renderScanResults(data) {
  const { verdict, config, matchedSignatures, lazyDFA, trace, comparison } = data;

  // 1. Dynamic Formal Strip Updates
  elements.modelQCount.textContent = `${lazyDFA.dfaStatesGenerated} states`;
  elements.modelKVal.textContent = `${config.k} edit${config.k === 1 ? '' : 's'}`;

  // 2. Verdict Banner
  if (verdict === 'MATCHED') {
    elements.verdictBanner.className = 'verdict-banner verdict-matched';
    elements.verdictTag.textContent = 'VERDICT: MATCHED / DETECTED';
    elements.verdictStatusText.textContent = `MALWARE SIGNATURE MATCH FOUND (${matchedSignatures.length} DETECTIONS)`;
    
    const sigNames = matchedSignatures.map(m => `"${m.name}" [${m.id}] (min edits: ${m.minErrors}, offset: ${m.byteOffset})`).join(', ');
    elements.verdictDetails.textContent = `Polymorphic automaton accepted pattern within edit distance budget k=${config.k}. Matched: ${sigNames}`;
  } else {
    elements.verdictBanner.className = 'verdict-banner verdict-clean';
    elements.verdictTag.textContent = 'VERDICT: CLEAN';
    elements.verdictStatusText.textContent = 'NO MALWARE SIGNATURES DETECTED';
    elements.verdictDetails.textContent = `A CLEAN result confirms no configured signature matched within edit budget k=${config.k}. All input bytes were scanned without reaching an accepting state.`;
  }

  // 3. Telemetry Counters
  elements.metricStatesGen.textContent = lazyDFA.dfaStatesGenerated;
  elements.metricTransComp.textContent = lazyDFA.transitionsComputed;
  elements.metricCacheHits.textContent = lazyDFA.cacheHits;
  elements.metricHitRate.textContent = `${lazyDFA.cacheHitRate.toFixed(1)}%`;
  elements.metricHitRateSub.textContent = `${lazyDFA.cacheHits} / ${lazyDFA.cacheHits + lazyDFA.transitionsComputed} queries`;
  elements.metricExecTime.textContent = `${lazyDFA.executionTimeUs.toFixed(1)} \u03BCs`;

  const throughput = (lazyDFA.executionTimeUs > 0)
    ? (config.bytesScanned / (lazyDFA.executionTimeUs / 1000.0)).toFixed(1)
    : (config.bytesScanned * 1000).toFixed(1);
  elements.metricThroughput.textContent = `${throughput} B/ms`;

  // 4. Matched Signatures Manifest
  elements.matchesManifest.innerHTML = '';
  if (!matchedSignatures || matchedSignatures.length === 0) {
    elements.matchesManifest.innerHTML = '<span class="empty-hint">No matches detected. Clean stream within edit budget.</span>';
  } else {
    matchedSignatures.forEach(m => {
      const item = document.createElement('div');
      item.className = 'match-item';
      item.innerHTML = `
        <span class="match-sig-id">${escapeHtml(m.id)} (${escapeHtml(m.name)})</span>
        <span class="match-sig-details">Pattern: "${escapeHtml(m.pattern)}" &bull; Min Edits: <strong>${m.minErrors}</strong> &bull; Byte Offset: <strong>${m.byteOffset}</strong></span>
      `;
      elements.matchesManifest.appendChild(item);
    });
  }

  // 5. Comparison Table & Bars
  renderComparisonView(comparison);

  // 6. Interactive Byte Tape & Trace
  renderByteTape(trace);

  // 7. Automaton State Graph
  renderAutomatonGraph(lazyDFA.stateRegistryDump, lazyDFA.activeTransitionsDump);

  // Set scrubber to final step or 0
  if (trace && trace.length > 0) {
    setScrubberStep(trace.length - 1);
  } else {
    resetStepInspector();
  }
}

// Render Comparison View
function renderComparisonView(cmp) {
  if (!cmp) return;
  const { exact, directNFA, lazyDFA } = cmp;

  // Verdicts
  elements.cmpExactVerdict.textContent = exact.verdict;
  elements.cmpExactVerdict.className = exact.verdict === 'MATCHED' ? 'badge badge-danger' : 'badge badge-success';

  elements.cmpNfaVerdict.textContent = directNFA.verdict;
  elements.cmpNfaVerdict.className = directNFA.verdict === 'MATCHED' ? 'badge badge-danger' : 'badge badge-success';

  elements.cmpDfaVerdict.textContent = lazyDFA.verdict;
  elements.cmpDfaVerdict.className = (lazyDFA.verdict === 'MATCHED' ? 'badge badge-danger' : 'badge badge-success') + ' col-highlight';

  // Matched Counts
  elements.cmpExactMatches.textContent = `${exact.matchedCount} sigs`;
  elements.cmpNfaMatches.textContent = `${directNFA.matchedCount} sigs`;
  elements.cmpDfaMatches.textContent = `${lazyDFA.matchedCount} sigs`;

  // States
  elements.cmpExactStates.textContent = `${exact.statesGenerated} nodes`;
  elements.cmpNfaEvals.textContent = `${directNFA.stateEvaluations} evals`;
  elements.cmpDfaStates.textContent = `${lazyDFA.dfaStatesGenerated} states`;

  // Transitions
  elements.cmpExactTrans.textContent = `${exact.transitionsComputed} transitions`;
  elements.cmpDfaTrans.textContent = `${lazyDFA.transitionsComputed} (Misses)`;
  elements.cmpDfaHits.textContent = `${lazyDFA.cacheHits} Hits`;

  // Latencies
  elements.cmpExactTime.textContent = `${exact.executionTimeUs.toFixed(1)} \u03BCs`;
  elements.cmpNfaTime.textContent = `${directNFA.executionTimeUs.toFixed(1)} \u03BCs`;
  elements.cmpDfaTime.textContent = `${lazyDFA.executionTimeUs.toFixed(1)} \u03BCs`;

  // Latency Bars
  const maxTime = Math.max(exact.executionTimeUs, directNFA.executionTimeUs, lazyDFA.executionTimeUs, 0.001);
  const pExact = Math.max(5, (exact.executionTimeUs / maxTime) * 100);
  const pNfa = Math.max(5, (directNFA.executionTimeUs / maxTime) * 100);
  const pDfa = Math.max(5, (lazyDFA.executionTimeUs / maxTime) * 100);

  elements.barFillExact.style.width = `${pExact}%`;
  elements.barFillNfa.style.width = `${pNfa}%`;
  elements.barFillDfa.style.width = `${pDfa}%`;

  elements.barValExact.textContent = `${exact.executionTimeUs.toFixed(1)} \u03BCs`;
  elements.barValNfa.textContent = `${directNFA.executionTimeUs.toFixed(1)} \u03BCs`;
  elements.barValDfa.textContent = `${lazyDFA.executionTimeUs.toFixed(1)} \u03BCs`;
}

// =========================================================================
// Interactive Trace Scrubber & Byte Tape
// =========================================================================

function renderByteTape(trace) {
  elements.byteTape.innerHTML = '';
  if (!trace || trace.length === 0) {
    elements.byteTape.innerHTML = '<span class="empty-hint">No trace steps available.</span>';
    return;
  }

  trace.forEach((step, idx) => {
    const cell = document.createElement('div');
    cell.className = 'byte-cell';
    cell.dataset.stepIdx = idx;

    const isHit = step.cacheStatus === 'HIT';
    const isAccepting = step.acceptedSignatures && step.acceptedSignatures.length > 0;
    if (isAccepting) cell.classList.add('cell-accepting');

    cell.innerHTML = `
      <span class="byte-cell-idx">#${step.stepIndex}</span>
      <span class="byte-cell-char">${escapeHtml(step.byteChar)}</span>
      <span class="byte-cell-tag ${isHit ? 'tag-hit' : 'tag-comp'}">${isHit ? 'HIT' : 'COMP'}</span>
    `;

    cell.addEventListener('click', () => {
      stopPlayback();
      setScrubberStep(idx);
    });

    elements.byteTape.appendChild(cell);
  });
}

function setScrubberStep(idx) {
  if (!state.scanResult || !state.scanResult.trace || state.scanResult.trace.length === 0) return;
  const trace = state.scanResult.trace;
  const validIdx = Math.max(0, Math.min(idx, trace.length - 1));
  state.currentStepIndex = validIdx;

  // Update Byte Tape active class
  document.querySelectorAll('.byte-cell').forEach((cell, i) => {
    if (i === validIdx) {
      cell.classList.add('active-cell');
      cell.scrollIntoView({ behavior: 'smooth', block: 'nearest', inline: 'center' });
    } else {
      cell.classList.remove('active-cell');
    }
  });

  // Update counter tag
  elements.stepCounterTag.textContent = `Step ${validIdx + 1} / ${trace.length}`;

  // Update Step Detail Inspector
  const step = trace[validIdx];
  renderStepInspector(step);

  // Highlight graph state and transition
  highlightGraphStep(step.fromDFAState, step.toDFAState);
}

function renderStepInspector(step) {
  if (!step) return;

  const isHit = step.cacheStatus === 'HIT';
  elements.stepCacheStatusBadge.textContent = isHit ? 'CACHE HIT (Table Reused)' : 'COMPUTED (Cache Miss)';
  elements.stepCacheStatusBadge.className = isHit ? 'badge badge-brass' : 'badge badge-neutral';

  elements.stepByteIdx.textContent = step.stepIndex;
  elements.stepByteChar.textContent = `"${step.byteChar}"`;
  elements.stepByteHex.textContent = `${step.byteHex} (${step.byteAscii})`;
  elements.stepDfaTransition.textContent = `q${step.fromDFAState} \u2192 q${step.toDFAState}`;

  if (step.acceptedSignatures && step.acceptedSignatures.length > 0) {
    const accList = step.acceptedSignatures.map(s => `${s.id} (minErrors: ${s.minErrors})`).join(', ');
    elements.stepAcceptedSigs.textContent = accList;
    elements.stepAcceptedSigs.style.color = 'var(--status-matched)';
  } else {
    elements.stepAcceptedSigs.textContent = 'None (Non-accepting state)';
    elements.stepAcceptedSigs.style.color = 'var(--text-muted)';
  }

  // Pre-Closure Moves
  elements.stepRawNfaStates.innerHTML = '';
  if (step.rawNFASubset && step.rawNFASubset.length > 0) {
    step.rawNFASubset.forEach(st => {
      const tok = document.createElement('span');
      tok.className = 'state-token';
      tok.textContent = st;
      elements.stepRawNfaStates.appendChild(tok);
    });
  } else {
    elements.stepRawNfaStates.innerHTML = '<span class="empty-hint">None</span>';
  }

  // Epsilon-Closure Additions
  elements.stepEpsAddedStates.innerHTML = '';
  if (step.epsilonClosureAdded && step.epsilonClosureAdded.length > 0) {
    step.epsilonClosureAdded.forEach(st => {
      const tok = document.createElement('span');
      tok.className = 'state-token token-eps';
      tok.textContent = st;
      elements.stepEpsAddedStates.appendChild(tok);
    });
  } else {
    elements.stepEpsAddedStates.innerHTML = '<span class="empty-hint">0 additions (Fixed point)</span>';
  }

  // Canonical Deterministic Subset
  elements.stepCanonicalId.textContent = `DFA State q${step.toDFAState}`;
  elements.stepFullSubsetStates.innerHTML = '';
  if (step.fullNFASubset && step.fullNFASubset.length > 0) {
    step.fullNFASubset.forEach(st => {
      const tok = document.createElement('span');
      tok.className = 'state-token';
      if (step.acceptedSignatures && step.acceptedSignatures.length > 0) {
        // Highlight accepting state tuple if pos == pattern len
        tok.classList.add('token-accepting');
      }
      tok.textContent = st;
      elements.stepFullSubsetStates.appendChild(tok);
    });
  } else {
    elements.stepFullSubsetStates.innerHTML = '<span class="empty-hint">Empty State Subset</span>';
  }
}

function resetStepInspector() {
  elements.stepCounterTag.textContent = 'Step 0 / 0';
  elements.stepCacheStatusBadge.textContent = '—';
  elements.stepByteIdx.textContent = '—';
  elements.stepByteChar.textContent = '—';
  elements.stepByteHex.textContent = '—';
  elements.stepDfaTransition.textContent = '—';
  elements.stepAcceptedSigs.textContent = 'None';
  elements.stepRawNfaStates.innerHTML = '<span class="empty-hint">None</span>';
  elements.stepEpsAddedStates.innerHTML = '<span class="empty-hint">None</span>';
  elements.stepCanonicalId.textContent = 'q0';
  elements.stepFullSubsetStates.innerHTML = '<span class="empty-hint">None</span>';
}

function startPlayback() {
  if (!state.scanResult || !state.scanResult.trace) return;
  state.isPlaying = true;
  elements.scrubPlay.innerHTML = 'Pause &#10074;&#10074;';
  const speed = parseInt(elements.playbackSpeed.value, 10) || 300;

  state.playTimer = setInterval(() => {
    if (state.currentStepIndex >= state.scanResult.trace.length - 1) {
      stopPlayback();
    } else {
      setScrubberStep(state.currentStepIndex + 1);
    }
  }, speed);
}

function stopPlayback() {
  state.isPlaying = false;
  elements.scrubPlay.innerHTML = 'Play &blacktriangleright;';
  if (state.playTimer) {
    clearInterval(state.playTimer);
    state.playTimer = null;
  }
}

// =========================================================================
// Visual Automaton State Graph (SVG Engine)
// =========================================================================

function renderAutomatonGraph(states, transitions) {
  elements.svgGraphLayer.innerHTML = '';
  if (!states || states.length === 0) return;

  const nodeRadius = 22;
  const numStates = states.length;

  // Calculate layout coordinates
  // For small to medium automata (<= 25 states), lay out along multiple horizontal ranks
  const nodePositions = new Map();
  const rankWidth = 110;
  const rankHeight = 90;
  const maxCols = 6;

  states.forEach((st, i) => {
    const col = i % maxCols;
    const row = Math.floor(i / maxCols);
    // Slight offset on odd rows for organic visual flow
    const x = 50 + col * rankWidth + (row % 2) * 20;
    const y = 60 + row * rankHeight;
    nodePositions.set(st.dfaStateId, { x, y });
  });

  // Group transitions by from->to to combine multiple byte labels
  const edgeMap = new Map();
  if (transitions) {
    transitions.forEach(tr => {
      const key = `${tr.fromState}->${tr.toState}`;
      if (!edgeMap.has(key)) {
        edgeMap.set(key, {
          from: tr.fromState,
          to: tr.toState,
          labels: []
        });
      }
      const labels = edgeMap.get(key).labels;
      if (labels.length < 3 && !labels.includes(tr.byteChar)) {
        labels.push(tr.byteChar);
      }
    });
  }

  // Draw Edges (Path & Labels)
  edgeMap.forEach((edge, key) => {
    const p1 = nodePositions.get(edge.from);
    const p2 = nodePositions.get(edge.to);
    if (!p1 || !p2) return;

    const gEdge = document.createElementNS('http://www.w3.org/2000/svg', 'g');
    gEdge.setAttribute('id', `edge-${edge.from}-${edge.to}`);
    gEdge.classList.add('graph-edge-group');

    let d = '';
    let labelX = 0;
    let labelY = 0;

    if (edge.from === edge.to) {
      // Self loop
      const r = nodeRadius;
      d = `M ${p1.x - 10} ${p1.y - r} C ${p1.x - 25} ${p1.y - r - 35}, ${p1.x + 25} ${p1.y - r - 35}, ${p1.x + 10} ${p1.y - r}`;
      labelX = p1.x;
      labelY = p1.y - r - 22;
    } else {
      // Directed arc
      const dx = p2.x - p1.x;
      const dy = p2.y - p1.y;
      const dist = Math.hypot(dx, dy);
      const ux = dx / dist;
      const uy = dy / dist;

      // Start and end points on node perimeter
      const startX = p1.x + ux * nodeRadius;
      const startY = p1.y + uy * nodeRadius;
      const endX = p2.x - ux * (nodeRadius + 4);
      const endY = p2.y - uy * (nodeRadius + 4);

      // Check if reverse edge exists for curvature
      const reverseKey = `${edge.to}->${edge.from}`;
      const hasReverse = edgeMap.has(reverseKey);
      const curveOffset = hasReverse ? 20 : 0;

      const midX = (startX + endX) / 2 - uy * curveOffset;
      const midY = (startY + endY) / 2 + ux * curveOffset;

      d = `M ${startX} ${startY} Q ${midX} ${midY} ${endX} ${endY}`;
      labelX = midX;
      labelY = midY - 4;
    }

    const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    path.setAttribute('d', d);
    path.setAttribute('fill', 'none');
    path.setAttribute('stroke', '#A3A8A0');
    path.setAttribute('stroke-width', '1.5');
    path.setAttribute('marker-end', 'url(#arrowhead)');
    path.classList.add('edge-path');

    const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
    text.setAttribute('x', labelX);
    text.setAttribute('y', labelY);
    text.setAttribute('text-anchor', 'middle');
    text.setAttribute('font-family', 'JetBrains Mono, monospace');
    text.setAttribute('font-size', '10px');
    text.setAttribute('fill', '#656860');
    text.textContent = edge.labels.join(',');

    gEdge.appendChild(path);
    gEdge.appendChild(text);
    elements.svgGraphLayer.appendChild(gEdge);
  });

  // Draw State Nodes
  states.forEach(st => {
    const pos = nodePositions.get(st.dfaStateId);
    if (!pos) return;

    const gNode = document.createElementNS('http://www.w3.org/2000/svg', 'g');
    gNode.setAttribute('id', `state-node-${st.dfaStateId}`);
    gNode.setAttribute('cursor', 'pointer');
    gNode.classList.add('graph-state-node');

    // Outer double ring for accepting states
    if (st.isAccepting) {
      const ring = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
      ring.setAttribute('cx', pos.x);
      ring.setAttribute('cy', pos.y);
      ring.setAttribute('r', nodeRadius + 3.5);
      ring.setAttribute('fill', 'none');
      ring.setAttribute('stroke', '#A78352');
      ring.setAttribute('stroke-width', '1.5');
      gNode.appendChild(ring);
    }

    // Base Circle
    const circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
    circle.setAttribute('cx', pos.x);
    circle.setAttribute('cy', pos.y);
    circle.setAttribute('r', nodeRadius);
    circle.setAttribute('fill', '#FFFFFF');
    circle.setAttribute('stroke', st.isAccepting ? '#A78352' : '#C2BDAA');
    circle.setAttribute('stroke-width', '2');
    circle.classList.add('node-circle');
    gNode.appendChild(circle);

    // Label Text (q0, q1, ...)
    const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
    text.setAttribute('x', pos.x);
    text.setAttribute('y', pos.y + 4);
    text.setAttribute('text-anchor', 'middle');
    text.setAttribute('font-family', 'Newsreader, serif');
    text.setAttribute('font-size', '13px');
    text.setAttribute('font-weight', '600');
    text.setAttribute('fill', '#343831');
    text.classList.add('node-label');
    text.textContent = `q${st.dfaStateId}`;
    gNode.appendChild(text);

    // Node click inspector
    gNode.addEventListener('click', () => {
      alert(`DFA State q${st.dfaStateId}\nAccepting: ${st.isAccepting}\nUnderlying NFA Tuples (${st.nfaSubset.length}):\n${st.nfaSubset.join(', ')}`);
    });

    elements.svgGraphLayer.appendChild(gNode);
  });

  applyGraphTransform();
}

function highlightGraphStep(fromState, toState) {
  // Reset all state nodes
  document.querySelectorAll('.graph-state-node').forEach(node => {
    const circle = node.querySelector('.node-circle');
    const label = node.querySelector('.node-label');
    if (circle && label) {
      circle.setAttribute('fill', '#FFFFFF');
      label.setAttribute('fill', '#343831');
    }
  });

  // Reset all edges
  document.querySelectorAll('.edge-path').forEach(path => {
    path.setAttribute('stroke', '#A3A8A0');
    path.setAttribute('stroke-width', '1.5');
    path.setAttribute('marker-end', 'url(#arrowhead)');
  });

  // Highlight Current toState
  const activeNode = document.getElementById(`state-node-${toState}`);
  if (activeNode) {
    const circle = activeNode.querySelector('.node-circle');
    const label = activeNode.querySelector('.node-label');
    if (circle && label) {
      circle.setAttribute('fill', '#263A2E');
      label.setAttribute('fill', '#FFFFFF');
    }
  }

  // Highlight active edge
  const activeEdge = document.getElementById(`edge-${fromState}-${toState}`);
  if (activeEdge) {
    const path = activeEdge.querySelector('.edge-path');
    if (path) {
      path.setAttribute('stroke', '#263A2E');
      path.setAttribute('stroke-width', '2.5');
      path.setAttribute('marker-end', 'url(#arrowhead-active)');
    }
  }
}

function applyGraphTransform() {
  const { x, y, scale } = state.graphTransform;
  elements.svgGraphLayer.setAttribute('transform', `translate(${x}, ${y}) scale(${scale})`);
}

// Graph Pan & Zoom Handlers
elements.automatonSvg.addEventListener('mousedown', e => {
  state.isDraggingGraph = true;
  state.dragStart = { x: e.clientX - state.graphTransform.x, y: e.clientY - state.graphTransform.y };
});

window.addEventListener('mousemove', e => {
  if (!state.isDraggingGraph) return;
  state.graphTransform.x = e.clientX - state.dragStart.x;
  state.graphTransform.y = e.clientY - state.dragStart.y;
  applyGraphTransform();
});

window.addEventListener('mouseup', () => {
  state.isDraggingGraph = false;
});

elements.automatonSvg.addEventListener('wheel', e => {
  e.preventDefault();
  const zoomFactor = e.deltaY < 0 ? 1.1 : 0.9;
  state.graphTransform.scale = Math.max(0.4, Math.min(2.5, state.graphTransform.scale * zoomFactor));
  applyGraphTransform();
});

elements.btnGraphResetZoom.addEventListener('click', () => {
  state.graphTransform = { x: 40, y: 30, scale: 1.0 };
  applyGraphTransform();
});

// =========================================================================
// Graphviz DOT Export Generator
// =========================================================================

function generateGraphvizDOT() {
  if (!state.scanResult || !state.scanResult.lazyDFA) return '// No active scan result';
  const { stateRegistryDump, activeTransitionsDump } = state.scanResult.lazyDFA;

  let dot = 'digraph LazyDeterminizedDFA {\n';
  dot += '  rankdir=LR;\n';
  dot += '  size="8,5";\n';
  dot += '  node [shape=circle, fontname="Courier", style=filled, fillcolor="#FFFFFF", color="#656860"];\n';
  dot += '  edge [fontname="Courier", color="#343831"];\n\n';

  // Initial State pointer
  dot += '  __start [shape=none, label="", width=0, height=0];\n';
  dot += '  __start -> q0;\n\n';

  // Nodes & Accepting states
  stateRegistryDump.forEach(st => {
    if (st.isAccepting) {
      dot += `  q${st.dfaStateId} [shape=doublecircle, fillcolor="#FAF6ED", color="#A78352", label="q${st.dfaStateId}\\n${st.acceptedSignatures.join(',')}"];\n`;
    } else {
      dot += `  q${st.dfaStateId} [label="q${st.dfaStateId}"];\n`;
    }
  });

  dot += '\n';

  // Edges
  if (activeTransitionsDump) {
    activeTransitionsDump.forEach(tr => {
      dot += `  q${tr.fromState} -> q${tr.toState} [label="${tr.byteChar}"];\n`;
    });
  }

  dot += '}\n';
  return dot;
}

elements.btnExportDot.addEventListener('click', () => {
  const dot = generateGraphvizDOT();
  elements.dotCodeArea.value = dot;
  elements.dotModalBackdrop.style.display = 'flex';
});

elements.btnCloseDotModal.addEventListener('click', () => {
  elements.dotModalBackdrop.style.display = 'none';
});

elements.btnDismissDot.addEventListener('click', () => {
  elements.dotModalBackdrop.style.display = 'none';
});

elements.btnCopyDot.addEventListener('click', () => {
  navigator.clipboard.writeText(elements.dotCodeArea.value);
  elements.btnCopyDot.textContent = 'Copied to Clipboard!';
  setTimeout(() => {
    elements.btnCopyDot.textContent = 'Copy DOT to Clipboard';
  }, 1500);
});

// =========================================================================
// Report Table 6.1 Formal Verification Suite Runner
// =========================================================================

async function executeVerificationSuite() {
  elements.suiteSummaryBadge.textContent = 'Running 8 Tests...';
  elements.suiteSummaryBadge.className = 'badge badge-neutral';
  elements.btnReRunSuite.disabled = true;

  try {
    const res = await fetch(`${API_BASE}/api/run-test-suite`, { method: 'POST' });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const data = await res.json();
    state.verificationSuiteResult = data;

    renderVerificationSuiteTable(data);

    if (data.allPassed) {
      elements.suiteSummaryBadge.textContent = `All ${data.passedCount}/${data.totalTests} Tests Passed (100%)`;
      elements.suiteSummaryBadge.className = 'badge badge-success';
    } else {
      elements.suiteSummaryBadge.textContent = `${data.passedCount}/${data.totalTests} Passed (${data.failedCount} Failed)`;
      elements.suiteSummaryBadge.className = 'badge badge-danger';
    }

  } catch (err) {
    console.error('Test suite error:', err);
    elements.suiteSummaryBadge.textContent = 'Test Suite Error';
    elements.suiteSummaryBadge.className = 'badge badge-danger';
  } finally {
    elements.btnReRunSuite.disabled = false;
  }
}

function renderVerificationSuiteTable(data) {
  elements.verificationSuiteTbody.innerHTML = '';
  if (!data || !data.testResults) return;

  data.testResults.forEach(r => {
    const tr = document.createElement('tr');
    tr.innerHTML = `
      <td class="code-font" style="text-align: center; font-weight: 600;">${r.caseId}</td>
      <td style="font-weight: 500;">${escapeHtml(r.name)}</td>
      <td class="code-font">"${escapeHtml(r.pattern)}"</td>
      <td class="code-font">"${escapeHtml(r.input)}"</td>
      <td class="code-font" style="text-align: center;">${r.k}</td>
      <td style="font-size: 11px; color: var(--text-muted);">${escapeHtml(r.ruleTested)}</td>
      <td class="code-font">${r.expectedVerdict}</td>
      <td class="code-font" style="color: ${r.exactVerdict === 'MATCHED' ? 'var(--status-matched)' : 'var(--text-muted)'}">${r.exactVerdict}</td>
      <td class="code-font" style="color: ${r.nfaVerdict === 'MATCHED' ? 'var(--status-matched)' : 'var(--text-muted)'}">${r.nfaVerdict}</td>
      <td class="code-font col-highlight" style="color: ${r.dfaVerdict === 'MATCHED' ? 'var(--status-matched)' : 'var(--status-clean)'}; font-weight: 600;">${r.dfaVerdict}</td>
      <td class="code-font" style="text-align: center;">${r.cacheHits}</td>
      <td class="code-font" style="text-align: right;">${r.dfaTimeUs.toFixed(1)}</td>
      <td style="text-align: center;">
        <span class="badge ${r.passed ? 'badge-success' : 'badge-danger'}">${r.passed ? 'PASS' : 'FAIL'}</span>
      </td>
    `;
    elements.verificationSuiteTbody.appendChild(tr);
  });
}

// =========================================================================
// Event Listeners Binding
// =========================================================================

// Preset selection
elements.presetSelect.addEventListener('change', e => {
  const idx = parseInt(e.target.value, 10);
  applyPreset(state.testPresets[idx]);
});

// Input stream typing
elements.inputTextStream.addEventListener('input', updateByteCount);

// Run scan button
elements.btnRunScan.addEventListener('click', executeScan);

// Run Table 6.1 buttons
elements.btnRunTable61.addEventListener('click', () => {
  executeVerificationSuite();
  elements.panelTraceScrubber = document.getElementById('panel-verification-suite');
  elements.panelTraceScrubber.scrollIntoView({ behavior: 'smooth' });
});

elements.btnReRunSuite.addEventListener('click', executeVerificationSuite);

// Add signature button
elements.btnAddSignature.addEventListener('click', () => {
  const newId = `SIG00${state.signatures.length + 1}`;
  state.signatures.push({
    id: newId,
    name: 'New Custom Signature',
    pattern: 'NEW_PATTERN'
  });
  renderSignaturesTable();
});

// Reset signatures to disk contents
elements.btnResetSignatures.addEventListener('click', async () => {
  const res = await fetch('/api/presets');
  if (res.ok) {
    const data = await res.json();
    state.signatures = data.signatures || [];
    renderSignaturesTable();
  }
});

// Scrubber Stepper buttons
elements.scrubFirst.addEventListener('click', () => {
  stopPlayback();
  setScrubberStep(0);
});

elements.scrubPrev.addEventListener('click', () => {
  stopPlayback();
  setScrubberStep(state.currentStepIndex - 1);
});

elements.scrubPlay.addEventListener('click', () => {
  if (state.isPlaying) {
    stopPlayback();
  } else {
    startPlayback();
  }
});

elements.scrubNext.addEventListener('click', () => {
  stopPlayback();
  setScrubberStep(state.currentStepIndex + 1);
});

elements.scrubLast.addEventListener('click', () => {
  stopPlayback();
  if (state.scanResult && state.scanResult.trace) {
    setScrubberStep(state.scanResult.trace.length - 1);
  }
});

// HTML escaping utility
function escapeHtml(str) {
  if (str === null || str === undefined) return '';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#039;');
}

// Start application
window.addEventListener('DOMContentLoaded', initializeApp);
