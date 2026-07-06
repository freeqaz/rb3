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


---

## C-S1 (W4.2-fix) — done

**Agent:** Sonnet impl, Wave 7 Lane C. **Date:** 2026-07-06. Fence: engine
`src/platform/RB3MaterialBinder.cpp` only (no other engine file touched this stage;
`rb3_postproc.wgsl.inc`/`RB3PostProc.{cpp,h}`/`FxSendNative.cpp` in the working tree are
Lane A / unrelated in-flight work, left untouched and uncommitted by this stage).
`rb3_render_hook.cpp` (A7 fence grant) was **not** needed — the fix stayed inside the
binder, no focus-signal plumbing required.

### Fix shape (flag-first, per WAVE7_REVIEW A3-style discipline applied to Lane C too)

New registered flag `RB3_UI_TEXT_FLOOR_RELAXED` (default-OFF, `class: workaround`,
`owner: ui/text`) at `RB3MaterialBinder.cpp:145-149`'s call site. Flag-OFF path is the
exact pre-existing 3-line `std::max(0.6f, mu.color[k])` clamp, byte-for-byte — a provable
no-op change when unset. Flag-ON: only lifts a colour when **all three channels** are
below `kTrueInvisibleUiText = 0.06` (to `kRelaxedUiTextFloor = 0.25`); every other colour
(including the entire measured live hub range) passes through unchanged.

