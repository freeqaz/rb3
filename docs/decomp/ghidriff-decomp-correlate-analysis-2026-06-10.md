# ghidriff decomp_correlate — cost analysis, cross-compiler value verdict, redesign (2026-06-10)

**Subject:** `../ghidriff/ghidriff/decomp_correlate.py` (branch `rb3-improvements`) — the
unconditional last-resort "Decomp Match" stage at the end of `VersionTrackingDiff.find_matches`
(`version_tracking_diff.py:275`).

**Context docs:** `ghidriff-calibration-2026-06-09.md` (Decomp Match = 0/15 precision same-ISA),
`ghidriff-improvement-plan-2026-06-09.md`, `ghidra-bsim-perf-investigation-2026-06-10.md` +
`-verification-` (BSim O(n×m) stall + candidate-cap status).

**TL;DR:** the stage burned **2h42m for 60 garbage matches** on the completed Bank8↔Xenon run.
It is now skippable (`--no-decomp-correlate` — use it on every cross-compiler run), and an
improved fuzzy path exists behind default-off flags (`--decomp-correlate-fuzzy`). But the honest
verdict is that fuzzy decomp-TEXT similarity is a weak re-implementation of BSim, and the
cross-compiler recovery budget is better spent fixing BSim's perf (top-K candidate cap) than
tuning this stage. Build verdict: **ship the off-switch as essential; treat the fuzzy path as a
cheap, uncalibrated experiment — not a primary correlator.**

---

## 1. Cost characterization (measured, Bank8↔Xenon run, `ghidriff-xenon/ghidriff.log`)

Two runs are in the log. Run 1 (with BSim+seeds, pools p1:25,588) entered the stage at 04:50:21
and was **killed** mid-stage (fresh engine start at 06:06:59). Run 2 (BSim off) **completed** it:

- Stage window: `06:09:54 → 08:51:33` = **9,699 s (2h41m39s)**.
- Pools: p1 = 31,404, p2 = **33,338** (the log line "p1:31404 p2:31404" was an f-string bug —
  pre-patch `decomp_correlate.py:17` printed `len(p1_missing)` twice; fixed in the rewrite).
- Yield: **60 'Decomp Match' accepts** (p1 missing 31,404→31,344; p2 33,338→33,278).

### Where the time goes (pre-patch code)

1. **Decompiles are O(p1+p2), not O(p1×p2) — but strictly single-threaded.**
   `enhance_sym` memoizes per symbol (`ghidra_diff_engine.py:468` key `{sym.iD}-{program.name}`,
   `:470` guard), so each function is decompiled exactly once: the first p1 iteration walks and
   decompiles the *entire* p2 pool; later inner loops are memo hits. Total ≈ 31,404 + 33,338 =
   **64,742 sequential decompiles**. At the observed throughput that is ~100-135 ms each →
   roughly **70-90 % of the 2h42m**. The stage never touches the engine's worker pool (the
   report phase at `ghidra_diff_engine.py:1736` parallelizes the *same* call across
   `max_workers`; this stage calls it inline at `decomp_correlate.py:25/33` pre-patch).
2. **Each decompile drags dead weight.** `get_decomp_info=True` also walks the full listing
   twice (instructions + mnemonics) and runs `BasicBlockModel` over every block
   (`ghidra_diff_engine.py:513-533`) — none of which the comparison uses (only `['code']`).
3. **The compare loop is a ~1.05-billion-iteration Python/JNI loop.** 31,404 × 33,338 inner
   iterations, each doing: a `p2_matches.contains()` JNI call on a Ghidra `AddressSet`, an
   `enhance_sym` f-string-key memo lookup, and — the avoidable part — **recomputing
   `remove_code_sig(decomp1)` for every p2** (pre-patch `decomp_correlate.py:35`;
   `remove_code_sig` does `''.join` + `split` + `splitlines` on multi-KB strings,
   `ghidra_diff_engine.py:1526-1541`). At ~1-3 µs/iter that is **~17-50 min** of pure loop
   overhead — the minority share, but material.
4. **Timeouts were NOT the dominant cost** (contrary to the working hypothesis from the killed
   run). The stage hard-codes the `enhance_sym` default `timeout=15`
   (`ghidra_diff_engine.py:457`) — it **ignores `--decompiler-timeout`** — and run 2 logged only
   23 `Failed to decompile` (3 of them `process: timeout`) ≈ 45 s total. The killed run 1 most
   likely *looked* hung because the stage is ~2.7 h of silent sequential decompiles with zero
   progress logging.

**Pre-existing footgun worth knowing:** the memo key omits `get_decomp_info`/timeout (the better
key is commented out at `ghidra_diff_engine.py:467`), so a function that failed/timed out here at
15 s stays a failure in the later report phase, which wanted `--decompiler-timeout` (default 60).

