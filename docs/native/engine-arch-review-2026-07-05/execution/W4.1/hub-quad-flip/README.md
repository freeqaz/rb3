# RB3_HUB_MENU_QUAD_HIDE — coordinator flip-decision package

**Wave 7, Lane C, stage C.S3 (KEY=W4.1).** Packaging only — this stage does **not** flip the
flag. It hands the coordinator everything needed to decide.

## What the flag does

`RB3_HUB_MENU_QUAD_HIDE` (engine `NativeCompatFlags.classification.json`, class `workaround`,
owner `ui/hub`, default **off**) hides `playnow.lsw` — a `LabelShrinkWrapper` child of
`mb_playnow.grp`/`menu_buttons.grp` in the 360-ARK `ui/main/main_hub.milo` — which draws as an
opaque light-grey rounded-capsule quad mid-screen on `main_hub` with **no retail counterpart**
(root-caused in Wave 6, W4.1 subitem (a); SYS-5 family). Landed `b537d275`, lane-verified
Wave 6 (`W4.1/STATUS.md`, C.S5).

Fix location: `src/band3/meta_band/MainHubPanel.cpp`, `#ifdef HX_NATIVE`-guarded, ~20 lines.
`playnow.lsw` is a *sibling* object to the PLAY NOW label text, not a container/parent of it —
hiding the wrapper quad has no structural reason to touch the label, and this package proves it
doesn't.

## Build under test

- rb3 `HEAD` at capture time (no rb3 source changed by this package — `git log --oneline -1
  -- src/` unaffected by C.S3).
- engine `HEAD` at capture time: `7943bfa` (`W3.3-fix: luminance-preserving highlight-ceiling
  guard, default-OFF`), one commit past the coordinator-bumped pin `1b045d9`. Built fresh in
  `native/build-agent-C-S3-hubquad` (Clang/Debug, not the shared `build-native`).
- **W4.2 flag state (`RB3_UI_TEXT_FLOOR_RELAXED`) and W3.3-fix state
  (postproc ceiling patch): both left at their current default — OFF (unset).** C.S2
  independently verified both directions of the W4.2 fix (flag-off byte-identical /
  flag-on improves contrast) and recommended **FLIP** but the coordinator has not executed it
  yet, so "default" today is still flag-OFF for both. This package is captured against that
  same currently-shipping decision state — not a pre-empted flip — so the montages below are an
  apples-to-apples extension of what C.S1/C.S2 already looked at on `main_hub`. If the
  coordinator flips W4.2 before acting on this package, the *only* expected visual change on
  these captures is text contrast (focused item darker-on-gold) — re-run `capture.py` to confirm
  if that order changes.

## Method

`capture.py` boots `rb3-native` headless (`RB3_HTTP=1`, `RB3_FIXED_CLOCK=1`) to `main_hub_screen`
**four times**: OFF, OFF, ON, ON (`RB3_HUB_MENU_QUAD_HIDE` unset / unset / `=1` / `=1`) — a
genuine two-boot A/A pair per state, not just one shot per side. Each boot settles 3s past
`main_hub_screen` arrival (venue-backdrop parallax + MOTD ticker are still animating; this
matches the Wave-6 `lane-verify-capture.py` settle convention) before screenshotting. Two ROI
crops are cut from every capture:

- **quad ROI** `x[380,900] y[365,410]` — the capsule itself.
- **label ROI** `x[140,700] y[195,330]` — PLAY NOW / QUICKPLAY / START A ROAD CHALLENGE.

## Captures (`captures/`)

| File | What it shows |
|---|---|
| `off_1_full.png` / `off_2_full.png` | flag-OFF (default), two independent boots — **A/A pair**. |
| `on_1_full.png` / `on_2_full.png` | flag-ON (`RB3_HUB_MENU_QUAD_HIDE=1`), two independent boots — **A/A pair**. |
| `montage_2x2_off_vs_on.png` | All four full frames in one grid (OFF row / ON row) for a single-glance A/A+A/B read. |
| `quad_roi_off_vs_on.png` | The capsule ROI, OFF vs ON, stacked — the quad is opaque grey (OFF) vs gone/venue-visible (ON). |
| `label_roi_all_4.png` | The PLAY NOW / QUICKPLAY / START A ROAD CHALLENGE ROI from **all four** boots stacked — this is the "the label still renders" proof: read top-to-bottom, the text is present, legible, and pixel-similar in every one regardless of flag state. |
| `retail_vs_flagon.png` | Retail (Xbox 360, `images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`) side-by-side with flag-ON. Retail has no capsule anywhere on screen — matches ON. |
| `retail_vs_flagoff.png` | Same retail shot side-by-side with flag-OFF, for direct contrast — the capsule is native-only, visibly absent from retail. |

Raw uncropped boot logs + the working directory this package's screenshots came from:
`/tmp/w41-hubquad-flip-cs3/` (not committed — regenerate with `capture.py` if needed; the
committed `captures/` files above are the reviewer-facing artifact).

## Findings

1. **Quad renders correctly per flag state, A/A-stable.** Both OFF boots show the capsule in the
   same screen position (frame-to-frame content behind it differs slightly — venue parallax +
   ticker phase — but the capsule itself is present, opaque, unchanged shape/position in both).
   Both ON boots show it absent in both. No flake observed in 2/2 + 2/2.
2. **PLAY NOW label is unaffected by the hide — this was the blocker check.** `label_roi_all_4.png`
   demonstrates the label text (all three menu-stack lines) renders identically across all four
   boots: the wrapper quad hidden by the flag is not a parent/container of the label, and there is
   no side effect. **This is not a blocker** — the package can proceed as a normal flip
   recommendation, not a bug report.
3. **Retail ground truth has no such quad anywhere on `main_hub`** (`retail_vs_flagon.png` /
   `retail_vs_flagoff.png`) — flag-ON matches retail's absence of the capsule; flag-OFF's capsule
   is demonstrably native-only render debris.

## Recommendation (packaging stage only — coordinator decides)

Numerically and visually this reads as a clean flip candidate: flag-OFF is the bug (native-only
quad with no retail analog), flag-ON matches retail, and the one thing worth checking before
flipping (label survival) is proven clean by direct ROI comparison across four independent boots.
No new risk surfaced by this package. See `REVIEWER_CHECKLIST.md` for the point-by-point sign-off
the coordinator should walk before flipping `kHubMenuQuadHideDefaultOn` (or equivalent) `0→1`.

## Fence

Touched only `docs/native/engine-arch-review-2026-07-05/execution/W4.1/hub-quad-flip/` (this
package). No `src/`, no engine files, no classification.json edits, no flag flip. Reused the
existing `RB3_HUB_MENU_QUAD_HIDE` flag registered in Wave 6 — nothing new to register.
