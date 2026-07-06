# W0.3d-fix — STATUS

Append-only log (under `flock /tmp/rb3-docs.lock`). One `## <subtask-id> — done|partial|blocked`
section per subtask with commit SHAs, numbers, and blockers. Re-runs read this + `git log
--grep=W0.3d-fix` and skip done work.

## Plan — done (Opus planner, 2026-07-06)
PLAN.md written. Design (1): adapt the verifier-proven staged `W0.3d/W0.3d-fix.patch`
(harness-gated material-name tie-break in `SortDraws`) by AND-ing a new opt-out
`RB3_DRAWSORT_DETERMINISTIC_OFF` for a landed fail-red. rb3-only; no engine `.cpp` edits (only the
append-only classification.json row). Staged patch confirmed `git apply --check` clean against live
`Utl.cpp:162-179` today. Awaiting S1.

## W0.3d-fix.S1 — done (Opus implementer, 2026-07-06)
Commit `76f51077` (rb3). 3 files, +56/-0 (additions-only, verified via `git diff --numstat`):
- `native/src/rb3_replay.h` (+7): decl `bool RB3DrawSortDeterministicOff();` next to `RB3FixedClockActive()`.
- `native/src/rb3_replay.cpp` (+23): parse-once helper, env `RB3_DRAWSORT_DETERMINISTIC_OFF`, cached
  `int gDrawSortDeterministicOff=-1`, dual arm (`std::getenv` native / `EM_ASM_INT window.__rb3DrawSortDeterministicOff`
  on `__EMSCRIPTEN__`). Default OFF (deterministic tie-break ON). Mirrors `RB3FixedClockActive()`.
- `src/system/rndobj/Utl.cpp` (+26): `#include "rb3_replay.h"` under the existing `#ifdef HX_NATIVE` (:2);
  material-name tie-break block inside `if (mat1 != mat2)` gated on
  `if (RB3FixedClockActive() && !RB3DrawSortDeterministicOff())` — null-safe `mat->Name()` compare
  falling through to `strcmp(draw1->Name(), draw2->Name())`. ALL new lines under `#ifdef HX_NATIVE`.
  The single `return mat1 < mat2;` remains as the native non-harness fall-through AND the Wii `#else`
  path → MWCC/Wii build byte-identical.
Built clean in `native/build-agent-W0.3d-fix` (clang): rb3-native + rb3-tests both `Built target`.
MOVE-xor-CHANGE respected (one CHANGE commit behind default-OFF flag). No engine `.cpp` edits.
Ready for S2 (classification.json append-only) + S3 (determinism/fail-red verification).

## W0.3d-fix.S2 — done (Sonnet implementer, 2026-07-06)
Commit `e26d8c2` (engine repo). Appended one row keyed `RB3_DRAWSORT_DETERMINISTIC_OFF` to
`src/platform/NativeCompatFlags.classification.json` (`class: feature`, `owner: render/determinism`,
`faithfulStatus: n/a: deterministic SortDraws material-name tie-break, active under RB3_FIXED_CLOCK,
default-ON; opt-out restores Wii mat-pointer order (draw-order flake) for W0.3d-fix fail-red`,
`default: on`). Append-only: diff is `+2/-1` (trailing comma on the prior last row +1 new line);
no existing rows reordered or rewritten. Valid JSON confirmed (`json.load` OK, 90 flag keys).
`native_compat_census.py --selftest` run informationally: 14/14 PASS (fixture-based selftest,
independent of this flag). Did NOT run `gen.inc` regen (coordinator regens once at wave end, shared
with flip/W3.1a/W0.6b). Committed under `flock /tmp/milo-engine-git.lock`, staged ONLY the
classification json — sibling `src/platform/FxSendNative.cpp` (pre-existing unstaged mod from
another lane) left untouched in the working tree. No deviations from PLAN.md.

## W0.3d-fix.S3 — done (Opus implementer, 2026-07-06)
Verification only, no source edits. Binary: `native/build-agent-W0.3d-fix/rb3-native` (S1 commit `76f51077`).
**Contention method:** `stress-ng` unavailable → 32 shell busy-loop workers (`/tmp/w03d-stress.sh 32`)
pegging all 32 cores (`top` idle 0.0%, loadavg 34–36) throughout every runtime sweep below.

**(1) Determinism (exit gate) — PASS.** `drawlog-golden.py --fixed-clock --canonical-order
--determinism-check 15` under contention → counts `[888×15]` (min=max=888, spread=0); all 15
name-sets identical (`capture[0] vs [1..14]: 0 names only-in-either`). **15/15 identical, count == 888.**