### Why exact equality ~never fires cross-compiler

The compared text is raw decompiler output minus everything before the first `{`. Any function
containing a call or data reference embeds `FUN_<addr>`/`DAT_<addr>` tokens, and the address
spaces don't even overlap (Wii `0x80……` vs Xenon `0x82……`) — so **any function with at least one
reference can never be text-equal across the two programs**, regardless of how similar the code
is. Variable names (`iVar1`, `uStack_28`) depend on varnode numbering and differ too. The only
possible hits are *address-free trivial bodies* (`return;`, `return 0;`, small arithmetic
stubs). The 60 accepts are exactly that: interchangeable stubs, paired greedily with the *first*
identical stub on the other side (`break` at pre-patch `:47`) — i.e., essentially arbitrary
assignment within stub families. Calibration on Bank5↔Bank8 already measured this signal at
**0/15 correct**; nothing about the cross-compiler setting improves it, because the only pairs
the exact test can reach are the ones with zero distinguishing content.

---

## 2. VALUE: would *fuzzy* normalized decomp similarity recover real cross-compiler matches?

**The hypothesis is directionally right but the implementation vehicle is wrong.**

Evidence that the decompiler-abstraction signal carries cross-compiler:

- Run 1 (BSim ON, seeded): **BSIM accepted 6,315 pairs** where ExactBytes=0 and
  ExactInstructions=63. BSim *is* decompiler-output similarity — feature vectors extracted from
  the decompiler's normalized data-flow (varnode/op features designed to be compiler-agnostic),
  LSH-indexed. That 6,315 (precision uncalibrated, but thresholded on similarity×confidence) is
  the strongest empirical datapoint that "same source, two compilers → similar decompiler
  abstraction" is real.
- The other cross-compiler survivors on run 2 were StringsRefs (610), VTCombinedReference (722,
  in 3.6 s), SwitchSig (3) — reference/string signals, orthogonal to decomp similarity.

Evidence that difflib-over-normalized-TEXT is a weak proxy for it (offline demo, no Ghidra —
`normalize_decomp` from the new code on hand-written Ghidra-style renderings of the same small
guarded-setter as MWCC/Wii vs MSVC/Xenon output, plus a different-function negative control):

| comparison | raw exact | normalized exact | normalized difflib ratio |
|---|---|---|---|
| same source fn, two compilers (favorable: structure identical, MSVC adds one temp) | False | False | **0.714** |
| different fns, similar size/shape | False | False | 0.222 |

Even a *deliberately favorable* pair — identical control flow, the only delta being one extra
decompiler temp — lands at 0.714, **below** any reasonable accept threshold (the shipped default
is 0.85), because a single temp variable rewrites several lines. Real cross-compiler pairs
diverge much more: MWCC `-inline noauto` vs MSVC aggressive inlining, different CSE/loop forms,
Xenon savegprlr prologue artifacts, different type inference (`int` vs `uint` vs `undefined4`).
Meanwhile sibling overloads and stlport template families (calibration found near-dup families
of ~600) sit *above* genuine cross-compiler pairs in text similarity. So the usable threshold
window is narrow-to-empty: **high thresholds only catch near-identical renderings (mostly the
same trivial-leaf population the exact test reaches, now safely 1:1-gated); low thresholds drown
in template-family false positives.** Line-level difflib measures *rendering* similarity; BSim
measures *data-flow* similarity — which is the actual invariant across compilers.

Calibration is possible but needs a Ghidra run we must not start now: Bank5↔Bank8 with
`--skip-correlators` disabling the exact stages so true pairs survive to this stage, scored
against the 26,517-pair name oracle (`tools/ghidra/calibrate_ghidriff.py` already scores match
types separately; the new paths emit distinct names `Decomp Normalized Match` /
`Decomp Fuzzy Match` for exactly this purpose).

---

## 3. SPEED: the redesign (implemented behind `--decomp-correlate-fuzzy`)

The rewritten stage (`decomp_correlate.py`, ghidriff `rb3-improvements`):

1. **O(p1+p2) parallel one-shot decompiles.** `decompile_pool()` fans `enhance_sym` across
   `ThreadPoolExecutor(max_workers)` — same pattern as the report phase (`:1736`), using the
   existing per-program `DecompInterface` queue (`setup_decompliers` creates `max_workers`
   instances per program under `--threaded`). Timeout = `min(--decompiler-timeout, 15)` s;
   failed/timed-out decompiles are *skipped*, not compared. Estimated wall for the Xenon-size
   pool: 64.7k decompiles / 24 workers ≈ **7-25 min** (vs 2h42m), and the result is memoized for
   the report phase that would decompile these functions anyway.
