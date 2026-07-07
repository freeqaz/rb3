# W4.4-TEXTCOLOR — STAGE T — STATUS

Lane: Wave-16 Lane T (focused-text color + ROWFIX flip package).
Outcome: **READY_FOR_FLIP.** ROWFIX Part B (focused-row DARK text on the Part-A
bright bar) is **FIXED, rb3-side**, folded into the existing `RB3_ROWFIX`
(default-OFF). No engine touched, no classjson append, no new flag. All
READY_FOR_FLIP gates pass. **Coordinator flips `RB3_ROWFIX` (Part A + Part B
together) after E1.**

## 0. Method
Native headless (`RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free port, frame-count
settling, pgid cleanup), `native/build-native` under `/tmp/rb3-native-build.lock`.
Observation via a temporary `RB3_ROWTXT_DBG` probe in `UILabel::DrawShowing`
(logged per-label text + alt-state + main/alt font-mat colors; **removed before
commit**). Pixels measured with PIL/numpy (`evidence/rowfix_gate.py`).

## 1. OBSERVATION — the A1 premise, refined
A1 correctly REFUTED the "RndText glyph shader ignores font-material color" claim:
the native draw path is **immediate** (`Mesh_Wgpu.cpp DrawMeshImmediate`
snapshots `mat->GetColor()` into the per-draw uniform at line 48/230), so a
material color set right before `mText->DrawShowing()` DOES reach pixels, and
material sharing/late-clobber is NOT the mechanism. The hub's dark focused text
(contrast 2.20) proves the path.

But the A1 **prime suspect** (alt-font mat gets GetStateColor, never the override)
was only half the story. The `RB3_ROWTXT_DBG` probe on the focused RANDOM SONG
row showed **`colOv=0`, `mainCol=(0.87,0.87,0.87)`** — the label drew with **NO
color override at all** and stayed at **`kNormal` state** (light `GetStateColor`).
Tracing `UIListSlot::Draw`: the focused row's `uicolor` is **NULL**
(`DisplayColor`/`SlotColorOverride` return 0 for these rows; the label owns its
own `GetStateColor`). The Wave-15 Part B only mutated a **non-null** `uicolor`, so
its `uicolor &&` guard **skipped every focused row** — the darken never fired.

## 2. ROOT CAUSE (Part B failure)
1. **`uicolor==null` for the focused row** → Part B's `uicolor &&` guard skips it;
   there is no provider UIColor object to darken. (song rows + special rows
   SETLISTS/PARTY SHUFFLE/RANDOM SONG all draw with `colOv=0`.)
2. **Alt-font artist text** — normal song rows are alt-style (`altEn=1`: title =
   main font, artist = alt font). `UILabel::DrawShowing` (:279-292) applies only
   `mAltTextColor`/`GetStateColor` to the alt mat, never `mColorOverride`, so even
   a working main-font darken would leave the artist light.

## 3. FIX — `RB3_ROWFIX` (default-OFF), rb3-side
- **`UIListSlot.cpp` (Part B core):** for the highlighted display element of the
  fill-bearing list (Part A latched `RB3RowfixFillDrawn`), force a DARK label
  color — reuse the provider's UIColor when present (mutate/restore in place),
  else supply a **file-static dark UIColor** (`0.06,0.05,0.02`) as `uicolor` into
  the element `Draw`. Immediate draw snapshots the color; restore-after is safe.
- **`UILabel.cpp` (alt-font propagation):** when `RB3RowfixActive() &&
  mColorOverride`, propagate the override to the ALT font material too (the
  torso-of-the-fix so a song's italic **artist** text also darkens). `getenv`-gated
  → Wii + flag-OFF byte-identical.
- Scoping unchanged: `UIList::DrawShowing` resets `RB3RowfixFillDrawn` per list;
  only the list that drew the `ml_highlight` fill darkens its focused text.

## 4. GATES (bin `native/build-native/rb3-native`)
| gate | result |
|------|--------|
| song_select directional (RANDOM SONG, d0) | OFF `solidfill_frac=0.25` dim frame + light text (FAIL) → ON `solidfill_frac=0.93 fill=179 glyph=74 contrast=2.41` solid bright bar + **DARK** text **PASS** |
| song_select normal song (Antibodies, d5) | title+artist BOTH dark (probe `mainCol=altCol=0.06`); fill 179, dark legible glyphs **PASS** |
| partdiff (choose_difficulty overshell) | **IDENTICAL both arms** (fill 141, text_p5 58/60, contrast ~2.3); overshell ≠ `ml_highlight` UIList → ROWFIX no-op, already-correct polarity → **no regression** |
| hub post-grade contrast ≥2.0 | **2.20 IDENTICAL both arms** (flag-ON no-op on hub: focused labels use `GetStateColor`, `mColorOverride` null; Part B scoped to fill-bearing list) **PASS** |
| Wave-7-rescued labels | hub CAREER/TRAINING/GET MORE SONGS + news ticker legible flag-ON (`e1_hub_ON.png`) **PASS** |
| W4.2 floor | unaffected (hub contrast identical + flag-OFF byte-identical) **PASS** |
| SETLISTS red-band (both arms) | **0.00% red both arms** (redDom −2.3/−2.5) — preserved (no depth edit) **PASS** |
| flag-OFF drawlog | `drawlog-golden --fixed-clock --canonical-order --scene splash_screen` = **792 draws, canonical PASS** |
| flag-OFF byte-identical | all edits `RB3RowfixActive()`-gated / `#ifdef HX_NATIVE` → Wii + flag-OFF unchanged |
| DC3 zero-blast | **N/A — no engine touched** (rb3 `src/system/ui/` only) |

## 5. READY_FOR_FLIP
`RB3_ROWFIX` now encodes the complete two-part fix (A: bright fill; B: dark text)
and both work. Directional gate GREEN on the song_select focused row; hub/partdiff
unaffected; SETLISTS red-band preserved; flag-OFF byte-identical (792). **The flag
is safe to flip default-ON** (coordinator flips after E1 per the acceptance).

## Files
- rb3: `src/system/ui/UIListSlot.cpp` (Part B null-color fix + `UIColor.h` include),
  `src/system/ui/UILabel.cpp` (alt-font override propagation + `UIListWidget.h` include).
- engine: **none**. classjson: **none** (reused `RB3_ROWFIX`). `FxSendNative.cpp` UNTOUCHED.
- E1 (`evidence/`): `e1_songselect_{OFF,ON}_depth{0,5}.png`,
  `e1_hub_{OFF,ON}.png`, `e1_partdiff_{OFF,ON}.png`, `rowfix_gate.py`.
- Checkpoint: `/tmp/wave16-checkpoints/T.json`.
