# W28-CROWD-OWNER — PLAN

**Lane:** primary CROWD ownership lane, Wave 28. **Charter:** name WHO owns the clip
set the hub crowd drivers resolve against and WHOSE crowd we have measured since W23,
then apply exactly ONE lever (A fix / B re-charter). Discriminator-first,
checkpoint-before-fix binding.

## STEP 0 — the four discriminators (all before any fix code)

Probes added this wave (all `#ifdef HX_NATIVE`, gated under existing envs, byte-inert
Wii `#else`):
- **A1 beat stamps** — `UIScreen::UnloadPanels` marker + `UIPanel::CheckUnload` UNLOAD
  line (`src/system/ui/{UIScreen,UIPanel}.cpp`, probe-line-only).
- **A2a CLIPSWAP** — unsampled `gPrevClips` per-driver transition detector in
  `CharDriver::Poll` (+ `PathName` ownership chains) and a `src=setclips` attribution
  line in `CharDriver::SetClips`.
- **A2b DEFCLIP** — serialized default-clip name captured at LOADS via a manual
  read+resolve replicating `ObjPtr::Load` (the milo chunkstream only seeks forward, so
  peek+seek-back faults — see STATUS "false start").

**Boot recipe:** ONE boot, `RB3_CROWD_PANEL_DBG=1 CHARDRV_PROBE=crowd CHARDRV_BT=1
RB3_FIXED_CLOCK=1`, stderr+stdout → one file, hold 18 s at main_hub
(`scripts/native/_w28_crowd_step0_boot.py`).

1. **Name the torn-down owner** — symbolize the CHARDRV_REPLACE_BT backtrace, interleave
   with the panel-unload markers.
2. **Ownership chains** — PathName() of drivers + bound clip sets at every mClips swap.
3. **E5 DEFCLIP** — serialized default-clip name; decide NULL faithful vs gap.
4. **Wii-GT identity** — sv3_a vs sv8; main_hub.dta:744; runtime dir dumps decisive.

Checkpoint: `/tmp/wave28-checkpoints/CROWD-step0.json` (owner-verdict, identity-verdict,
per-item evidence, chosen lever + why).

## Lever (chosen from STEP-0 verdict)

- **A** (`RB3_HUB_CROWD_CLIPBIND`, default-OFF): fix the clip-set BINDING layer — ONLY
  if the dump shows the drivers are wrongly bound to a dead/non-resident set.
- **B** (re-charter): if the observed crowd is splash-owned and faithfully dies, name
  the REAL hub walkers + their native state + the acceptance target set, and STOP.

## Gates (STATUS table)

STEP-0 checkpoint · batch_objdiff (touched == baseline) · drawlog-golden flag-OFF 792 ·
rb3-tests 116/0 · prewarm boot (ui edit) · lever-specific acceptance · A7 evidence honesty.

## Owned files

`src/system/char/CharDriver.cpp` (probes + lever-A writes), `CharClip*.cpp` (read+narrow),
`src/system/ui/{UIScreen,UIPanel,PanelDir}.cpp` (probe-line / read), native probe files.
NOT owned: Crowd.cpp:884-1000 oracle, RndMesh loader, hands/FOREARM (CLOSED),
rb3_session_trace.cpp, FxSendNative.cpp.