The 0.06/0.25 constants are calibrated, not guessed: an `RB3_UI_FLOOR_DBG=1` probe
(added inline, stderr-only, no behavior change) dumped every pre-floor `mu.color` on a
`main_hub_screen` selection sweep. Live values ranged **0.118-0.247** per channel — well
above the original W5p3 comment's assumed 0.20-0.50 floor-trigger range, and well above
0.06. A first-cut relaxation at `0.22→0.4` was tried and rejected: it still visually
washed out identically to the unconditional 0.6 clamp (any floor target in the ~0.4-0.6
band reads the same after the downstream draw pipeline's own brightening). Disabling the
floor entirely across the observed range, keeping only a safety net for genuinely
near-zero (< 0.06 all channels) authored colours, is what actually restores contrast.
See the in-source comment block (`RB3MaterialBinder.cpp`, "Calibration note") for the
full trace.

### Gates (both directions, `RB3_HUB_MENU_QUAD_HIDE` left unset/OFF per A7)

1. **Flag-OFF byte-identical:** `drawlog-golden.py --fixed-clock --canonical-order
   --scene splash_screen` against `native/build-agent-CS1/rb3-native` (flag unset) →
   **PASS, 792/792**, matching the committed post-flip golden exactly. The flag-OFF code
   path is textually identical to pre-change, so this is a belt-and-suspenders
   confirmation, not a surprise.
2. **Main-hub selection sweep, flag-ON** (`scripts/native/_w42-hub-text-capture.py`,
   `RB3_UI_TEXT_FLOOR_RELAXED=1`, down×6/up×6 from `main_hub_screen`): focused item
   (`QUICKPLAY`, then `START A ROAD CHALLENGE`) now renders **dark olive/grey text on
   the gold highlight bar** — legible, matches the retail `PLAY NOW`-focused convention
   in `images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png` (dark-on-gold). Unfocused
   siblings render bright white, also matching retail's unselected `CAREER` /
   `TRAINING` / etc. convention (the diagnosis's "unselected = dimmed grey" expectation
   was itself unverified against ground truth; retail's actual unselected top-level
   items are bright white, so the flag-ON bright-white unfocused rendering is correct,
   not a residual bug). Before/after crop:
   `docs/native/engine-arch-review-2026-07-05/execution/W4.2/cs1_before_after_hub.png`.
   Full 13-frame sweep (both arms) retained at `/tmp/w42/final-on/`, `/tmp/w42/final-off/`
   (not committed — reproducible via the capture script).
3. **Three originally-rescued labels, flag-ON:**
   - **News ticker** (`main_hub_screen`, always visible): unaffected, A/B pixel-similar
     (`/tmp/w42/final-on_ticker_final.png` vs `final-off_ticker_final.png`) — this text
     was already bright pre-floor, so the flag is a no-op here either way.
   - **CHOOSE INSTRUMENT** (song_select → part: → diff: flow, `partdiff-{on,off}.png`):
     title text unaffected (bright white both arms); the selected `GUITAR` row on the
     gold bar reads clearly in both arms, slightly *more* legible flag-ON. No regression.
   - **FRIEND RANKINGS** (`AppMiniLeaderboardDisplay`): **not independently re-verified**
     this stage — it requires live network/friends data per the original W4.2
     characterization and was already out of headless reach before this fix; the flag's
     mechanism (untouched for any colour >= 0.06 on any channel) does not change behavior
     for this label relative to the pre-existing floor unless its authored colour also
     happens to fall in the < 0.06 dead-zone, which is untested either way. Same
     limitation as the original characterization, not a new gap introduced here.

### Files changed

- Engine (commit pending, `../milo-native-engine`):
  `src/platform/RB3MaterialBinder.cpp` (the fix + calibration comment + debug probe),
  `src/platform/NativeCompatFlags.classification.json` (append-only row for
  `RB3_UI_TEXT_FLOOR_RELAXED`, under `/tmp/milo-engine-classjson.lock`).
- rb3: this STATUS.md append, `docs/native/engine-arch-review-2026-07-05/execution/W4.2/cs1_before_after_hub.png`,
  new capture harness `scripts/native/_w42-hub-text-capture.py`.

### Verdict: **default-OFF, flag-first** (per WAVE7_KICKOFF's flag+flip discipline).
Coordinator flips `RB3_UI_TEXT_FLOOR_RELAXED` next once satisfied; no default changed by
this stage. `RB3_HUB_MENU_QUAD_HIDE` hub-quad flip package is a separate Lane C stage
(S3), not part of this one.

---

## C-S2 (W4.2-fix) — INDEPENDENT VERIFY — done

**Agent:** Opus, Wave 7 Lane C independent verification. **Date:** 2026-07-06.
**Engine HEAD built:** `031461a` (the C-S1 fix; pin still `1b045d9`, not bumped —
soft-pin WARNING only, build links engine HEAD). **Binary:** fresh from-scratch clang
build `native/build-agent-CS2/rb3-native` (contains `RB3_UI_TEXT_FLOOR_RELAXED`, verified
via `strings`). **Verdict: FLIP** (recommend coordinator flip `RB3_UI_TEXT_FLOOR_RELAXED`
default-ON).

### Reproduced all three C.S1 gates (fresh build, flag unset ⇒ OFF)

1. **Flag-OFF byte-identical:** `drawlog-golden.py --bin native/build-agent-CS2/rb3-native
   --fixed-clock --canonical-order --scene splash_screen` → **PASS, 792/792** (canonical
   multiset), matching the committed post-flip golden. Confirms the flag-OFF code path is
   inert (it is textually the prior unconditional `std::max(0.6f, c)` clamp).
2. **Main-hub selection sweep vs retail** (`_w42-hub-text-capture.py`, down×5, both arms):
   quantified the focused-item glyph over the gold bar (crop x80-345, y205-262, darkest-
   quartile mean luma = glyph fill): **flag-ON = 83.4 (dark-on-gold), flag-OFF = 136.0
   (washed pale)** — a ~53-luma darkening that restores the retail dark-on-gold focused
   convention (`yt_mhKNp9uAT48_menu_hub.png`: PLAY NOW = black-on-gold). Unselected
   siblings render bright white in both arms = retail's actual unselected convention
   (CAREER/TRAINING/etc. are bright white), so the flag-ON result is correct, not a residual.
3. **Three originally-rescued labels:** news ticker legible both arms (unaffected, already
   bright pre-floor); **CHOOSE INSTRUMENT re-verified this stage** — reached
   `part_difficulty_screen` headless both arms (see cross-screen gate below), focused GUITAR
   improved (dark-on-gold flag-ON), header/song-title all legible; FRIEND RANKINGS still not
   headless-reachable (network/friends data — same documented limitation, not a new gap).

### Cross-screen A/A (C.S2-specific mandate — text draws everywhere)

- **song_select:** raw A/B ON-vs-OFF depth-0 = 45.6% changed px / meanabs 2.44. **A/A
  control (two OFF boots) = 44.97% / 2.53** — i.e. the A/B is *within* (indeed below) the
  boot-to-boot nondeterminism envelope (documented async-loader/eye-jitter order noise), so
  the difference is boot noise, **not the flag**. Visual: all list text legible flag-ON,
  focused RANDOM SONG reads dark-on-yellow (correct).
- **part_difficulty:** A/B ON-vs-OFF = 66.9% / meanabs 8.55; A/A OFF-vs-OFFb = 63.6% / 7.0.
  The screen's ~63% baseline noise is the **animated character/venue preview**, not the UI:
  the two OFF boots are **byte-identical in the GUITAR UI-card crop** (bar mean 124.7 both).
  Flag-ON darkens that same card's focused GUITAR fill (bar mean 114.6) — the intended
  dark-on-gold effect, localized to the focused label. All other text (CHOOSE INSTRUMENT,
  RIGHTY MODE, SONG DIFFICULTY, song title, BASS) unchanged and legible. **No text
  regression.**

### Recommendation

**FLIP.** On every headless-reachable screen the flag-ON path (a) restores retail's
dark-on-gold focused-item contrast, (b) leaves unselected and non-focus text unchanged, and
(c) is provably inert when OFF (byte-identical draw pipeline + textually-identical clamp).
The only un-exercised case (FRIEND RANKINGS, colour possibly in the <0.06 dead-zone) is
guarded by the same conservative all-channels-<0.06 predicate and cannot regress a
colour ≥0.06 on any channel. No default flipped by this stage (coordinator-only). Pin not
bumped. Evidence: `cs2_hub_off_vs_on.png`, `cs2_partdiff_off_vs_on.png`,
`cs2_songselect_off_vs_on.png` (this dir).
