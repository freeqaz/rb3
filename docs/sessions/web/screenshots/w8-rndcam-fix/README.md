# W8 RndCam projection investigation — 2026-05-30

**Note:** The `*.png` files in this directory are gitignored (see `.gitignore:124`). To reproduce, build the web target on branch `wt-web-w8-cam-verify` against engine branch `w8-rndcam-projection`, start the server, and run a capture with `MILO_CAM_FOV_SCALE` set in `ENV` before the engine boots.

**RB3 branch:** `wt-web-w8-cam-verify` (worktree)
**Engine branch:** `w8-rndcam-projection` (worktree at `/tmp/milo-engine-w8-cam`)
**Engine HEAD on this branch:** built from `e6c8f86` + one commit (NativeSettings::fovScale wired into BandRnd projection + NativeSettings::Init() called in BandRnd::StartGpuInit).

---

## Engine fix shipped

`src/platform/Rnd_Wgpu_RB3.cpp::BandRnd::WriteSceneUniforms`:
- Reads `NativeSettings::Get().fovScale` and applies it as `sy = fovScale / tanHalf` (sx scales together via `sx = sy/aspect`, preserving aspect ratio). Default 1.0 = no-op.
- `BandRnd::StartGpuInit` now calls `NativeSettings::Get().Init()` to seed the struct from env vars (`MILO_CAM_FOV_SCALE`, `MILO_CAM_NEAR`, `MILO_CAM_FAR`, etc) on the rb3 backend the same way `Rnd_Wgpu.cpp` does for DC3. Without this seed, the engine-side `NativeSettings` stayed at compile-time defaults on the rb3 backend regardless of env, and the FOV knob exposed via the HTTP settings endpoint / ImGui DebugPanel was effectively dead.

Default behavior is unchanged (fovScale=1.0). The knob is now operational via:
- `MILO_CAM_FOV_SCALE=<value>` env (browser preRun `ENV.MILO_CAM_FOV_SCALE='2.0'`)
- HTTP PUT `/api/settings?fovScale=2.0` (when DebugPanel/HTTPserver build flags enabled)
- ImGui DebugPanel FOV-Scale slider (debug builds)

## Reference screenshots

### `01_main_hub_default_fovScale_1_0.png`
Default behavior with `fovScale=1.0`. Visually identical to the W7 baseline — menu buttons (PLAY NOW / CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS) overlap vertically in the top-left of the screen. **No regression vs `wt-web-w7-button-stack` HEAD.**

