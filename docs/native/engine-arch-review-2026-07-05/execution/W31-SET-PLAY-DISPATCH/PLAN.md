# W31-SET-PLAY-DISPATCH — PLAN

Lane A (primary). Base SHA `fd119705`. Owned TUs: `src/system/bandobj/{BandCharacter,BandCamShot,BandDirector}.cpp` + `src/band3/game/`.

## Goal
Make the song-authored venue-mood stream (`[play]`/`[intense]`/`[mellow]`/`[solo]`)
dispatch `set_play` to the on-stage band natively, so members leave idle and perform.

## STEP-0 discriminators (checkpointed BEFORE any fix — three-supersessions rule)
- **(i)** Is the venue-mood event data parsed natively? (BandDirector census / song data.)
- **(ii)** Who should send `set_play`? (Study the Wii-faithful path.)
- **(iii)** A5: static enumeration of resident perf clips (`stand_rhythm_*`,
  `stand_solo_*`) for prop-tip tracks — decides the cones/fans acceptance leg.

## Execution
1. Static trace of the set_play authoring + routing chain across song data →
   BandDirector → BandWardrobe → char DTA → OnSetPlay.
2. Read-only HX_NATIVE probe (`RB3_SETPLAY_PROBE`, `SETPLAY_KEYS`/`SETPLAY_SEND`)
   + `RB3_BANDPERF_PROBE` + `CHARDRV_PROBE` to localize the break empirically.
3. Root-cause + minimal faithful fix in an owned TU, verified by objdiff (100%)
   AND a songMs-matched native A/B census (baseline vs fix).

## Outcome
Root cause = a **decomp arg-order bug** in `BandDirector::SyncProperty` (the
intensity `SendMessage(_val.Sym(), inst)` should be `SendMessage(inst, _val.Sym())`).
Fix is **unconditional** (no flag, not HX_NATIVE-gated): brings `SyncProperty` to
**100%** match AND routes the mood to the correct band member. See STATUS.md.
