# W4.3-C1 — Focused-text contrast (hub / song-select row / partdiff GUITAR)

Key: W4.3-C1. Stage C1. Checkpoint: /tmp/wave12-checkpoints/C1.json.

## Acceptance binding (A7, A11)
- SKIP regression bisect — binder byte-unchanged a94762f..146fd19; Wave-7 flag-ON
  already shows pale-on-gold. START at path tracing.
- Gate (A11, re-runnable): focused-bar ROI, text stroke = p5 luma, bar field = p60
  luma, PASS = p60/p5 >= 2.0. Retail calibrates ~3-6:1; current build ~1.1-1.3.

## Declared edit ranges (before editing)
- milo-native-engine/src/platform/RB3MaterialBinder.cpp: insert a flag-gated clamp
  right after the `mu.color[3] = c.alpha` assignment (~line 134) — new block only,
  no reorder of existing lines. (LANDED.)
- milo-native-engine/src/platform/NativeCompatFlags.classification.json: append one
  flag row after RB3_UI_TEXT_FLOOR_STRICT (append-only, under classjson flock).
- Diagnostic probes (temporary, all reverted before commit): RB3MaterialBinder.cpp
  (RB3_C1_DBG), Rnd_Wgpu_RB3.cpp (RB3_C1_ORDER), src/system/ui/UILabel.cpp
  (RB3_UILABEL_DBG). NONE remain in the tree.

## Fence
- Do NOT edit Lane B's Rnd_Wgpu_RB3.cpp regions (:4360-4735). The gate-passing fix
  turned out to live in the render/postproc path — ESCALATED, not edited here.

## Harness
- /tmp/c1-diag/diag.py <tag> [ENV=val ...] — headless boot to main_hub, PNG capture.
- /tmp/c1-diag/gate.py <img> x0 y0 x1 y1 — p60/p5 ROI gate.
