# Impl: `all-inst-crash` — vocal-track ("All Instruments") load crash

**Status:** DONE — verified. Vocals now loads, the vocal pitch-tube/lyric HUD
renders, and gameplay survives ~21.5s into the song with no SIGSEGV/SIGABRT.

**Branch:** `wt-task-all-inst-crash`
**Commit:** `6342ed1ece8714c82d194530fe0317b538f0ca10` (base `979401c2`)
**Engine:** no `milo-native-engine` change (both fixes are rb3-repo only).

---

## What changed (3 files, all native-only / `#ifdef HX_NATIVE`)

### 1. `native/src/rb3_netsession_native.cpp` — Layer 1 (SIGSEGV)
`RB3InitNativeNetSession()` now also does `TheNet.mSession = TheNetSession;`
(plus `#include "net/Net.h"`). This is the scout's prototyped+verified diff,
landed verbatim. `network/Net.cpp` is not compiled natively, so `TheNet` is a
zero-filled weak blob and `TheNet.GetNetSession()` returned null; the vocal path
`Singer::CreateMicClientID -> TheNet.GetNetSession()->HasUser()` null-derefed
(SIGSEGV at `0x48`). Mirroring console `Net::Init()` makes
`TheNet.GetNetSession() == TheNetSession` (the valid native session), fixing all
7 `TheNet.GetNetSession()` call sites. Native-only TU — zero Wii impact.

### 2. `src/band3/game/Singer.cpp` — Layer 2a (SIGABRT, `mAmbiguousData`)
The four `&mAmbiguousData[0]` pointer-loops (`AllScoresAreIn` @314,
`ResolveAmbiguity` @458, `AddAmbiguousPart` @603, `SetAssignedPart` @659) get an
`#ifdef HX_NATIVE  if (!mAmbiguousData.empty())  #endif` guard. `&mAmbiguousData[0]`
calls `operator[](0)`, which aborts on an empty vector under the native
libstdc++'s `_GLIBCXX_ASSERTIONS`. The 5th site (`DisableAmbiguousPart` @624) is
already inside an outer `if (mAmbiguousData.size() != 0)` so it cannot abort — I
left it unguarded and added a one-line comment documenting why (avoids a
redundant nested guard).

### 3. `src/band3/bandtrack/VocalTrack.cpp` — Layer 2b (SIGABRT, `mNotes`) — NEW, uncovered by this work
**This is the site that actually fires in the vocal-gameplay path** (the
scout's symbolized Layer-2 stack was the `mAmbiguousData` one at frame ~2753;
the live repro aborts earlier, at frame ~1585, in `VocalTrack::UpdateScrolling`).
`UpdateScrolling` forms raw begin/end/index pointers into
`std::vector<VocalNote> mNotes` via `&notes->mNotes[i]` — including the
deliberate one-past-the-end `&notes->mNotes[notes->mNotes.size()]`, which
**always** trips `__n < size()` under `_GLIBCXX_ASSERTIONS`. 8 sites
(1365/1366/1368/1370 begin+end pointers, 1450/1553/1660 alt-offset, 1670 lead-
offset, original line numbers).

Fixed with a file-scoped macro placed after the includes:
```cpp
#ifdef HX_NATIVE
#define VN_PTR(vec, i) ((vec).data() + (i))   // data() valid on empty; data()+size() = canonical end
#else
#define VN_PTR(vec, i) (&(vec)[i])            // textually identical to the original on Wii
#endif
```
Every `&notes->mNotes[i]` became `VN_PTR(notes->mNotes, i)`. The Wii branch
expands to `&(notes->mNotes)[i]` — same AST, byte-identical codegen.

This sibling-file site is exactly the one the scout's FIX DESIGN flagged: *"also
scan sibling vocal files for the same `&vec[0]` idiom on vectors that can be
empty."* It is the documented **deviation/extension** from the scout doc, made on
evidence (a fresh symbolized abort in the live repro).

---

## Match-neutrality (shared source touched outside native/src)

Both shared `.cpp` units rebuilt with MWCC (`tools/ninja-locked`) in the
worktree; `report.json` match% identical before vs after (the `#ifdef HX_NATIVE`
guards + the Wii macro branch are invisible/textually-equivalent to MWCC):