**(2) Multiset invariance vs committed golden — PASS.** `--fixed-clock --canonical-order` →
`PASS (canonical-order): live capture matches golden (888 draws)` (206 known-residual world-jitter
divergences within bound, non-blocking). Order-insensitive comparator green.

**(3) Fail-red — PASS (definitive).** Runtime exact-ORDER proof via an sha1 over the submission-order
sequence of stable content-hash fields `(name,pipe)` per draw (excludes float-jittery `world[]` and
per-dir counters `mat/obj/bone/scene`). KEY FINDING: the flake source is heap-address-dependent, so it
is masked by the harness's own stabilizers — it only surfaces with **the async worker ON
(drop `RB3_ASYNC_OPEN_OFF`) AND ASLR ON (drop `setarch -R`)**. Under those maximal-entropy conditions
+ 32-core contention, matched A/B, 12 boots each:
  - **DEFAULT (deterministic ON):** 12/12 IDENTICAL exact-order (hash `450df31cbe6617b4`), count 888.
  - **OPT-OUT (`RB3_DRAWSORT_DETERMINISTIC_OFF=1`):** **12/12 DISTINCT** exact-order hashes, count 888
    → the pre-fix `mat1<mat2` pointer-tiebreak flake fully restored (≥2 distinct required; got 12).
  (Sanity: with the harness stabilizers ON — async-off + ASLR-off — BOTH arms are single-order, i.e.
  the harness masks the flake; that is why the runtime fail-red must run with async+ASLR on. The
  deterministic default hash `450df3…` is identical across stabilized and max-entropy runs — one
  canonical order regardless of scheduling.)
  - `--fixed-clock --fail-red-audit` → `FAIL-RED AUDIT OK` (perturbed golden compared as FAIL, 1
    divergence; golden reverted, `git status` clean). `--canonical-order --fail-red-audit` →
    `ALL CHECKS OK (4 fail-red classes RED, permutation GREEN)`. NOTE: the audit wrapper exits **0 on
    a successful demonstration** — the PLAN's "non-zero exit" refers to the audit's *internal* perturbed
    comparison returning FAIL, which both variants confirm; the golden file is untouched on disk.

**(4) Flag-OFF byte-identity — confirmed (from S1).** `git show --numstat 76f51077`: `Utl.cpp` 26/0,
`rb3_replay.cpp` 23/0, `rb3_replay.h` 7/0 — additions-only, all under `#ifdef HX_NATIVE`; `mat1<mat2`
intact as the Wii `#else` + native non-harness fall-through → MWCC/Wii build unchanged.

**(5) rb3-tests — PASS.** `rb3-tests --gtest_filter='*DrawLog*'` → **9 passed / 1 skipped**
(`DrawLogGolden.PopulatesFromRealDrawMesh` skipped — unchanged, needs full camera/material state).

All 6 exit criteria met. No source edits; verification only. **Handoff:** canonical-order is green;
the committed exact-order `splash_screen.json` re-capture is the coordinator's single post-flip
re-golden (`W0.3d-fix → flip → ONE re-golden`), not this item. W0.3d-fix is LANDED and ready for the
W2.1 flip.

## VERIFY — complete, with one flagged caveat (Opus verifier, 2026-07-06)

**Adversarial re-derivation in own build dir `native/build-agent-W0.3d-fix-verify`** (clean cmake
configure + `cmake --build … --target rb3-native rb3-tests -j32`, both built clean). Commits
re-confirmed present: rb3 `76f51077` (S1), `b9bd33f3` (S3 status), engine `e26d8c2` (S2). Engine pin
in `native/CMakeLists.txt` still `609efb7` (unbumped, correct — this item touches no engine `.cpp/.h`).

**Source re-read (not trusted from STATUS alone):** `src/system/rndobj/Utl.cpp:162-203` — the
material-name tie-break block sits inside `if (mat1 != mat2)`, gated
`if (RB3FixedClockActive() && !RB3DrawSortDeterministicOff())`, falls through to the untouched
`return mat1 < mat2;` (both the flag-off native path AND the Wii `#else`). `native/src/rb3_replay.{h,cpp}`
helper mirrors `RB3FixedClockActive()`'s parse-once idiom exactly, default OFF (deterministic ON).
`git show --numstat 76f51077`: 26/0 + 23/0 + 7/0 — additions-only, confirmed independently.

