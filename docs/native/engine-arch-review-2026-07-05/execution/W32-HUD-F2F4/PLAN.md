# W32-HUD-F2F4 — PLAN (Lane C)

Base SHA rb3 30546499, engine pin 24c4f95. Build dir: native/build-agent-W32-HUD-F2F4 (clang, exit 0).
Harness: RB3_HTTP=1 RB3_FIXED_CLOCK=1, free ports, pgid-only cleanup.
Owned exclusive-write: native/src/rb3_render_hook.cpp (A2). Engine writes = engineAckNeeded, never stage FxSendNative.cpp (A14).

## TWO mechanisms, TWO checkpoints (one-family REFUTED, do not re-merge)

### F2 — score-pill fill (MEDIUM)
STEP-0: HEADMAT material dump keyed to sb_bg.mesh (score pill). Then bind/blend/prelit fix per flag tier.
A11: fix acts on mechanism the dump NAMES (bind/blend/prelit), NOT a hardcoded pill tint; digits legible white-on-dark.
Two matched retail-paired states.

### F4 — star-row unearned slots (LOW-MEDIUM)
STEP-0: enumerate star-row milo slot meshes + what hides unearned natively vs retail.
A11: captures at TWO matched states with different earned counts; filled==earned AND dim==5-earned in BOTH, retail-paired.

## Lint-4 (A13) registry sweep — BEFORE any "engine drops X" claim
Sweep RB3_HUB_TEXT_CONTRAST, W4.2 text floor, ROWFIX, SCOREBOARD_TOPRIGHT, W2.7 FilterSubdir,
RB3_NO_HUB_HIGHLIGHT_FIX, RB3_NO_BUTTON_GLYPH_FIX (registry = engine NativeCompatFlags.{gen.inc,classification.json}).

## Acceptance per item
mechanism named (dump quoted) -> fix -> retail-paired before/after crops at matched frames ->
drawlog-golden PASS -> objdiff clean if any decomp TU touched.