| unit | before | after |
|---|---|---|
| `main/band3/game/Singer` | 56.52346% | **56.52346%** |
| `main/band3/bandtrack/VocalTrack` | 45.507305% | **45.507305%** |

Zero delta — Wii build byte-identical.

---

## Verification (evidence under `/tmp/rp2-all-inst-crash/`)

Repro: `/tmp/rp2-all-inst-crash/repro_vocals_ext.py` — the scout's
`repro_vocals.py` with the gameplay observation window extended from 6 → 40
autohit cycles (~24s, past the frame-1585 and frame-2753 aborts).

**BEFORE** (stock main-repo `rb3-native`, port 8711):
```
*** CRASHED during load (code 139) ***          # SIGSEGV
# /tmp/rb3-repro-8711.log: "caught SIGSEGV (signal 11) at 0x48"
#   right after K8_DBG Band::Band ... trackSym=vocals -> ADD
```
Screenshot: `/tmp/rp2-all-inst-crash/before/01_part_diff.png` (never reached
gameplay).

**Intermediate** (worktree, Layer-1 only, port 8712): SIGSEGV gone, reached
`game_screen`, but new SIGABRT at frame 1585 —
`std::vector<VocalNote>::operator[]` in `VocalTrack::UpdateScrolling`
(`/tmp/rb3-repro-8712.log`). This is how Layer 2b was discovered.

**AFTER** (worktree, all fixes, port 8713):
```
reached game_screen ... vocals
gameplay i=0..36 frame 3292 -> 6907, songMs 0 -> 21502.6   # ~21.5s of real playback
survived gameplay: final frame=6907 songMs=21502.6 screen=game_screen
# /tmp/rb3-repro-8713.log: 0 crash/abort signatures
```
Screenshots (`/tmp/rp2-all-inst-crash/after/play_*.png`): the **vocal
pitch-tube/lyric HUD renders** at the top of the screen across mid-song frames
(play_20 @ songMs~7.7s, play_32 @ ~16s), venue scene + gameplay lighting/bloom
behind it. "Connect a Mic" prompt is expected (headless, no physical mic;
autohit drives scoring).

**Regression check** (guitar path, port 8714):
`scripts/native/keyboard-to-gameplay.py --diff hard` →
`PASS: game_screen reached, song playing (songMs=33310)`. No regression from the
netsession wiring or the VocalTrack macro.

---

## LANDING NOTES (for the orchestrator)

- **3 files, all behind `#ifdef HX_NATIVE`** (or in `native/src/`). Wii build is
  byte-identical (verified). Safe to land independently of any matching work.
- **Singer.cpp overlap with `wt-singer-push`:** that concurrent worktree edits
  Singer.cpp at lines **258, 267 (HandlePhraseEnd), 416 (SuddenOctaveShift)** —
  match-tuning, no `#ifdef`. My hunks are at **311, 462, 612, 638, 676**. **No
  line overlap** (closest gap ~44 lines, well outside git's ±3 context), so the
  two should cherry-pick cleanly in either order. If a conflict does surface,
  it'll be trivial context drift — keep both sets of hunks; they're disjoint
  edits to different functions.
- **Cross-task overlap:** no other render-polish task touches `VocalTrack.cpp`,
  `Singer.cpp`, or `rb3_netsession_native.cpp` as far as the wave-2 set goes
  (this is the only crash task). The `VN_PTR` macro is self-contained in
  `VocalTrack.cpp`.
- **Order:** no ordering constraint relative to sibling tasks. Land whenever.
- **Layer-1 = the scout's verbatim prototype** (just committed instead of left
  uncommitted in the scout worktree). Layer-2b (`VocalTrack`) is the new,
  on-evidence extension and is the one that gates a *playable* vocal session —
  the `mAmbiguousData` guards alone would NOT have produced a clean playthrough.

## Cleanup
Scout worktree still referenced for the repro/evidence; the orchestrator can
remove `.claude/worktrees/scout-all-inst-crash` and
`.claude/worktrees/task-all-inst-crash` after landing.
