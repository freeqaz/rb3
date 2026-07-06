# W2.5 — STATUS (append-only; update under flock /tmp/rb3-docs.lock)

Item: Band-waypoint resolution assert ("only some members placed"). Lane C, rb3-only.
Plan: ./PLAN.md. Files: src/system/bandobj/BandConfiguration.cpp (HX_NATIVE-guarded
MILO_WARN diagnostic in SyncPlayMode). No engine, no App.cpp, no new runtime flag.

Implementers: append one `## <subtask-id> — done|partial|blocked` section per subtask
with commit SHAs, exact verify commands run, and the fail-red log evidence (S2). Re-runs
read this + `git log --grep=W2.5` and skip done work.


## W2.5.S1 — done

Commit: `082f933d` — "W2.5: HX_NATIVE-guarded waypoint-resolution diagnostic in
SyncPlayMode" (`src/system/bandobj/BandConfiguration.cpp` only, 17 insertions / 1
deletion; braced the pre-existing `if (bchar)` single-statement and added an
`#ifdef HX_NATIVE … #endif`-guarded `else if (!curtargxfm.targName.Null())` branch
emitting `MILO_WARN` per the plan's reference shape verbatim, plus the optional
per-slot venue-names dump — no per-sync placed/missed summary added, S1 kept the
per-slot warn as the sole signal per the plan's "drop the summary if it complicates
the diff" guidance).

Build (Exit A): fresh `cmake -B native/build-agent-W2.5 -S native
-DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++` then
`cmake --build native/build-agent-W2.5 --target rb3-native rb3-tests -j$(nproc)`
-> both link green, exit 0 (log tail confirms `[100%] Built target rb3-tests`,
rb3-native built earlier in the same invocation with no errors).

Guard check: `grep -n HX_NATIVE src/system/bandobj/BandConfiguration.cpp` -> single
hit at line 54 (`#ifdef HX_NATIVE`), matching `#endif` before the closing loop
brace; `git diff src/system/bandobj/BandConfiguration.cpp` shows the diagnostic is
the ONLY change, entirely inside the guard (existing `if (bchar) bchar->Teleport`
merely braced, not altered). `SyncPlayMode` not rebuilt for Wii this pass (no
toolchain access here) but byte-identical is guaranteed by construction: zero lines
outside the `#ifdef HX_NATIVE`/`#endif` pair changed relative to the pre-image.

