# Impl: `festival-venue` — festival_01 venue-load SIGSEGV FIXED

Follow-up to wave-8 `crowd-venues` (Fix C, `402c8561`). Fix C made non-small_club
venues load via the MetaPerformer venue override; `arena_06` loads clean but
`festival_01` SIGSEGVed during venue load (the KNOWN-gap tracked in
CAMPAIGN_SUMMARY.md:71-72). This task root-causes and fixes it.

- **rb3 branch:** `wt-task-festival-venue`
  (worktree `/home/free/code/milohax/rb3/.claude/worktrees/task-festival-venue`)
- **rb3 commit:** `da6a3641` — `fix(native): guard null TheBandDirector in
  festival venue-load path (festival_01 SIGSEGV)`
- **Engine:** NO engine change. No `MILO_ENGINE_PIN` bump (pin stays `1010f5f`).
- **File touched (ONE):** `src/band3/meta_band/MetaPerformer.cpp` (+37 lines, all
  inside `#ifdef HX_NATIVE` / `#else`).
- **Wii byte-identical:** YES — proven by a same-embedded-path A/B mwcc compile
  (orig vs edited source → identical `MetaPerformer.o`, `cmp` clean, 150984 bytes).

---

## TL;DR

The festival reward-vignette screen's `exit` DTA handler runs
`{meta_performer load_festival}` → `MetaPerformer::LoadFestival()` →
`TheBandDirector->LoadVenue(...)`, but **`TheBandDirector` is NULL** in the native
menu context (no gameplay `BandDirector` milo-object has been constructed yet). The
unconditional null-`this` deref faults reading `mAsyncLoad` at `this+0x136` — which
is exactly the reported `SIGSEGV ... at 0x136`. arena/small_club never route
through `LoadFestival`, so only festival hit this once Fix C made it reachable.

Fix: guard the null `TheBandDirector` in the three festival/venue-flow deref sites,
HX_NATIVE-only. The in-gameplay festival venue is loaded independently by
`BandDirector::EnterVenue`'s Fix-C force-load (a member call, `this ==
TheBandDirector`, always valid), so skipping the redundant cosmetic vignette-exit
load makes `festival_01` **load** instead of crash.

---

## REPRO

Harness (re-derived; `/tmp` was cleaned): `/tmp/festival-venue/verify_venue.py`
(+ the gdb wrapper `run_gdb_repro.sh` / `festival_gdb_repro.py`). Boots rb3-native
headless, navigates splash→hub→song_select via raw pad presses, injects
`{meta_performer set_venue_override festival_01}` at song_select, then drives
part→difficulty→ready→game_screen (the override sticks: `get_venue_override` reads
`festival_01`). The canonical nav mirrors `scripts/native/keyboard-to-gameplay.py`.
Ports 9901-9909.

- BEFORE (at the pre-fix worktree build / matching the orig source): with the
  festival_01 override the run SIGSEGVs during venue load (process exits code 139),
  reproducibly. arena_06 + default small_club reach `game_screen` clean.

## BACKTRACE (gdb -batch, the real stack — the engine's own handler only `_exit`s
after a terse `backtrace_symbols_fd`, so gdb is what pins the site)

```
Thread 1 "rb3-native" received signal SIGSEGV, Segmentation fault.
0x...e8b785 in BandDirector::LoadVenue(Symbol, LoaderPos) ()        <-- +85
#0  BandDirector::LoadVenue(Symbol, LoaderPos)
#1  MetaPerformer::LoadFestival()
#2  MetaPerformer::Handle(DataArray*, bool)
#3  virtual thunk to MetaPerformer::Handle(DataArray*, bool)
#4  DataArray::Execute()
#5  DataNode::Evaluate() const
...#8 DataArray::ExecuteScript(int, Hmx::Object*, DataArray const*, int)
#9  Hmx::Object::HandleType(DataArray*)
#10 UIScreen::Exit(UIScreen*)        <-- festival reward-vignette screen `exit` script
#11 BandScreen::Exit(UIScreen*)
#12 UIManager::GotoScreenImpl(UIScreen*, bool, bool)
#13 UIManager::PopScreen(UIScreen*)
...
```

Faulting instruction + registers at frame 0:
```
=> LoadVenue+72: mov  -0x58(%rbp),%rax     ; rax = `this` (TheBandDirector) -> 0x0
   LoadVenue+75: add  $0x1a8,%rdi          ; &this->mVenue (the VenueLoader arg)
=> LoadVenue+85: movzbl 0x136(%rax),%ecx   ; read this->mAsyncLoad  -> SIGSEGV (rax=0)
   rax = 0x0   (this == NULL)
   p (void*)TheBandDirector  ==>  $1 = (void *) 0x0     (global is null)
