# rb3-viewer — standalone .milo asset renderer

A headless CLI mode of `rb3-native` that loads one `.milo` asset against the real
engine + BandRnd (WGPU) backend and writes a PNG — for debugging rendering bugs
on individual assets without booting the whole game. First customer: the
"white wig / long hair" CharHair artifact.

It is the proto-viewer (`RB3_RENDER_MESH=1`) plus: char/hair object factories, a
real argv CLI, dependency-milo (`--subdir`) loading, optional CharHair physics
settling (`--sim`), mesh-hide filtering, richer camera control, and a `--list`
census mode.

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
```

## Notes / gotchas

- **Cross-milo NOTIFYs are expected.** A hair *resource* milo references shared
  objects (`hair_shared_spec.tex`, `bone_hair.mesh`, `*.pal`, `world.wind`) that
  live in other milos merged in at runtime. Standalone, they fall back
  gracefully (`NOTIFY: ... couldn't find ...`) — not `Can't make` errors. Use
  `--subdir` to resolve them if a bug depends on them.
- **`--sim` drives the sim but the static draw is un-skinned.** `--sim N` runs
  `TheTaskMgr.SetSecondsAndBeat(t,beat,false)` + `CharHair::Poll()` on every
  `CharHair` (no `Character` needed). That moves the `bone_hair_*` transforms,
  but the viewer draws the strand mesh (a *skinned* mesh) at its rest pose via
  `DrawMesh` — it does not run a skinning/`PoseMeshes` pass — so on a standalone
  hair *resource* milo (whose shared `bone_hair.mesh` is also absent) the
  rendered frame may look identical to the static one even though the sim ran.
  The console shows `--sim N steps over K CharHair object(s)` + per-step polls as
  proof the physics executed. Making the sim visually reflected needs the
  skeleton + skinning path (Lane-G / a follow-up), not this loader.
- **`--list` skips GPU** but still registers all factories (via
  `PreInitRender`, which is pure factory registration) — required, or the load
  hits Unknown-class stream desync → heap corruption.
- **GPU sandbox:** if GPU init fails, the tool exits 2 with a hint; re-run with
  the sandbox disabled.

## Implementation

- `native/src/rb3_viewer.cpp` — the mode (`RunViewer`), CLI parse, factory set,
  subdir/sim/draw/readback.
- Dispatch in `native/src/main_native.cpp` (next to `RB3_RENDER_MESH`).
- Reuses `native/src/rb3_render_mesh.{h,cpp}` exports: `ViewerComputeBounds`,
  `ViewerMakeCamera` (added), plus the shared bounds/framing math.
- Char factory set mirrors `native/tests/test_charload5b.cpp` +
  `CharHair::Init` / `OutfitConfig::Init` / `RndAmbientOcclusion::Init`.
  `BandCharDesc::Register()` is called before `OutfitConfig::Init()` because the
  latter news a `BandCharDesc` (else Unknown-class crash). `Character` is
  deliberately NOT registered (its ctor needs the `char_test` overlay).
