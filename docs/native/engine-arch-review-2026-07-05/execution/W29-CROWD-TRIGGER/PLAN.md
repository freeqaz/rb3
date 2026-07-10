# W29-CROWD-TRIGGER — PLAN

**Lane:** primary Wave-29 world/vignette scene-trigger lane. **Charter:** make the
main_hub crowd walkers animate per the W28 RECHARTER acceptance target set by finding
and fixing WHY the streetslomo scene never triggers its walk clips. STEP-0
discriminator-first, checkpoint-before-fix binding.

## STEP 0 — three discriminators (all checkpointed BEFORE any lever)

Boot recipe (new script `scripts/native/_w29_crowd_trigger_boot.py`): ONE boot,
`CHARDRV_PROBE='*' CHARDRV_BT=1 RB3_CROWD_PANEL_DBG=1 RB3_FIXED_CLOCK=1`, stderr+stdout
→ one file, hold ~22 s at main_hub through the splash(sv8 cityscape)→main_hub(sv3
streetslomo) transition. Key change vs W28: `CHARDRV_PROBE='*'` (W28 used `=crowd`,
which filtered out the actual walkers).

- **(i) Working-reference trace** — symbolize the `CHARDRV_PLAY_BT` chains for the
  beat-0 cityscape `crowd1-5` plays AND the beat-2.433 streetslomo plays; name the
  ISSUING mechanism with PathName. Checkpoint `crowd-step0-i.json`.
- **(ii) CharCache/FileMerger discriminator (W28-E1)** — is `C13_PROBE`
  (CharCache.cpp:68) in the working play path? Checkpoint `crowd-step0-ii.json`.
- **(iii) streetslomo trigger census** — runtime `/api/dta/eval` + static
  `sv3_a.milo_xbox` strings; name why `PanelDir::Enter streetslomo_ao nTriggers=0`.
  Checkpoint `crowd-step0-iii.json`.

## Lever / outcome

Chosen at the STEP-0 checkpoint from the STEP-0 verdict. STEP-0 REFUTED the W28
premise (the walk IS triggered and renders) → **Lever B (honest re-charter)**, outcome
`RECHARTER`. No fix code — a lever would hack correct behavior. Flag
`RB3_VIGNETTE_TRIG_REPLAY` reserved per CA5, UNUSED (see `crowd-step0-flag.json`).

## Gates (STATUS table)

STEP-0 checkpoints before any lever · batch_objdiff N/A (no Wii function touched) ·
drawlog-golden `--fixed-clock --canonical-order` 792 (flag-OFF) · rb3-tests · boot A/B
N/A (no flag) · A7 evidence honesty.

## Owned files (this lane, staged by path)

- `scripts/native/_w29_crowd_trigger_boot.py` — NEW STEP-0 boot harness.
- `docs/.../W29-CROWD-TRIGGER/{PLAN.md, STATUS.md, RECHARTER.md, evidence/*}`.

READ-ONLY (Lane 2's / probes pre-exist): `CharDriver.cpp`, `CharClip*.cpp`,
`CharIKHand.cpp`, `boot-to-song.py`. PROTECTED (no touch): `Crowd.cpp:884-1000`
oracle, RndMesh loader, sidecar/goldens, census/classjson, pin, `rb3_session_trace.cpp`,
engine `FxSendNative.cpp`.
