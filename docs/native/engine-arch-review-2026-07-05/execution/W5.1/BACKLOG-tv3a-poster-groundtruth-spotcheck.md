# Backlog (Wave 9, optional, low-priority) — tv3_a cork-board poster ground-truth spot-check

Filed by: W5.1 (Lane C), stage C.S2, per C.S1's recommendation (`W5.1/STATUS.md`,
`/tmp/wave8-checkpoints/C-S1.json`). **This is the only follow-up C.S1 identified — everything
else about SYS-5 ("black poster quads") is CLOSED, not deferred.**

## What's being asked

Confirm, against a Wii/Dolphin retail capture, whether the `tv3_a` transition vignette's central
"SHOW … ALL AGES" cork-board poster (`showtonight_poster` mesh, `show_tonight_poster_01.tex`) is
supposed to read as a **bright/legible** poster in retail, or is genuinely an authored near-black
concert flyer as it renders on native today.

## Why this is optional / low-priority, not a real bug lead

C.S1's census (zero new code, `RB3_HEADMAT_DBG=1`) already proved the mesh is not a rendering
defect on the axes that matter for a fix:

- The texture **binds and samples** (`hasTex=1`, `diffuse='show_tonight_poster_01.tex'`) — this
  rules out every missing-texture / null-diffuse / bind-failure family, including W2.7's.
- The faint embossed "SHOW"/"ALL AGES" outline text is visible **in** the dark field — a
  bind failure or flat-black fallback could not produce that; only a real, sampled, dark-toned
  texture can.
- It is seen during a **transition vignette flythrough** (`tv3_a`, cork-board camera dolly), not
  steady-state gameplay or menu content — the "anomalous" read in the original Wave-6 report was
  an artifact of a fast camera pass across a naturally dark asset, not a persistent visual defect
  a player would dwell on.

So there is no case where a code fix (game/asset-side or engine-side) is warranted **unless**
ground truth shows the retail poster is meaningfully brighter — i.e. this item exists purely to
rule out an authored-vs-lighting-fidelity nuance, not because a bug is suspected.

## If someone picks this up

1. Get a Wii/Dolphin capture of the `tv3_a` cork-board flythrough (same asset:
   `orig-assets/extracted/world/vignette/transition/gen/tv3_a.milo_xbox`) at the point the
   `showtonight_poster` mesh is in frame and roughly matched camera distance/angle to
   `census/tv3a_corkboard_black_poster.png`.
2. Compare tone/legibility side by side. If retail is comparably dark → **no action, close
   permanently**. If retail is clearly brighter (poster legible, not a near-black field) →
   file a fresh Wave-9+ item scoped to the render-backend levers C.S1 already named:
   - `RB3MaterialBinder.cpp` unlit/`useEnviron` handling for this material, or
   - `Rnd_Wgpu_RB3.cpp` vignette-scene lighting engagement for `tv3_a`.
   Both are engine files outside every Wave-8 lane's fence — this would be a fresh staged patch,
   not a continuation of W5.1.
3. Either way, this is a single spot-check, not a re-open of the full census — do not re-run
   `census.py`/`vignette.py` unless the retail comparison itself motivates new census questions.

## Status

Not started. Filed 2026-07-06 by W5.1/C.S2. No owner assigned.
