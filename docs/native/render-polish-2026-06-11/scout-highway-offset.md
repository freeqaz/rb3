# Scout — `highway-offset`

**Status:** ROOT CAUSE FOUND + FIX VERIFIED with a live env-var A/B. One-line fix,
no engine change, match-neutral (HX_NATIVE-only path).

**TL;DR:** The note highway renders pushed right-of-center and skewed because the
existing native `CAMERA_FRAME_FIX` sets the gameplay camera's lateral position to
the wrong value. `camRotX` defaults to **−4.0** (`native/src/rb3_native_settings.h:34`)
but the highway's visual centerline (now-bar + surface meshes) is at **world x = 0**.
The camera ends up at world x = −4, i.e. **+4 world units left of the highway center**,
so the highway projects to the right. **Change the `camRotX` default from `-4.0f` to
`0.0f`.** Verified live: `CAM_ROTX=0` centers the highway (matches retail);
`CAM_ROTX=-4` (default) reproduces the bug; `+4`/`+8` over-shoot to the left.

---

## 1. SYMPTOM

During gameplay the 5-lane note highway is **shifted right of screen-center and
skewed** — its near (bottom) end leans right while the far (top) vanishing point sits
near center, so it is NOT the head-on, symmetric highway retail renders.

Repro (headless native, port range 8641–8649):
```
python3 scripts/native/keyboard-to-gameplay.py --port 8641 --diff hard \
    --out /tmp/rp-highway-offset --game-burst 24 --verbose
```

Evidence screenshots (all under `/tmp/rp-highway-offset/`):
- `burst_15.png` — clean native gameplay frame; highway visibly right + skewed.
- `native_with_centerline.png` — same frame, red line = exact screen center
  (x=640/1280). The entire highway sits right of the line.
- `native_with_computed_edges.png` — green = where a *centered* highway would project
  (from exact engine camera math); the rendered highway is offset right of it.
- `fix_before_after.png` — **top = camRotX=−4 (default, right-shifted), bottom =
  camRotX=0 (fix, centered).**
- `camrotx_ab_montage.png` — camRotX ∈ {−4, 0, +4, +8} sweep with centerlines.
- `compare_native_vs_retail.png` — native vs retail guitar/drums.

Quantitative (native 1280×720, blue-surface mask center per row, far→near):
- Highway center drifts **~0.50W (far) → ~0.56–0.58W (near)** — rightward lean.
- Retail Wii drums (`images/retail-screenshots/yt_qRagnZCIMzk_gameplay_drums.png`):
  center is **constant ≈ 0.49–0.50W** across all clean rows (vertical, head-on).

---

## 2. ROOT CAUSE  (confirmed, not hypothesis)

The gameplay camera `game.cam` is laterally mis-positioned relative to the highway
geometry. Measured with a temporary `RndCam`/mesh probe in a throwaway worktree
build (HWY_PROBE, since removed):

| Object | World x | Source |
|---|---|---|
| `game.cam` (the cam the highway renders through) | **−4.00** | probe `[HWY_GAMECAM]` |
| `_now_bar_rising_sun.mesh` (now-bar / strike line center) | **0.00** | probe `[HWY_OBJ]` |
| `_surface_keys.mesh` (highway surface) | **0.00** | probe `[HWY_OBJ]` |
| `rotater.grp` local/world x | **−4.00** | probe `[HWY_TRK]` |

`game.cam` basis is clean: `right=(1,0,0)`, `fwd=(0,0.9926,−0.1218)`,
`up=(0,0.1218,0.9926)`, `yfov=0.3674`, `screenRect=(0,0,1,1)` (full). **No yaw, no
roll, no shear, no sub-rect.** So the projection/view math is fine — the *only* defect
is that the camera's line of sight (the world-x=−4 plane) does not pass through the
highway center (world x = 0). The highway center is therefore at **camera-local
x = +4**, projecting right-of-center.

Exact projection of the highway center (world x=0) through the measured camera
(matches the pixel measurement to within noise):

| Depth | screen-x of highway center |
|---|---|
| now-bar (~94) | 0.564W |
| mid (~168) | 0.536W |
| far gems (~224) | 0.527W |

→ right-shifted, drifting toward center with depth = exactly the observed skew.

### Why the camera is at −4 (the actual bug site)

This is a **residual of the existing `CAMERA_FRAME_FIX`** in
`src/system/track/TrackDir.cpp` (`TrackDir::DrawShowing`, HX_NATIVE, lines ~208–273).
RB3's single-player track layout is supposed to be centered by the milo
`N_player_<aspect>` configuration object's `apply` handler calling
`set_track_offset`/`set_side_angle`/`set_screen_rect_x = 0` — but **that handler's
track commands never execute in the native port** (documented in
`TrackPanelDir::ConfigureTracks`, same file family). So the gem track keeps the
authored **multi-player** default (rotater.grp fanned out + a side roll), which swings
`game.cam` off-axis.

