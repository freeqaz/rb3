# W32-PROP-FAN — PLAN

Lane B (F1 family). Base SHA rb3 `30546499`, engine pin `24c4f95` (== HEAD).
Build dir: `native/build-agent-W32-PROP-FAN`. Harness: `RB3_HTTP=1
RB3_FIXED_CLOCK=1`, free ports, pgid-only cleanup.

## Charter (amended)
Target: in-song prop-tip fan/cone artifacts — drumstick tips, guitar neck, kit
cones, magenta stick-fan guitar — driven by instrument-MIDI drivers
(CharDriverMidi / CharIKMidi / CharIKSliderMidi), NOT the mood/set_play stream
(W31 Lane A A5 re-scope). W31 set_play (body clips) already fixed — do NOT
regress (CHARDRV_PLAY ~80, 3 intensities, sit ~26).

## Owned surfaces (A7)
- Exclusive-write: `src/system/char/CharDriverMidi.cpp` / `CharIKMidi.cpp` /
  `CharIKSliderMidi.cpp`.
- `src/band3/` sender TU(s): owned ONLY after STEP-0 checkpoints real names.
- `BandCharacter.cpp`: read-only (A9), coordinator arbitration for any write.

## Steps
1. **STEP-0 (BLOCKING, E7 debt):** matched-songMs band-framing crop baseline of
   today's prop-tip artifacts via band-closeup-capture.py (guitar/drums/vocals).
   Quote in STATUS. No diagnosis before this.
2. **Discriminator-first, PER PROP CLASS** (sticks / neck / kit cones /
   stick-fan guitar), pointer-keyed + matrix-relative (lint 1):
   - (a0) drivers not bound/created natively
   - (a) bound and fed (nonzero MIDI-derived streams reaching bones)
   - (b) bound but STARVED (stream dead natively — set_play precedent)
   - (c) driven but wrong-basis (SKEL class → STOP, closed family)
   Read-only default-OFF probe if needed; check dispatch path's >=99% functions
   FIRST (arg-order lesson).
3. **Verdict + action:**
   - branch (b)/(a0) w/ decomp-routing root cause → fix on owned TUs (or
     checkpointed band3 senders); objdiff clean; set_play A/B unchanged.
   - branch (c) → STOP memo (SKEL closed, binding).
   - branch (a) w/ artifact → diagnosis memo + recharter proposal, no fix.

## Prior art (read)
- W31-HUBWALKER-SHARDS/VERDICT.md: HUB-walker fans/cones = SKEL bind-mismatch,
  STOP. That is the HUB shell (band-preview walkers). My charter is the IN-SONG
  gameplay context — distinct, must re-discriminate.
- W31-REPRO/evidence/NOTES.md Bug 3: in-song magenta stick-fan guitar + kit
  cones mapped (hypothesis) to F1 undriven-prop-bone / instrument-MIDI family.
- W31-SET-PLAY-DISPATCH/STATUS.md: set_play body-clip fix (SyncProperty arg
  swap). Prop-tip half re-scoped here.
