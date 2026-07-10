# W30-BAND-PERF-CLIP — STATUS

**Outcome: PARTIAL** — mechanism NAMED with symbolized-backtrace evidence and PROVEN
by a songMs-matched A/B, via a **default-OFF DEMONSTRATION lever**
(`RB3_BAND_PERF_FORCE_PLAY`). All mechanical acceptance bullets met. The lever is
explicitly NON-FAITHFUL (it hardcodes a fixed Play intensity); the FAITHFUL
reproduction — dispatching the song-authored venue-mood `set_play` stream — spans more
than one lever and is **recharter-scoped for Wave 31**.

Base SHA `fdc4d628` (CA7 post-CA-adoption). All edits `#ifdef HX_NATIVE` → Wii `.o`
byte-identical.

## Named mechanism (working-reference style)

The on-stage band plays only idle+expression clips in-song because **the
performance-intensity selector never advances past IDLE.** Every in-song band-anim
request is a `play_group` dispatched by `BandCamShot::StartAnim`'s DTA anim-script; it
sets the correct posture GROUP (`stand`/`sit`) but leaves `mPlayFlags` at the
idle-realtime mask `IR` (`0x1000`). The performance clips (`stand_rhythm_*` = flag
class `P`/`PM`, `stand_solo_*` = `PS`) ARE resident in that same group, but
`PlayMainClip`'s `GetClip(mask)` only ever asks for `IR`, so it resolves
`stand_realtime_idle_*`/`stand_idle_*`. The **only** code path that rewrites the
intensity bits of `mPlayFlags` is `BandCharacter::OnSetPlay` (the `set_play` message:
`mPlayFlags & 0xFFF80FFF | newflags`). `set_play` has **no C++ sender anywhere in the
tree** (grep-proven — only the `Symbol` declaration); it is dispatched purely by DTA
scripts tied to the song's venue/mood authoring. Natively `set_play` fires **only at
beat 0** during init (empty group, flags left `IR`) and **never in-song**, so the band
never leaves idle. Retail's authored `[play]`/`[intense]`/`[mellow]`/`[solo]` venue
mood events drive that `set_play` stream; the native build does not dispatch it.

**Symbolized dispatch backtrace** (census1, addr2line against the run binary):
```
BandUI::Draw → UIManager::Poll → UIScreen::Poll → UIPanel::Poll → WorldDir::Poll
 → CameraManager::PrePoll → CameraManager::StartShot_ → BandCamShot::StartAnim
  → Hmx::Object::HandleType → DataArray::ExecuteScript → ExecuteBlock → DataIfElse
   → BandCharacter::Handle → OnPlayGroup → PlayGroup → SetState  [only play_group; mask=2]
```
FlagString decode (`BandCharacter::FlagString`, BandCharacter.cpp:1066): idle class
`IR`=0x1000 `I`=0x2000 `II`=0x4000; **play class** `PM`=0x8000 `P`=0x10000 `PI`=0x20000
`PS`=0x40000. Resident `stand_rhythm_norm=0xe0310000` (`&0x7F000`→`P`),
`stand_rhythm_mel=0xe0b08000` (`PM`), `stand_solo=0xe0340000` (`PS`) vs idle
`stand_idle_norm=0x307003` (`IR|I|II`).

## Acceptance (quoted verbatim from kickoff Lane 1, as amended by CA1-CA8), self-graded

> "- Named mechanism with symbolized-backtrace evidence; raw stderr gzipped into
> `execution/W30-BAND-PERF-CLIP/evidence/raw/` as committed deliverables; STATUS
> carries a per-log `grep -c` count table for EVERY probe tag (A7)."

MET. Mechanism named above with symbolized backtrace; 4 raw gz committed under
`evidence/raw/`; A7 table below.

> "- If lever landed: songMs-matched `--fixed-clock` A/B (pair by nearest songMs from
> health.jsonl, never shot index) where the ON run's `CHARDRV_PLAY` census shows
> instrument-performance clips playing in-song (nonzero plays of drum/strum/groove-
> class clips with sustained playing counts) and OFF reproduces the W29 idle-only
> census. Screenshot pair of a band member mid-performance."

MET. `--fixed-clock` A/B, songMs-paired (OFF 2582/6970/11314/15671/20071 vs ON
2569/6956/11318/15664/20074). ON `CHARDRV_PLAY` census = **55** rhythm/solo plays
(`stand_rhythm_norm/ext/mel_*`, `stand_solo_norm_*`, sustained across the window); OFF =
**0** (idle-only, reproduces W29). Screenshot pair
`evidence/shots/bandperf_{OFF,ON}_songMs1131x.png`. CAVEAT (honest): the ON run is the
DEMONSTRATION lever, not the faithful path.

