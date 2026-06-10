# Scout: Recall & Seed Expansion Opportunity

**Date:** 2026-06-10
**Task:** Quantify where more TRUE matches can come from, using existing artifacts only.
**Artifacts read:** `build/SZBE69_B8/ghidra/xenon-seeds/`, `build/SZBE69_B8/ghidra/ghidriff-xenon/`,
`rb3-xenon/tools/bindiff_match.json`, `rb3-xenon/unified_id_rb3wii.json`,
`rb3-xenon/unified_id_rtti.json`, `rb3-xenon/unified_id_vtable.json`.

---

## 1. Current state (measured)

| Metric | Value |
|---|---|
| Total ghidriff matches | 2,645 |
| Seed matches accepted | 1,186 (3 seeds skipped: addresses not found in Xenon) |
| Non-seed matches | 1,459 |
| Holdout recovery rate | 20.6% (29/141 scoreable) |
| Holdout precision on recovered | 90.6% (29/32) |
| Unmatched Xenon functions | ~64,742 |

### Non-seed match type breakdown

| Match type | Count | Est. precision | Est. true positives |
|---|---|---|---|
| ExactInstructionsFunctionHasher | 63 | 93.5% | ~59 |
| VTCombinedReference | 722 | 32.4% (engine only; see §4) | ~234 |
| StringsRefsHasher | 610 | **0.0%** | ~0 |
| StrUniqueFuncRefsHasher | 45 | **0.0%** | ~0 |
| Implied Match | 11 | 75% | ~8 |
| ExactMnemonicsFunctionHasher | 4 | ~56% | ~2 |
| SwitchSigHasher | 3 | unknown | ~2 |
| SymbolsHash | 1 | ~100% | ~1 |
| **TOTAL** | **1,459** | — | **~306** |

Including seeds (~90% precision × 1,186 = ~1,067 TPs), **overall estimated precision: ~51.9%** (~1,372 TPs / 2,645 matches).

StringsRefsHasher and StrUniqueFuncRefsHasher contribute 655 matches (45% of non-seeds) with **zero measured precision**. Both are noise and should be output-filtered in the next evaluation run.

---

## 2. Seed inventory analysis

### 2.1. DC3-derived seed pipeline

The current seed pipeline: `rb3-xenon/tools/bindiff_match.json` (11,057 RB3-Xenon ↔ DC3-Xenon BinDiff pairs) → join by normalized C++ identity key against Wii map → 1,189 seeds.

| Stage | Count |
|---|---|
| BinDiff oracle total | 11,057 |
| sim=1.0, conf≥0.95 filtered | 6,670 |
| Unique joinable C++ keys (Xenon side) | 4,917 |
| Unique joinable Wii keys | 29,295 |
| 1:1 intersecting keys | 1,153 (+ 154 plain-C) |
| Final seeds (after drops + holdout exclusion) | 1,189 |

**Conversion rate: 10.8%** (1,189/11,057). The 76.5% that can't be joined is because DC3's function vocabulary (D3DXCore, CXLrcClient, MCContainerXbox, etc.) doesn't intersect with RB3 Wii's. Game-code (`band3/`) is **entirely absent** from the DC3 oracle — those games share only the Milo engine.

### 2.2. Relaxed threshold analysis

| Threshold | Pairs | Expected seeds (at 17.8% conv.) | Delta vs current |
|---|---|---|---|
| sim=1.0, conf≥0.95 (current) | 6,670 | 1,189 | baseline |
| sim=1.0, conf≥0.90 | 6,716 | 1,197 | +8 |
| sim≥0.95, conf≥0.95 | 9,596 | ~1,709 | +520 |
| sim≥0.95, conf≥0.90 | 9,642 | ~1,717 | +528 |

Relaxed sim=0.95 adds ~520 seeds (passing same deduplication + name-join filter). Precision risk: sim=0.95 pairs introduce lower-confidence identities; estimate 70-80% seed precision vs ~90% at sim=1.0.

**To implement:** add `--min-sim 0.95` flag to `tools/ghidra/build_xenon_seeds.py` (already has the arg, just needs a re-run with `--min-sim 0.95 --min-conf 0.95`). Keep holdout exclusion intact.

