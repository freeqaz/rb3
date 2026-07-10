# W27-CROWD — PLAN

Lane: hub crowd walkers repair (ui/world panel-residency), per WAVE27_KICKOFF.md
"Lane W27-CROWD" + COORDINATOR ACCEPTANCE A1-A10.

## Premise (from W26 hand-off)
W26 root-caused the missing hub crowd walkers to a UI panel-unload teardown:
`splash->main_hub` transition unloads the `streetslomo` vignette panel's `WorldDir`,
tearing down the crowd `CharClipSet`; `streetslomo_clips.milo` never reloads, so the
8 crowd drivers stay `animating=0`. Charter = restore residency (lever A) or reload+
re-fire (lever B) from the ui/world/interstitial layer, flag-gated default-OFF.

## STEP 0 (binding, checkpoint-first) — executed
Per A1/A2, the faithful retention mechanism is a **refcount handshake**, and the
Wii-GT hypothesis (sv3 resident across splash->hub via interstitial->regular-panel
refcount overlap) is pre-pinned from the Xbox DTA. The binding native question:
**which panel/WorldDir owns the streetslomo CharClipSet natively, and where does its
refcount hit 0?**

Discriminator probes added (all `#ifdef HX_NATIVE`, env-gated `RB3_CROWD_PANEL_DBG`,
byte-inert to the decomp build — 7/7 touched fns 100% objdiff):
1. `UIPanel::CheckLoad`/`CheckUnload` — per-panel `mLoadRefs` transition (names the
   panel + refs, resolves the W26 UnloadPanels-vs-UnloadInterstitials ambiguity).
2. `UIScreen::LoadPanels`/`UnloadPanels` — call-site markers (screen name).
3. `BandScreen::LoadInterstitials`/`UnloadInterstitials` — interstitial resolution.
4. `PanelDir::Enter` — which PanelDir enters at the transition + its trigger list.

Cross-checked against the existing `CHARDRV_PROBE=crowd` (CharDriver Enter/Clear/Poll
state, read-only) and `{rb3_crowd_census}` DTA tool.

## Outcome
**Panel-residency REFUTED.** See STATUS.md. The panel is resident; the crowd freeze
is a char-layer clip-REPLAY divergence, not a ui/world teardown. No faithful in-grant
lever exists. Delivered: refutation + corrected root cause + char/world hand-off.

## Gates targeted
batch_objdiff (7 touched fns) · drawlog-golden flag-OFF · rb3-tests 116/0 ·
boot A/B (no flag — stability) · A7 revisit refcount/leak + census · A10 prewarm rail.