2. **Normalization** (`normalize_decomp`): strip the signature, renumber address-bearing labels
   (`FUN_/DAT_/LAB_/…` → `FUN_0, FUN_1, …` in first-seen order, preserving "same callee called
   twice" structure) and decompiler value names (`iVar1/uStack_28/local_30/…` → `V0, V1, …`),
   collapse whitespace, drop blank lines.
3. **Exact-normalized pass:** dict bucket join, O(p1+p2), accepting only **1:1 unique-text**
   pairs (`Decomp Normalized Match`). The uniqueness gate is what the legacy stage lacked — it
   kills the interchangeable-stub assignment outright (calibration lesson 7: uniqueness is
   itself a high-value correlator feature).
4. **Bounded fuzzy pass** (`Decomp Fuzzy Match`): inverted index over rare tokens
   (document-frequency ≤ 16) → ≤ 32 candidates per p1 function → exact length-ratio upper-bound
   prefilter → `difflib.SequenceMatcher` over normalized lines → accept at
   ≥ `--decomp-correlate-min-ratio` (default 0.85) **and** best-vs-runner-up margin ≥ 0.02,
   then globally greedy highest-ratio-first with each endpoint used once. Bodies < 5 normalized
   lines are excluded from fuzzy (stub ocean). Worst-case compare cost: 31k × 32 difflib calls
   over ~tens of lines ≈ **~1 min**, vs the 1.05-billion-iteration legacy loop.
5. **Legacy path kept byte-for-byte semantics** (default ON, per upstream behavior) with one
   identical-semantics perf fix: `remove_code_sig(decomp1)` hoisted out of the inner loop
   (halves the string churn), and the `p2:` log f-string bug fixed.

Flags (wired through parser → `__main__.py` → engine ctor, mirroring
`--implied-min-ratio`/`--vt-ref-correlators`):

- `--no-decomp-correlate` — **skip the stage entirely**. *Use this on every cross-compiler run
  now*; the legacy stage costs hours and its accepts are noise.
- `--decomp-correlate-fuzzy` (default off) — swap the exact walk for the normalized+fuzzy path.
- `--decomp-correlate-min-ratio` (default 0.85) — fuzzy accept threshold.

`py_compile` checked with `build/SZBE69_B8/ghidra/ghidriff-venv/bin/python`. Default behavior
(no flags) is unchanged except the two no-semantic-change fixes above.

---

## 4. Verdict: build vs skip

**The off-switch was essential and is shipped.** Every cross-compiler ghidriff invocation should
carry `--no-decomp-correlate` (or `--decomp-correlate-fuzzy` if experimenting). 2h42m → 0 for a
stage whose output is worse than nothing (60 noise pairs that *consume* their endpoints).

**The fuzzy path is a cheap experiment, not a primary correlator — do not invest further until
calibrated.** Reasons, honestly weighed:

1. **It is dominated by BSim on its own axis.** BSim already implements
   "compiler-abstracted decompiler similarity" with the right representation (data-flow
   features, not text), proper indexing, and it demonstrably fired 6,315× cross-compiler. Its
   only problem is the O(n×m) degenerate-bin stall — which has a diagnosed fix path (top-K
   candidate cap per `ghidra-bsim-perf-verification-2026-06-10.md`; the naive 500-cap is lossy,
   the top-K variant is the shippable one). **Fixing BSim perf is strictly higher-yield than
   tuning difflib thresholds here.**
2. **The call-graph axis is covered.** VTCombinedReference recovered 722 (unseeded run-2 pool)
   to 1,190 (seeded run 1) matches in *seconds*, and StringsRefs another ~500-600. Those are the
   proven cross-compiler signals on this corpus.
3. **The fuzzy text signal's own window looks narrow** (offline demo: favorable-pair 0.714 vs
   default gate 0.85 vs template-family collision risk below ~0.7), and its precision cannot be
   validated without a calibration run we can't start while the current re-run owns the machine.
4. What the fuzzy path *does* deliver today at near-zero risk: the **exact-normalized 1:1 pass** —
   a strict precision upgrade over the legacy stage's stub-garbage matching for ~1/200th of the
   cost — plus distinct match-type names so the existing oracle tooling can calibrate both passes
   the moment a run is affordable.

**Recommended next actions, in order:** (a) add `--no-decomp-correlate` to
`tools/ghidra/run_ghidriff.sh`-style cross-compiler invocations; (b) land the BSim top-K cap and
re-enable BSim for Xenon — that is the real "fuzzy decomp correlator"; (c) opportunistically
calibrate `--decomp-correlate-fuzzy` on Bank5↔Bank8 with exact stages skipped; promote or delete
based on the numbers.