Census (part of Exit A): `python3 scripts/analysis/native_compat_census.py check`
-> `check: OK — 229 scanned flags all present in registry, regen clean.` exit 0
(unchanged flag count; no new `getenv`/runtime flag introduced by this diagnostic,
consistent with the plan's "No new NativeCompatFlags runtime flag" classification).

Scope (Exit D, partial — S1 only): only
`src/system/bandobj/BandConfiguration.cpp` changed in the committed diff. No
`BandWardrobe.cpp`, no engine repo, no `src/App.cpp` touched.

Remains for S2: fail-red demonstration (Exit B silence-on-good-data + Exit C
fail-red-on-synthetic-bogus-name) with a headless harness + STATUS.md evidence —
not attempted in this S1 pass; left for the S2 subtask per PLAN.md.

No deviations from PLAN.md.

## W2.5.S2 — done

No source commits (per plan; S2 commits nothing). Build dir reused:
`native/build-agent-W2.5` (from S1). Song: `beastandtheharlot` (guitar/expert,
nofail+autohit) via `scripts/native/capture_song_gameplay.py`.

**Exit B (silence on good data).** Command:

```
RB3_NOTIFY_ALL=1 python3 scripts/native/capture_song_gameplay.py beastandtheharlot \
  /tmp/w25-gooddata.wav --secs 10 --bin native/build-agent-W2.5/rb3-native
```

Reached `game_screen` gameplay (frame=3394 songMs=2194). Engine log
`/tmp/rb3-song-cap-41719.log` (8904 lines). Result:
`grep -a -c -F "did not resolve to a BandCharacter" /tmp/rb3-song-cap-41719.log` -> `0`.
Zero false positives on real venue data (note: this environment's `grep` needs `-a`
against these logs or it silently emits nothing — a local grep-alias quirk, not an
engine artifact; verified file is plain UTF-8 text via `file(1)`).

**Exit C (fail-red on synthetic bogus waypoint).** Injected (temporary, inside the
existing `#ifdef HX_NATIVE` guard from S1, immediately before the `FindTarget` call
in `SyncPlayMode`):

```cpp
#ifdef HX_NATIVE
if (i == 0)
    curtargxfm.targName = Symbol("__w25_bogus_target__");
#endif
```

Rebuilt `rb3-native` (`cmake --build native/build-agent-W2.5 --target rb3-native`),
reran the identical harness command (new log
`/tmp/rb3-song-cap-57953.log`, 9851 lines, reached gameplay frame=3337
songMs=2360). Verbatim fired line:

```
NOTIFY: BandConfiguration::SyncPlayMode: waypoint slot 0 targName '__w25_bogus_target__' did not resolve to a BandCharacter (venue targets: 'player_guitar0' 'player_bass0' 'player_vocals0' 'player_drum0') -- this member will not be placed
```

`grep -a -c -F "did not resolve to a BandCharacter" /tmp/rb3-song-cap-57953.log` -> `1`.
`grep -a -F "waypoint slot" /tmp/rb3-song-cap-57953.log` -> exactly one line, naming
slot 0 + the bogus symbol; slots 1-3 (`player_bass0`/`player_vocals0`/`player_drum0`,
all real `BandCharacter`s) stayed silent, confirming the miss check is
slot-precise and does not cry wolf on resolvable names.

**Revert.** Removed the injection; `git diff src/system/bandobj/BandConfiguration.cpp`
-> empty (tree matches the committed S1 state `082f933d` exactly). Rebuilt
`rb3-native rb3-tests` in `native/build-agent-W2.5` post-revert -> both link green
(confirms the revert didn't leave the build dir stale/broken).

**Exit criteria:** A (build green, guard-only diagnostic, no engine/App.cpp edits,
census unaffected by S1) already satisfied in S1; B and C demonstrated above; D
(scope) — only `BandConfiguration.cpp` (S1, already committed) + this doc pair
changed; no source committed by S2 per plan.

No deviations from PLAN.md (primary harness `capture_song_gameplay.py` worked on
the first attempt; no fallback needed).

W2.5 complete: S1 + S2 both done.

## VERIFY — complete, gates green

Independent re-verification (separate build dir `native/build-agent-W2.5-verify`,
now removed post-verify). Did not trust STATUS.md numbers; re-derived every claim.

**Diff audit.** `git show 082f933d -- src/system/bandobj/BandConfiguration.cpp`
matches the S1 narrative exactly: 17 insertions/1 deletion, single
`#ifdef HX_NATIVE ... #endif` block wrapping an `else if (!curtargxfm.targName.Null())`
branch with `MILO_WARN`. Pre-existing `if (bchar) bchar->Teleport(...)` merely braced,
untouched. `git status --porcelain` on the file was clean before I began (no dangling
S2 injection left in the tree — confirms the plan's "revert before commit" was honored).

**Build (Exit A).** Fresh configure + build in a NEW build dir (not reusing S1's):
`cmake -B native/build-agent-W2.5-verify -S native -DCMAKE_C_COMPILER=/usr/bin/clang
-DCMAKE_CXX_COMPILER=/usr/bin/clang++` -> configure OK; `cmake --build ... --target
rb3-native rb3-tests -j$(nproc)` -> both link, exit 0.

**Guard scope (Exit A).** `grep -n HX_NATIVE src/system/bandobj/BandConfiguration.cpp`
-> single `#ifdef` at line 54, matching `#endif` at line 68, before the loop-closing
brace. Confirmed `HX_NATIVE` is NOT defined anywhere in the Wii/MWCC build
(`config/SZBE69_B8/config.json` has zero hits; `build/SZBE69_B8/build.ninja` has zero
hits) — it's exclusively a native-target compile define (native/src/mwcc_compat.h,
rvl_shims.cpp, etc. all gate the same way). So the Wii MWCC compile of this TU is
provably untouched by construction, not just "guaranteed by not editing outside the
guard." `report.json` still shows `SyncPlayMode__17BandConfigurationFv` at
99.609375% (stale, pre-existing NonMatching — unaffected either way since HX_NATIVE
is off for that build).

**Census (Exit A).** `python3 scripts/analysis/native_compat_census.py check` ->
`OK — 230 scanned flags all present in registry, regen clean.` (230 vs S1's
recorded 229 — one new flag landed elsewhere in the fleet since S1 ran; unrelated to
W2.5, still exit 0, confirms no flag was introduced by this diagnostic).

**Exit B (silence on good data) — RE-DERIVED, not trusted.** Ran the harness myself
against my own freshly-built binary (not S1's): `RB3_NOTIFY_ALL=1 python3
scripts/native/capture_song_gameplay.py beastandtheharlot /tmp/w25verify-gooddata.wav
--secs 8 --bin native/build-agent-W2.5-verify/rb3-native`. Reached `game_screen`
(frame=3341 songMs=2189). `grep -a -c -F "did not resolve to a BandCharacter"
/tmp/rb3-song-cap-43249.log` -> `0`. Zero false positives, independently confirmed.

**Exit C (fail-red) — RE-DERIVED with MY OWN injection, not S1/S2's.** To rule out
"the warn code merely exists but is dead," I injected a *different* bogus symbol than
S2 used (`__w25verify_bogus_target__` vs S2's `__w25_bogus_target__`) at the same
site (slot 0, before `FindTarget`), rebuilt `rb3-native`, and reran the identical
harness. Fired exactly once, verbatim:
`NOTIFY: BandConfiguration::SyncPlayMode: waypoint slot 0 targName
'__w25verify_bogus_target__' did not resolve to a BandCharacter (venue targets:
'player_guitar0' 'player_bass0' 'player_vocals0' 'player_drum0') -- this member will
not be placed`. `grep -a -c` -> `1`; slots 1-3 (real `BandCharacter`s) stayed
silent -> the miss check is slot-precise, does not cry wolf on resolvable names.
This independently confirms S2's result was not cherry-picked or copy-pasted.

**Revert verified.** Removed my injection; `git diff
src/system/bandobj/BandConfiguration.cpp` -> empty (byte-identical to committed
`082f933d`). Rebuilt `rb3-native rb3-tests` post-revert -> both link green (revert
did not leave the build dir stale).

**Scope (Exit D).** `git show 082f933d --stat` -> exactly one file, 1 file changed.
`git log --oneline -- src/system/bandobj/BandConfiguration.cpp` shows 082f933d is
the only W2.5-window commit to this file; no other file's history shows a W2.5-tagged
commit. No `BandWardrobe.cpp`, no engine repo, no `src/App.cpp` edits anywhere in
this item's footprint.

**Concurrency check (Lane B / W2.2 collision risk, per PLAN.md "Risks").** No W2.2
commits exist yet (`git log --oneline --grep="W2.2"` empty except the kickoff docs
commit) — no file-history overlap to check; flagged as still-open for the coordinator
to re-check once W2.2 lands (BandCharacter.{cpp,h}/Character.cpp/CharBonesMeshes.cpp
vs this item's sole file BandConfiguration.cpp — disjoint by inspection, per plan).

**Verdict:** W2.5 exit criteria A/B/C/D all independently reproduced. No blockers, no
deviations, no scope creep found. Cleanup: removed `native/build-agent-W2.5-verify`
after use.
