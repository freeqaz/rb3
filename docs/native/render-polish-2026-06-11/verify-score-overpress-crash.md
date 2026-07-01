# Verify: score-screen over-press SIGSEGV — wave-6 `score-overpress-crash`

**Verdict: CONFIRM.** The original crash is gone. An independent pre-fix A/B
reproduces the exact documented SIGSEGV before the fix and shows it does NOT
reproduce after. The Wii match claim (99.38% → 100.0%) is independently
verified. Normal/legit sequence playback is unaffected.

- Reviewer: independent adversarial reviewer (Opus).
- Fix under review: rb3 `b4bcbe39` (`src/system/synth/Sequence.cpp`,
  `SerialGroupSeqInst::Poll`); engine pin unchanged (`15ce606`).
- `crashReproducesPostFix = false` (post-fix binary survives every over-press run).
- Evidence: `/tmp/rp6rev-score-overpress-crash/` (BEFORE-{guitar,vocals}-crash.log,
  repro.py). Throwaway pre-fix worktree torn down after use.

---

## What was claimed (impl doc)

`SerialGroupSeqInst::Poll` used `if (mIt++ != mSeqs.end())` — a post-increment
that tests the always-`!= end()` pre-value inside the while-loop, then
dereferences the already-advanced iterator. When the last child sequence
finishes, `mIt` is bumped to `end()` and `*mIt` reads one `ObjPtr<SeqInst>` past
the ObjVector → `SeqInst::Start()` on a garbage/null pointer → SIGSEGV. Triggered
by the `button_select` confirm SFX (a serial sequence) on the quickplay endgame
score screen. Fixed to Bank-8: `++mIt; if (mIt != end()) deref` — match-improving
(99.38% → 100.0%, un-`#ifdef`'d).

## What I verified independently

### (c) Pre-fix BEFORE binary — crash REPRODUCES (proves the fix is causal)

Built a true pre-fix native binary in a throwaway worktree at the fix's parent
(`8993d9bd`, `git checkout 8993d9bd -- Sequence.cpp` to restore the buggy
`mIt++`), **same engine pin `15ce606`** → a clean A/B that isolates only the
synth change. Disassembly confirmed the bug: after `add $0x18` (++) the
post-increment saves the OLD iterator into the comparison temp, re-checks
`end()` against the stale value (always != end inside the loop), then derefs the
advanced `mIt` and calls `Start()` — no end-check on the value actually
dereferenced.

Running the documented over-press repro (`{game jump 600000}` →
`coop_endgame_popups_screen` → `coop_endgame_screen`, then hammer Confirm):

| track  | pre-fix result | backtrace |
|--------|----------------|-----------|
| guitar | **SIGSEGV** at confirm #3 (`load_nextsong_screen`) | `#0 0x0` → `#1 SeqInst::Start` (this=0x5555622e9720) Sequence.cpp:405 → `#2 SerialGroupSeqInst::Poll` Sequence.cpp:505 → Sequence::SynthPoll → SynthPollable::PollAll → Synth::Poll → App::RunOneFrame |
| vocals | **SIGSEGV** at confirm #3 | same: `SeqInst::Start` (this=0x4c8, garbage) ← `SerialGroupSeqInst::Poll:505` |

This is the impl doc's backtrace exactly (jump-through-null-vtable inside
`SeqInst::Start`, called from the buggy deref line of `SerialGroupSeqInst::Poll`),
reproduced by me from a from-scratch build — not trusting the recorded log.

### (a) Post-fix (current master) binary — crash GONE, guitar AND vocals

Same harness, same path, against the already-built composed master binary
(`native/build-native/rb3-native`):

| run | track  | confirms | result |
|-----|--------|----------|--------|
| 1 | guitar | 15 | **survived**, 0 crash sigs; `coop_endgame → load_nextsong → game_screen` (loads next song) |
| 2 | vocals | 15 | **survived**, 0 crash sigs |
| 3 | guitar | 15 | **survived** (repeat) |
| 4 | guitar | 15 | **survived** (repeat) |

The post-fix binary advances cleanly through `load_nextsong_screen` — the exact
screen where the pre-fix binary died — and keeps running into the next song.
0 crash signatures across all four logs. (Confirmed in the binary disasm too:
after `add $0x18` it re-calls `end()` and `cmp; je` BEFORE the deref — the
corrected pre-increment-then-recheck shape.)

### (a, natural-EOF) + (d) Legit sequence playback unaffected

`scripts/native/scoring-test.py` (natural EOF, NO jump) on the post-fix binary:
played a full ~4-min song to its natural end, band score climbed 0 → **156,973
pts**, reached `coop_endgame_screen` with **5 stars**, no crash. A full song of
continuous gem-hit SFX + the endgame UI sequences exercises the same
serial-group Poll path the fix touches — proving the `++mIt` change does NOT
break legitimate sequence playback (the whole point of requirement (d)).

### (b) Wii match claim — independently verified two ways

- Live objdiff: `Poll__18SerialGroupSeqInstFv` fuzzy_match_percent = **100.0**.
- `report.json`: `Poll__18SerialGroupSeqInstFv` = **100.0000**; the sibling
  `RandomIntervalGroupSeqInst::Poll` stays at its pre-existing 97.6640 (untouched,
  as documented — no collateral regression), all other Poll variants 100%.
- Diff scope: `git diff b4bcbe39^ b4bcbe39` touches ONLY the one
  `if (mIt++ != ...)` line in `SerialGroupSeqInst::Poll` (plus an explanatory
  comment). Parallel/Random/RandomInterval group Polls are not touched.

## Residuals / notes (not blockers)

- The repro reaches the endgame via `{game jump}`, which zeroes the score
  (`Performer::Restart`) — that is the documented scoring HARNESS ARTIFACT, NOT
  the crash. The crash is SFX-sequence-driven and independent of how the endgame
  screen is reached; the natural-EOF scoring-test (which keeps the score) reaches
  the same `coop_endgame_screen` with no crash, so the A/B is sound.
- This is a generic synth fix: `SerialGroupSeqInst::Poll` runs for ANY serial
  sequence whose final element ends mid-Poll, so it removes a latent crash beyond
  the score screen (any UI SFX / ambience).

## Conclusion

CONFIRM. Pre-fix: deterministic SIGSEGV in `SerialGroupSeqInst::Poll →
SeqInst::Start` on over-pressing Confirm (guitar + vocals). Post-fix: 5/5 runs
survive (4 over-press repro + 1 full natural-EOF song), 0 crash signatures.
Match-improving decomp fix verified at 100.0%, single-function scope, no sibling
regression, legit sequence playback healthy. `crashReproducesPostFix = false`.
