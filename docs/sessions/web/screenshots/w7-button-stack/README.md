# W7 button-stack investigation — 2026-05-30

**Note:** The `*.png` files in this directory are gitignored (see `.gitignore:124`). To reproduce, build the web target on branch `wt-web-w7-button-stack` (master HEAD `7ccce282`), start the server, and run `scripts/web/quick-capture.mjs`.

**Branch:** `wt-web-w7-button-stack` (worktree)
**Master HEAD:** `7ccce282`
**Engine main:** `33cf117`
**Verdict:** investigation only — NO fix shipped from rb3 side. Root cause is engine-side. See `docs/plans/web-port/W6_VISUAL_POLISH.md` § V15.

---

## Files

### `02_main_hub_baseline.png`
Plain main_hub capture on master tip — shows PLAY NOW correctly positioned + focused (green hilight box), and the unfocused CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS labels visually overlapping each other (the symptom W7 was dispatched to root-cause). Capture at frame 353 of main_hub_screen.

### `02_main_hub_btnprobe.png`
Captured with the (since-reverted) BTNPROBE in `MainHubPanel::Enter()` active. Visual is identical to the baseline; the file is kept for symmetry with the log output that pairs with this image:

```
BTNPROBE menu_buttons.grp local pos=(-401.830 0.000  32.404)  world pos=(-401.830 0.000  32.404)
BTNPROBE mb_playnow.grp   local pos=(   0.000 0.000   0.000)  world pos=(-401.830 0.000  32.404)
BTNPROBE mb_career.grp    local pos=( 371.545 0.000 -29.757)  world pos=( -30.286 0.000   2.647)
BTNPROBE mb_trainers.grp  local pos=( 371.545 0.000 -29.757)  world pos=( -30.286 0.000   2.647)
BTNPROBE mb_shop.grp      local pos=( 371.545 0.000 -29.757)  world pos=( -30.286 0.000   2.647)
BTNPROBE mb_musicstore.grp local pos=( 371.545 0.000 -29.757)  world pos=( -30.286 0.000   2.647)
BTNPROBE mb_playnow.btn   local pos=(  25.000 0.000  72.000)  world pos=(-376.830 0.000 104.404)
BTNPROBE mb_career.btn    local pos=(-346.540 -0.120 72.750)  world pos=(-376.826 -0.120 75.397)
BTNPROBE mb_trainers.btn  local pos=(-346.540  1.250 44.067)  world pos=(-376.826  1.250 46.713)
```

The world Z values for the `.btn` children (104.4 / 75.4 / 46.7) match retail's vertical layout proportions (~29-unit intervals between adjacent items). They render at ~half that screen-space gap on web, which is what causes the visible "stacking." Root cause is the world-Z-to-screen-Y projection mapping (engine-side `RndCam` frustum / FOV / aspect handling), not the button positions themselves.

---

## Probe instrumentation (REVERTED in the committed tree)

Two HX_NATIVE-gated probes were added to capture this data and immediately reverted:

1. `src/system/rndobj/PropAnim.cpp::SetFrame` — logs every `PropKeys` target/class/type for any anim named `*button_reveal*`.
2. `src/band3/meta_band/MainHubPanel.cpp::Enter` — logs `RndTransformable::LocalXfm`/`WorldXfm` for every menu-related Hmx::Object (mb_*.grp, mb_*.btn, *.lsw, menu_buttons.grp).

Probe code is preserved in this README in case anyone needs to re-run it. To re-instrument: drop the snippets back into the source, rebuild, capture with `scripts/web/quick-capture.mjs`.

The capture helper itself (`scripts/web/quick-capture.mjs`) is committed for future probing use.
