# ghidriff improvement plan — Bank5↔Bank8 + the Wii→Xenon prize (2026-06-09)

Source review of `../ghidriff` (upstream clearbluejar/ghidriff @ `4f5d895`, ~5k LOC Python) against
two use cases: (1) Bank5↔Bank8 rename recovery (currently 162 port-safe of a nominal 35k pool), and
(2) **cross-compiler symbol porting Wii (MWCC/Gekko) → rb3-xenon (MSVC/Xenon)** — same game, where
the existing Wii→Xenon BinDiff oracle is only **0.32 precision** and DC3's 11,057-name oracle covers
engine code but **cannot name `band3/` game code**. (2) is the prize.

See also: `ghidra-vt-handoff-2026-06-09.md` (VT Tier-1/Tier-2 fixes), memory
`project_ghidriff_divergence_index.md`, rb3-xenon's `unified_id_rb3wii.json` work.

## How ghidriff matches (architecture, from source)

`VersionTrackingDiff.find_matches` (`version_tracking_diff.py:62-81`) runs a fixed cascade, each
stage removing accepted matches from the pool: SymbolsHash (exact name, ≥3 chars) → ExactBytes →
ExactInstructions → StructuralGraphExact (CFG triple) → ExactMnemonics → [BSim, seeded by the exact
stages] → BulkInstructions → SigCallingCalled → StringsRefs → StrUniqueFuncRefs → SwitchSig →
StructuralGraph-fuzzy (one-to-many, name in hash) → **BulkBasicBlockMnemonic (one-to-many)** →
**Implied matches** (`implied_matches.py:117-205`, call-graph tracing from matched neighbors) →
Decomp exact-text match (`decomp_correlate.py`).

Key properties found in source:

- **Implied matches carry NO similarity score and pass NO gate** (`implied_matches.py:200-204`).
  The only post-hoc filter is `ratio==0.0 and blocks_ratio==0.0` → drop, at report time
  (`ghidra_diff_engine.py:1722-24`). Everything else lands in output.
- All ghidriff's own correlators are **hash-equality** (no scored/fuzzy accept). It does NOT use
  Ghidra VT's *scored* correlators (LSH function-reference correlator — the one we just parallelized
  3-4×, duplicate-function, combined-reference). BSim is the only similarity stage, off by default
  history aside, and **we ran with `--no-bsim`**.
- Correlator selection/order/thresholds are NOT CLI-tunable; the engine is subclass-friendly
  (override `find_matches`). Reported "similarity" in output = post-hoc difflib ratios
  (`ghidra_diff_engine.py:1688-1769`), not correlator confidence.
- Cross-language diff (Gekko vs Xenon) raises unless `--force-diff` (`ghidra_diff_engine.py:1462`).
  Nothing else in the matchers is x86-specific; `bl` is recognized as call (`correlators.py:56,97`).

## What our 2026-06-09 Bank5↔Bank8 run actually did (ghidriff.log)

Invocation (`tools/ghidra/run_ghidriff.sh`): `--engine VersionTrackingDiff --force-diff --no-bsim
--min-func-len 4 --no-symbols`. Ghidra 12.1 DEV, diff_time ≈ 3h20m (implied-match phase dominated,
p1/p2 pool 12,665 each).

Match-type counts (log Counter): `BulkBasicBlockMnemonicHash` **11,136,884** (pathological — sorted
mnemonic multisets collide across tiny PPC getter/setter stubs; one-to-many explodes),
`Implied Match` 5,686 (ungated), SymbolsHash 5,675 (crippled by `--no-symbols` — true same-name pool
is 41,655), ExactInstructions 3,233, StringsRefs 906, ExactBytes 886, StructuralGraphExact 587,
rest <200. Rename candidates after `distill_ghidriff.py`: 4,659, of which **89% Implied Match**
(avg m_ratio 0.43, 35% near-zero) → only 162 port-safe. Distill's TRUST verdict also mislabels
8-byte byte-collision stubs (ExactBytes ⇒ m_ratio 1.0 ⇒ TRUST despite `base_match=false`).

Root causes of the noise, ranked:
1. Ungated implied matches (no score exists to gate on).
2. BulkBasicBlockMnemonic one-to-many on a stub-heavy MWCC C++ binary.
3. No scored correlator in the pipeline at all (BSim off; VT reference correlators unused) — so
   between "exact hash" and "implied guess" there is nothing.
4. `--min-func-len 4` admits the 8-byte stub ocean into every stage.

## The unused calibration oracle (do this first, zero re-run)

The **41,655 same-name Bank5↔Bank8 pairs are ground truth for name-blind matching**, and our
existing run matched (mostly) name-blind. `distill_ghidriff.py` already joins matches to both
CodeWarrior maps by address. Therefore a small script over the existing `divergence_index.json` +
maps can compute, **per correlator type**: precision = P(same mangled symbol | matched by that
correlator), stratified by function size. No Ghidra run needed.

This table is the decision input for everything below: it tells us which correlators can be trusted
at what size threshold on *this exact codebase/compiler family* before we point the pipeline at
Xenon, where ground truth is scarce (146 validated pairs).

## Track 1 — Bank5↔Bank8 quick wins (config + small fork patches)

