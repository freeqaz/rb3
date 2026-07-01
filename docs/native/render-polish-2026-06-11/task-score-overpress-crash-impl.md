# Task: score-screen over-press SIGSEGV — WAVE-6 implement

**Status: DONE + VERIFIED.** Root-caused with a real gdb backtrace, fixed, and
proven the crash no longer reproduces across multiple guitar + vocals repeats.

- Worktree branch: `wt-task-score-overpress-crash`
- Commit: `0c3c0523` (rb3 worktree)
- File: `src/system/synth/Sequence.cpp` (one function, un-`#ifdef`'d)
- Engine change: **NONE** (`needsEngine = false`)
- Wii: **match-IMPROVING** decomp-bug fix — `SerialGroupSeqInst::Poll`
  **99.38% → 100.0%** (`diff_score = 0/4000`); allowed un-`#ifdef`'d per task rules.

---

## SYMPTOM (repro)

Wave-5 score-detail residual #2: pressing Confirm on the quickplay endgame score
screen (`coop_endgame_screen`) null-derefs (SIGSEGV at `(nil)`). The wave-5 doc
hypothesized a "next-song-load null-deref because quickplay has no next song" in
the MetaPerformer/NextSongPanel UI path. **That hypothesis was wrong** — the crash
is in the SYNTH/SEQUENCE path triggered by the `button_select` UI SFX that the
Confirm handler plays.

Repro harness: `/tmp/rp6-score-overpress-crash/repro.py` — boots headless to
gameplay, `{game jump 600000}` to song-end, auto-advances popups →
`coop_endgame_screen`, then over-presses Confirm. Crash reproduces on confirm #2-3
for both guitar and vocals.

```
[repro] score-detail screen='coop_endgame_screen'
[repro] over-pressing Confirm x8
[repro]   confirm #1 (screen now 'coop_endgame_screen')
[repro]   confirm #2 (screen now 'coop_endgame_screen')
[repro]   confirm #3 (screen now '?')        <- process died (connection refused)
```

## BACKTRACE (gdb -batch, before fix)

`/tmp/rp6-score-overpress-crash/BEFORE-gdb-backtrace.txt`:

```
Thread 1 "rb3-native" received signal SIGSEGV, Segmentation fault.
0x...bca7e0 in SeqInst::Start() ()
#0  SeqInst::Start()
#1  SerialGroupSeqInst::Poll()
#2  Sequence::SynthPoll()
#3  SynthPollable::PollAll()
#4  Synth::Poll()
#5  App::RunOneFrame(int)
#6  App::RunWithoutDebugging()
#7  App::Run()
#8  RunGame(int, char**)
#9  main
```

The non-gdb signal handler also logged `caught SIGSEGV (signal 11) at (nil)` — a
null/garbage function pointer being called inside `SeqInst::Start()`.

## ROOT CAUSE — `SerialGroupSeqInst::Poll` off-by-one end-iterator deref

`src/system/synth/Sequence.cpp:498`. Before:

```cpp
void SerialGroupSeqInst::Poll() {
    while (mIt != mSeqs.end()) {
        if ((*mIt) && (*mIt)->IsRunning())
            return;
        if (mIt++ != mSeqs.end()) {   // post-increment: tests the PRE-increment
            SeqInst *si = (*mIt);     //   value (always != end() in this loop),
            if (si)                   //   then derefs the ALREADY-advanced mIt
                si->Start();
        }
    }
}
```

Inside the `while (mIt != mSeqs.end())` body, `mIt` is by definition `!= end()`,
so `mIt++ != mSeqs.end()` is **always true** — it never guards anything. The
post-increment bumps `mIt`, and the body then dereferences the new `mIt` via
`*mIt`. When the last child sequence finishes (its `IsRunning()` is false), `mIt`
is incremented to `end()` and `*mIt` reads **one `ObjPtr<SeqInst>` past the
ObjVector** — garbage memory. The garbage `SeqInst*` (non-null) is passed to
`si->Start()`, whose virtual dispatch jumps through a junk vtable → SIGSEGV.

**Trigger on the score screen:** the `coop_endgame_panel` Confirm handler runs
`{play_instr_sfx $user button_select}` (`ui/endgame/endgame.dta:150`). The
`button_select` UI SFX is a serial sequence. Once its final element ends, the next
`Synth::Poll → SerialGroupSeqInst::Poll` walks `mIt` off the end and crashes. This
is independent of song count / next-song state — over-pressing Confirm just plays
the SFX often enough to land a frame where the sequence's last element has finished
mid-Poll.

### Confirmed against the Bank-8 target

The target asm (`build/SZBE69_B8/asm/system/synth/Sequence.s` `.L_809AB0FC`)
**increments `mIt`, stores it back, then re-tests `mIt != end()` BEFORE the
deref**:

```
.L_809AB0FC:                       ; reached when (*mIt) null OR !IsRunning()
  lwz   r4, 0x40(r31)              ; r4 = mIt
  addi  r4, r4, 0xc                ; ++mIt  (sizeof ObjPtr<SeqInst> = 0xc)
  stw   r4, 0x40(r31)              ; store mIt back
  cmplw r4, r0                     ; mIt == end() ?
  beq   .L_809AB130                ; yes -> loop top (NO deref)
  lwz   r3, 0x8(r4)               ; *mIt   (only after the end() re-check)
  cmpwi r3, 0x0
  beq   .L_809AB130
  bl    Start__7SeqInstFv
```

So the source had a genuine decomp bug: it used `mIt++` (post-increment + test of
the wrong value) instead of `++mIt` followed by a fresh `end()` re-check.

## FIX

`SerialGroupSeqInst::Poll` — replace `if (mIt++ != mSeqs.end())` with a
pre-increment + re-check:

```cpp
++mIt;
if (mIt != mSeqs.end()) {
    SeqInst *si = (*mIt);
    if (si)
        si->Start();
}
```

This is byte-faithful to Bank 8 (`diff_score 0/4000`, fuzzy 99.38% → 100.0%), so it
ships un-`#ifdef`'d. Affects native + web (shared engine source); the Wii build
improves (no behavior change on a well-formed sequence — only the past-end read
that the target never performs is removed).

## FILES CHANGED (why)

- `src/system/synth/Sequence.cpp` — `SerialGroupSeqInst::Poll` (~line 498):
  `mIt++`-then-deref → `++mIt` then re-test `end()` before deref. Match-improving
  decomp-bug fix.

## VERIFICATION (before / after)

All evidence under `/tmp/rp6-score-overpress-crash/`.

| scenario | BEFORE (master) | AFTER (this fix) |
|---|---|---|
| guitar, Confirm ×8 on score screen | SIGSEGV after confirm #2-3 (`SeqInst::Start` at nil) | survives 8 confirms + 10s watch; `coop_endgame_screen → load_nextsong_screen → game_screen` (sane: loads next song) |
| vocals, Confirm ×8 | SIGSEGV | survives, same sane chain |
| guitar repeats ×2 (Confirm ×10) | — | both clean, 0 crash sigs |
| no-confirm control (auto-advance) | (worked) | still stable, no crash |
| `song-end-test --require-endgame` (worktree bin) | — | **PASS** — endgame STABLE 25s, 2379 frames, no abort |
| `keyboard-to-gameplay --diff hard` (worktree bin) | — | **PASS** — boot→hub→select→part_diff→gameplay, song playing, synth/SFX exercised throughout, no regression |

- AFTER engine logs (9413/9414): **0** crash signatures.
- objdiff (worktree): `Poll__18SerialGroupSeqInstFv` fuzzy **100.0**, `diff_score 0`;
  every sibling in `main/system/synth/Sequence` unchanged
  (`RandomIntervalGroupSeqInst::Poll` stays 97.66%, untouched).

Evidence files:
- `BEFORE-gdb-backtrace.txt` — the SeqInst::Start SIGSEGV backtrace.
- `BEFORE-engine-9411-crashtail.txt` / `engine-9412-crashregion.log` — pre-fix crash logs.
- `engine-9413.log` (guitar) / `engine-9414.log` (vocals) — clean AFTER runs.
- `repro.py` — reusable over-press repro (guitar/vocals, `--no-confirm`, `--gdb`).
- `kbd/` — boot-to-gameplay regression screenshots.

## landingNotes

- **Land target:** rb3 master. Cherry-pick `0c3c0523` (or apply the 1-function diff
  to `src/system/synth/Sequence.cpp`, ~line 498).
- **No engine change, no pin bump.** `needsEngine = false`. (The fix is in the
  rb3-tree `src/system/synth/` — shared by native + web + the Wii decomp build, NOT
  the `milo-native-engine` repo.)
- **Match-IMPROVING exception (not `#ifdef`'d):** this is a genuine decomp-bug fix
  proven against the Bank-8 target asm (99.38% → 100.0%, `diff_score 0/4000`).
  After landing, regenerate the Wii report (`tools/ninja-locked
  build/SZBE69_B8/report.json`) — `Poll__18SerialGroupSeqInstFv` flips to 100% and
  overall code% ticks up slightly. NOT byte-identical (Wii .o improves), but the
  Wii build behavior on a well-formed sequence is unchanged.
- **No sibling conflict.** Touches only `Sequence.cpp`; no other wave-6 task is in
  `system/synth/`. Independent land order. Does NOT touch `Rnd_Wgpu_RB3.cpp` /
  `standard_wgsl.inc` / `meta_band/`.

## Notes / follow-ups (out of scope)

- The other documented residual — "zero autohit score" on the score-detail screen —
  is the harness artifact (`autohit` yields 0), not a defect; unchanged here.
- This is a generic synth bug: `SerialGroupSeqInst::Poll` runs for ANY serial
  sequence whose last element ends while polling (UI SFX, ambience, etc.), so the
  fix removes a latent crash beyond the score screen. The other group-Poll variants
  (`Parallel`/`Random`/`RandomInterval`) use a different, already-correct iteration
  shape and were left untouched.