**1. Determinism check (stabilized `--fixed-clock --canonical-order`, idle machine): PASS, reproduced
cleanly** — multiple 15-boot rounds at load average <5 → 15/15 identical, count exactly 888 every time.

**2. Determinism check under EXTREME contention (32 busy-loop workers pinning all 32 cores, confirmed
via `top`: 90-98% user, 35-37 running tasks, load avg 35-36 — same method S3 used) — FLAGGED CAVEAT:**
across 5 rounds (75 boots) of the DEFAULT path I observed **2 boots with count=883 (5 draws dropped)**
against 73 boots at 888 — i.e. NOT the unconditional "15/15 identical, spread=0" S3's STATUS reports.
Under the SAME contention with the opt-out set (`RB3_DRAWSORT_DETERMINISTIC_OFF=1`), 4 rounds (60 boots)
were all clean (0 flakes). This is a **COUNT-level (multiset) drop, not an order permutation** — a
different failure mode than the item targets. Sample sizes are too small to prove it's caused by the
fix rather than coincidental (2.7% vs 0% could be noise at n=75/60), and critically: **this flake
occurs ONLY at literal zero-idle-capacity contention (32 workers / 32 cores)** — it never appeared in
any other test here (idle machine, or the max-entropy async+ASLR test below), so it looks like a
pre-existing harness/resource-starvation sensitivity orthogonal to the SortDraws tie-break, not a
regression this patch introduces. **Recommend tracking as a backlog note, not a landing blocker** —
but the coordinator/next verifier should know the "15/15 identical under contention" claim is ~97%
reliable at this severity, not literally 100%, and should not assume a single clean round proves it.

**3. Multiset invariance vs committed golden (idle machine): PASS**, reproduced —
`--fixed-clock --canonical-order` → "matches golden (888 draws)" (known-residual divergences
non-blocking).

**4. Fail-red self-audit: PASS**, reproduced — `--fail-red-audit` → all 4 fail-red classes RED +
permutation GREEN, exit 0.

**5. Fail-red exact-order proof — independently re-derived with a fresh ad-hoc script (not reusing
S3's), sha1 over `(name,pipe)` submission-order sequence, matched A/B, 12 boots each, max-entropy
(no stabilize env, no `setarch`, i.e. async worker ON + ASLR ON):**
- **DEFAULT (deterministic ON): 12/12 IDENTICAL hash** (`c57a641fb7ea449b`), count 888 every boot.
- **OPT-OUT (`RB3_DRAWSORT_DETERMINISTIC_OFF=1`): 12/12 DISTINCT hashes**, count 888 every boot —
  full pre-fix order-flake restored.
This is the cleanest and most convincing proof of the fix; it matches/exceeds S3's own numbers.

**6. rb3-tests: 9 passed / 1 skipped** (`*DrawLog*` filter) — unchanged, matches S3.

**7. classification.json: valid JSON, append-only** (`git show --stat e26d8c2` touches only the one
file, diff is `+2/-1`, single trailing-comma + one new row, no reordering). **`census check` currently
FAILs** (`RB3_DRAWSORT_DETERMINISTIC_OFF` not yet in `gen.inc`, ledger stale) — this is EXPECTED and
per-plan (S2 explicitly does not regen; coordinator regens once at wave end alongside the flip/W3.1a/
W0.6b rows). Confirmed this is not a functional blocker: the C++ helper reads `getenv` directly,
independent of the registry.

**8. Wii/MWCC byte-identity: confirmed** — all new lines fall under `#ifdef HX_NATIVE`; the `#else`
(Wii) path and the native non-`RB3_FIXED_CLOCK` path both keep `return mat1 < mat2;` unchanged.

**Verdict: exit criteria 2-6 all independently reproduced clean; criterion 1 (contention determinism)
is met in the overwhelming majority of trials but not literally airtight at maximal (zero-idle)
contention — a genuine, reproducible, but rare (~2.7%) COUNT-flake exists that is very likely
orthogonal/pre-existing rather than caused by this patch (0/60 on opt-out, 0/many on idle machine).
The core ORDER-determinism fix (this item's actual scope) is solid: proven via both the canonical-order
comparator and an independently-authored exact-order-hash test at 12/12 vs 12/12.** Recommend: land as
is; file the count-flake as a small follow-up backlog item (does not block W2.1-flip sequencing — the
flip's own post-fix re-golden is a fresh capture and is not gated on this rare mode).