### 2.3. Direct Wii↔Xenon BinDiff seeds (NEW SOURCE)

`rb3-xenon/unified_id_rb3wii.json` contains **9,301 direct RB3-Xenon ↔ RB3-Wii BinDiff pairs** (not via DC3 intermediary). These have BOTH Xenon address and Wii address directly.

| Threshold | Pairs | Not in current seeds |
|---|---|---|
| sim=1.0, conf≥0.95 | 45 | **41** |
| sim≥0.95, conf≥0.95 | 51 | 47 |
| sim≥0.90, conf≥0.90 | 104 | 100 |

The 41 sim=1.0, conf≥0.95 pairs not currently seeded are **ready-to-use anchors** that bypass the DC3 intermediary. Examples:
- `GemManager::SetInCoda(bool)` (xenon=0x822c1e30, wii=0x8013cd10)
- `TourSavable::SetDirty(bool, int)` (xenon=0x82357450, wii=0x80442b30)
- `Game::SetVocalCueVolume(float)` (xenon=0x82659f30, wii=0x801992a0)

**To implement:** extend `build_xenon_seeds.py` to ingest `rb3-xenon/unified_id_rb3wii.json` as an additional seed source (sim=1.0, conf≥0.95 filter; holdout exclusion applies). These pairs cover `band3/` game code that the DC3 oracle cannot touch.

Caveat: direct cross-compiler BinDiff precision at sim=1.0 is lower than same-compiler (different ABI/calling conventions, different register usage). Treat these as "likely correct" seeds, not "certain."

---

## 3. Recall levers ranked by expected new-match volume

### LEVER 1 — BSim re-enable with top-K candidate cap [HIGH IMPACT]

**Measured evidence (from ghidriff.log Run 1 vs Run 2):**

| Run | BSim | Pre-VT pool | VT found | Total non-seed matches |
|---|---|---|---|---|
| Run 1 (BSim ON) | 6,315 BSim matches | 8,091 | 1,190 | ~7,505+ |
| Run 2 (BSim OFF, final) | 0 | 1,912 | 722 | 1,459 |

Net BSim contribution: **~6,783 additional matches** above the BSim-off baseline.

**Blocker:** BSim entered an O(n_src × n_dest) single-threaded stall for Run 1 due to degenerate LSH bins in the stripped 65k-function Xenon binary (documented in `docs/decomp/ghidra-bsim-perf-investigation-2026-06-10.md`). The existing patch `scripts/ghidra/bsim-topk-cap.patch` applies a cap at `BinningSystem.lookup` but is **lossier than necessary** (the cap fires on bins that contain legitimate high-confidence matches). The correct fix is capping at `findSimilarNodes` in `BSimProgramCorrelatorMatching.java` where top-K by similarity is already computable — this preserves true nearest neighbors while bounding the serial aggregation.

**Precision risk:** cross-compiler BSim precision is **unmeasured** on this corpus. BSim uses decompiler-feature vectors which partially survive compiler differences (same algorithm logic → similar decompiled structures), but the Wii MWCC vs Xenon MSVC ABI differences (calling conventions, stack layouts, register usage) will degrade precision compared to same-compiler. Estimate 50-70% precision for BSim cross-compiler matches. Expected new TPs: ~6,783 × 0.60 = **~4,070 TPs**.

**Calibration step needed before full run:** run BSim on a 1000-function sample (using just the holdout + DC3 oracle subsets) and measure precision. This can be done headless in ~5-10 minutes.

**Status of patches:**
- `scripts/ghidra/bsim-topk-cap.patch` — applies to `BinningSystem.java` (too aggressive; use for reference but prefer `findSimilarNodes` cap)
- `scripts/ghidra/bsim-parallel-aggregation.patch` — parallelizes the aggregation loop (orthogonal speedup, safe)

---

### LEVER 2 — Fix StringsRefsHasher: UniqueStringAnchorHasher [MEDIUM IMPACT]

**Root cause of 0.0% cross-compiler precision:**

