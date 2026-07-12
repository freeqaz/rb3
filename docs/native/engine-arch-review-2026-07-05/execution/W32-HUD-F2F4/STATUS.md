# W32-HUD-F2F4 — STATUS (Lane C)

Base SHA rb3 30546499, engine pin 24c4f95. Build: native/build-agent-W32-HUD-F2F4 (clang, exit 0).
Harness: RB3_HTTP=1 RB3_FIXED_CLOCK=1, boot-to-song.py, free ports, pgid cleanup.
Owned exclusive-write: native/src/rb3_render_hook.cpp (A2) — **UNTOUCHED this wave** (no render-hook code written).
TWO mechanisms, TWO checkpoints (one-family REFUTED, not re-merged): W32-HUD-F2.json + W32-HUD-F4.json.

## A13 lint-4 registry sweep (BEFORE any "engine drops X" claim) — CLEAN
Registry = ../milo-native-engine/src/platform/NativeCompatFlags.{gen.inc,classification.json}
(no NATIVE_COMPAT_LEDGER.md file; the .gen.inc + classification.json ARE the registry). Quoted rows:
- `RB3_HUB_TEXT_CONTRAST` = off, Workaround, ui/hub (focused hub highlight-bar alpha clamp) — orthogonal.
- `RB3_UI_TEXT_FLOOR_STRICT` = off, Workaround, ui/text (relaxed UI-text colour floor opt-out) — orthogonal.
- `RB3_ROWFIX_DBG` = off, Probe, unclassified — orthogonal.
- `RB3_HUD_SCOREBOARD_TOPRIGHT` = on, Workaround, ui/hud — "makes K9 SKIP the scoreboard-anchor
  neutralization so the score plate keeps its authored TOP-RIGHT position ... score pill 88% width
  [TrackPanelDir.cpp:344]" — POSITIONAL ONLY (confirms my pill IS correctly top-right, drawlog rect x977-1280).
- `RB3_APPLY_HANDLER_FIX_OFF` = on, Workaround, bandobj/trackpanel — scoreboard right/left.grp
  x-translation neutralization — POSITIONAL only.
- `RB3_REFRACTION_FIX_OFF` = on, render/refraction — "song_select bottom_square_refraction cull fix" —
  a DIFFERENT refract mesh (song_select square), NOT the scoreboard.
- `RB3_NO_HUB_HIGHLIGHT_FIX` = on; `RB3_NO_BUTTON_GLYPH_FIX` = on — glyph/hub, orthogonal.
NONE acts on scoreboard_bkgrnd/scoreboard_refract (pill fill/opacity) or BandStarDisplay (star show-state).
No shipped default-ON flag contradicts the F2/F4 mechanisms below.

## Instruments used (STEP-0, real material dumps — not guesses)
- `/api/drawlog?prov=1` (RB3_DRAWLOG + RB3_DRAWLOG_PROV): per-draw mesh/mat/blend/zmode/matColor/rect/order.
- `RB3_HEADMAT_DBG` (engine RB3MaterialBinder.cpp:250): per-mesh full material dump (diffuse tex, hasTex,
  blend, alphaCut, zmode, color, prelit, useEnviron).
- `RB3_HUD_STAR_DBG` (TrackPanel.cpp:622): star-input diagnostic.
- Pixel sampling (magick) native vs retail (images/retail-screenshots/).

---

## F2 — score-pill fill (MEDIUM) — REAL BUG, mechanism NAMED, fix = ENGINE (engineAckNeeded)

### Symptom (retail-paired)
Retail pill = opaque DARK NAVY silver-rimmed face + bright WHITE digits ("78,250"). Native pill =
light/washed silver-white glossy face + low-contrast grey digits ("846"), venue bleeding through the body.
Evidence: evidence/native_pill_exact.png (native, 846), evidence/retail_pill_star.png
(retail yt_qRagnZCIMzk_gameplay_guitar.png, 78,250).

### Mechanism NAMED (material dump quoted verbatim, HEADMAT)
The pill is 3 UNLIT layers drawn in order refract(i=281) -> bg(i=282) -> lens(i=288); ALL textures BOUND:
```
[HEADMAT] mesh='sb_refract'  mat='scoreboard_refract.mat'  diffuse='scoreboard_refract_diff.tex' hasTex=1 blend=3 zmode=0 color=(0.35,0.35,0.35,a=0.90) prelit=0 useEnviron=0
[HEADMAT] mesh='sb_bg.mesh'  mat='scoreboard_bkgrnd.mat'   diffuse='scoreboard_frame.tex'        hasTex=1 blend=3 zmode=0 color=(1.00,1.00,1.00,a=1.00) prelit=0 useEnviron=0
[HEADMAT] mesh='sb_lens'     mat='scoreboard_lens.mat'     diffuse='scoreboard_lens.tex'         hasTex=1 blend=4 zmode=0 color=(0.70,0.70,1.00,a=0.90) prelit=0 useEnviron=0
```
Blend enum (src/system/rndobj/Mat.h): 3=kBlendSrcAlpha, 4=kBlendSrcAlphaAdd. prelit=0 && useEnviron=0 => unlit.
- sb_bg (scoreboard_frame.tex) = the SILVER FRAME (bevel rim, transparent center) — renders CORRECTLY natively.
- sb_refract (scoreboard_refract_diff.tex, grey 0.35, alpha 0.9) = the DARK BACKING — contributes ~0 natively.
- sb_lens = additive blue gloss.
`backplate.grp` in the milo = sb_bg.mesh itself; there is NO missing solid-fill mesh.

