# W30-BAND-PERF-CLIP — PLAN

**Base SHA:** `fdc4d628` (post-CA-adoption; CA7). Engine pin `17807afd…` unchanged.

## Question
W29 raw census proves the on-stage band plays ONLY idle + expression clips in-song
(`stand_realtime_idle_*`, `exp_rocker_*`/`exp_banger_*`, `still`, `idle_b_*`; grep 0
for drum/tom/snare/hihat/groove/tip). Retail plays instrument-performance clips.
Name the missing mechanism.

## Architecture found (read-first, before any probe)
- Only C++ sender of `play_group` = `BandCamShot::StartAnim` (BandCamShot.cpp:351),
  sending `cur.mAnimGroup` per target char. `set_play` has NO C++ sender (DTA-only) —
  confirms CA6.
- Receiving hub: `BandCharacter::SetState(cc, playFlags, mask, …)` (:3870) — sets
  `mGroupName`, then `PlayMainClip` (:404) does `clipdir->Find<CharClipGroup>(mGroupName)`
  and picks a clip by flag mask. `play_group`→`OnPlayGroup`→`PlayGroup`→`SetState`;
  `set_play`→`OnSetPlay`→`SetState` (changes intensity flags, keeps group).
- So the load-bearing datum = **what `mGroupName` values the on-stage band receives
  in-song**: only `realtime_idle`/face groups (⇒ gap is on the SEND side: camshot data
  or song anim-event stream), vs performance group names that fail to resolve in
  `mClips` (⇒ asset/loading gap).

## STEP 0 — three discriminators (checkpoint BEFORE any lever)
- **(i) Call census.** `BANDPERF_STATE` in `SetState` (unfiltered by char name) +
  `BANDPERF_STATE_BT` symbolized backtrace (execinfo, same pattern as CHARDRV_PLAY_BT).
  Records every group/state request in-song and who dispatched it.
- **(ii) Asset census.** `BANDPERF_CLIPS` one-shot enumeration in `PlayMainClip` via
  the bound driver's `ClipDir()` (CA8): print mClips PathName + every `CharClipGroup`
  and `CharClip` name once per clipdir. Answers: are performance groups RESIDENT?
- **(iii) Event-stream census.** Same `BANDPERF_STATE` log, in-song window: count
  distinct group requests; 0-arriving vs idle-only-arriving is the split. Send side
  `BANDPERF_SHOT` in `BandCamShot::StartAnim` logs each `mAnimGroup` dispatched.

Probes: `RB3_BANDPERF_PROBE` (enable), `RB3_BANDPERF_BT` (backtrace),
`RB3_BANDPERF_CLIPS` (enumerate). All getenv-gated default-OFF, `#ifdef HX_NATIVE`,
byte-identical `#else` (pure additions ⇒ Wii `.o` unchanged).

## Then
ONE lever at the layer STEP 0 names (`RB3_BAND_PERF*`), or honest recharter if the
mechanism spans >1 lever. Bounds: ≤6 boot runs.

## Owned surfaces
BandDirector.cpp/.h, BandCharacter.cpp/.h, BandCamShot.cpp/.h (probe-only per CA4),
BandPerformer.cpp (negative-check per CA6), execution/W30-BAND-PERF-CLIP/**.
