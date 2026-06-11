# scout-fret-held — held frets don't render as solid/lit on the now-bar

**Issue key:** `fret-held` · **Wave-1 scout (opus)** · 2026-06-11
**Status:** ROOT CAUSE FOUND (renderer/material, not input) · needs **engine-repo** change
**Ports used:** 8661-8669 · **Evidence:** `/tmp/rp-fret-held/`

---

## 1. SYMPTOM

When you hold a fret key during guitar gameplay (e.g. green = `1`/`A`), the
retail game lights/depresses that fret's now-bar "smasher" button solid while
held (a per-color additive glow on the smasher plate). In the native+web port
the now-bar shows the 5 colored fret buttons but **holding a fret produces no
visible change** — no lit/held glow appears. Scoring is unaffected (strum still
hits), so this is visual-only.

**Repro (headless native):**
1. Boot to guitar gameplay, hard:
   `python3 scripts/native/keyboard-to-gameplay.py --port 8661 --diff hard --out /tmp/rp-fret-held --game-burst 4`
2. Hold green and screenshot the now-bar. I scripted this in
   `/tmp/rp-fret-held/drive_only.py <port> <bit>` (bit 1 = `kPad_R2` = green),
   which navigates an already-running instance to gameplay, then re-enqueues the
   fret press and captures the now-bar.

**Evidence screenshots (now-bar crop, x360-y540..920-700 of 1280×720):**
- baseline (nothing held): `/tmp/rp-fret-held/final_baseline_crop.png`
- green held:             `/tmp/rp-fret-held/final_held_green_crop.png`

The two crops are **pixel-identical on the now-bar** — the green smasher button
is not lit/depressed/glowing while green is held.

---

## 2. ROOT CAUSE

**The entire input → controller → smasher chain works perfectly in native.**
The glow is invisible because the **`gem_smasher_glow.mesh` material is black
(`color = 0,0,0`) with no diffuse texture bound**, drawn with additive blend
(`kBlendAdd`). The native standard shader gates the diffuse texture behind the
material color (`baseColor = material.color * texture`), so a black material
color + additive blend contributes **zero** to the framebuffer → invisible.

### How I proved the input/logic chain is fine (worktree probe, now removed)

I built rb3-native in a throwaway worktree with `FRET_DBG`-gated `fprintf`s in
`GemSmasher::SetGlowing`, the `GemSmasher` ctor, and
`GuitarController::OnMsg(ButtonDownMsg)`, drove it to gameplay, and held green.
Clean stderr (grep with `-a`; the milo logs contain UTF-8 so plain `grep`
**silently** treats the file as binary and prints nothing — a real gotcha here):

```
[FRET_DBG] ctor slot=0..4 keys=0 dir=0x… glowMesh=0x…(non-nil) pressTrig=(nil) releaseTrig=(nil)   ×5
[FRET_DBG] GC::OnMsg(Down) btn=1 disabled=0 local=1 msgUser==lUser=1 slot=0    ×40 (== my 40 presses)
[FRET_DBG] SetGlowing slot=0 b=1 keys=0 glowMesh=0x…(non-nil) dir=… showing=1   ×40
```

So, every time green is held:
- the message reaches `GuitarController::OnMsg`, passes every gate (not disabled,
  local user matches), and `ButtonToSlot(kPad_R2)` resolves **slot 0**;
- `mSink->FretButtonDown` → `GemPlayer::FretButtonDown` →
  `mTrack->SetFretButtonPressed(0,true)` → `GemManager::SetSmasherGlowing` →
  `GemSmasher::SetGlowing(true)` runs, the glow mesh is **non-NULL**, the smasher
  dir is **showing**, and `mGemSmasherGlow->SetShowing(true)` is called.

The held-fret path is **message-driven** (the port already sends these via
`rb3_joypad_native.cpp` → `SendButtonMessages` → subscribed `GuitarController`).
Nothing in the input/joypad shim is missing.

### Why the shown glow mesh renders nothing (the actual bug)

`SetGlowing` calls `mGemSmasherGlow->SetShowing(true)`; the mesh becomes showing
but draws invisibly. I dumped the glow mesh's geometry and material at the rising
edge and compared against the **visible** sibling smasher-button mesh:

```
GLOW   mesh='gem_smasher_glow.mesh'  geomOwner='_gem_smasher_glow.mesh' verts=0 faces=20
SIBLING(visible) '_gem_smasher_guitar_0.mesh'                          verts=0 faces=222
GLOWmat  ='gem_smasher_glow.mat'    blend=2(kBlendAdd) color=(0,0,0,1) diffuseTex='(null)'
BUTTONmat='gem_smasher_guitar.mat'  blend=1(kBlendSrc) color=(1,1,1,1)
```

- **Geometry is fine.** `NumVerts()` (uncompressed) is 0 for BOTH the glow and the
  visibly-rendered button — these meshes use **compressed verts**. The native
  renderer's `EnsureMeshUploaded` (`milo-native-engine/src/platform/MeshGpuCache.cpp`)
  follows `GetGeomOwner()` and uploads via the compressed-vert path; the glow's
  20 faces upload like the button's 222. (My first read of `verts=0` as "empty
  geometry" was a red herring — the sibling proves it renders fine.)
- **The material is the bug.** `gem_smasher_glow.mat` has
  `color = (0,0,0)` and **no diffuse texture bound** (`GetDiffuseTex()==null`),
  blend `kBlendAdd`. The standard shader
  (`milo-native-engine/src/gfx/standard_wgsl.inc`) computes:
  ```
  let baseColor = material.color.rgb * vertexTint;          // line 705 → (0,0,0)
  baseColor = baseColor.rgb * diffuseSample.rgb;            // line 712 → still 0 (and no tex anyway)
  ...
  finalColor = baseColor.rgb (prelit) or baseColor*lighting; // → 0
  ```
  With additive blend the contribution to the framebuffer is `0` → the glow is
  invisible. The visible button works because its material is `color=(1,1,1)`
  opaque with a baked per-button texture.

### Why the glow material is black with no texture (the upstream cause)

The smasher milo ships the bright per-color glow textures
(`square_smasher_bright_{green,red,yellow,blue,orange}.tex`) and a
`particle_slot_colors.anim`. On Xbox/Wii the smasher's `set_color` DTA
(`orig-assets/extracted/ui/track/gem_smasher.dta`, the `(set_color …)` handler)
runs `{particle_slot_colors.anim set_frame $slot}` at reset, which is what binds
the per-slot bright texture / color onto `gem_smasher_glow.mat`. On native that
recolor is **not landing on the glow material** (it stays at its authored
`color=(0,0,0)`, `diffuseTex=null`), so even when shown there's nothing to add.
This is the same family as the A1 hit-flame gap: the *logic* fires but the
DTA-anim-driven FX material state isn't realized on native.

> Net: `fret-held` is a **renderer/material-state** gap, NOT an input gap. Two
> independent things conspire (black material color *and* unbound diffuse tex),
> so a fix must restore at least the texture binding, ideally the slot color too.

---

## 3. FIX DESIGN

This is **NOT** an input/joypad change — do not touch `rb3_joypad_native.cpp` or
the message path; they are proven correct.

Two viable approaches (A is the faithful fix; B is a cheaper engine fallback).
**A is preferred; confirm A's anim hypothesis first with the probe in §4.**

### Option A — make `set_color`/`particle_slot_colors.anim` recolor the glow mat (asset/DTA-anim realization)
- **Where:** the per-slot color application. `GemSmasher::Reset` (`src/band3/bandtrack/GemSmasher.cpp`)
  triggers the dta `reset` → `set_color` → `particle_slot_colors.anim set_frame`.
  Confirm on native whether that anim actually runs and whether it targets
  `gem_smasher_glow.mat`'s diffuse-tex + color. If the anim runs but the native
  PropAnim/ColorAnim path doesn't drive `RndMat::mDiffuseTex` / `mColor`, that's
  the engine gap to close (likely in `milo-native-engine` PropAnim/anim apply).
- **Risk:** medium — anim-application is shared engine code; must not regress
  other anim-driven material recolors. **Match-neutral** for the Wii build (any
  fix is `HX_NATIVE`-gated or engine-side; `GemSmasher.cpp` itself is shared
  game code — keep edits behind `#ifdef HX_NATIVE`).