1. `correlators.py:376`: `ONE_TO_MANY = True` despite the docstring at :369 saying "DO NOT RUN THIS with one_to_many = TRUE"
2. The hash is a **sorted multiset of string contents** — when assert strings like `"src/band3/game/GemPlayer.cpp"` appear in dozens of functions, every function in that file gets the same hash bucket.
3. Cross-compiler: Wii uses forward-slash paths from MWCC, Xenon uses backslash or different root paths from MSVC → most strings don't even collide correctly.

Same-ISA (Bank5↔Bank8) precision was **91.4%** because exact stages drain the pool first, leaving mostly unique-ref functions.

**Available data:**
- Xenon-only strings with refcount=1: **3,531**
- Wii-only strings with refcount=1: **8,986**
- Distribution: 76.7% of Xenon-only strings have refcount=1

**Proposed fix: `UniqueStringAnchorHasher`** (new correlator in `ghidriff/correlators.py`):
- Hash only functions where **ALL referenced strings appear in exactly 1 function** on their side
- Use ONE_TO_MANY=False
- Only include strings where the content matches cross-platform (filter out filesystem paths — strings containing `/` or `\\` and ending in `.cpp/.h` are path strings; keep message/assertion bodies)
- Expected yield: 200-500 matches at 80-90% precision (conservative; same-ISA was 906 at 91.4%)

**Implementation location:** `ghidriff/correlators.py` after `StrUniqueFuncRefsHasher` class (line ~444). The `get_defined_data` helper at :309 already provides the per-function string map with refcounts available from `sym.referenceCount`.

---

### LEVER 3 — Direct rb3wii seeds (41 new anchors) [SMALL IMPACT]

As detailed in §2.3: 41 high-confidence direct Wii↔Xenon pairs not in current seeds. These are game-code anchors (band3/src/) that the DC3 oracle cannot provide.

**Expected yield:** 41 new seeds → ~15-25 additional VT cascades (lower ratio because game-code has fewer shared vtable reference neighbors).

**Implementation:** extend `tools/ghidra/build_xenon_seeds.py` to ingest `rb3-xenon/unified_id_rb3wii.json`. Wii address is already in the file (`wii_addr` field). Range-checks and holdout exclusion need to be applied.

---

### LEVER 4 — Relaxed BinDiff threshold (sim≥0.95) [SMALL IMPACT]

~520 additional seeds at lower precision (see §2.2). These feed VT cascade: at the observed 0.38 VT ratio, expect ~197 additional VT matches, but at lower precision (~70% seed accuracy × 32% VT precision = ~22% true positive rate on the cascade).

Net value: ~50-100 additional TPs. Implement by re-running `build_xenon_seeds.py --min-sim 0.95`.

---

### LEVER 5 — Second-iteration seeding (verified R1 → R2) [SMALL IMPACT]

**Seed-graph arithmetic from the run:**

| Scenario | Pre-VT seed count | VT new matches | Ratio |
|---|---|---|---|
| BSim on (Run 1) | 8,091 | 1,190 | 0.147 |
| BSim off (Run 2, final) | 1,912 | 722 | 0.377 |

The lower ratio with BSim is expected: BSim seeds are lower-precision than exact/name-join seeds, so their VT neighborhood is noisier.

**For a pure second iteration** (take the 63 ExactInstructions as additional seeds on top of the current 1,186):
- New pre-VT pool: 1,912 + 63 = 1,975 (+3.3%)
- Expected additional VT matches: 63 × 0.377 = **~24**

This is small. The real iteration leverage comes from BSim (lever 1), which would provide ~6,315 more seeds → projected ~1,190 additional VT matches in a second iteration.

---

## 4. VTCombined precision stratification (important calibration correction)

