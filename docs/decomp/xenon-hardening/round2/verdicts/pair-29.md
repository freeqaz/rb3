# Pair 29 verdict — WRONG

**Claimed identity:** Wii `ActiveScoreType__12MusicLibraryCFv` (0x80300f00, `MusicLibrary.o`) == Xenon `0x8233afb0`
**Match type:** SwitchSigHasher (simconf -1, non-BSim)

## Verdict: WRONG (confidence: high)

The Xenon function at 0x8233afb0 is **`BandTrack::SetInstrument(TrackInstrument)`**, NOT
`MusicLibrary::ActiveScoreType`. The SwitchSigHasher matched on jump-table *shape* (both
functions have a 10-case switch) but the bodies are entirely different functions from
different TUs.

## Decisive evidence

The Xenon body's referenced string `'unrecognized instrument type "%d"'` plus the Symbol
literals `guitar / vocals / real_guitar / real_bass / real_keys / pending` match
**exactly** one Wii source function: `BandTrack::SetInstrument` at
`src/system/bandobj/BandTrack.cpp:510-547`:

```cpp
void BandTrack::SetInstrument(TrackInstrument inst) {
    mTrackInstrument = inst;                 // Xenon: *(iVar1 + 0x10) = (int)param_2
    switch (inst) {                          // Xenon: switch((int)param_2 + 2)  [enum base -2]
    case kInstGuitar:  mInstrument = guitar;  break;
    case kInstBass:    mInstrument = bass;    break;
    case kInstDrum:    mInstrument = drum;    break;
    case kInstVocals:  mInstrument = vocals;  break;
    case kInstKeys:    mInstrument = keys;    break;
    case kInstRealGuitar: mInstrument = real_guitar; break;
    case kInstRealBass:   mInstrument = real_bass;   break;
    case kInstRealKeys:   mInstrument = real_keys;   break;
    case kInstNone:    mInstrument = none;    break;
    case kInstPending: mInstrument = pending; break;
    default:                                  // Xenon default: MakeString<i>(...) assert
        MILO_NOTIFY_ONCE(MakeString("unrecognized instrument type \"%d\"", inst));
        break;
    }                                         // each case stores Symbol → *(iVar1 + 0xc) = mInstrument
}
```

Xenon body structure (from evidence pack pseudo-C):
- `*(iVar1 + 0x10) = (int)param_2` = `mTrackInstrument = inst` (first stmt).
- 10 lazy-init `Symbol` ctors (`??0Symbol@@QAA@PBD@Z` = `Symbol::Symbol(char const*)`) guarded
  by the `DAT_82c8d094` bitmask = the local static `Symbol`s in `SetInstrument` (Wii map
  confirms `@GUARD@SetInstrument__9BandTrack...@_dw` / `@LOCAL@...` guard symbols).
- `switch((int)param_2 + 2)` over cases 0–9 → store chosen `Symbol` to `*(iVar1 + 0xc)`
  (`mInstrument`); default → `MakeString<i>` assert.

The true Wii counterpart is in the map:
`005e7b90 ... 805f59b0 ... SetInstrument__9BandTrackF15TrackInstrument ... BandTrack.o`
— **`BandTrack.o`, system/bandobj — a different TU than the claimed `MusicLibrary.o`.**

## Why the claimed Wii function is NOT a match

`ActiveScoreType__12MusicLibraryCFv` (Wii ground-truth asm + m2c) is a **void-arg const
getter** on `MusicLibrary` that:
- Calls `GetBandUsersInSession`, `GetTrackType`, `GetControllerType`,
  `ControllerTypeToTrackType`, `TrackTypeToScoreType`, `GetPreferredScoreType`,
  `_MemOrPoolFreeSTL` — **none** of which appear in the Xenon body.
- References `MusicLibrary.cpp` + `Bad ScoreType ...` assert strings via
  `MakeString<PCc,i,PCc>` — **disjoint** from the Xenon string set.
- Computes its switch index internally (`var_r31`) and **returns an integer score type**;
  its 10 switch cases each read a member field `0x70(this)` and conditionally set the
  return value. The Xenon function instead switches on its *argument* and stores **Symbols**
  into a struct.

Wrong-match signals present: disjoint string sets (both sides have strings), disjoint
callee sets, different operation kind (int getter vs Symbol setter), different arity
(void vs 1 arg). Only the jump-table case count (10) coincides — exactly what
SwitchSigHasher keys on, and exactly why it false-matched.

## For the next agent

- This is a clean **SwitchSigHasher false positive**: two unrelated 10-case switches
  collided. Consider down-weighting/filtering SwitchSigHasher-only matches that lack
  corroborating string/callee overlap (all 3 SwitchSig pairs in this sample: 27 = ?,
  28 = leaf no-string, 29 = WRONG here — the stratum looks weak).
- The Xenon function 0x8233afb0 should be re-attributed to
  `SetInstrument__9BandTrackF15TrackInstrument` (Wii 0x805f59b0, BandTrack.o) if a
  corrected mapping is built.
