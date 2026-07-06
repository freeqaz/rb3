# W2.5 — Band-waypoint resolution assert ("only some members placed")

**Lane C (rb3-only, tiny, parallel). Planner: Opus. Status: PLANNED.**
Parent: `REFACTOR_PLAN.md:97` (W2.5) · `ARCHITECTURE_REVIEW.md` bug-map ("independent
game-side data gap, NOT the transform engine") · `WAVE3_KICKOFF.md` acceptance E2 ·
`WAVE3_REVIEW.md` amendment E2 (fail-red on a synthetic unresolvable waypoint).

## Objective

Surface *why* only some band members get placed at venue/gameplay setup by making the
one silent failure mode loud: a waypoint `TargTransform.targName` that does **not**
resolve to a `BandCharacter`. Today that member is simply skipped with no signal.

This is a **game-side data-gap diagnostic**, independent of the SYS-1 transform/bind
engine work (Lanes A/B). No placement logic changes — we only add a loud, non-fatal
`MILO_WARN` on the miss, plus (optional) a per-sync summary. rb3-only, one source file.

### Current code (MEASURED — line numbers current as of this plan)

`src/system/bandobj/BandConfiguration.cpp:43-54` — the entire resolution site:

```cpp
void BandConfiguration::SyncPlayMode() {
    int idx = ConfigIndex();
    for (int i = 0; i < 4; i++) {
        TargTransform &curtargxfm = mXfms[i].xfms[idx];
        mXfms[i].mWay->SetLocalXfm(curtargxfm.xfm);
        BandCharacter *bchar = TheBandWardrobe->FindTarget(
            curtargxfm.targName, TheBandWardrobe->mVenueNames
        );
        if (bchar)
            bchar->Teleport(mXfms[i].mWay);      // <-- silent skip when bchar == 0
    }
}
```

Resolution semantics (so the diagnostic does not cry wolf):
- `BandWardrobe::FindTarget(sym, names)` → `TargetNames::FindTarget(sym)` returns `-1`
  for a **Null/empty** symbol (`BandWardrobe.cpp:169-177`) → `bchar == 0`. An empty
  `targName` is a *legitimately unfilled slot* (bands can have < 4 members). **Do NOT
  warn on empty targName.**
- The real bug is a **non-empty** `targName` that fails to resolve (name not present in
  `mVenueNames.names[0..3]`, or the resolved `mTargets[idx]` is null). That member is
  skipped → "only some members placed."
- So the miss condition is exactly: `bchar == 0 && !curtargxfm.targName.Null()`.

### Faithfulness constraints (binding)

- **`SyncPlayMode__17BandConfiguration` is 99.6% NonMatching** in the Wii decomp
  (`build/SZBE69_B8/report.json`; `objects.json:828` = `NonMatching`). The diagnostic
  MUST be wrapped in `#ifdef HX_NATIVE … #endif` so the MWCC/Wii compile of
  `SyncPlayMode` is **byte-identical to today** (the guard excludes the new code). This
  file has 0 existing `HX_NATIVE` guards; we introduce the standard port pattern used
  pervasively elsewhere.
- **Non-fatal by design.** Use `MILO_WARN` ("log misses loudly"), **not** a hard
  `MILO_ASSERT`/`MILO_FAIL`. A fatal assert would abort the native offline flow if real
  venue data ever legitimately references a missing name — same reasoning as the
  `MILO_FAIL_DTA`→notifier downgrade under `HX_NATIVE` (`Debug.h:99-104`). The brief's
  E2 says "the assert/log fires"; a loud WARN satisfies "log."
- Native runtime path is confirmed observable: `MILO_WARN` → `TheDebugNotifier` →
  `Debug::Notify` → stdout line `NOTIFY: <msg>` (`Debug.cpp:175,432`). Per-boot dedup
  is on by default (`RB3NotifyFirstSeen`, opt-out `RB3_NOTIFY_ALL=1`) — fine, our warn
  fires only a handful of times (event-driven venue setup, not per-frame).
- **`mVenueNames` is already reachable** from this function (`BandConfiguration.cpp:49`
  reads `TheBandWardrobe->mVenueNames`), so logging the available venue target names for
  diagnosis needs no new access.

### Flag / behavior classification (settled)

This subtask is **NOT game-behavior-changing**: placement logic is untouched; we only
add HX_NATIVE-only diagnostic logging that is inherently gated by `MILO_DEBUG` build
semantics + per-boot dedup. Therefore the brief's default-OFF/gate-staging requirement
(which is W2.2's B1 anti-revert protocol) **does not apply here** — the applicable
requirement is E2 (fail-red), delivered by S2. **No new `NativeCompatFlags` runtime flag
is introduced by the default plan**, so the W0.6 census must simply stay green
(`scripts/analysis/native_compat_census.py` exit 0, unchanged). If the implementer
chooses to add a suppress flag (e.g. `RB3_WAYPOINT_WARN_OFF`), it MUST be registered in
`NativeCompatFlags.classification.json` + ledger regen with census exit 0 — but the
default plan does not add one (event-driven, low-noise, no reason to silence the signal).

## Subtasks

### W2.5.S1 — Add the HX_NATIVE-guarded waypoint-resolution diagnostic
- **model:** sonnet
- **goal:** In `BandConfiguration::SyncPlayMode`, warn loudly (non-fatal) for every
  non-empty `targName` that fails to resolve to a `BandCharacter`; leave placement logic
  and the Wii decomp byte-exact.
- **files:** `src/system/bandobj/BandConfiguration.cpp` (only)
- **steps:**
  1. Inside the `for (i=0..4)` loop, after the existing `if (bchar) bchar->Teleport(...)`,
     add an HX_NATIVE-guarded `else`-branch miss check. Reference shape (adapt to match
     surrounding style; `curtargxfm`/`i`/`TheBandWardrobe->mVenueNames` already in scope):
     ```cpp
     if (bchar) {
         bchar->Teleport(mXfms[i].mWay);
     }
     #ifdef HX_NATIVE
     else if (!curtargxfm.targName.Null()) {
         MILO_WARN(
             "BandConfiguration::SyncPlayMode: waypoint slot %d targName '%s' did not "
             "resolve to a BandCharacter (venue targets: '%s' '%s' '%s' '%s') -- this "
             "member will not be placed",
             i, curtargxfm.targName,
             TheBandWardrobe->mVenueNames.names[0], TheBandWardrobe->mVenueNames.names[1],
             TheBandWardrobe->mVenueNames.names[2], TheBandWardrobe->mVenueNames.names[3]);
     }
     #endif
     ```
     - Pass `Symbol` directly to `%s` (established idiom in THIS file:
       `ConfigIndex` does `MILO_FAIL("invalid mode %s", playmode)` at line 39). If the
       toolchain rejects it, fall back to `.Str()` per member.
     - `Symbol::Null()` is the correct emptiness test (used at `BandWardrobe.cpp:170`).
  2. **Optional** (only if it reads cleanly): accumulate a `placed`/`missed` count over
     the loop and, still under `#ifdef HX_NATIVE`, emit one summary
     `MILO_WARN("... placed %d/4, %d unresolved", placed, missed)` after the loop when
     `missed > 0`. This directly answers "only some members placed." Keep the per-slot
     warn as the primary signal; drop the summary if it complicates the diff.
  3. Do NOT touch `BandWardrobe.cpp`, any engine file, or `src/App.cpp`. Do NOT edit
     outside the `#ifdef HX_NATIVE` block (protects the 99.6% Wii match).
- **verification:**
  - `cmake -B /home/free/code/milohax/rb3/native/build-agent-W2.5 -S /home/free/code/milohax/rb3/native -DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++`
    then `cmake --build /home/free/code/milohax/rb3/native/build-agent-W2.5 --target rb3-native rb3-tests -j$(nproc)` → both link green.
  - Confirm the guard: `grep -n HX_NATIVE src/system/bandobj/BandConfiguration.cpp` shows
    the new block; the diagnostic is the ONLY change and it is inside the guard.
  - `python3 scripts/analysis/native_compat_census.py` (or the census gtest) exits 0
    (no new flag ⇒ ledger unchanged).
  - Commit under `flock /tmp/rb3-git.lock` staging ONLY
    `src/system/bandobj/BandConfiguration.cpp`, message prefix `W2.5:`. This is a
    behavior-adding (diagnostic-only) commit — a CHANGE commit, not a MOVE.

### W2.5.S2 — Fail-red demonstration (synthetic unresolvable waypoint) + good-data silence
- **model:** sonnet
- **goal:** Prove the diagnostic (a) stays silent on real shipping venue data and
  (b) fires on a synthetic unresolvable `targName`, per amendment E2. Evidence recorded
  in STATUS.md; the synthetic edit is temporary and reverted.
- **files:** none committed. Temporary, reverted-before-commit edit to
  `src/system/bandobj/BandConfiguration.cpp` (the injection); evidence goes into
  `docs/.../W2.5/STATUS.md`.
- **steps:**
  1. **Good-data (silence) run.** Boot rb3-native headless into a venue/gameplay state
     where `SyncPlayMode` runs (venue setup calls it via `BandWardrobe::SetVenueDir`
     → `sync_play_mode_msg` → `mModeSink->Handle`). Primary harness:
     `scripts/native/capture_song_gameplay.py` (reaches gameplay with a placed band).
     Alternates if it does not reach the venue band path:
     `scripts/native/band-closeup-capture.py`, or the W0.5
     `scripts/native/patch-lineup-capture.py` (placed lineup). Capture stdout/log and
     confirm **zero** lines matching
     `grep -F "did not resolve to a BandCharacter"` → no false positives on good data.
     (Use `RB3_NOTIFY_ALL=1` in the env so no NOTIFY is deduped away during the check.)
  2. **Confirm reach + fail-red.** Temporarily inject a synthetic unresolvable name for
     one slot, inside the existing `#ifdef HX_NATIVE` region so it compiles only natively,
     e.g. immediately before the `FindTarget` call:
     `if (i == 0) curtargxfm.targName = Symbol("__w25_bogus_target__");`
     Rebuild `rb3-native`, rerun the same harness with `RB3_NOTIFY_ALL=1`, and confirm the
     log now contains exactly one warn for `slot 0` with targName `__w25_bogus_target__`
     while slots 1-3 stay silent. This simultaneously proves the code path is reached AND
     the warn fires (the "temporarily rename one waypoint" demo the brief names).
  3. **Revert** the injection, rebuild, rerun step 1 → silent again. The committed tree
     from S1 is unchanged (S2 commits nothing to source).
  4. Append to STATUS.md: exact harness command(s), the observed warn line (verbatim),
     the silent-run grep result, and confirmation the injection was reverted (`git diff
     --stat src/system/bandobj/BandConfiguration.cpp` shows only the S1 change or clean).
- **verification:**
  - Silent run: `grep -c -F "did not resolve to a BandCharacter" <log>` → `0`.
  - Fail-red run: same grep → `>= 1`, and the matched line names `slot 0` +
    `__w25_bogus_target__`.
  - `git diff src/system/bandobj/BandConfiguration.cpp` after revert shows the injection
    is gone (only the S1 diagnostic remains, already committed).
  - **Fallback if no headless harness reaches `SyncPlayMode`** (venue band path not
    exercised in the reachable flow): drive it directly — locate the live
    `BandConfiguration` (it is `TheBandWardrobe`'s mode sink) and send it the
    `sync_play_mode` action via `/api/dta/eval` (`HANDLE_ACTION(sync_play_mode,
    SyncPlayMode())`, `BandConfiguration.cpp:101`) after a venue is loaded; keep the
    same inject/observe/revert structure. If neither reach nor a DTA trigger is
    achievable in the timebox, S2 stops at "characterize + record the exact blocker in
    STATUS.md" and the fail-red is demonstrated via a minimal rb3-tests driver that calls
    the miss branch directly — but this is the last resort, not the plan.

## Exit criteria (measurable)

- **Exit A (build/faithfulness):** `rb3-native` + `rb3-tests` build green in
  `native/build-agent-W2.5`; the diagnostic is entirely within `#ifdef HX_NATIVE`;
  `report.json` `SyncPlayMode__17BandConfiguration` fuzzy % unchanged (99.6% — not
  rebuilt, but guaranteed byte-identical by the guard); **zero** engine and **zero**
  `src/App.cpp` edits; W0.6 census exit 0 (no new flag).
- **Exit B (no false positives):** a normal headless venue/gameplay boot produces
  **zero** waypoint-resolution warns (`grep -c` → 0).
- **Exit C (fail-red, E2):** with one waypoint `targName` synthetically renamed to a
  non-existent symbol, the diagnostic fires for exactly that slot (grep `>= 1`, names the
  slot + bogus name); reverting silences it. Evidence (commands + verbatim log line) in
  STATUS.md.
- **Exit D (scope):** only `src/system/bandobj/BandConfiguration.cpp` changed in the
  committed diff (plus the two W2.5 doc files); no `BandWardrobe.cpp`, no engine, no
  `App.cpp`.

## Files touched

- `src/system/bandobj/BandConfiguration.cpp` — the diagnostic (S1, committed).
- `docs/native/engine-arch-review-2026-07-05/execution/W2.5/PLAN.md` — this plan.
- `docs/native/engine-arch-review-2026-07-05/execution/W2.5/STATUS.md` — append-only log.
- (S2 only, temporary + reverted, never committed: an injection line in
  `BandConfiguration.cpp`.)

## Risks / conflicts

- **Same-directory adjacency with Lane B (W2.2):** Lane B edits `BandCharacter.{cpp,h}`,
  `Character.cpp`, `CharBonesMeshes.cpp` in `src/system/` (char/bandobj); W2.5 edits
  `src/system/bandobj/BandConfiguration.cpp` — a **different file**, no line overlap
  (review amendment D2). Coordinator cross-diff of the per-lane file lists should confirm
  disjoint. Flagged, not a blocker.
- **Lane A (engine, W0.3c→W1.6):** engine repo only (`Rnd_Wgpu_RB3.cpp` + TUs). W2.5 is
  rb3-only and engine READ-NOTHING → no overlap.
- **W2.2 is engine-READ-ONLY & rb3-only; W2.5 rb3-only** — no engine collision on either.
- **No `src/App.cpp` edits** (entangled with a sibling agent + W0.3b seams) — respected.
- **Match preservation:** `SyncPlayMode` is 99.6% NonMatching; the `#ifdef HX_NATIVE`
  guard keeps the MWCC compile byte-identical. Risk only if an implementer edits outside
  the guard — S1 verification checks the guard explicitly.
- **Fail-red reachability:** if the headless flow does not exercise the venue band path,
  S2 has the `/api/dta/eval sync_play_mode` fallback, then a minimal test-driver last
  resort. Low likelihood — the "only some members placed" bug is itself a native
  gameplay-observed symptom, so the path is reached in gameplay.
