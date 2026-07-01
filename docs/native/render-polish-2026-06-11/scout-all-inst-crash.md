# Scout: `all-inst-crash` — "All Instruments" / vocal-track load crash

**Status:** ROOT CAUSE found (two layers). Layer-1 fix prototyped + verified in a
worktree (vocals now loads + plays). Layer-2 fix designed (uncovered by layer-1
fix; aborts a few seconds into vocal gameplay).

**Headline:** Selecting **vocals** (the path to test the vocal display — what the
user calls "All Instruments" mode) crashes the native/web port in two places:
(1) a **null-pointer SIGSEGV** because `TheNet.GetNetSession()` is null natively
(`Singer::CreateMicClientID` derefs it), and (2) once that's fixed, a
**`_GLIBCXX_ASSERTIONS` SIGABRT** from `&mAmbiguousData[0]` on an *empty* vector
in `Singer::AllScoresAreIn`. Both are native-port-only (Wii is unaffected); both
must be fixed to reach a stable vocal playthrough.

---

## 1. SYMPTOM

"All Instruments mode" in this port = bringing the **vocal track** online (the
native port runs one local synth user; selecting the vocals part is how you make
the vocal display/HUD appear). Guitar/bass/drums/keys load fine; **vocals
crashes during song load**, so the vocal display can never be reached.

### Repro (headless, deterministic)

Repro script written for this investigation:
`/tmp/rp-all-inst-crash/repro_vocals.py` (boots native → main_hub → song_select
→ part_difficulty, injects `track:vocals` via `/api/input`, commits the load,
watches for a crash). Run on the **stock** main-repo binary:

```bash
cd /home/free/code/milohax/rb3
python3 /tmp/rp-all-inst-crash/repro_vocals.py --port 8652 --track vocals
#   -> "*** CRASHED during load (code 139) ***"   (SIGSEGV)
```

Full backtrace (run under gdb, `--gdb`): `/tmp/rp-all-inst-crash/primary_sigsegv_gdb.txt`.

Baseline guitar run (no crash, for contrast):
`scripts/native/keyboard-to-gameplay.py --port 8651 --diff hard` → reaches
game_screen, song plays. Screenshots: `/tmp/rp-all-inst-crash/baseline/`.

The crash is a SIGSEGV at address `0x48` (null + member offset). Engine log
shows it fires right after `GameConfig::AssignTrack ... trackSym=vocals` →
`Band::Band ... -> ADD` (the vocal player being constructed).

---

## 2. ROOT CAUSE

### Layer 1 — null `TheNet.GetNetSession()` (SIGSEGV) — PRIMARY

gdb backtrace (decisive):

```
#2  NetSession::HasUser (this=0x0, user=0x...)   native/src/rb3_netsession_native.cpp:110
#3  Singer::CreateMicClientID (this=0x...)        src/band3/game/Singer.cpp:168
#4  Singer::Singer(VocalPlayer*, int)             src/band3/game/Singer.cpp:121
#5  VocalPlayer::VocalPlayer(...)                 src/band3/game/VocalPlayer.cpp:73
#6  Band::NewPlayer(...)                          src/band3/game/Band.cpp:482
#8  Band::Band(...)                               src/band3/game/Band.cpp:82
#9  Game::Game()                                  src/band3/game/Game.cpp:212
```

`Singer::CreateMicClientID` (Singer.cpp:168):

```cpp
if ((!TheNet.GetNetSession()->HasUser(u) || !u->IsLocal()) && !u->IsNullUser()) {
```

`Net::GetNetSession()` (src/network/net/Net.h:26) returns `Net::mSession`. On
console, `Net::Init()` (src/network/net/Net.cpp:70) sets
`mSession = NetSession::New()` (and `NetSession`'s ctor sets the global
`TheNetSession = this`, so `TheNet.GetNetSession() == TheNetSession`).

**Natively, `src/network/net/*.cpp` is NOT compiled** (not in any glob in
`native/CMakeLists.txt`). `TheNet` is therefore a **256-byte zero-filled weak
stub** (`native/src/band3_link_stubs.s:1191` + `native/src/rb3_web_globals.cpp:82
RB3_WEB_ZERO_OBJ(TheNet)`), so `TheNet.mSession == nullptr`. `Net::Init()` never
runs natively, so nothing ever populates it.

The separate global `TheNetSession` *is* valid natively — it is created by
`RB3InitNativeNetSession()` (`native/src/rb3_netsession_native.cpp:294`, called
from the native patch in `SystemInit`, `src/system/os/System.cpp:761`). That's
why menu nav (which uses `TheNetSession->HasUser`) works. **The two pointers were
never reconciled**: `TheNetSession` is set, `TheNet.mSession` is left null. The
vocal path is the first code to call through `TheNet.GetNetSession()`, so it's
the first to fault. (The other 6 `TheNet.GetNetSession()` call sites —
`Performer.cpp:258`, `NetSync.cpp:180/303/326`, `BandUI.cpp:133/419` — are
latent; they only run in multiplayer/teardown paths.)

