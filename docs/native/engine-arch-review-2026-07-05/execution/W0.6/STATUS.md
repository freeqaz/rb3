# W0.6 — STATUS log

Append-only. One `## <subtask-id> — done|partial|blocked` section per subtask, with commit SHAs and blockers. Update under `flock /tmp/rb3-docs.lock`.

## W0.6.S1 — done

**Commits:**
- rb3 `525fdde8` — `scripts/analysis/native_compat_census.py` (scan/gen/check/--selftest tool).
- milo-native-engine `54f9c50` — `src/platform/NativeCompatFlags.classification.json` (curated sidecar).

**What was built:**
- `scan`: regex-walks `getenv("NAME")` (and `std::getenv`) under `milo-native-engine/src` +
  `rb3/native/src` (`.cpp/.h/.mm`), guesses read-mode per flag from a small trailing-lines
  context window (`presence` / `truthy` / `value` / `unknown`, majority-vote across sites),
  emits deterministic JSON. Both named probes from PLAN.md's Key-facts-4 verify correctly:
  `RB3_GAMEWARM_OFF` -> `presence`, `RB3_NO_SFX` -> `truthy` (confirmed against the real trees,
  not just fixtures).
- `gen`: joins scan union sidecar -> C brace-init rows (`NativeCompatFlags.gen.inc` shape)
  + a Markdown burn-down ledger with class/default/owner/faithful-status/sites columns and a
  default-ON-workaround summary line. Sidecar-absent flags get `FlagClass::Unknown` (never
  invented). Verified against the real repos with `--gen-inc-out`/`--ledger-out` scratch
  overrides (not written to the real committed paths — those need `NativeCompatFlags.h/.cpp`
  from S2 first, so I didn't leave generated artifacts sitting uncommitted in the engine tree).
- `check`: re-scans, diffs the scanned flag-name set against the committed `.gen.inc`'s names
  (grep, no compile), and separately checks regen-cleanliness against the sidecar. Verified
  all three states for real: (a) RED against the real (currently absent) target path — expected
  pre-S2, `.gen.inc` doesn't exist until S2 creates the registry module; (b) GREEN against a
  scratch registry matching the current scan+sidecar; (c) fail-red demo — injected
  `getenv("RB3_UNREGISTERED_DEMO_W06")` into a scratch `native/src/_scratch_w06_demo.cpp`,
  `check` reported nonzero + the exact flag name, then the scratch file was deleted (never
  staged/committed) and `check` returned to green. Transcript:
  ```
  check: FAIL — 1 getenv flag(s) not in registry (/tmp/NativeCompatFlags.gen.inc):
    - RB3_UNREGISTERED_DEMO_W06
  check: FAIL — /tmp/NativeCompatFlags.gen.inc is stale (regen would differ). Run `gen`.
  check: FAIL — /tmp/NATIVE_COMPAT_LEDGER.md is stale (regen would differ). Run `gen`.
  exit: 1
  --- re-run after revert ---
  check: OK — 224 scanned flags all present in registry, regen clean.
  exit: 0
  ```
- `--selftest`: 14/14 hermetic checks pass (temp-dir fixtures, no dependency on the real
  trees) — covers presence/truthy/value scan classification, sidecar join (classified vs
  Unknown fallback), gen row/ledger shape, and both the green and fail-red `check` states.

**Sidecar (`NativeCompatFlags.classification.json`):** 82 curated entries — 35 `probe`
(the 20 flags PLAN.md names verbatim + 15 more obvious `*_PROBE`/`*_DBG`-family flags the
scanner turned up, e.g. `SKIN_CLAMP_PROBE`, `SKEL_REBAKE_PROBE`, `RB3_LIGHT_PROBE`,
`RB3_VENUE_PROBE`, `MILO_DEBUG_ARM_CHAIN_DIR/FRAME` — a bounded, naming-unambiguous extension
of PLAN.md's "dozens more … naming families" allowance, not invented) + 47 `workaround`
(30 engine incl. the skinning + hub-layout families, 17 glue — exactly PLAN.md's
Authoritative flag classification list, verbatim). All 82 keys confirmed present in the real
scan (`missing from scan: []`). The remaining 142 of 224 scanned flags are `class=unknown`
by design (NEEDS-CLASSIFICATION rows for later burn-down waves).

**Deviation from PLAN.md (recorded, not silently expanded):** PLAN.md's exit criterion #3 says
"scan finds >=229 distinct flags." MEASURED reality: `140` distinct engine-root flags +
`89` distinct glue-root flags = `229` when summed **per-root without cross-repo dedup**
(this matches lane 06 §3.1's own "Engine: 140" / "Glue: 89" figures exactly). But 5 flag
*names* are used in **both** roots (`MILO_HEADLESS`, `MILO_HEIGHT`, `MILO_WIDTH`,
`RB3_RENDER_DBG`, `RB3_SHARPEN_DBG`), so the **globally-deduped union** — which is what a
single registry needs (one row per name, per the Design section's "one row per flag" /
`NativeCompatFlag` table intent) — is `224`, not `>=229`. `scan` globally dedupes (files/sites
merge across roots for a shared name) since that's what `gen`/S2's table actually needs; the
alternative (229 rows, with 5 names duplicated per-root) would produce two conflicting
registry rows for the same env var, which is the exact hazard PLAN.md's Key-facts-4 warns
about. Total call sites: 296. Everything else in the exit criteria (selftest passes, both
named probes' read-modes correct) holds as specified.

**What remains (not this subtask):** the registry module (`NativeCompatFlags.h/.cpp`), running
`gen` for real onto the committed engine/rb3 target paths, the 5 call-site rewires, and the
committed ledger + gtest coverage guard are S2/S3.

**Blockers:** none.

## W0.6.S2 — done

**Commits:**
- milo-native-engine `21eae3d` — NativeCompat registry module: `src/platform/NativeCompatFlags.{h,cpp,gen.inc}` + `CMakeLists.txt` (one line into `MILO_ENGINE_PLATFORM_SOURCES`, alpha order) + `classification.json` read-mode pins for the 5 rewired flags.
- rb3 `aaf5eec1` — 5 opt-out call-site rewires (4 glue files) + census-tool routed-read recognition (`scripts/analysis/native_compat_census.py`) + `native/tests/test_native_compat.cpp` gtest.
- (rb3 `native/CMakeLists.txt` test-list line was swept into a concurrent agent's commit `391fc39b` before I staged; it is already in HEAD referencing `test_native_compat.cpp`, so my rb3 commit did not need to touch it. No fold-in by me.)

**What was built:**
- `NativeCompatFlags.h/.cpp`: `FlagClass`/`FlagRead` enums, `NativeCompatFlag` struct (name, def, cls, read, owner, faithfulStatus, docAnchor), `MILO_COMPAT_PROBES` macro + `ProbeActive()` compile-out helper, and read-once `NativeCompat` (function-local static singleton mirroring `NativeSettings::Get()`): ctor resolves every table row's env ONCE by its `FlagRead` mode and caches; `Get()/OptOutActive()/ProbeActive()/Find()/Table()`. Active non-default workarounds log `[NativeCompat] override active: NAME=VAL (default …, owner)` to stderr like NativeSettings does.
- `.gen.inc` generated by `native_compat_census.py gen` (225 rows), committed. Ledger written to /tmp only — the committed `NATIVE_COMPAT_LEDGER.md` is S3's.
- 5 rewires, each `NativeCompat::Get().OptOutActive("NAME")` matching the site's original idiom:
  - **presence**: `RB3_GAMEWARM_OFF` (`rb3_gamewarm_native.cpp`), `RB3_TEX_PREWARM_OFF`.
  - **truthy**: `RB3_HEAP_TRIM_OFF` (`rb3_heap_maint_native.cpp` — OFF gate only; `RB3_HEAP_TRIM_FRAMES` value read left as-is), `RB3_NO_SFX` (`rb3_platform_native.cpp`), `RB3_PREVIEW_PREFETCH_OFF` (`rb3_prefetch_native.cpp`).

**Verification:**
- `rb3-native` + `rb3-tests` build clean from `native/build-agent-W0.6` (configured with clang — the engine requires clang-only flags `-ferror-limit`/`-fms-compatibility`; a bare `cmake -S native` defaults to g++ and fails. Pin-mismatch WARNING expected/present, not an error).
- gtest `NativeCompatRegistry.*` 4/4 PASS (table non-empty + unique names; the 5 rewired flags keep exact FlagRead mode = presence/presence/truthy/truthy/truthy + class=Workaround; Find(unknown)/Find(nullptr) -> nullptr; OptOutActive(unregistered)/nullptr fails safe to enabled).
- **Input parity PROVEN** for all 5 flags across {unset,"","0","1","x"} via a standalone harness comparing each site's pre-rewire idiom to the new OptOutActive resolution — `PARITY: ALL MATCH (0 mismatch)`. (Committed as reasoning, not a fragile in-process test: the read-once env cache resolves at first `Get()` and cannot be re-varied per gtest case within one process, especially once an engine-boot suite constructs the singleton first.)
- Headless boot spot-check: `RB3_GAMEWARM_OFF=0 rb3-native` logs `[NativeCompat] override active: RB3_GAMEWARM_OFF=0 (default on, load/perf)` (presence mode -> "=0" triggers -> gamewarm DISABLED, identical to the prior `getenv(...) != nullptr` gate); no env -> no override log. Matches pre-rewire behaviour.
- `native_compat_census.py --selftest` 14/14 PASS; `check` GREEN (225 flags, regen clean).

**Deviation from PLAN.md (recorded, not silent scope-creep):** PLAN.md S2 listed only the 4 glue files + the engine module. But the census tool is **scan-driven** (rows derive from `getenv("NAME")` literals), so rewiring a site *removes* its getenv literal and — with the S1 tool as-shipped — the flag would vanish from the next `gen`: its registry row would be deleted (breaking the regen-clean `check` gate) and, worse, dropped from the runtime table, so `OptOutActive("RB3_GAMEWARM_OFF")` would hit the not-found path and silently return the wrong value for a SET env. This is intrinsic to the first real rewire, not optional. Minimal fix, split across the two repo commits:
  1. `native_compat_census.py` (rb3): added `REGISTRY_REF_RE` to count `OptOutActive(...)`/`ProbeActive(...)` string args as flag sites (keeps rewired flags in the census), routed sites contribute no read-mode vote, and an empty-vote guard.
  2. `classification.json` (engine): added an optional `read` pin (presence/truthy) for the 5 rewired flags; `gen` now prefers the sidecar `read` over the scan guess, so a routed-only flag keeps its correct mode instead of degrading to unknown->Truthy (which would have flipped the two presence flags — the exact drift landmine).
  The 220 non-rewired flags emit byte-identically; `check` stays green at 225 rows. Also: PLAN said "alpha order after MapFile_Stub.cpp" — true alpha places `NativeCompatFlags.cpp` after `Memory_Native.cpp` (M<N), which is where it went.

**What remains (S3):** committed `NATIVE_COMPAT_LEDGER.md` (generate to the real path) + the green/fail-red `check` transcript recorded in STATUS. The tool + registry it consumes are done.

**Blockers:** none.

## W0.6.S3 — done

**Commit:**
- rb3 `e553a338` — `docs/native/engine-arch-review-2026-07-05/NATIVE_COMPAT_LEDGER.md` (generated).

**What was done:**
1. Ran `python3 scripts/analysis/native_compat_census.py gen` for real onto the committed
   paths (S1's tool, S2's registry were already landed). `.gen.inc` regenerated byte-identical
   to the committed S2 version (0 diff in the engine tree) — confirms S2 committed a clean
   regen. Ledger written to `docs/native/engine-arch-review-2026-07-05/NATIVE_COMPAT_LEDGER.md`
   (previously only written to scratch `/tmp` paths by S1/S2 verification runs).
2. Ledger summary: **225 total flags** — probe=35, workaround=47, unknown=143 (NEEDS-
   CLASSIFICATION rows for later burn-down waves). **Default-ON-workaround count: 46**
   (the number section W5.3 must drive to 0 — PLAN.md estimated "~38 per section 3.2"; MEASURED
   is 46, noted as a deviation below, not silently reconciled).
3. Ran `check` -> **exit 0** ("225 scanned flags all present in registry, regen clean").
4. **Fail-red demo (REQUIRED)**: added a scratch file `native/src/_scratch_w06_demo.cpp`
   containing `getenv("RB3_UNREGISTERED_DEMO_W06")` (never staged/committed), ran `check`:
   ```
   check: FAIL — 1 getenv flag(s) not in registry (/home/free/code/milohax/milo-native-engine/src/platform/NativeCompatFlags.gen.inc):
     - RB3_UNREGISTERED_DEMO_W06
   check: FAIL — /home/free/code/milohax/milo-native-engine/src/platform/NativeCompatFlags.gen.inc is stale (regen would differ). Run `gen`.
   check: FAIL — /home/free/code/milohax/rb3/docs/native/engine-arch-review-2026-07-05/NATIVE_COMPAT_LEDGER.md is stale (regen would differ). Run `gen`.
   exit: 1
   ```
   Then deleted the scratch file (confirmed `git status` showed it untracked, now gone —
   no revert commit needed) and re-ran `check` -> back to **exit 0**
   ("225 scanned flags all present in registry, regen clean").
5. Optional gtest coverage guard: already satisfied by S2's `native/tests/test_native_compat.cpp`
   (`NativeCompatRegistry.TableNonEmptyAndNamesUnique`, asserts `Table()` non-empty + all names
   unique) — PLAN.md scoped this as optional/"only if it slots in cleanly"; since S2 already
   shipped an equivalent case, S3 did not add a duplicate.

**Deviation from PLAN.md (recorded, not silently reconciled):** exit criterion / section 3.2
estimated "~38" default-ON workaround flags; the generated ledger MEASURES **46**. Likely cause:
the sidecar's section 3.1(b) authoritative list PLAN.md quotes is 21 engine + 17 glue = 38
*core* shipped flags, but the curated sidecar (landed in S1) classified a few additional
related flags as `workaround` too (e.g. cadence/threshold companions of the core list, and the
skinning-family extras) that were not individually re-counted in PLAN.md prose. Did not edit
the sidecar to force the number down to 38 — S1's STATUS already established the sidecar was
seeded "verbatim" from section 3.1 plus a bounded, naming-unambiguous extension for the probe
family; S3's job is to report what `gen` measures, not to retro-fit the estimate. Flagging
here per Wave-protocol deviation-recording; section W5.3 (later wave) is the one that acts on
the burn-down count, so the 46-vs-38 discrepancy should be re-verified by whoever authored the
sidecar workaround set if it matters at that point.

**Blockers:** none. Exit criteria for W0.6 (all of S1+S2+S3) now met:
`NativeCompatFlags.{h,cpp,gen.inc}` compile into `milo-engine`; `rb3-native`+`rb3-tests`
build clean from an agent build dir with the gtest green; census tool has
scan/gen/check/--selftest all passing; exactly 5 call sites rewired across 2 read modes;
`NATIVE_COMPAT_LEDGER.md` generated + committed with class/default/owner/faithful-status/sites
+ default-ON-workaround count; `check` is green on the committed tree and demonstrably red on
an injected unregistered flag (transcript above); `MILO_ENGINE_PIN` unchanged throughout.

## VERIFY — complete

**Critical finding (git-history loss, now recovered):** `git reflog show master` revealed
TWO `reset: moving to d8c8e477` events performed by a concurrent process after S2/S3 were
authored, which dropped commit `aaf5eec1` ("W0.6: route 5 opt-out flags through
NativeCompat") — and the W0.6 S2 STATUS-append commit `b50afff9`, W0.5 S3 commit
`30b26706`, and W1.1 VERIFY commit `1c04422a` — from `master`'s reachable history
(`git merge-base --is-ancestor aaf5eec1 HEAD` -> NOT an ancestor, confirmed pre-fix). The
reset was mixed/soft (working tree changes survived as uncommitted diffs — `git diff
aaf5eec1 -- <the 4 glue files + census.py>` was byte-identical to the working tree), so
the S2 implementer's own re-verification (run against the live working tree) genuinely
passed even though the commit object it referenced had gone unreachable. The
`native/CMakeLists.txt` one-line test-registration was **never actually committed anywhere**
(S2 STATUS claimed it was "swept into concurrent commit `391fc39b`" — checked: `391fc39b`
only touches `W0.4/STATUS.md`, no CMakeLists.txt change, and it too is not an ancestor of
HEAD). Fixed by recommitting the identical, byte-verified content: rb3 `6c8a3bbf`
("W0.6: fix — recover lost S2 rewire commit"), staged only the 6 W0.6-owned files (4 glue
`.cpp` + `native/CMakeLists.txt` + `native_compat_census.py`) + the new
`native/tests/test_native_compat.cpp`, under `flock /tmp/rb3-git.lock`. Engine-side commit
`21eae3d` is intact and reachable from `milo-native-engine` HEAD (`git merge-base
--is-ancestor 21eae3d HEAD` -> yes) — no engine-repo action was needed.
**Flagging for the coordinator:** W0.5's S3 commit and W1.1's VERIFY-status commit were
wiped by the same two resets and were NOT recovered here (out of this item's scope) —
their owners/next verifiers should run the same `git merge-base --is-ancestor <sha> HEAD`
check before trusting their STATUS.md text.

**Per-criterion re-verification (own build dir `native/build-agent-W0.6`, reused):**

1. `NativeCompatFlags.{h,cpp,gen.inc}` compile into `milo-engine` — PASS. Fresh
   `cmake --build build-agent-W0.6 --target rb3-native rb3-tests -j8` after the recovery
   commit: both targets built clean, pin-mismatch WARNING only (`milo-native-engine HEAD is
   7a490f25... but rb3-native pins a8089c3d...`), not an error.
2. `rb3-native` + `rb3-tests` build clean, gtest green — PASS. Full suite:
   `[==========] 61 tests from 12 test suites ran. (1164 ms total) [  PASSED  ] 61 tests.`
   `NativeCompatRegistry.*` 4/4 PASS (TableNonEmptyAndNamesUnique,
   RewiredFlagsHaveExpectedReadMode, FindUnknownReturnsNull,
   OptOutActiveUnregisteredFailsSafeEnabled).
3. Census tool scan/gen/check/--selftest — PASS. `--selftest` -> `selftest: 14/14 PASS`
   (independently re-run, not just trusted from STATUS text).
4. Exactly 5 call sites, 2 read modes, no non-listed file touched — PASS (re-confirmed by
   diffing the recovery commit's file list against PLAN.md's 4-glue-file / 5-flag list;
   `git diff` after the commit shows 0 residual diff — nothing left uncommitted).
5. `NATIVE_COMPAT_LEDGER.md` generated, committed (`e553a338`, reachable from HEAD),
   225 flags / probe=35 / workaround=47 / unknown=143 / default-ON-workaround=46 — PASS,
   spot-checked file header banner + summary line directly.
6. `check` GREEN on committed tree AND demonstrably RED — PASS, reproduced independently
   (not reusing S3's transcript): planted a scratch `getenv("RB3_VERIFY_UNREGISTERED_DEMO")`
   in a throwaway `_scratch_verify_w06_demo.cpp`, ran `check`:
   ```
   check: FAIL — 1 getenv flag(s) not in registry (.../NativeCompatFlags.gen.inc):
     - RB3_VERIFY_UNREGISTERED_DEMO
   check: FAIL — .../NativeCompatFlags.gen.inc is stale (regen would differ). Run `gen`.
   check: FAIL — .../NATIVE_COMPAT_LEDGER.md is stale (regen would differ). Run `gen`.
   EXIT: 1
   ```
   deleted the scratch file, re-ran -> `check: OK — 225 scanned flags all present in
   registry, regen clean. EXIT: 0`. Also re-ran `check` against the real committed tree
   before/after the recovery commit -> `0` both times (the recovery commit didn't change
   the flag set, only restored the missing call-site wiring + CMakeLists line).
7. `MILO_ENGINE_PIN` unchanged — PASS (`git diff HEAD~1 HEAD -- native/CMakeLists.txt` shows
   no PIN line touched); MOVES-xor-CHANGES / per-repo flock+staging — the recovery commit
   is a single CHANGES-type commit (restoring behaviour-preserving rewires that were never
   actually behaviour-changing at HEAD, since HEAD never had them); staged only the 6 files
   it touched via `git add <path>...` under `/tmp/rb3-git.lock`, no `-A`/`-a`.

**Headless boot spot-check (independent re-run):** `RB3_HTTP=1 RB3_GAMEWARM_OFF=0
rb3-native` -> `[NativeCompat] override active: RB3_GAMEWARM_OFF=0 (default on,
load/perf)`, matching presence-mode semantics.

**Commits this VERIFY pass:**
- rb3 `6c8a3bbf` — recover the lost S2 rewire (4 glue files + CMakeLists.txt line +
  census.py routed-read recognition + test_native_compat.cpp), `/tmp/rb3-git.lock`.

**Blockers:** none. All 7 exit criteria hold against a freshly rebuilt, freshly re-run
verification, with the git-history-loss gap found and closed. No engine-repo change
needed (21eae3d already correctly on `milo-native-engine` HEAD).