```
`0x136` (310) = the `mAsyncLoad` bool member offset in `BandDirector`. Reading it
off a null `this` faults at address `0x136` — matching the verify-doc symptom.

---

## ROOT CAUSE

`MetaPerformer::LoadFestival()` (MetaPerformer.cpp:1067-1070) is:
```cpp
void MetaPerformer::LoadFestival() {
    MILO_LOG("LOADING VENUE: %s\n", mVenue.mStr);
    TheBandDirector->LoadVenue(mVenue, kLoadBack);   // <-- TheBandDirector == NULL natively
}
```
It is invoked ONLY via the `load_festival` action (MetaPerformer.cpp:1679), fired
from exactly one place: the `(exit {meta_performer load_festival})` handler of
`campaign_rewardvignette_festival_screen` in
`orig-assets/extracted/ui/accomplishments/campaign_rewardvignette.dta:128-129` —
the festival reward cinematic screen (panel `rv5_a.milo`, a `world/vignette/reward/`
vignette, NOT the gameplay world).

`TheBandDirector` is set in the `BandDirector` ctor (BandDirector.cpp:107-110) when
a `BandDirector` milo-object is constructed (it registers as the `banddirector`
`DataVariable`), and nulled in the dtor. On the retail Wii flow a gameplay
BandDirector object exists by the time this vignette-exit script runs, so the
global is non-null. In the native port the vignette-exit path runs in a menu
context **before any BandDirector object is constructed**, so the global is null
and the deref faults.

Why festival-specific: arena/small_club gameplay enters via the normal
`BandDirector::EnterVenue` (sole caller `BandDirector::Enter`, BandDirector.cpp:182)
— a **member** function, so `this == TheBandDirector` and is always valid; its
HX_NATIVE Fix-C force-load (BandDirector.cpp:~631-687) reads the same
`get_venue_override` and force-loads the venue. festival uses the *additional*
`LoadFestival` vignette-exit path (because setting the override to a `festival_*`
venue trips `mFestivalReward` in `SetVenue`, MetaPerformer.cpp:1063-1064, which
gates the festival reward-vignette screen). That path has no valid BandDirector
natively. (Confirmed: festival's in-gameplay venue still loads via EnterVenue once
the vignette-exit crash is guarded — see VERIFICATION.)

---

## FIX (all `#ifdef HX_NATIVE`, in `src/band3/meta_band/MetaPerformer.cpp`)

Three sites that deref `TheBandDirector` in the festival/venue flow:

1. **`LoadFestival()` (~:1067-1070 → +15 guarded lines):** early-return when
   `!TheBandDirector`. THE confirmed crash. Safe early-return (void, no stream).
2. **`ClearVenues()` (~:1087-1097 → +7 guarded lines):** same `if (!TheBandDirector)
   return;` guard. `clear_venues` fires from `ui/game.dta:182`, `tour_quests.dta`,
   `seldiff.dta` — menu/game-flow DTA that can run natively before a BandDirector
   exists; same null-`UnloadVenue` hazard.
3. **`SyncLoad()` (~:1316-1326 → +15 guarded lines):** guard the deref WITHOUT an
   early return (wrap the `if (mVenue==gNullStr) ... else if ... ` block in
   `if (TheBandDirector) { ... }`), with the original code preserved verbatim in the
   `#else` branch — because `SyncLoad` continues parsing the rest of the BinStream
   after this block; an early return would corrupt the stream read. (Net-sync
   receive path, only reachable in networked sessions; the guard is correct + cheap.)

The minimal fix for the reported crash is site #1; #2/#3 close the same pattern on
adjacent festival/venue paths that could surface next.

---

## VERIFICATION (after, evidence under `/tmp/festival-venue/`)

| # | criterion | result | verdict |
|---|---|---|---|
| 1 | `get_venue_override` sticks at festival_01 | `festival_01` | as designed |
| 2 | festival_01 reaches `game_screen` (was SIGSEGV) | yes, frame 4238/4601, **3 runs** | PASS |
| 3 | festival_01 song plays | `is_playing=1`, songMs 5995/6032 | PASS |
| 4 | festival venue actually renders | outdoor festival stage + overhead truss + daytime sky (screenshot) | PASS |
| 5 | festival_01 no SIGSEGV/SIGABRT | process alive through gameplay burst | PASS (was code 139) |
| 6 | **arena_06** still loads (negative control) | `game_screen`, no crash, bypasses vignette path | PASS (unaffected) |
| 7 | **default small_club** still loads (negative control) | `game_screen`, `is_playing=1`, songMs 2383 | PASS |
| 8 | Wii byte-identical | same-path A/B `MetaPerformer.o` `cmp` clean, 150984==150984 | PASS |

