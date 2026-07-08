# Lane CROWD — PLAN

**Bug (SWEEP S1, ROI-confirmed):** retail main_hub shows ~3 walking crowd
silhouettes down the center street; native draws ZERO skinned draws there.

**Re-anchored (A4/A5):** the street scene is the shell vignette
`world/vignette/shell/gen/sv3_a.milo_xbox` (NOT main_hub.milo). Walkers are
authored in `crowd_chars.grp`/`characters.grp` + sub-milos
`sv3/a/streetslomo/streetslomo{,_clips,_ao}.milo` + shared
`world/shared/vignette_chars.milo`. NOT WorldCrowd → the crowd-rebind path is
misdirected as primary.

## Step 0 — discriminator (CHECKPOINT verdict BEFORE any fix)

Census the sv3_a crowd actors BY NAME in the LIVE tree at main_hub after a
multi-second dwell. Determine branch:
- (a1) sub-milo/shared-milo LOAD FAILURE (streetslomo.milo / vignette_chars.milo not loaded)
- (b) loaded but NOT DRAWN (objects exist but SetShowing(false)/draw-gated/culled)
- (c) loaded but MIS-POSED/off-screen
- (d) loaded but NEVER ANIMATED (CharClipSet/driver not polled natively)

Tooling:
1. Confirm which dir loads sv3_a in native (DirLoader::Find / roots).
2. Add a read-only `rb3_crowd_census` DTA func (native/src, HX_NATIVE): walk the
   loaded roots incl. the vignette dir; report Character/RndMesh/CharClipSet
   objects with name, dir, showing, worldpos. Name-scoped to sv3_a/streetslomo
   dirs. NO engine edits, NO BandCharacter/Crowd edits.
3. Boot main_hub headless (RB3_HTTP=1 RB3_FIXED_CLOCK=1), dwell several seconds,
   census + ROI query the center-street band + screenshot.
4. RB3_NO_CROWD_REBIND A/B = 1-boot dedupe ONLY.

## Fix (only if tractable, A6 guardrail)

New seam, vignette-dir-scoped (name-gated sv3_a/streetslomo), default-OFF. If
branch a1/d (deep load/CharSync-Poll gap): NARROW + report + hand off.

## Gates
- E1 vs GT (STRUCTURAL — 3 walking figures in center-street band)
- drawlog-792 flag-OFF byte-identical
- batch_objdiff==baseline on any touched src/system unit
- if any src/system/world TU touched: one GAMEPLAY-venue crowd A/B (oracle protected)
