# W4.3-C2b4 PLAN — C2b (song-select album art) + C4 (hub ticker) Y-anchor family

KEY=W4.3-C2b4, STAGE=C2b+C4 (SYS-5 layout offsets). Own build dir
`native/build-agent-W4.3-C2b4`. Engine pin 44716f4 (untouched).

## Read first
- `docs/native/engine-arch-review-2026-07-05/execution/WAVE13_KICKOFF.md` (A1-A10 binding)
- `docs/native/engine-arch-review-2026-07-05/execution/W4.3-C2b4/COORDINATOR_NOTE.md`
  (mid-wave note: C4's real root cause is Y-anchor spacing not font-scale/wrap,
  per the Wave-12 C34 side agent; "same family as C2b" is explicitly flagged as
  likely FALSE and to be tested on C2b's own merits)
- `execution/W4.3-C2a/STATUS.md`, `execution/W4.3-C2/STATUS.md`,
  `execution/W4.3-C34/STATUS.md` (prior grounding)

## Exact file ranges (re-derived by symbol on current tree)
- `src/band3/meta_band/MainHubPanel.cpp`
  - `Poll()` — diagnosis comment block + `RB3_HUB_TICKER_DBG` probe
  - `UpdateHeader()` at line 301 — `RB3_HUB_TICKER_YFIX` fix
- `src/band3/meta_band/SongSelectPanel.cpp`
  - `FinishLoad()` at line 44 — `RB3_SS_ART_YFIX` fix (new) + pre-existing
    `RB3_C2B_XFM_DBG` / `RB3_C2B_ART_NUDGE` calibration probes (this wave)
  - `Poll()` at line 360 — `RB3_C2B_XFM_DBG` re-log (settle-state confirmation)
- `src/system/bandobj/InlineHelp.cpp` — `DrawShowing()` ~line 388, extended
  `RB3_HOLDLABEL_DBG` probe (wrap_width/size)
- `src/system/rndobj/Text.h` — added `WrapWidth() const` getter after `SetSize`
- `src/system/ui/UILabel.cpp` — `DrawShowing()` ~line 294-307, extended
  `RB3_HOLDLABEL_DBG` probe (wrap_width/size)

## Method
1. Probe authored milo xfm vs drawn rect for each element (getenv-gated,
   HX_NATIVE only).
2. C4: picked up from the C34 side-agent's font-scale/wrap hypothesis; probe
   data REFUTED it (real smaller size + real nonzero wrap already applied).
   True cause = Y-anchor spacing (message.lbl vs expand_message_area.ihp,
   ~6u vs a confirmed-good ~20.8u pair). Empirically nudge Z, confirm no
   occlusion, ship default-OFF flag fix in MainHubPanel::UpdateHeader().
3. C2b: raw WorldXfm() comparison between album_art.grp/header.grp did NOT
   cleanly indicate the gap (100s-of-units differences on both X and Z,
   unlike C4's clean single-axis pair) — pivoted to the same empirical
   nudge-and-capture method as C4, but discovered wall-clock settling produces
   a spurious non-monotonic trend (entrance-animation timing jitter across
   separate process launches); switched to frame-count-settled captures
   (poll `/api/health` frame counter, wait for +200 frames past
   song_select_screen entry) which gave a clean monotonic result. Calibrated
   -120.0f on album_art.grp's local Z as the fix magnitude.
4. Both fixes: game-side, `#ifdef HX_NATIVE` + `getenv`-gated, default-OFF.
   No engine render TUs touched (A9 respected; Lane G owns that region).

## Gates
- Before/after captures vs retail for E1: done for both (see STATUS.md).
- Drawlog 792 flag-OFF unchanged: `python3 scripts/native/drawlog-golden.py
  --fixed-clock --canonical-order --bin native/build-agent-W4.3-C2b4/rb3-native`
  → PASS (792 draws, 237 known-residual divergences within bound) with both
  new flags unset — confirms flag-OFF is unreachable/no-op by construction.
- Settle-frames: frame-count-based settling (not wall-clock) required for the
  C2b calibration; a wall-clock-only harness produces a false-negative-looking
  non-monotonic trend. Documented as a methodology finding for future lanes.

## Verdict on task #80 (shared-family hypothesis)
See STATUS.md "Shared-family verdict" section — PARTIAL/INCONCLUSIVE-BUT-NOT-BLOCKING:
both are Y-anchor/panel-origin-family symptoms (an authored spacing/position gap
between two siblings that the native renderer doesn't correct), but the concrete
mechanism differs (single-axis label-to-label spacing for C4 vs a diagonal/
multi-axis whole-group offset for C2b) — not literally "the same bug", so each
ships as its own independent flag/fix rather than a single shared mechanism.
