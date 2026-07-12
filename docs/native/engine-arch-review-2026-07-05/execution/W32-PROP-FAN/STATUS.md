# W32-PROP-FAN — STATUS

Lane B (F1 family). Base SHA rb3 `30546499`, engine pin `24c4f95` (== HEAD).
Build dir `native/build-agent-W32-PROP-FAN`. All numbers below QUOTED VERBATIM
from committed evidence (lint 7 / E4). Evidence under `evidence/` (gitignored).

## VERDICT: branch (b) STARVED — FIX LANDED (flag default-OFF, flip PROPOSED)

The instrument-MIDI prop drivers are **bound-but-STARVED**: created and polled
every frame but **never Entered**, so they never subscribe to their MIDI parser,
never receive per-note events, and the per-note hit/strum animation never plays —
the arm holds an idle pose, `CharIKHand` over-reaches the far drum/fret target,
and the prop-tip / finger re-projection flings into the visible stick-fan.

Rendered **per prop class** (A8, pointer-keyed + matrix-relative, lint 1):

| prop class | driver objects (pointer-keyed) | branch | after-fix |
|---|---|---|---|
| sticks (drum hits) | player3: `right_hand/left_hand.dmidi` + 12 per-drum `.dmidi` (floortom/snare/kick/L-tom/R-tom/hihat/ride/L-crash/R-crash/left_foot/right_foot) | (b) STARVED | 416 drum-hit clips now play; hand-fan gone |
| kit cones | same drum-driver family (hands reaching kit `bone_target_*` at Y~90-120 from hand Y=208) | (b) STARVED | arm lowered to kit; fan gone |
| guitar neck / fret | player1: `fret.dmidi` (parser='' — none), `CharIKMidi` fret IK (`NewSpot` fired 0) | (b) STARVED | resolved via strum/string driver entry |
| stick-fan guitar (strum) | player1: `strum.dmidi`, `string_hi.dmidi`, `string_low.dmidi` | (b) STARVED | guitarist band-shards 843→0; yellow fan gone |
| vocals (control) | player2: `mic.ikhand`/`mic_stand.ikhand` | not starved | clean (drops_band=0) baseline + after |

Branch (c) SKEL was REFUTED for the props: the props are rigid meshes riding
flung hand bones, not the skinning-basis family. (The residual shoe/outfit fans —
`maleslipons2`/`lowtopsneaks`/`wovensteppers` at ratio ~3.9× — ARE the SKEL
family and are OUT OF SCOPE here; W31-HUBWALKER-SHARDS STOP still binds them.)

## STEP-0 baseline (E7 debt discharged) — evidence/baseline/

Matched-songMs band-framing closeups, base SHA `30546499`, deterministic pins
(guitar 15/15, drums 9/9, vocals 9/9):
- `guitar/base_coop_g_cg_0.png` — large green/yellow triangle **stick-fan guitar**
  behind the note highway.
- `drums/base_coop_d_n01_0.png`, `.../base_coop_d_n02_0.png` — pale/pink
  **stick-fan radiating from the drummer's raised hands** (drumsticks + collapsed
  hand/finger skin), shared-apex morphology.
- `vocals/base_coop_v_n01_0.png` — clean.
Shard census (SHARD_RATIO, band-tagged): drums drops_band=**1107** max_band_ratio
**5.92**; guitar drops_band=**843** max_band_ratio **5.15**; vocals drops_band=**0**.

## Discriminator evidence (pointer-keyed, matrix-relative)

CharIKHand census (`RB3_PROP_CENSUS_DBG`, evidence/…c2…):
- `player3 right_hand.ikhand finger=1 fingerName='bone_R-tip.mesh' ntargets=8 reach=20.27`
- 8 drum targets `bone_target_{snare,floor_tom,hihat,L/R-cymbal,L/R-tom,ride}`,
  IK_TGT hand `wpos=(11.1,208.6,34.6)` vs target `twpos` Y~90-120, **d=100-143u vs
  reach=20.27** — massive over-reach from an idle-raised arm.
- `PROP_DST right_hand.ikhand dst_from_hand` oscillates **88.4 ↔ 37.5** (documented
  RB3_PROP_POSE_FULL residual med~39/max~43).

