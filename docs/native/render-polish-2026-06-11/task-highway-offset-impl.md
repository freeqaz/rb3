# task-highway-offset — Implementation

**Status:** DONE  
**Branch:** `wt-task-highway-offset`  
**Commit:** `95a666d2` (rb3 only; no engine change)

---

## What changed

Single file, one default value corrected:

**`native/src/rb3_native_settings.h` line 34**

```cpp
// Before:
float camRotX = -4.0f;   // was "empirically-centred" but is 4 units left of highway center

// After:
float camRotX = 0.0f;    // highway centerline (now-bar/surface) is at world x=0
```

Comment was also updated to explain why 0.0 is correct: the CAMERA_FRAME_FIX in
`TrackDir::DrawShowing` sets `rotater.grp.local.x = TheNativeSettings().camRotX`, and
the highway surface/now-bar meshes sit at world x = 0, so the camera must also be at
x = 0 for a head-on, centered view.

**Match-neutral:** `camRotX` is only consumed inside the `#ifdef HX_NATIVE` block in
`TrackDir::DrawShowing` (the `CAMERA_FRAME_FIX` block, gated on
`gHxNativeNumUsedGemTracks == 1`). No Wii-matched `.o` is touched. No objdiff
re-verification required.

**No engine repo change.** `milo-native-engine` is unaffected; no pin bump needed.

---

## Verification

### Procedure

1. Built `rb3-native` in the worktree with the fix (`cmake --build native/build-native --target rb3-native`). Build succeeded cleanly.

2. Captured BEFORE screenshots (negative control: `CAM_ROTX=-4` env override re-applies the
   old broken default via `InitFromEnv`):
   ```
   CAM_ROTX=-4 python3 scripts/native/keyboard-to-gameplay.py \
       --bin native/build-native/rb3-native --port 8701 --diff hard \
       --out /tmp/rp2-highway-offset/before --game-burst 6 --verbose
   ```

3. Captured AFTER screenshots (new binary, no env override — uses new default 0.0):
   ```
   python3 scripts/native/keyboard-to-gameplay.py \
       --bin native/build-native/rb3-native --port 8702 --diff hard \
       --out /tmp/rp2-highway-offset/after --game-burst 6 --verbose
   ```

4. Both runs completed: `PASS: game_screen reached, song playing`.

### Screenshots

All under `/tmp/rp2-highway-offset/`:

- `before/burst_00.png` through `burst_05.png` — gameplay with `CAM_ROTX=-4`; highway
  visible to the right of screen center, fret buttons skewed rightward.
- `after/burst_00.png` through `burst_05.png` — gameplay with new default 0.0; highway
  centered, now-bar and fret buttons at screen center.
- `fix_before_after_annotated.png` — side-by-side with a red vertical centerline;
  top = before (track right of center), bottom = after (track on center).

### Pass criteria met

| Criterion | Result |
|---|---|
| Highway now-bar / fret buttons at ~0.50W (centered) | PASS — visually confirmed at screen center |
| Highway no longer skewed rightward (near end offset ≠ far end) | PASS — track appears symmetric far→near |
| Negative control: `CAM_ROTX=-4` reproduces right-shift | PASS — confirmed in BEFORE burst |
| No regressions in non-gameplay screens (menu, song select, part select) | PASS — `keyboard-to-gameplay` traversed all screens without error |
| Multiplayer not broken (CAMERA_FRAME_FIX gated on single-player: `gHxNativeNumUsedGemTracks==1`) | N/A — verified code path is gated; single-agent can't multi-player test headless |

---

## Landing notes (for orchestrator)

- **No conflicts expected.** The only changed file is `native/src/rb3_native_settings.h`,
  which is native-only and not touched by any other concurrent task in this campaign (the
  sibling tasks all modify different source areas: `diff-grid`, `char-render`, `crowd`,
  `gem-polish`, `all-inst-crash`, `fret-held`, `menu-lighting`).
- **No engine bump.** No changes to `milo-native-engine`; `MILO_ENGINE_PIN` in
  `native/CMakeLists.txt` stays at `8fb669d`.
- **Cherry-pick is trivial** — the commit is exactly one file with a one-function change in
  the literal default; no merge conflicts are possible unless another agent also modified
  `rb3_native_settings.h`, which none of them should have.
- The `CAM_ROTX` env var still works as a runtime override post-fix (it is read in
  `NativeSettings::InitFromEnv`); existing invocations with `CAM_ROTX=<N>` will continue
  to work for A/B testing, they just seed from 0.0 instead of -4.0 when unset.
