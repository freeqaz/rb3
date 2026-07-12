# W31-HUD-GLYPHS — STATUS (Lane B)

Base SHA fd119705, engine pin b36bcfc. Build: native/build-agent-W31-HUD-GLYPHS (clang, exit 0).
Harness: RB3_HTTP=1 RB3_FIXED_CLOCK=1 RB3_UI_FLOOR_DBG=1, free ports, pgid-only cleanup.

## Lint-4 registry sweep (BEFORE any mechanism claim)
Engine flag registry = src/platform/NativeCompatFlags.{h,cpp,gen.inc,classification.json}
(no NATIVE_COMPAT_LEDGER.md file exists; the .gen.inc + classification.json ARE the registry).
Checked RB3_HUB_TEXT_CONTRAST, W4.2 UI text floor (RB3_UI_TEXT_FLOOR_STRICT, default-relaxed),
ROWFIX, SCOREBOARD_TOPRIGHT, FilterSubdir white-texture (W2.7 black-head). NONE of these act on
the useAlphaAsRGB text/color-icon classification — they adjust material COLOR/contrast or scoreboard
POSITION, downstream/orthogonal to the glyph-atlas RGB-vs-alpha routing. No shipped default-ON flag
contradicts the F3 mechanism below.

## STEP-0 — F3 glyph traced end-to-end (song_select footer pill = anchor)
Mechanism NAMED: **color-icon misclassification -> useAlphaAsRGB collapses RGB glyph artwork to a
solid white cell-mask blob.**
Culprit material: **buttons.mat** (button-prompt glyph font, milo buttons.milo_xbox), 7150 draws/frame.
Trace:
1. milo: button-prompt glyph = unnamed RndText submesh from buttons.mat (RGB artwork + alpha = cell mask).
2. engine RB3MaterialBinder.cpp:53  isTextMeshHeur = mesh->Name()[0]=='\0' -> TRUE.
3. game  rb3_render_hook.cpp:245     isColorIcon set ONLY if matName contains 'icon'; 'buttons.mat'
   lacks 'icon' -> isColorIcon=FALSE.
4. engine RB3MaterialBinder.cpp:336  useAlphaAsRGB = (isTextMeshHeur && !isColorIcon) -> TRUE.
5. shader standard_wgsl.inc:768       baseColor.rgb = white * diffuseSample.a -> solid-cell alpha,
   RGB artwork DISCARDED -> SOLID WHITE BLOB.
Confirmed buttons.mat is a color-icon (RGB detail + alpha cell mask): retail MENU glyph is a white
'+' in a circle (retail_footer.png) — the '+' detail lives in RGB, alpha is the solid circle; our
render collapses to a featureless white DOT (crop_footer_overshell.png).

RB3_UI_FLOOR_DBG material census at song_select (draws/frame):
  Pentatonic_* (letter fonts, alpha->RGB CORRECT) — many
  buttons.mat 7150         <- lacks 'icon' -> WHITE BLOB (F3 culprit)
  icons_tour.mat 5203      <- contains 'icon' -> color-icon path OK
  instrument_icons_small(.mat/_zdisable) 5120+1886 <- contains 'icon' -> OK
  instrument_icons.mat 1236 <- contains 'icon' -> OK

## Fix TU + A8 gate
Fix TU = native/src/rb3_render_hook.cpp QueryDrawMaterialPolicy :245 (GAME-SIDE, one-line predicate
extension: also p.isColorIcon=true when matName contains 'buttons', behind default-OFF flag
RB3_BUTTON_GLYPH_ICON). Per Lane B grant, rb3_render_hook.cpp edits require COORDINATOR SIGN-OFF
-> engineAckNeeded=true, NO write performed this wave. Proposed edit recorded in checkpoint.

## Late-add (2026-07-12) — song-select per-instrument difficulty icons
Verdict: **PRESENT** on a focused SONG row (Been Caught Stealing / Jane's Addiction,
21/22_song_select_focused*.png, crop_diffsidebar.png). instrument_icons_small.mat CONTAINS 'icon'
-> already gets the color-icon RGB path -> guitar/bass/drums/keys/mic rings + PRO variants + 5-pip
difficulty rows all render with artwork (one guitar row shows red-filled pips). NOT missing, NOT
white-fallback. The user's 'missing' report was almost certainly a header/shortcut row focused
(no song -> no icons, correct). Does NOT join the fix class; sidebar remains faithful (2026-07-02).