The native fix walks the rendered camera's parent chain and (a) zeroes the
`rotater_roll.grp` rotation, (b) zeroes `game.cam.mScreenRect.x`, and (c) sets
`rotater.grp.local.x = TheNativeSettings().camRotX`. **`camRotX` is the lateral
centering knob, and its default is wrong:**

```
// native/src/rb3_native_settings.h:34
float camRotX = -4.0f;   // <-- BUG: comment claims "centres the guitar surface"
```

The `-4.0` was an empirical guess ("the value that centres the guitar surface") that
**overshoots**: it puts the camera at world x = −4 while the surface/now-bar are at
world x = 0, leaving a +4 right-shift. The correct value is **0.0** (camera aligned to
the highway center at world x=0).

### A/B proof (live `CAM_ROTX` env knob, no rebuild — it is the seeded default)

| `CAM_ROTX` | result |
|---|---|
| **−4 (default)** | highway right of center (the bug) |
| **0** | highway **centered** on the highway/now-bar — matches retail |
| **+4** | highway pushed left of center |
| **+8** | highway further left |

Monotonic and clean → `camRotX = 0` is the centered value. See
`fix_before_after.png` and `camrotx_ab_montage.png`.

---

## 3. FIX DESIGN

**Primary fix (1 line, recommended):**

`native/src/rb3_native_settings.h:34` — change the `camRotX` default:
```cpp
float camRotX = 0.0f;   // was -4.0f; highway centerline (now-bar/surface) is at world x=0
```
Update the misleading "-4 … centres the guitar surface" comment accordingly. This is
the field the CAMERA_FRAME_FIX reads every frame (`TheNativeSettings().camRotX` in
`src/system/track/TrackDir.cpp`), so the change takes effect with no other edits.

- **Repo:** rb3 only (`rb3/native/src/…`). **No milo-native-engine change.**
- **Match-neutral:** the consumer is inside `#ifdef HX_NATIVE` in
  `TrackDir::DrawShowing`; the Wii match build never compiles it. The default
  literal in a native-only settings file does not affect any matched `.o`.
- **Multiplayer-safe:** the CAMERA_FRAME_FIX block is gated on
  `gHxNativeNumUsedGemTracks == 1`, so this only touches single-player framing;
  2+ player fan-out is untouched.
- **Risk:** very low. The `rotater_roll.grp` neutralization and `mScreenRect.x = 0`
  reset in the same block are unchanged and already correct. Only the lateral offset
  literal changes.

**Watch-outs for the implementer:**
- Do NOT also shift `game.cam` elsewhere or move the highway geometry — the geometry
  is correct (surface/now-bar at world x=0); only the camera centering value is wrong.
- Re-confirm the exact best value on the *current* engine pin if it has moved (the
  measured surface x=0 is era-stable, but re-run the A/B if in doubt; 0 was clean).
- If a future change makes the highway center non-zero (e.g. a real
  `set_track_offset` port), `camRotX` should track the new surface x.

**Optional follow-up (out of scope for this fix, note for later):** the deeper
correctness fix is to actually execute the milo `1_player_<aspect>` apply-handler's
track commands so `set_track_offset 0`/`set_side_angle 0`/`set_screen_rect_x 0` run
natively (removing the need for the CAMERA_FRAME_FIX hack entirely). That is a larger
DTA-dispatch investigation; the 1-line default fix is the correct immediate action.

---

## 4. VERIFICATION

1. Edit `native/src/rb3_native_settings.h` (`camRotX = 0.0f`), then:
   ```
   cmake --build native/build-native --target rb3-native
   ```
2. Capture gameplay:
   ```
   python3 scripts/native/keyboard-to-gameplay.py --port 8641 --diff hard \
       --out /tmp/verify-hwy --game-burst 6 --verbose
   ```
3. **Pass criteria:** in the burst frames the highway is centered — its now-bar /
   strike line straddles screen center (x≈640/1280), and the left/right lane edges are
   symmetric about center (not pushed right). Compare against
   `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_drums.png` (vertical, centered).
4. **Negative control:** `CAM_ROTX=-4 python3 …` must reproduce the old right-shift
   (proves the knob is the cause and the default is the fix).
5. **Multiplayer regression check:** run a 2-player config if available and confirm the
   fan-out is unchanged (the fix block is `gHxNativeNumUsedGemTracks==1`-gated, so it
   should be; verify no new centering of multi-player tracks).

Quick numeric check (reuse this scout's method): blue-surface-mask the highway and
confirm the per-row center is ≈0.50W and roughly constant far→near (was 0.50→0.58W).

---

## 5. REFERENCE SCREENSHOTS NEEDED

**None.** Existing references suffice:
- `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_drums.png` — the cleanest
  centered/head-on highway ground truth (Wii, vertical, centered ≈0.49–0.50W).
- `images/retail-screenshots/yt_qRagnZCIMzk_gameplay_guitar.png` and
  `fandom_gameplay_guitar.png` — guitar highway references (note: in the fandom shot
  the highway is intentionally framed right because a band char fills the left; the
  drums Wii shot is the better centering reference).

The fix was verified against the native render directly via the live `CAM_ROTX` A/B,
so no new ground-truth capture is required.