### Pixel proof (venue bleeds through => backing opacity ~0; venue-independent)
native pill body (846pts): srgba(142,10,52),(122,8,49),(85,8,33)  ~=  venue srgba(148,8,55)
retail pill body:          srgb(31,10,12),(26,5,7),(34,13,13)      vs  retail venue-left srgb(73,57,61)
Native pill body == venue (bleed-through). Retail pill body is opaque dark AND distinct from retail venue
(darker), so retail HAS a real backing that native LACKS. Quantitative: if sb_refract rendered at alpha 0.9,
diffuse.a=1 the body would be ~0.37 grey; measured 0.557 red == pure venue => refract contributes ~0.

### Leading hypothesis + proposed fix (engineAckNeeded)
scoreboard_refract.mat is a Wii REFRACTION material whose opacity does not derive from diffuse-texture alpha;
native's generic unlit-textured path multiplies coverage by diffuse.a (~0 for scoreboard_refract_diff.tex),
so the dark backing renders transparent -> venue bleeds. FIX (ENGINE, RB3MaterialBinder.cpp): for the HUD
score-backing (classified via a new render-hook predicate on scoreboard_refract/scoreboard_bkgrnd), source
coverage from material color.alpha (or force opaque backing). Two-part: render-hook classifier (my TU) +
engine mu handling (engineAckNeeded, no push — shared engine tree not edited, A14). Default-ON + RB3_NO_*
opt-out earned by ON-vs-OFF crops after ack (W31 F3 precedent).
A11: acts on the blend/alpha mechanism the dump names, NOT a hardcoded pill tint. No render-hook tint hack written.

### Scope note
No existing DrawMaterialPolicy field (GameRenderHook.h:114) forces opacity, so F2 cannot be a render-hook-only
fix; it needs a new engine field/handler -> engineAckNeeded. No code written this wave.

---

## F4 — star-row "unearned slots" (LOW-MEDIUM) — FAITHFUL NON-BUG (premise REFUTED)

### The premise is FALSE
W30 FINDINGS.md F4: "Retail always renders the full 5-slot star row (unearned slots as dim outlines) from
count-in onward." This is a MISREAD: it compared native-at-0-stars (1 disc) against
retail yt_qRagnZCIMzk_gameplay_guitar.png at 4 stars (5 discs) — mismatched states (the A11 anti-gaming trap).

### Mechanism (source, quoted) — progressive reveal, NOT always-5
src/system/bandobj/BandStarDisplay.cpp:
- `SetupStars()` creates exactly 5 stars: `for (int i=0;i<5;i++) Find<RndDir>(MakeString("star%d",i),...)`.
- `ResetStars()`: `for i in mStars: reset.trig; if (i>0) star->SetShowing(false);` — HIDES stars 1..4 at 0 stars.
- `SetNumStars(f)`: as each star is earned, fills star[i] and `if (i<size) mStars[i]->SetShowing(true)` reveals
  the NEXT slot. Net: visible discs = (earned filled) + (1 dim next) = earned+1.
Match status (build/SZBE69_B8/report.json, unit main/system/bandobj/BandStarDisplay, matched_code 94.33%):
**ResetStars 100.00%, SetupStars 100.00%, SyncObjects 100.00%, Reset 100.00%, SetNumStars 94.56%.**
The reveal logic is FAITHFUL Wii behavior — retail runs this exact code.

### Retail evidence proves retail ALSO grows the row (not always-5)
- fandom_gameplay_guitar.png @ 50,280 (2 stars): 2 filled + 1 dim = **3 discs** (evidence/F4_retail_2star_3disc.png)
- yt_qRagnZCIMzk_gameplay_guitar.png @ 78,250 & _drums @ 100,536 (4 stars): 4 filled + 1 dim = **5 discs**
  (evidence/F4_retail_4star_5disc.png)

### Native evidence (A11 two-matched-states, driven by autohit; RB3_HUD_STAR_DBG)
- 846 pts / 0 stars = 1 dim disc (evidence/F4_native_0star_1disc.png)
- 17,411 pts / 1 star = 1 filled + 1 dim = **2 discs** (evidence/F4_native_1star_2disc.png)
- 33,674 pts / 3 stars = 3 filled + 1 dim = **4 discs** (evidence/F4_native_3star_4disc.png)
Native discs render correctly (filled = white star on silver rim; dim = dark star on silver rim), matching retail.

### Verdict
Native and retail obey the IDENTICAL rule: visible = earned filled + 1 dim outline = earned+1, revealed
progressively. Demonstrated across FIVE distinct star counts (native 0/1/3, retail 2/4). The A11 formula
"dim == 5-earned" was written under the false always-5 premise, which retail's 2-star=3-disc screenshot
disproves. **F4 is a faithful non-bug — NO code change.**

---

## Deliverable summary
- F2: real bug, mechanism NAMED with pixel + material-dump proof; fix is engine-side (engineAckNeeded). No code.
- F4: faithful non-bug (premise refuted). No code.
- native/src/rb3_render_hook.cpp UNTOUCHED (git clean). No decomp TU touched -> no objdiff/drawlog gate needed.
- Claims: /tmp/wave32-claims/W32-HUD-F2F4.txt (rb3_render_hook.cpp). F2 engine TU named:
  ../milo-native-engine/src/platform/RB3MaterialBinder.cpp (engineAckNeeded, not staged).

## COORDINATOR ADJUDICATION (close-out, 2026-07-12)

F2: engineAck NOT granted this wave — the proposed engine fix
(RB3MaterialBinder coverage from color.alpha for the HUD backing) changes
blend semantics with potential blast radius beyond the pill; chartered as
**W33-F2-PILL** with mandatory ON-vs-OFF + cross-screen material sweep.
F4: CLOSED — NOT A BUG. Progressive star reveal is faithful (BandStarDisplay
100% matched; retail's own screenshots show the row growing). The W30 finding
was an A11-trap (native-0-stars compared against retail-4-stars).