## F3 class verified across screens (single mechanism)
- song_select: footer 'HOLD TO MAKE SETLIST' + 'HOLD FOR SHORTCUTS' pills WHITE; 'num' comma over
  '123'; Player-1 top-right disc WHITE (b_footer_whiteblobs_F3.png, b_songselect_focused_header.png).
- overshell: 'MENU' button dot WHITE (both song_select + hub).
- hub: overshell 'MENU' dot WHITE (b_main_hub.png / hub_bottom). Same buttons.mat everywhere.
One mechanism, one fix, whole class.

## SPLIT MEMO — F2/F3/F4 are THREE DISTINCT mechanisms (charter 'one family' hypothesis REFUTED)
Evidence: b_native_pill_star_F2F4.png (native) vs b_retail_pill_star.png (retail).

- **F3 = color-icon misclassification (buttons.mat).** CONFIRMED. Fix = 1-line predicate in
  rb3_render_hook.cpp:245 (add 'buttons' to the isColorIcon strstr) behind default-OFF flag
  RB3_BUTTON_GLYPH_ICON. Risk LOW (buttons.mat proven RGB-artwork+alpha-mask via retail '+' glyph).
  A8: rb3_render_hook.cpp needs COORDINATOR SIGN-OFF -> engineAckNeeded, NO write this wave.
  PRICE: ~1 line + flag append; verification = re-run song_select/hub capture, footer pills show
  the '+'/RB button artwork instead of white blobs.

- **F2 = score-pill fill renders WHITE (not the glyph family).** CONFIRMED distinct. Retail pill =
  DARK silver-rimmed face + white digits; native = WHITE/light glossy face + dark digits. The pill
  is a NAMED HUD panel/background mesh, NOT an unnamed RndText glyph -> it does NOT route through
  useAlphaAsRGB. Candidate causes (need a targeted pill-mesh material dump to pin the line): (a) the
  dark fill texture unbound -> white fallback; (b) prelit/unlit forcing the white base colour;
  (c) blend translucency. PRICE: 1 probe boot (HEADMAT-style dump keyed to the pill mesh name) +
  a bind/blend fix in the HUD material path. MEDIUM. Separate sub-charter — NOT covered by the F3 fix.

- **F4 = star-row unearned-slot visibility (NOT a texture-bind at all).** CONFIRMED distinct. Retail
  draws the full 5-slot star row (4 filled + 1 dim outline); native draws only earned slots (1 badge
  at ~1 star). The empty slot meshes are hidden by showing-state/anim-frame, not mis-textured.
  PRICE: inspect the HUD star-row milo group for 5 authored slot meshes + why unearned slots are
  hidden natively (showing state). LOW-MEDIUM. Separate sub-charter — orthogonal to F2 and F3.

## VERDICT
Charter's 'ONE HUD material/texture-bind family' is REFUTED: F2/F3/F4 are three independent
mechanisms. F3 (the chartered anchor) is fully traced end-to-end with a named TU + priced 1-line
fix, but its fix TU (rb3_render_hook.cpp) is coordinator-sign-off-gated -> engineAckNeeded=true,
NO code written. F2 and F4 each priced as separate sub-charters. Late-add difficulty icons = PRESENT
(not a defect on a focused song row).

## COORDINATOR ACK + F3 LANDED (close-out, 2026-07-12)

A8 sign-off GRANTED. The F3 fix landed coordinator-executed in
native/src/rb3_render_hook.cpp (predicate extension after B10), following the
B8 opt-out precedent instead of the proposed opt-in flag: fix is **default-ON**
with escape hatch `RB3_NO_BUTTON_GLYPH_FIX` (classification row default=on,
engine 24c4f95). Default-ON earned by ON-vs-OFF song_select captures on the
merged tree (song-select-capture.py, depth 0):
- OFF (`RB3_NO_BUTTON_GLYPH_FIX=1`): footer hint pills + overshell MENU glyph =
  featureless solid-white lozenges/dot (the F3 blob).
- ON (default): real glyph artwork renders — yellow button pill on the footer
  hint, play-arrow-in-circle on the overshell MENU chip. instrument_icons_* and
  Pentatonic text paths unchanged (separate predicates).
Evidence: /tmp/w31-f3/{on,off}/native_depth_00.png + crop_footer_{on,off}.png
(hint TEXT differs between shots only by footer-carousel phase — Pentatonic
path, not this fix). F2 and F4 remain split sub-charters for the W32 menu.