### `02_main_hub_fovScale_1_5.png`
With `fovScale=1.5` (zoom in). Buttons spread out vertically — vertical-gap symptom partly mitigated — BUT the cam zooms in toward the venue center, pushing the left-positioned buttons off the screen left edge (they're authored at world X=-377). So fovScale alone doesn't recover retail's framing.

### `03_main_hub_fovScale_2_0.png`
With `fovScale=2.0` (zoom in more). Even more vertical spread, but even more of the left-positioned content is clipped off the left edge.

## What this PROVES

The projection math in `WriteSceneUniforms` does correctly scale Y with sy/depth — `fovScale` linearly changes the per-frame vertical screen extent for the same world Z, exactly as the V15 root-cause hypothesis predicted. The engine path is RESPONSIVE to a projection-side fix.

## What this DOES NOT fix — V15 not solved

The V15 symptom (menu buttons overlapping vertically in the top-left) is NOT just a projection-matrix bug. The W8 investigation runtime-probed every `Select()`'d camera through every frame and found:

| cam | yfov | world pos | drawn meshes |
|---|---|---|---|
| `world.cam` | 51.5° | (110, 60, 300) | sky + venue (city backdrop, signs, sidewalks) |
| `Cam.cam` | 35.0° | (8.6, -1034, -374) | (unclear — not gameplay or UI) |
| `[ui.cam]` | 34.5° (= 0.6024 rad) | (0, -768, 0) | UI meshes incl. menu button labels (`highlight_main.mesh`, the unnamed text meshes, `connect_controller_refract.mesh`, `dimmer.mesh`) |
| `overshell.cam` | (per-frame) | (0, -787, 23) | overshell HUD (`mucha_pattern`, `bg_shadow*`, `difficulty_bar*`, ...) |
| `meta.cam` | 33.9° | (0.05, -768, 0.1) | (presumably meta_panel overshell) |

So `[ui.cam]` at Y=-768 yfov=34.5° draws the menu buttons. Cam config `(fov 34.516) (y -768) (near 1) (far 1000)` from `extracted/(..)/(..)/system/run/config/default.dta` `(ui (cam ...))` is loaded correctly.

For mb_playnow.btn at world Z=104.4 (from the W7 BTNPROBE), the projection math gives:
- depth = 0 - (-768) = 768
- NDC.y = sy * z / depth = (1/tan(0.3012)) * 104.4 / 768 = 3.222 * 104.4 / 768 = **0.438**
- pixel-Y at 576 = (1 - 0.438) / 2 * 576 = **162 px from top**

For mb_trainers.btn (z=46.7): NDC.y = 0.196 → pixel-Y = 232. **Span = 70 px.**

But the actual rendered span between PLAY NOW and TRAINING is **~15-25 px**. A factor of ~3x discrepancy.

The projection is computing exactly what the cam+data should produce. The bug is downstream:
- Possibly: the actual rendered meshes are not at the `mb_*.btn` world positions (LabelShrinkWrapper / RndText may reposition / scale text meshes to different world coords than the .btn group).
- Possibly: a different cam (e.g. meta.cam at slightly different pos) is the one actually drawing the labels, with different framing.
- Possibly: the buttons' `LabelShrinkWrapper.m_pShow` toggling (per `01_main_button_reveal.anim`) leaves them at the wrong WorldXfm.

A W9 follow-up needs to log the actual world position of each labeled text mesh AT DRAW TIME (not at panel Enter time) to see where they really sit when the camera projects them. The W7 BTNPROBE only sampled at panel `Enter()`.

## Reproducing the captures

```bash
# Worktree setup (already done):
cd /home/free/code/milohax/milo-native-engine
git worktree add /tmp/milo-engine-w8-cam -b w8-rndcam-projection HEAD

cd /home/free/code/milohax/rb3
tools/setup-worktree.sh web-w8-cam-verify
cd .claude/worktrees/web-w8-cam-verify

# Build web with the engine fork (the symlink at .claude/worktrees/milo-native-engine
# points to the main engine checkout; we override with MILO_ENGINE_PATH directly):
source ~/emsdk/emsdk_env.sh
cd native
emcmake cmake -S . -B build-web \
    -DMILO_ENGINE_PATH=/tmp/milo-engine-w8-cam \
    -DRB3_WEB_RELEASE=OFF -DRB3_WEB_CLOSURE=OFF
cmake --build build-web -- -j$(nproc) rb3-web
cp build-web/rb3-web.{js,wasm} web/build/

# Start server
cd ..  # back to worktree root
python3 native/web/server.py --port $(cat .worktree-port) > /tmp/w8-server.log 2>&1 &

# Capture (set fovScale via ENV.MILO_CAM_FOV_SCALE in Module.preRun)
# See /home/free/code/milohax/rb3/scripts/web/w8-camprobe.mjs for the template.
node scripts/web/w8-camprobe.mjs $(cat .worktree-port) /tmp/main_hub.png
```

## Diagnostics scripts left in this worktree

- `scripts/web/w8-camprobe.mjs` — capture helper that sets a configurable env var in `ENV` before main() runs. Adjust the preRun closure to enable `MILO_CAM_FOV_SCALE`, `MILO_W8_CAMPROBE` (engine-side cam-Select probe), `MILO_W8_MESHDUMP_FRAME=<n>` (one-frame mesh-name dump), etc.

(The engine-side `MILO_W8_CAMPROBE` / `MILO_W8_MESHDUMP_FRAME` probes were left out of the committed engine diff — they were single-use diagnostics. Re-add them locally if you want to rerun the same investigation on a future engine state.)