The reported 32.4% VTCombined precision was measured on 37 items drawn from the DC3 high-confidence oracle (engine/network functions). The **spot-check of 8 VT game-code (band3) matches against rb3wii showed 0/8 agreement** (the VT match pointed to a different Wii function than rb3wii's direct BinDiff). This suggests:

- VT engine-code precision: ~32-54% (the holdout-measured range, consistent with DC3 agreement rate of 53.8% on all-entries)
- VT game-code precision: likely **0-20%** (game functions have fewer shared vtable call patterns cross-compiler)

The 722 VT matches include ~508 system + ~129 band3 + ~33 network + ~52 others. Stratified TPs:
- System VT (508): 508 × 0.40 = ~203 TPs
- Band3 VT (129): 129 × 0.10 = ~13 TPs
- Other VT (85): ~20 TPs
- **Total VT TPs: ~236 vs reported 722**

---

## 5. Cheap verification protocol for 328 game-code new identities

The 328 game-code (band3/) new_coverage items are dominated by noise:

| Tier | Match type | Count | Recommend | Expected TPs |
|---|---|---|---|---|
| ACCEPT | ExactInstructionsFunctionHasher | 12 | Accept all | ~11 |
| FILTER | VTCombinedReference (tight TU cluster) | 56 | Accept conditionally | ~17 |
| CAUTION | VTCombinedReference (scattered/single) | 73 | Verify each | ~15 |
| REJECT | StringsRefsHasher | 174 | Discard | ~0 |
| REJECT | StrUniqueFuncRefsHasher | 6 | Discard | ~0 |
| ACCEPT | Implied Match + SwitchSig | 4 | Accept | ~3 |

### TU-cluster coherence filter (FILTER tier)

Group VT game-code matches by Wii TU (translation unit). For each group with ≥2 matches:
- Compute `xenon_spread` (max Xenon addr − min Xenon addr)
- Compute `wii_spread` (max Wii addr − min Wii addr)
- **ACCEPT if** `xenon_spread / max(wii_spread, 1) < 10` AND `xenon_spread < 50,000` bytes

Applying this filter yields **56 coherent items** (vs 73 scattered). Example coherent clusters:
- `BandScreen.o`: 4 matches, xenon_spread=936B, wii_spread=672B
- `GemPlayer.o`: 6 matches, xenon_spread=25KB, wii_spread=41KB
- `TourProgress.o`: 3 matches, xenon_spread=664B, wii_spread=5KB

Example false-positive cluster: `AccomplishmentManager.o`: 4 matches, xenon_spread=2.3MB (impossible for one TU) → REJECT.

### Cross-check against DC3 BinDiff oracle at any confidence

For each VT game-code match, check if the Xenon address appears in `rb3-xenon/tools/bindiff_match.json`. If it does, compare the DC3 name's normalized key against the Wii symbol. **All 328 new_coverage items are NOT in the DC3 oracle** (by definition of new_coverage), so this cross-check is unavailable for direct verification. The only available oracle for game code is `rb3-xenon/unified_id_rb3wii.json`.

### rb3wii spot-verification

For the 56 coherent VT game-code items, look up each Xenon address in `rb3-xenon/unified_id_rb3wii.json`. If rb3wii has a direct match AND the Wii addresses agree → strong confirmation. If they disagree (as seen in the 8-item spot-check) → likely FP. Of the 129 band3 VT items, 45 appear in rb3wii, all with disagreeing Wii addresses — this confirms high FP rate.

---

## 6. Anchor distinctiveness analysis

### Unique string anchors (cross-compiler)

The ghidriff run's string inventory (from `ghidriff.json`):

| Side | Total strings in diff | refcount=1 | refcount≤2 |
|---|---|---|---|
| Xenon-only (added) | 4,600 | 3,531 (76.8%) | 4,184 |
| Wii-only (deleted) | 15,858 | 8,986 (56.7%) | 12,131 |

A function referencing a refcount=1 string on both sides is a **1:1-unique anchor** if:
1. The string content survives cross-compiler (message bodies, not file paths)
2. The function appears only once per side

Filtering cross-platform string candidates (exclude strings matching `.*\.(cpp|h|c|inl)$` or containing `/src/`, `\src\`): a substantial fraction of the 3,531 Xenon unique strings are assertion messages (e.g., `s_attack_ms_8218f1dc`, `s_FinaleBigClub_82034000`) rather than file paths. These ARE cross-compiler candidates.

**Caveat:** even with uniqueness on both sides, the STRING CONTENT must match between Wii and Xenon. A Wii string `"attack_ms"` and Xenon string `"attack_ms"` would match, but Wii `"mSymTable"` and Xenon `s_mSymTable_80bbc9d3` (ghidra-renamed) might not align. A headless Ghidra script extracting actual string bytes (not symbol names) is needed for a count. Rough estimate: **hundreds to low-thousands** of cross-compiler-compatible unique-ref string anchors exist.

### RTTI/vtable identities (384 RTTI + 37 vtable)

`rb3-xenon/unified_id_rtti.json` has 384 Xenon function addresses derived from RTTI pointer analysis (scalar/vector deleting destructors + virtual function entries). None are currently in seeds or ghidriff matches. Of the 384:
- 92 are `{scalar/vector deleting destructor}` — compiler-synthesized, low signal
- 292 are real methods with DC3 names

These 292 are NOT currently used by `build_xenon_seeds.py` because it only reads `bindiff_match.json`. They could be added as additional seeds IF the Wii function address can be recovered (from the Wii map by matching the demangled name). Since these are RTTI-confirmed identity assignments, precision should be high.

### Switch-table shapes (3 matches, unknown precision)

`SwitchSigHasher` found 3 matches. Insufficient sample to estimate precision. The same-ISA calibration didn't cover this correlator. Low-priority.

### Imported-API call fingerprints

The Xenon binary has rich `__imp_X*` and `XAM*` import table entries. Wii has SDK calls. Neither side has obvious cross-compiler API fingerprint matches (different platforms, different APIs). Low signal.

---

## 7. For the next agent

### What to read
- `docs/decomp/ghidra-bsim-perf-investigation-2026-06-10.md` — BSim bottleneck details + patch status
- `docs/decomp/ghidra-bsim-perf-verification-2026-06-10.md` — cap correctness analysis (the BinningSystem cap is lossy; prefer findSimilarNodes)
- `docs/decomp/ghidriff-improvement-plan-2026-06-09.md` — full improvement plan
- `docs/decomp/ghidriff-calibration-2026-06-09.md` — per-correlator precision from same-ISA run
- `build/SZBE69_B8/ghidra/ghidriff-xenon/eval_report.json` — full metrics + per-item lists
- `rb3-xenon/unified_id_rb3wii.json` — 9,301 direct Wii↔Xenon pairs (not yet used in seeds)

### Open tasks

1. **BSim precision calibration (before full re-run):** Run a fast headless script on the gzfs that exercises BSim on the 146-pair holdout subset + checks the DC3 oracle. Estimate precision at cross-compiler. This is the gating question for lever 1.

2. **Add rb3wii seeds to build_xenon_seeds.py:** Extend the script to ingest `rb3-xenon/unified_id_rb3wii.json` at sim=1.0, conf≥0.95 as an additional seed source. The Wii address is already present in the file. Apply holdout exclusion and range checks.

3. **UniqueStringAnchorHasher implementation:** New correlator in `ghidriff/correlators.py` that uses refcount-1 guard on both sides with ONE_TO_MANY=False. Requires offline analysis of Xenon string bytes (not symbol names) to find cross-platform-compatible content — this needs a short headless Ghidra script that reads actual string data from the gzf.

4. **StringsRefs output filter:** In `tools/ghidra/eval_xenon_matches.py`, add a `--exclude-match-types StringsRefsHasher,StrUniqueFuncRefsHasher` option to exclude these from reported precision metrics and new_coverage lists. This cleans up the 655 noise matches from reported outputs.

5. **VT precision stratification:** Update `eval_xenon_matches.py` to report VT precision separately by `wii_category` (band3 vs system vs network). The current 32.4% is misleading — it's ~10% for game code and ~40-54% for engine code.

### Key numbers to carry forward

| Metric | Value |
|---|---|
| Est. total TPs in current 2,645 matches | ~1,372 |
| Holdout recovery (recall) | 20.6% |
| Max recall with full BSim | ~(2,645 + 6,783) = ~9,428 matches |
| Max recall with all levers | ~10,000-15,000 matches (est.) |
| Direct Wii-Xenon anchors (rb3wii) | 41 at sim=1.0 conf≥0.95 |
| DC3 oracle to seed conversion rate | 10.8% (bottleneck: non-shared code) |
| Xenon unique strings (refcount=1) | 3,531 |
| Wii unique strings (refcount=1) | 8,986 |