> "- Gates: touched-decomp-file `batch_objdiff` baseline-exact; `python3
> scripts/native/drawlog-golden.py --fixed-clock --canonical-order` PASS; rb3-tests
> clean; boot A/B flag-ON no-crash. Wii `.o` byte-identical (probes/lever inside
> `HX_NATIVE`)."

MET — gates table below.

> "- Harness: `scripts/native/boot-to-song.py` (guitar-only `part:` verb — do NOT
> charter vocals/drums/keys runs; all members animate regardless of played part).
> Bounds: ≤6 boot runs before requesting recharter."

MET. All runs guitar/expert via boot-to-song.py. **4 boot runs used** (census1,
census2, abOFF, abON) of 6 budget.

## STEP-0 discriminators (checkpointed BEFORE the lever; discriminator-first)

`/tmp/wave30-checkpoints/perfclip-step0-{i,ii,iii}.json` (mirrored to
`evidence/checkpoints/`), all `buildSha=fdc4d628`.

- **(i) Call census** — SetState/PlayGroup/OnPlayGroup ARE called in-song (194×), ALL
  `play_group` (mask=2) from `BandCamShot::StartAnim`. `set_play` (mask=3) only at
  beat 0 init. Groups requested = `stand`/`sit`/`closeup`/`extreme_closeup` at flag
  `IR` (0x1000).
- **(ii) Asset census** — performance clips ARE RESIDENT. Gameplay clipdir =
  `body_clips (char/main/main.milo)`; `stand` group holds `stand_rhythm_*` (P/PM) +
  `stand_solo_*` (PS) alongside `stand_idle_*`; `sit` holds `idle_play_*`. **NOT a
  loading gap.** (First snapshot caught `body_clips` pre-SyncObjects with `groups={}`;
  fixed guard captured the populated groups.)
- **(iii) Event-stream census** — DTA-script dispatch WORKS (`play_group` parses +
  dispatches via `ExecuteScript`). The **intensity** stream (`set_play`) is DEAD
  in-song: no C++ sender, DTA-only, never fires. events-arrive-but-idle-only (group
  stream) vs zero (intensity stream).

## Lever

`RB3_BAND_PERF_FORCE_PLAY` — default-OFF, `#ifdef HX_NATIVE`, in
`BandCharacter::PlayMainClip`. When set and the requested mask is idle-class only
(idle bit in `0x7000`, no play bit in `0x78000`), promote it to `P` (0x10000) so
`GetClip` resolves a resident rhythm clip. Proves the gap is SELECTION not residency.
**NON-FAITHFUL** (hardcodes a fixed Play intensity, ignores song-authored mood
transitions and idle-during-breaks) — declined to mislabel it as the fix, per the
W29 Part-A no-fake-match discipline. The faithful fix (RECHARTER Wave-31) is to
dispatch the song's venue-mood `set_play` stream.

## A7 probe-count table (`grep -ac` on committed gz — coordinator greps these, NUL→`-a`)

| tag | census1 | census2 | abOFF | abON |
|---|---|---|---|---|
| BANDPERF_STATE | 194 | 168 | 87 | 4444 |
| BANDPERF_STATE_BT | 194 | 0 | 0 | 0 |
| BANDPERF_CLIP | 141 | 113 | 110 | 94 |
| BANDPERF_CLIPS | 10 | 10 | 0 | 0 |
| BANDPERF_CLIPFLAGS | 0 | 10 | 0 | 0 |
| BANDPERF_SHOT | 117 | 102 | 98 | 98 |
| CHARDRV_PLAY | 0 | 0 | 264 | 232 |

(BT only enabled in census1; CLIPS/CLIPFLAGS only in the CLIPS runs; CHARDRV_PLAY only
in the A/B runs where `CHARDRV_PROBE='*'` was set — honest per-run gating.)

**Headline number (recomputable from gz):** rhythm/solo `CHARDRV_PLAY` plays —
`abOFF_forceplay.log.gz` = **0**, `abON_forceplay.log.gz` = **55**
(`grep -acE "CHARDRV_PLAY.*clip='(stand_rhythm|stand_solo)"`).

## Gates

| gate | requirement | result |
|---|---|---|
| batch_objdiff `SetState__13BandCharacterFPCciibb` | == baseline | **99.08% PASS** (HX_NATIVE-only) |
| batch_objdiff `PlayMainClip__13BandCharacterFib` | == baseline | **92.2% PASS** (HX_NATIVE-only) |
| batch_objdiff `StartAnim__11BandCamShotFv` | == baseline | **100.0% PASS** (HX_NATIVE-only) |
| Wii `.o` byte-identical | all edits HX_NATIVE | **PASS** — 5 `#ifdef`/`#endif` balanced, 3 includes in pre-existing HX_NATIVE blocks; zero Wii-path lines |
| `drawlog-golden --fixed-clock --canonical-order` (default-OFF) | 792 PASS | **792 PASS** (307 known-residual within bound) |
| rb3-tests | 116/0, 7 skip | **116 pass / 0 fail / 7 skipped** (= W29 baseline; exit-time teardown SIGSEGV post-completion, tolerated) |
| boot A/B flag-ON no-crash | reachable | **PASS** — abOFF + abON both rc=0, 5/5 shots into gameplay |

## Retire-list probes used in STEP 0 (CA3 rider — coordinator executes retirement)

`CHARDRV_PLAY` (KEEP-list, used in A/B) — not on the retire list. **retire-list probes
used in STEP 0: none.** (Retire list: CHARDRV_ENTER, CHARDRV_REPLACE,
CHARDRV_REPLACE_BT, CHARDRV_DEFCLIP, CHARDRV_STARVE, CHARDRV_LIFE, C13_PROBE,
RB3_CROWD_PANEL_DBG — none used.) New `BANDPERF_*` probes + `RB3_BAND_PERF_FORCE_PLAY`
lever are live and default-OFF; coordinator decides retention at close-out.

## Files touched
- `src/system/bandobj/BandCharacter.cpp` — BANDPERF probes (SetState census + BT,
  PlayMainClip CLIPS/CLIPFLAGS enum + CLIP select) + `RB3_BAND_PERF_FORCE_PLAY` demo
  lever; `<execinfo.h>` + `char/CharClip.h` includes (both in existing HX_NATIVE block).
- `src/system/bandobj/BandCamShot.cpp` — `BANDPERF_SHOT` send-side probe (+`<cstdio>`).
- `docs/native/engine-arch-review-2026-07-05/execution/W30-BAND-PERF-CLIP/**`.