Screenshots: `/tmp/festival-venue/deliverable/festival_AFTER_loaded.png` (+`_2.png`)
— festival stage with truss/scaffolding + note highway + HUD, unmistakably the
festival venue (not small club). Raw burst frames
`/tmp/festival-venue/festival_01_game_0{0..5}.png`.

Logs: `/tmp/festival-venue/verify-festival_01-99{05,07}.log` (after),
`…-arena_06-9908.log`, `…-no_venue_override-9909.log` (controls),
`/tmp/festival-venue/gdb-festival_01-9902.log` + `gdb-checkbd.log` (the BEFORE
backtrace + null-`TheBandDirector` proof).

### Wii byte-identity proof (method)

The first naive A/B differed only by `addi rN, r4, +offset` constants all shifted by
exactly **+5** — the `-str reuse,pool` string-pool base shifting by the embedded
`__FILE__` length delta (`MetaPerformer.cpp` 17ch vs `MetaPerformer.orig.cpp` 22ch).
Re-ran with **equal-length embedded paths** (`_abtmp/edit/MetaPerformer.cpp` vs
`_abtmp/orig/MetaPerformer.cpp`), same exact mwcc command (`-d HX_WII -d MATCHING`,
NO `HX_NATIVE`) → `cmp` reports the two `.o` files **byte-for-byte identical**
(150984 bytes each). The Wii build never defines `HX_NATIVE`, so all added blocks
compile out and the `#else` (Wii) branch is the verbatim original.

---

## LANDING NOTES (orchestrator)

- **rb3-only, ONE file, ONE commit.** Branch `wt-task-festival-venue`, commit
  `da6a3641`. File `src/band3/meta_band/MetaPerformer.cpp` — three disjoint
  HX_NATIVE-guarded regions:
  - **`MetaPerformer::LoadFestival()`** (~line 1069, right after the `MILO_LOG`,
    before `TheBandDirector->LoadVenue(mVenue, kLoadBack)`): `#ifdef HX_NATIVE
    if (!TheBandDirector) return; #endif` + comment.
  - **`MetaPerformer::ClearVenues()`** (~line 1096, after the `SetSyncDirty` block,
    before `TheBandDirector->UnloadVenue(true)`): same guard + comment.
  - **`MetaPerformer::SyncLoad()`** (~line 1320, the `if (old != mVenue) { ... }`
    block): wrap the deref block in `#ifdef HX_NATIVE if (TheBandDirector) {…} #else
    …(verbatim original)… #endif` (NOT an early return — stream must keep parsing).
- **No engine commit, no `MILO_ENGINE_PIN` bump.** Pin stays `1010f5f`.
- **Conflict surface:** disjoint from all sibling tasks. Touches ONLY
  `src/band3/meta_band/MetaPerformer.cpp` (game-layer rb3 src). Wave-8 Fix C touched
  `src/system/bandobj/BandDirector.cpp` — a *different* file; this change is
  orthogonal and depends on Fix C only behaviorally (Fix C is what makes festival
  reachable). No overlap with the engine renderer/lighting/shard tasks.
- **Wii gate:** `MetaPerformer.o` byte-identical (proof above). `MetaPerformer.cpp`
  is `NonMatching` in `objects.json` (pre-existing unmatched fns unrelated to these
  three) — the byte-identity A/B is the correct match-neutrality proof here (objdiff
  per-function would also show 0 delta on these three fns since the Wii bytes are
  unchanged).
- **Land order:** standalone; land any time after Fix C (`402c8561`, already on
  master). Cherry-pick `da6a3641` (or merge the branch); no dependencies.

## Do NOT
- Do NOT turn the `SyncLoad` guard into an early `return` — it must keep consuming
  the BinStream (corrupts the remaining fields otherwise).
- Do NOT try to "construct a BandDirector" for the vignette path — the gameplay
  venue is already force-loaded by `EnterVenue`; the vignette-exit `LoadVenue` is
  redundant natively, and skipping it is the correct minimal fix.
- Do NOT regress Fix C — `BandDirector.cpp` is untouched by this task.

## Residual / follow-on (none block gameplay)
- `big_club` venue override not exercised here (Fix B / crowd-imposter territory).
  If it has its own vignette-exit or venue-script gap, the same null-`TheBandDirector`
  guard pattern applies; verify per-venue.
- The festival reward-vignette CINEMATIC itself (the `rv5_a.milo` panel) is a
  cosmetic flow; it now no longer crashes, but its visual fidelity in the native
  port was not the scope of this crash fix.
