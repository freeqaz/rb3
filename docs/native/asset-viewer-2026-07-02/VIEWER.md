# rb3-viewer — standalone .milo asset renderer

A headless CLI mode of `rb3-native` that loads one `.milo` asset against the real
engine + BandRnd (WGPU) backend and writes a PNG — for debugging rendering bugs
on individual assets without booting the whole game. First customer: the
"white wig / long hair" CharHair artifact.

It is the proto-viewer (`RB3_RENDER_MESH=1`) plus: char/hair object factories, a
real argv CLI, dependency-milo (`--subdir`) loading, optional CharHair physics
settling (`--sim`), mesh-hide filtering, richer camera control, and a `--list`
census mode.

**v2 adds a visible skinned draw** (bone motion now deforms the mesh) plus three
inspection features: `--test-bone` (rotate a bone and see it deform), `--pose-dump`
(dump every transform as JSON, after `--sim`), and `--draw-dir` (draw-order parity).

## Build

```bash
cmake --build native/build-native --target rb3-native   # ~3s incremental
```

## Run

Two equivalent triggers: `--viewer` in argv, or `RB3_VIEWER=1`.

```bash
# direct
native/build-native/rb3-native --viewer <milo-rel-path> [options]

# wrapper (build-if-stale + prints PNG path + census)
scripts/native/render-asset.py <milo-rel-path> [options]
```

The milo path is relative to `RB3_DATA`
(default `/home/free/code/milohax/rb3/orig-assets/extracted`) or absolute.
The mode is always headless (`MILO_HEADLESS` is forced on).

### Options

| flag | meaning |
|---|---|
| `--out <path.png>` | output PNG (default `/tmp/rb3_viewer.png`) |
| `--frames N` | GPU warmup frames rendered before readback (default 1) |
| `--sim N` | drive N CharHair settle steps at 30fps before the final render |
| `--subdir <milo>` | pre-load a dependency milo as a subdir (repeatable) |
| `--hide <substr>` | skip meshes whose name contains substr (repeatable) |
| `--only-showing` | draw only meshes with `Showing()==true` |
| `--azimuth d` / `--elevation d` | camera spherical angles in degrees |
| `--distance u` | camera stand-off distance (default: auto-frame from bounds) |
| `--cam-dir x,y,z` | explicit Milo-axis view direction (overrides az/el) |
| `--width W` / `--height H` | render size (default 640x480) |
| `--list` | print class/name census and exit (no GPU init) |
| `--verbose` | extra per-mesh logging |
| `--test-bone <name> <deg> [x\|y\|z]` | rotate one Trans (exact name, then substring) `deg` degrees about a local axis before the draw — the deterministic way to see the skinned mesh deform (default axis z) |
| `--draw-dir` | draw via the dir's own `RndDir::DrawShowing()` (draw-order / transparency parity) instead of the filtered mesh walk (`--hide`/`--only-showing` do not apply) |
| `--pose-dump <file>` | write every Trans's local+world xfm as JSON, **after** `--sim` (so it captures the simulated pose) |
| `--pose-dump-bones a,b,c` | comma-separated name substrings; keep only matching transforms in the dump |
| `--no-hair-parent` | debug: skip the standalone posing shims (parent + rebind + guard-off) for A/B |

Camera axes are Milo convention: **X=right, Y=forward/depth, Z=up**. The
auto-frame uses robust median-center + 90th-percentile-radius bounds (shared with
`RB3_RENDER_MESH`), so a few outlier verts don't blow up the framing.

## Examples

```bash
# Static render of the long female wig (the bug's first customer)
scripts/native/render-asset.py \
  char/main/hair/female/gen/female_hair_long_resource.milo_xbox \
  --out /tmp/female_hair_long.png

# Run CharHair physics 30 steps before capturing (wig-collapse repro)
scripts/native/render-asset.py \
  char/main/hair/male/gen/male_hair_crazyhawk_resource.milo_xbox \
  --sim 30 --out /tmp/crazyhawk_sim.png

# Resolve cross-milo tex/mesh refs by pre-loading the shared char milo
scripts/native/render-asset.py <hair.milo_xbox> \
  --subdir char/main/shared/gen/char_shared.milo

# Custom camera + size
scripts/native/render-asset.py <milo> --azimuth 40 --elevation 20 --distance 30 \
  --width 1280 --height 720

# Just inspect what's inside a milo (no GPU)
scripts/native/render-asset.py <milo> --list

# v2: rotate a hair bone 25deg and SEE the skinned mesh deform (the clean proof)
scripts/native/render-asset.py \
  char/main/hair/male/gen/male_hair_crazyhawk_resource.milo_xbox \
  --test-bone bone_hair_top-front01.mesh 25 z --out /tmp/crazyhawk_bend.png

# v2: dump the pose (after 30 sim steps) as JSON, only the strand tips
scripts/native/render-asset.py <hair.milo_xbox> --sim 30 \
  --pose-dump /tmp/pose.json --pose-dump-bones bone_hair

# v2: draw via the dir's own draw-order (transparency parity)
scripts/native/render-asset.py <milo> --draw-dir
```