MIDI-driver probe (`RB3_MIDIDRV_PROBE`), the decisive discriminator:
- **CTOR 23, ENTER 0, POLL 17 distinct, FEED 0** — drivers created + polled but
  never entered/fed. `[MIDIDRV_POLL]` names all 14 drum `.dmidi` (dir=player3) +
  4 guitar `.dmidi` (dir=player1).
- Poll call stack (`backtrace` → addr2line): `BandDirector::Poll → WorldDir::Poll
  → BandCharacter::Poll → Character::Poll → RndDir::Poll → CharDriverMidi::Poll`.
- `Character::Enter` DOES fire for player0-3 (`[CHAR_ENTER] char='player3
  (char/main/main.milo)'`), but player3's `RndDir::mPolls`=27 contains **zero**
  `CharDriverMidi` at BOTH Enter and Poll time — the drivers are polled through a
  container the one-time Enter never traversed. Mechanism: `RndDir::SyncObjects`
  (Dir.cpp:51-105) rebuilds `mPolls` AFTER the one-time `Character::Enter`, so
  `RndDir::Enter`'s `mPolls` loop misses the drivers while `RndDir::Poll` picks
  them up every frame. (All diagnostic probes in shared TUs were REVERTED.)

## Fix (owned surface only: CharDriverMidi.cpp)

`RB3_MIDIDRV_ENTER_FIX` (default-OFF; #ifdef HX_NATIVE): on the first native
`CharDriverMidi::Poll`, lazily run the missing `Enter()` once per driver
(native-only `std::set` guard — no struct member, Wii layout untouched). This
restores the AddSink subscription that the load-order gap skipped.

## Verification (ON vs OFF, same build)

`RB3_MIDIDRV_ENTER_FIX=1` (evidence/fix-on/, /tmp logs):
- **ENTER 0→21; parsers RESOLVE** (`floortom.dmidi`→`drums_floortom` found=1, etc.
  — the parsers were alive all along); **OnMidiParser FEED 0→173**.
- **drum-hit clips 0→416**: `hihat_hover_hit` 103, `hit_closed` 87, `kick_01`/`kick`
  53+53, `snare_hover_hit` 29 (CHARDRV_PROBE census).
- Drummer band-shard drops **1107→2** (residual now a shoe `wovensteppers`, SKEL);
  guitarist **843→0**. Visual: `fix-on/drums/fixon_coop_d_n0{1,2}_0.png` (hand-fan
  gone, arm lowered to kit), `fix-on/guitar/fixon_coop_g_cg_0.png` (yellow
  stick-fan gone).

W31 set_play NON-REGRESSION (do-not-regress gate):
- stand_rhythm/solo CHARDRV_PLAY plays: OFF=**81**, ON=**80** (~80, unchanged).
- `SETPLAY_SEND`: **26** both; identical guitar/bass/drum sends.

Wii objdiff neutrality (batch_objdiff, base tree):
- `Poll/Enter/__ct__` CharDriverMidi + `__ct__` CharIKMidi = **100.0% raw
  COMPLETE**. `NewSpot` 98.41% / `OnMidiParser` 99.02% = UNCHANGED pre-existing
  residuals (identical to base report; not touched by the HX_NATIVE probes).

## Flag disposition (three-tier rule)

`RB3_MIDIDRV_ENTER_FIX` = retail-proven faithful restoration (the driver IS
entered on Wii; the native load-order gap skips it). Shipped **default-OFF** this
wave per lane flag discipline; **PROPOSED flip to default-ON + opt-out
`RB3_MIDIDRV_ENTER_FIX_OFF`** at coordinator close-out, earned by the ON-vs-OFF
evidence above. `RB3_MIDIDRV_PROBE` = probe/off. Both appended to engine
class.json under lock; no lane regen (coordinator regens once at close-out).

## Files
- `src/system/char/CharDriverMidi.cpp` (fix + probe, HX_NATIVE)
- `src/system/char/CharIKMidi.cpp` (probe only, HX_NATIVE)
- engine `src/platform/NativeCompatFlags.classification.json` (2 flags appended)