- **Needs engine-repo change:** likely YES (anim → material apply), unless the
  anim is simply never triggered (then it's a `HX_NATIVE` call to re-run
  `set_color`/the recolor in `GemSmasher::Reset`/`SetGlowing`).

### Option B — engine-side: don't zero a textured/intended additive glow by black material color
- **Where:** `milo-native-engine/src/platform/MaterialSetup.cpp` + `src/gfx/standard_wgsl.inc`.
- For an **additive** (`kBlendAdd`) material that has a diffuse texture but a
  black/near-zero material color (the authored "modulate-by-script" case the
  native build can't drive), output the texture directly instead of
  `texture * matColor`. This mirrors the existing native heuristics for
  multiply-blend (`forcePrelit`/`kHeuristicMultiplyPrelit`, MaterialSetup.cpp:166)
  — an additive analogue. BUT note the glow mat **also has no diffuse texture
  bound on native**, so B alone is insufficient unless A also binds the bright
  texture. B is a safety net / general fix for "additive glow zeroed by black
  matColor"; it does not by itself fix the missing texture.
- **Risk:** higher blast radius (touches every additive material); strict visual
  A/B via `scripts/native/visual_diff.py` required.
- **Needs engine-repo change:** YES.

### Recommended path
1. Run the §4 probe to confirm whether `particle_slot_colors.anim` runs on native
   and what it's *supposed* to set on `gem_smasher_glow.mat`.
2. If the anim runs but doesn't reach the material → fix the engine anim→material
   apply (Option A, engine). If the anim never runs on native → re-trigger the
   per-slot recolor from `GemSmasher::Reset`/`SetGlowing` under `#ifdef HX_NATIVE`
   (Option A, game-side, match-neutral).
3. Keep Option B in reserve as the additive-glow shader safety net.

**Opt-out / verification gating:** land behind an env opt-out (e.g.
`RB3_FRET_GLOW_OFF`) consistent with the campaign's other default-on visuals.

---

## 4. VERIFICATION

**Confirm-the-anim probe (do this first, in a worktree):** in
`GemSmasher::SetGlowing`, on the `b && !mGlowing` rising edge, dump the glow
material under `FRET_DBG`:
`mGemSmasherGlow->Mat()->GetColor()`, `->GetBlend()`, `->GetDiffuseTex()` name.
Today it reads `color=(0,0,0,1) blend=2 diffuseTex=(null)`. **A correct fix makes
it read a non-black color and/or a bound `square_smasher_bright_<slot>.tex`.**
(Reusable harness: `/tmp/rp-fret-held/drive_only.py <port> <bit>` drives an
already-running instance to gameplay and holds the given fret bit; bits: green=1,
red=5, yellow=4, blue=6, orange=7 — `kPad_*` indices.)

**Visual pass criterion:** boot to gameplay, hold green, crop the now-bar
(box `(360,540,920,700)` of the 720p shot). The green smasher must show a
solid/lit additive glow that is **clearly different** from the baseline crop.
A/B the same frame (gems scrolling + venue motion make a naive full-frame diff
noisy — crop to the now-bar and ideally pause/freeze). Repeat per color (red/
yellow/blue/orange) to confirm the per-slot texture/color is correct.

`scripts/native/visual_diff.py` is the render-change gate for any engine-side
(Option B) change.

---

## 5. REFERENCE SCREENSHOTS NEEDED

- **A clean retail "fret held" now-bar shot** (Wii preferred): a frame where one
  fret button is held/lit solid on the highway, to pin down the exact glow color/
  intensity/shape per slot. The existing `images/retail-screenshots/` gameplay
  shots (`yt_qRagnZCIMzk_gameplay_guitar.png`, `fandom_gameplay_guitar.png`) show
  the now-bar but not a clearly-held-and-lit fret. `../xenia` could capture this
  by holding a fret on a paused/known frame.
- Nice-to-have: a retail shot of a **sustain being held** (smasher glowing during
  a sustain tail) — same glow material; confirms the lit-while-held look.