## Notes / gotchas

- **Cross-milo NOTIFYs are expected.** A hair *resource* milo references shared
  objects (`hair_shared_spec.tex`, `bone_hair.mesh`, `*.pal`, `world.wind`) that
  live in other milos merged in at runtime. Standalone, they fall back
  gracefully (`NOTIFY: ... couldn't find ...`) — not `Can't make` errors. Use
  `--subdir` to resolve them if a bug depends on them.
- **Skinned draw is now VISIBLE (v2).** The engine already skins any mesh with
  `owner->IsSkinned()` (BandRnd::DrawMesh reads `BoneOffsetAt(b) * boneTrans->
  WorldXfm()` live), but a standalone hair *resource* milo needs three shims,
  applied automatically when posing (`--sim` or `--test-bone`; disable with
  `--no-hair-parent`):
  1. **synthetic strand-root parents** — the strand roots are authored parented to
     a head bone that lives in the character milo, not the resource; loaded alone
     `Root()->TransParent()` is null, so `CharHair::SimulateInternal` early-outs
     and the sim never moves anything. An identity-world parent per rootless root
     makes the sim gate pass (root world xfm preserved, so rest is unchanged).
  2. **rest-pose bind rebake** — the mesh's authored inverse-bind offsets assume
     the in-game bind world; standalone the bones sit elsewhere, so skin ≠ identity
     and the engine's SKIN_CLAMP freezes verts at bind (bone motion never reaches
     the pixels). `SetBone(b, bone, calcOffset=true)` rebakes each offset so
     skin==identity at rest; `mNativeBonesRebound` then tells the engine the mesh
     is correctly bound (clamp off).
  3. **world→local bake** — `CharHair` advances bones via `SetWorldXfm`, which
     leaves LOCAL stale; the draw path re-dirties the chain and recomputes world
     from stale local, reverting to rest. After the sim the viewer rewrites each
     local so `parent*local` reproduces the settled world.
  The engine's V24 **SHARD_GUARD** (drops any skinned mesh spanning >2× its bind
  extent) is also turned off while posing, so a legitimately spread pose isn't
  dropped.
- **`--test-bone` is the reliable skinned-motion tool.** It rotates one bone
  deterministically and the mesh visibly + coherently deforms (e.g. crazyhawk 25°
  → one strand swings out, rest of the hawk intact). This is the DirectPose-style
  CPU pose the wig saga wanted.
- **`--sim` runs the REAL CharHair physics, but standalone it can diverge.** With
  no head-frame and no `CharCollide` volumes reachable, the free-running solver
  settles most strands but can fling/collapse a strand (crazyhawk flings one;
  ziggymullet collapses). The physics genuinely executes and now reaches the
  pixels — but for a *stable coherent* pose prefer `--test-bone`, and keep the
  in-game `band-closeup-capture.py` gate (Lane G) as the authoritative check for
  the actual CharHair fix. `--sim N` still prints per-step polls as proof it ran.
- **`--list` skips GPU** but still registers all factories (via
  `PreInitRender`, which is pure factory registration) — required, or the load
  hits Unknown-class stream desync → heap corruption.
- **GPU sandbox:** if GPU init fails, the tool exits 2 with a hint; re-run with
  the sandbox disabled.

## Implementation

- `native/src/rb3_viewer.cpp` — the mode (`RunViewer`), CLI parse, factory set,
  subdir/sim/draw/readback, and the v2 helpers: `SetupHairForSim` (synthetic
  strand-root parents), `RebindSkinnedMeshesToRest` (rest-pose bind rebake),
  `BakeSimPoseToLocal` (persist sim world into local), `ApplyTestBone`,
  `DumpPose`, `ViewerDrawFrameDir`. All native-only; no Wii/shared-engine change.
- Dispatch in `native/src/main_native.cpp` (next to `RB3_RENDER_MESH`).
- Reuses `native/src/rb3_render_mesh.{h,cpp}` exports: `ViewerComputeBounds`,
  `ViewerMakeCamera` (added), plus the shared bounds/framing math.
- Char factory set mirrors `native/tests/test_charload5b.cpp` +
  `CharHair::Init` / `OutfitConfig::Init` / `RndAmbientOcclusion::Init`.
  `BandCharDesc::Register()` is called before `OutfitConfig::Init()` because the
  latter news a `BandCharDesc` (else Unknown-class crash). `Character` is
  deliberately NOT registered (its ctor needs the `char_test` overlay).