| # | Change | Where | Expected effect |
|---|---|---|---|
| 1a | Re-run with `--bsim` (and consider `--bsim-full`), `--min-func-len 16+` | run_ghidriff.sh | First scored correlator in the loop; BSim is built for "same body, new name". /opt has the BSim feature. |
| 1b | Disable `BulkBasicBlockMnemonicHash` (+ fuzzy StructuralGraph one-to-many) for this corpus | fork: `--correlators` selection flag in `version_tracking_diff.py:62-81` | Kills the 11.1M-tag explosion; big runtime cut. |
| 1c | Gate implied matches: compute cheap mnemonic/blocks ratio at accept time, `--implied-min-ratio` (default keeps old behavior) | fork: `implied_matches.py:190-204` | Kills ~89% of rename noise at the source. **Upstreamable** — clear bug-fix shape. |
| 1d | Fix distill TRUST mislabel: require `base_match or len>=N` before ExactBytes ⇒ TRUST | `tools/ghidra/distill_ghidriff.py:85-102` | Honest verdicts on stub collisions. |

Honest expectation: most of the 35k Bank8-only symbols are genuinely new/changed-signature code
(2009→2010), not renames. Realistic recovery with 1a-1c is maybe high-hundreds to low-thousands of
additional safe pairs — useful for DWARF-type porting, not transformative. Track 1's main value is
**validating the improved pipeline against the calibration oracle** before Xenon.

## Track 2 — ghidriff as a symbol-porting tool (the Xenon enabler)

What's missing for cross-binary porting is not a better differ, it's a **seeded, scored matcher**:

| # | Change | Notes |
|---|---|---|
| 2a | `--seed-matches pairs.json` — pre-accept externally-known (addr,addr) matches before the cascade | rb3-xenon already HAS 11,057 DC3-derived engine names + 146 validated Wii pairs. Seeds power BSim seeding (`bsim.py:80-88` wants exactly this) and implied propagation. |
| 2b | Add Ghidra VT's scored reference correlators as a cascade stage with an accept threshold (similarity×confidence) | This is the **direct payoff of our Tier-2 work**: the parallelized LSH function-reference correlator is now 3-4× faster, and reference correlators propagate matches through call/data-ref graphs — exactly right for cross-compiler matching where bytes/mnemonics/CFG all differ but the call graph and string refs survive. |
| 2c | Machine-readable `matches.json` output: (addr1, addr2, correlator, score, name1, name2) | Today we reverse-engineer this from pdiff + maps in distill. |
| 2d | Cross-compiler-robust correlators to favor: StringsRefs (survives compilers; Wii dev keeps asserts, retail X360 strips some — partial), call-graph signature seeded from 2a, BSim (decompiler-feature vectors, designed for this) | De-prioritize: byte/instruction/mnemonic exact stages (near-zero cross-compiler yield), BulkBasicBlockMnemonic (off). |

Experiment design (after VT re-run finishes; do NOT touch rb3-xenon's live Ghidra project — a
concurrent BSim agent owns it; use a fresh ghidriff project dir importing `default.xex` +
`bank8_target.elf`, `--force-diff` for the language mismatch):

1. Run seeded ghidriff Wii↔Xenon with 2a-2d.
2. Score against ground truth: 146 validated Wii pairs (held out from seeds) + agreement rate with
   the 11,057 DC3-derived names in the overlap region + rb3-xenon's cluster-purity checks.
3. Success bar: beat 0.32 precision at comparable recall on game-code (`band3/`) functions;
   deliverable feeds rb3-xenon's existing `scripts/target_symbol_map.json` pipeline (names get
   re-derived MSVC-side by class::method+arity COFF matching — we port *identity*, not mangled names).

Sizing: Xenon has 65,548 functions, ~0 named game-code. Even +2-3k trustworthy game-code identities
would be a step-change for `band3/` decomp there (cluster pinning, split proposals, objdiff pairing).

## Constraints / gotchas

- VT re-run (session `RB3_b5_to_b8_opt`) still in progress — heavy 48G JVM; don't start Ghidra-heavy
  experiments until it completes. :8001 service is down until after.
- rb3-xenon Ghidra project + pyghidra :8002 are owned by a concurrent BSim agent — read-only, never
  kill, use separate project dirs for experiments.
- ghidriff venv: `build/SZBE69_B8/ghidra/ghidriff-venv` (py3.10); Ghidra builds need
  `JAVA_HOME=/usr/lib/jvm/java-26-openjdk`.
- The 6 GB ghidriff byproduct (`json/` 5.2G, `proj/` 691M, `gzfs/` 197M) is still pending cleanup —
  but `proj/` contains the imported+analyzed Bank5/Bank8 programs; **a Track-1 re-run can reuse it**
  (drop `--force-diff`? no — keep, it gates language check; the import/analyze is the reusable part).
  Decide cleanup AFTER Track 1.

## Recommended order

1. **Calibration script** (existing data, ~zero cost): per-correlator precision vs the 41,655-pair
   name oracle. Decision input for everything.
2. **Track 1 fork patches (1b, 1c) + BSim re-run (1a)** — validate against the oracle; 1c is an
   upstream-PR candidate alongside our VT fixes.
3. **Track 2 (2a-2c) + the Wii↔Xenon seeded experiment** — the actual prize.
