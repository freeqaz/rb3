# W4.2 — main-menu TEXT + SELECTED-ELEMENT rendering regression (characterization)

**Agent:** characterization-only (no source edits). **Date:** 2026-07-06.
**HEAD:** rb3 `69fffdac`, engine pin `8e7eddd`. Binary: `native/build-native/rb3-native`
(clean, built 18:12 before capture). Repro: `RB3_HTTP=1 RB3_FIXED_CLOCK=1 MILO_HEADLESS=1`,
nav `start,confirm` → `main_hub_screen`, `/api/input` up/down sweep, `/api/screenshot`.

## Verdict

- **Symptom:** the focused/selected main-hub menu item renders **pale near-white text on a
  washed-out pale-yellow bar** (very low contrast, near-illegible); unselected siblings render
  **bright white** instead of retail's dimmed grey.
- **Root cause (mechanism-grade, source-grounded):** the engine's unconditional UI-text
  **colour floor** `std::max(0.6f, mu.color[N])` at
  `milo-native-engine/src/platform/RB3MaterialBinder.cpp:145-149` clamps every UI-text material
  colour up to ≥0.6, destroying the per-focus-state colours RB3 authors.
- **Regression class: LONG-STANDING (not a Wave-6 change).** The floor was introduced
  **2026-05-30** by engine commit `08b3932` ("Phase 3 text material color floor in
  BandRnd::DrawMesh"), expanded by `33cf117` ("expand text-mesh predicate + color floor,
  W7-V3 Phase 1+3"), and relocated verbatim into `RB3MaterialBinder.cpp` by W1.3 `b206d44`
  (MOVE, byte-identical). It reproduces flag-independently and is ~5 weeks old.
- **Reproducible headless: YES.** Selection sweep confirms the highlight *bar* tracks nav
  correctly (moves QUICKPLAY↔ROAD CHALLENGE); only the *text fill colour* is wrong.

## Symptom inventory (per selection state)

Evidence: `evidence_montage.png` (this dir). Raw sweep PNGs at `/tmp/w42/sweep/` (not committed).

| Element | Retail ground truth | Ours (HEAD) | Wrong? |
|---|---|---|---|
| "PLAY NOW" (collapsed parent) | grey/dim | grey ghost | OK (dimmed via list alpha, not the floor path) |
| Selected item ("QUICKPLAY", or "ROAD CHALLENGE" after ↓) | **BLACK text** on **saturated gold** bar | **pale near-white** text on **washed pale-yellow** bar | **YES — text fill** |
| Unselected sibling | grey (~0.4–0.5) | **bright white** | **YES — over-bright** |
| Yellow highlight bar itself | gold, tracks focus | pale-yellow, **tracks focus correctly** | bar placement OK; only its saturation reads slightly washed |

The sweep (down×N / up×N via `/api/input`) shows the highlight bar **correctly moving** between
QUICKPLAY and START A ROAD CHALLENGE — so selection *logic* and highlight-mesh routing work; the
defect is purely **selected/unselected text FILL COLOUR**. (`get_highlighted_node` dta-eval isn't
bound on `main_hub`, returns 0; verified visually instead.)

## Mechanism (draw-state difference, end to end)

1. `src/system/ui/UILabel.cpp:266-290` — `UILabel::Draw` sets the font material colour per focus
   state: `mLabelDir->GetStateColor((State)mState, color); fontMat->SetColor(color)`. RB3 data
   authors **kFocused = dark/near-black** for highlight-mesh menu buttons (so it reads dark on the
   bright bar) and **unfocused = grey**.
2. `RB3MaterialBinder.cpp:133` reads that authored colour: `mat->GetColor()` → `mu.color[]`.
3. `RB3MaterialBinder.cpp:145-149` (only when `isLikelyUiText`) does
   `mu.color[k] = std::max(0.6f, mu.color[k])` — the **floor**. Black `(0,0,0)` → `(0.6,0.6,0.6)`;
   grey `(0.45,…)` → `(0.6,…)`.
4. Text glyph path: `prelit=1` + `useAlphaAsRGB=1` (`:196`,`:247`) → shader does
   `baseColor.rgb = mu.color.rgb; rgb *= glyphAlpha`. The floored `(0.6…)` becomes the glyph tint
   → **pale grey/white glyphs** regardless of the authored focus colour → low contrast on the gold
   bar and washed-white unselected labels.

The floor's original justification (comment `:135-144`) is real — some `.milo` data drives font
`GetColor()` to `(0.2..0.5)`, invisible on a near-black 720p panel. But it is a **blunt clamp**: it
cannot distinguish "dark text on a dark panel (needs lifting)" from "**dark text on a bright
highlight bar (must stay dark)**" or "grey-vs-white focus distinction (must be preserved)". So it
fixed one visibility case and broke the focus-state colour contract.

### Confirmation attempts
- `RB3_LIGHT_PROBE=1` capture (`/tmp/w42/lightprobe.log`, 8729 lines) dumps *pre-floor*
  `mat->GetColor()`. It confirms the highlight **bar** mat `highlight_main.mat` = `(0.82,0.82,0.17)`
  (already >0.6, floor is a no-op there → bar OK) and outline font `Pentatonic_outline_(5_00)4x.mat`
  = `(0.83,0.83,0.26)`. The per-focus **fill** label colours could not be dumped: the probe dedups
  on mesh *name*, and all glyph submeshes are unnamed RndText (collapse to one `mesh=''` slot).
  Getting them needs a one-line probe edit (out of scope — no source edits). The mechanism is
  nonetheless airtight from `UILabel::Draw` + the unconditional floor + the retail/ours pixel delta.
- A full engine bisect-rebuild (parent of `08b3932`) was **not attempted**: ~5 weeks of engine API
  drift make an old-engine rebuild of current rb3-native almost certainly non-compiling, and the
  cost/instability isn't justified given the mechanism + introducing commit are already pinned.

## Backlog proposal

**New item (engine, Wave 7 — engine-file, so staged/backlogged not landed this wave per hard rule
7–8): "UI-text focus-colour floor breaks focused-menu-item text."**

- **Smallest-scope fix, one file:** `milo-native-engine/src/platform/RB3MaterialBinder.cpp:145-149`.
  Make the floor no longer clobber authored dark/grey focus colours. Preferred options, cheapest
  first:
  1. **Gate + relax:** put the floor behind a registered flag and **lower the constant** (e.g.
     lift only when *all* channels are below a small ε, or lift toward a much lower target) so
     near-black focused text and the grey/white distinction survive. Must A/B against the labels
     the comment says it recovered (news-ticker / FRIEND-RANKINGS / CHOOSE-INSTRUMENT).
  2. **Scope out focused-highlight labels:** skip the floor for text drawn on a
     highlight-mesh button (the focused-label case). Needs a focus/highlight signal plumbed to the
     material bind — more plumbing, but the faithful fix (the floor should never touch
     dark-on-bright-bar text).
  3. **Faithful (largest):** remove the floor entirely and address dark-on-dark-panel visibility at
     its real cause (panel brightness), since Wii renders authored colours directly. Highest risk of
     re-breaking the recovered labels; do only with the A/B gate.
- **Verification gate for the fix:** re-run this sweep; assert selected-label glyph luma is *dark*
  over the gold bar and unselected is grey (not white) — plus no regression on the three recovered
  label sets. Fail-red = restore the floor → pale text returns.

### Collision check vs Wave 6 lanes
- **Lane C (W4.1 UI parity)** is **game-side only** (`src/band3/**`, UI `.dta`; `rb3_render_hook.cpp`
  is even forbidden to it). The floor lives in **engine** `RB3MaterialBinder.cpp` → **no file
  collision with Lane C.** This is a distinct defect from W4.1's items (grey ticker quad /
  song_select overlap / part_difficulty widgets).
- **Lane A** owns the engine render path but its live edits this wave are `WriteSceneUniforms` /
  placement-flip in `Rnd_Wgpu_RB3.cpp`, not the binder's floor block → low collision risk. Because
  it is an engine file, per the wave rules the fix is **staged as a patch + Wave-7 backlog item**,
  not landed here.
- **Lane D** (black head / grayscale venue) is unrelated (skin RT / camera-grade), different
  mechanism.

## Deliverable summary

- Regression verdict: **long-standing** (introduced engine `08b3932`, 2026-05-30; expanded
  `33cf117`; moved by W1.3 `b206d44`). Not a Wave-6 regression.
- Root cause: unconditional UI-text colour floor `std::max(0.6f, …)` at
  `RB3MaterialBinder.cpp:145-149` clobbers RB3's per-focus-state authored label colours.
- Proposed fix location: `milo-native-engine/src/platform/RB3MaterialBinder.cpp:145-149` (engine),
  file-disjoint from Wave-6 Lane C; stage as Wave-7 engine backlog item.