This is a **glue/wiring gap**, not a logic or match bug.

### Layer 2 — `&mAmbiguousData[0]` on an empty vector (SIGABRT) — uncovered after Layer 1

With Layer 1 fixed, the vocal track loads and `game_screen` is reached, audio
starts playing — then a few seconds in the build **aborts**:

```
stl_vector.h:1253: ... std::vector<Singer::AmbiguousData>::operator[](size_type):
                       Assertion '__n < this->size()' failed.

#  std::vector<Singer::AmbiguousData>::operator[]   (libstdc++ hardened bounds check)
#  Singer::AllScoresAreIn(...)        src/band3/game/Singer.cpp:314
#  VocalPlayer::Poll(float, SongPos)  src/band3/game/VocalPlayer.cpp:738
#  Band::Poll(float, SongPos&)        src/band3/game/Band.cpp:217
#  Game::Poll()                       src/band3/game/Game.cpp:1662
```

(Symbolized stack: `/tmp/rp-all-inst-crash/secondary_sigabrt.txt`. Fired during
**active gameplay** — frame 2753, audio stream just hit "playing" state.)

`Singer::AllScoresAreIn` (Singer.cpp:314):

```cpp
for (AmbiguousData *entry = &mAmbiguousData[0];
     entry != &mAmbiguousData[0] + mAmbiguousData.size(); entry++) { ... }
```

`mAmbiguousData` is a `std::vector<AmbiguousData>` that is `clear()`ed every poll
and only grows when pitch-ambiguity events occur; it is **empty most frames**.
`&mAmbiguousData[0]` calls `operator[](0)` to form the begin pointer — UB on an
empty vector, harmless on Wii / in a non-hardened build, but the native toolchain
links a libstdc++ with `_GLIBCXX_ASSERTIONS`, so `operator[]` does a bounds check
and `abort()`s. **Same idiom appears 5× in this file: lines 314, 458, 603, 624,
659** (all `for (AmbiguousData *... = &mAmbiguousData[0]; ... != &mAmbiguousData[0] + mAmbiguousData.size(); ...)`).
The backtrace hit 314 first, but any of the 5 can abort on an empty vector — fix
all of them. Grep `&mAmbiguousData\[0\]` to enumerate.

This file (`Singer.cpp`) is **heavily match-tuned** (POD word-struct tricks at
lines 60-89 reproducing MWCC's 8× unroll), so the `&v[0]` pointer-loop form is
*deliberate* to match Wii codegen. The fix must be match-neutral (Wii unaffected).

---

## 3. FIX DESIGN

Two independent fixes; both are **rb3-repo only** (no `milo-native-engine`
change needed). Both should land together so the vocal display is actually
testable.

### Fix 1 (Layer 1) — wire `TheNet.mSession` to the native session

In `native/src/rb3_netsession_native.cpp`, in `RB3InitNativeNetSession()`
(~line 294), after the session is created, mirror what `Net::Init()` does on
console:

```cpp
#include "net/Net.h"   // add near the existing #includes (TheNet)
...
void RB3InitNativeNetSession() {
    if (!TheNetSession)
        new RB3NativeNetSession();      // ctor sets TheNetSession = this
    TheNet.mSession = TheNetSession;    // <-- ADD: Net::GetNetSession() now valid
}
```

- `Net::mSession` is `public` (Net.h:33), so the assignment is legal.
- Fixes **all 7** `TheNet.GetNetSession()` call sites at once (not just Singer).
- Risk: low. It only makes `TheNet.GetNetSession()` return the same object
  `TheNetSession` already points to — exactly the console invariant. No Wii match
  impact (the edit is in a `#ifdef HX_NATIVE` native-only TU).
- **PROTOTYPED + VERIFIED** in worktree (see VERIFICATION). This is the exact
  diff I tested; it is safe to land verbatim.

### Fix 2 (Layer 2) — don't `operator[]` an empty `mAmbiguousData`

Make the empty case well-defined, match-neutrally. The cleanest match-safe form
is a native-only guard so Wii codegen is byte-identical:

```cpp
// Singer.cpp:314 (and :458, :603 — every &mAmbiguousData[0] loop)
#ifdef HX_NATIVE
    if (!mAmbiguousData.empty())
#endif
    for (AmbiguousData *entry = &mAmbiguousData[0];
         entry != &mAmbiguousData[0] + mAmbiguousData.size(); entry++) { ... }
```

Alternative (also match-neutral if you confirm it compiles identically on MWCC):
replace `&mAmbiguousData[0]` with `mAmbiguousData.data()` — `data()` is defined
on an empty vector and `data() == data()+size()` so the loop self-skips. **But**
this file is match-tuned; verify with objdiff that `data()` doesn't perturb the
Wii object before choosing it. The `#ifdef HX_NATIVE` guard is the zero-risk
choice.

- Apply to **all 5** sites (314, 458, 603, 624, 659). Grep `&mAmbiguousData\[0\]`
  in `src/band3/game/Singer.cpp` to be sure none are missed; also scan sibling
  vocal files for the same `&vec[0]` idiom on vectors that can be empty.
- Risk: low. Behavior is identical (an empty range is a no-op loop); only the UB
  pointer-formation is removed.
- Match-neutrality: with `#ifdef HX_NATIVE`, the Wii build is untouched. Confirm
  with `objdiff` on `AllScoresAreIn__6SingerFRCQ23std6vectorXTiZ` (or the
  unit `game/Singer`) before/after — expect 0 delta.

### Engine-repo change needed?  **NO** — both fixes are in `rb3/` source.

---

## 4. VERIFICATION

### Layer-1 fix verified (prototype, worktree `scout-all-inst-crash`)

Prototype applied in `tools/setup-worktree.sh scout-all-inst-crash`
(`/home/free/code/milohax/rb3/.claude/worktrees/scout-all-inst-crash`): added
`#include "net/Net.h"` + `TheNet.mSession = TheNetSession;` to
`RB3InitNativeNetSession()`. Built (clang; note the worktree had no
`build-native`, configured with
`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DDawn_DIR=.../dc3-decomp-deps/dawn/lib/cmake/Dawn -DMILO_ENGINE_PATH=/home/free/code/milohax/milo-native-engine`).

```bash
cd /home/free/code/milohax/rb3
RB3_BIN_OVERRIDE=/home/free/code/milohax/rb3/.claude/worktrees/scout-all-inst-crash/native/build-native/rb3-native \
  python3 /tmp/rp-all-inst-crash/repro_vocals.py --port 8654 --track vocals \
  --out /tmp/rp-all-inst-crash/vocals_wt
```

Result: **no SIGSEGV** — `reached game_screen`, overshell `track:vocals`, audio
plays, frames advance to ~2750. Screenshots in `/tmp/rp-all-inst-crash/vocals_wt/`
(`play_00..05.png`) — game runs with vocals selected. **Then the Layer-2 SIGABRT
fires** (proving Layer 1 is fixed and Layer 2 is the next blocker).

### Pass criteria after BOTH fixes land in main repo

```bash
cmake --build native/build-native --target rb3-native        # main repo, after edits
python3 /tmp/rp-all-inst-crash/repro_vocals.py --port 8652 --track vocals
#   PASS = reaches game_screen AND "survived gameplay" with NO SIGSEGV / SIGABRT.
```

Bump the repro's post-load observation window (it currently captures 6 frames;
the Layer-2 abort fired at frame ~2753, so loop ≥30 shots / ~20 s of gameplay to
be sure Layer 2 is truly gone). Also confirm no Wii-match regression on
`game/Singer` via objdiff (Layer-2 edit only).

### Worktree cleanup

When the implementation agent is done referencing it:
`git -C /home/free/code/milohax/rb3 worktree remove --force .claude/worktrees/scout-all-inst-crash`

---

## 5. REFERENCE SCREENSHOTS NEEDED

**None for the crash fix** — the crash is fully diagnosed from backtraces; no
ground-truth comparison is needed to fix it.

For the *follow-on* work this unblocks (testing the vocal display itself), a
retail **vocals-gameplay** screenshot would help — the lyric/pitch-tube HUD with
the note arrows scrolling. `images/retail-screenshots/` should be checked for an
existing vocals shot; if absent, capture one from `../xenia` (vocals part,
mid-song) so a later scout can diff the native vocal HUD against it. Not required
for this crash fix.

---

## Evidence files
- `/tmp/rp-all-inst-crash/repro_vocals.py` — the repro (supports `--gdb`, `--track`, `RB3_BIN_OVERRIDE`).
- `/tmp/rp-all-inst-crash/primary_sigsegv_gdb.txt` — Layer-1 full gdb backtrace (`this=0x0` in HasUser).
- `/tmp/rp-all-inst-crash/secondary_sigabrt.txt` — Layer-2 symbolized stack (empty-vector operator[]).
- `/tmp/rp-all-inst-crash/baseline/` — guitar baseline (works).
- `/tmp/rp-all-inst-crash/vocals_wt/` — vocals running with Layer-1 fix (play_00..05.png).
- Engine logs: `/tmp/rb3-repro-8652.log` (stock, SIGSEGV), `/tmp/rb3-repro-8653.log` (stock, gdb), `/tmp/rb3-repro-8654.log` (fixed, SIGABRT).
