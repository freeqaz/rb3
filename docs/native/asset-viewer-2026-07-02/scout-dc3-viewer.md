# Scout: DC3 milo-viewer architecture deep dive (for rb3-viewer)

Date: 2026-07-02. Task A of the rb3-viewer campaign (standalone .milo asset
renderer to debug e.g. the white-wig long-hair bug without a full game boot).

All paths absolute. DC3 repo = `/home/free/code/milohax/dc3-decomp`,
RB3 repo = `/home/free/code/milohax/rb3`, shared engine =
`/home/free/code/milohax/milo-native-engine`.

---

## 0. TL;DR for the implementer

- DC3's milo-viewer is ~2,900 lines across 8 viewer TUs + the standard
  engine-link CMake recipe. Its life cycle is: **curated engine init → GLFW/Dawn
  GPU up → subsystem factory registration (`FlowInit/CharInit/WorldInit/HamInit`)
  → `ObjDirPtr::LoadFile` the milo → `SyncObjects` → find `Character` →
  optional clips/visemes wiring → auto-frame orbit camera → mode dispatch
  (screenshot / video / interactive)**.
- **RB3 already has ~70% of a viewer** in
  `/home/free/code/milohax/rb3/native/src/rb3_render_mesh.cpp` (mode
  `RB3_RENDER_MESH=1` of `rb3-native`): SystemPreInit/SystemInit boot,
  `gBandRnd.InitGpu` headless GPU, `DirLoader::LoadObjects`, bounds-based
  camera synthesis, mesh walk + `DrawShowing`, `ReadbackHeadlessFrame` → PNG.
  What it *lacks* vs DC3's viewer: material/env-aware full-drawable draw
  (it draws only `RndMesh`, no `RndEnvironTracker` selection), character
  Poll/anim (CharClip playback, CharHair simulation), orbit/interactive
  windowed mode, multi-frame warmup with per-frame char Poll, subdir/merge
  loading, and CLI ergonomics.
- **Recommended shape for rb3-viewer**: a new `viewer/` source directory in
  `rb3/native/src/` with its own `main`, built as a separate `rb3-viewer`
  executable that reuses rb3-native's exact source list via the
  `get_target_property(SOURCES)` trick already proven by `rb3-tests`
  (`rb3/native/CMakeLists.txt:678-692`). Zero source duplication, all object
  factories + BandRnd + char/bandobj classes come along automatically.

---

## 1. How milo-viewer initializes the engine WITHOUT a full game boot

File: `/home/free/code/milohax/dc3-decomp/native/src/viewer/milo_viewer.cpp`
(490 lines). The init sequence in `main()` (lines 137–227):

```cpp
// milo_viewer.cpp:169-212 (abridged, comments mine)
setenv("MILO_RENDER", "1", 1);                 // 0 for export-only modes
if (cfg.screenshotPath || cfg.videoPath)
    setenv("MILO_HEADLESS", "1", 1);           // offscreen target, no window

InitMakeString();                              // MakeString buffer pool
SetFileChecksumData();                         // checksum table stub
SystemPreInit(argc, argv, "config/ham_preinit_keep.dta");  // curated preinit DTA
TheRnd.PreInit();                              // registers rndobj factories
SystemInit("config/ham_keep.dta");             // curated main config DTA
TheRnd.Init();                                 // stands up GpuDevice (WgpuRnd)

if (!gWgpuRnd || !gWgpuRnd->Gpu().IsReady()) return 2;   // hard-fail, exit 2
if (gWgpuRnd->Gpu().IsNullBackend())          return 2;   // sandbox/no-GPU guard

FlowInit();     // flow/ object factories
CharInit();     // ~45 char/* factories: Character, CharClip, CharDriver,
                // CharServoBone, CharFaceServo, CharEyes, CharHair, FileMerger…
WorldInit();    // world/ factories
HamInit();      // DC3 game-object factories (HamCharacter etc.)
Movie::Init();  // TexMovie support

GLFWwindow* window = gWgpuRnd->Gpu().Window();   // null when headless
InstallCameraCallbacks(window);                  // mouse orbit/pan/zoom
if (window) glfwSetKeyCallback(window, KeyCallback);
if (window) ImGuiBackend::Init(window, …);       // debug UI (windowed only)

RndCam* cam = Hmx::Object::New<RndCam>();
cam->SetFrustum(1.0f, 1000.0f, 0.6024f, 1.0f);
cam->Select();
```

Key points:

- **Curated DTA configs, not the shipping ones.** `ham_preinit_keep.dta` /
  `ham_keep.dta` live in the *extracted asset tree*
  (`/home/free/code/milohax/dc3-decomp/orig-assets/extracted/config/`). They are
  slimmed copies of the game's preinit/init configs that keep only what the
  loader/type system needs. RB3 has the exact analog already:
  `band_preinit_keep.dta` / `band_keep.dta` in
  `/home/free/code/milohax/rb3/orig-assets/extracted/config/`, used by both
  `RunGame` (`main_native.cpp:530-535`) and `RunRenderMesh`
  (`rb3_render_mesh.cpp:522-524`).
- **Env-var contract with the engine GPU layer** (read inside
  `Rnd::Init`/`GpuDevice`): `MILO_RENDER` (0 = skip GPU entirely, for
  export-only), `MILO_HEADLESS` (offscreen texture instead of GLFW surface),
  `MILO_WIDTH`/`MILO_HEIGHT` (`ViewerArgs.cpp:143-146` maps `--width/--height`
  onto these).
- **What it deliberately skips**: no `App` object, no UI subsystem
  (`TheUI`), no synth/audio device, no PlatformMgr/net/save, no TaskMgr-driven
  game loop, no panel/DTA screen flow. Time is driven manually via
  `TheTaskMgr.SetSecondsAndBeat(...)` (see §5) rather than the game's frame
  clock.
- **Render hook registration is automatic**: every viewer-class target links
  `src/dc3_render_hook.cpp`, whose file-scope
  `HamRenderHookAutoRegister gHamRenderHookAutoRegister;` calls
  `SetGameRenderHook(&gHamHook)` at static-init time
  (`dc3_render_hook.cpp:76-82`). Both hook methods (`DrawGameOverlay`,
  `RenderCharacterImpostors`) are deliberate no-ops today. RB3's equivalent is
  `/home/free/code/milohax/rb3/native/src/rb3_render_hook.cpp` (61 lines,
  same auto-register pattern) — already compiled into rb3-native.
- **Failure-mode ergonomics worth copying**: `setbuf(stdout, NULL)`,
  SIGSEGV/SIGABRT backtrace handler (`milo_viewer.cpp:117-127`), exit code 2
  for "GPU blocked by sandbox" with an explicit hint to run with
  `dangerouslyDisableSandbox`, and `_exit(rc)` at the end (line 488) to dodge
  the ObjectDir-vs-Dawn static-destructor teardown race (RB3's
  `RenderToPng` does the same, `rb3_render_mesh.cpp:472`).

## 2. Loading a .milo into an ObjectDir and getting it drawing

File: `/home/free/code/milohax/dc3-decomp/native/src/viewer/ViewerScene.cpp`.

### Load (`ViewerScene::Load`, lines 35–155)

```cpp
Symbol dirClass = DirLoader::GetDirClass(miloAbsPath);   // peek header class
FilePath fp(miloAbsPath);
baseDir.LoadFile(fp, false, false, kLoadFront, false);   // ObjDirPtr<ObjectDir>,
                                                         // sync (async=false)
baseScene = baseDir;                                     // -> ObjectDir*
rndScene = dynamic_cast<RndDir*>(baseScene);             // may be null (warn)
if (rndScene) rndScene->SyncObjects();                   // wire refs post-load
```

- `ObjDirPtr<ObjectDir>::LoadFile(fp, /*async*/false, /*share*/false,
  kLoadFront, false)` is the synchronous whole-file load. RB3's `ObjDirPtr` has
  the **identical member** (`rb3/src/system/obj/Dir.h:63`,
  `LoadFile__21ObjDirPtr<9ObjectDir>FRC8FilePathbb9LoaderPosb`), so this
  translates 1:1. RB3's render-mesh harness instead uses
  `DirLoader::LoadObjects(FilePath, nullptr, nullptr)`
  (`rb3_render_mesh.cpp:348`) which is equivalent for a viewer (returns
  `ObjectDir*` directly, no smart-ptr ownership).
- **Subdirectories** (`--subdir`, repeatable, lines 67–127): each extra milo is
  loaded into its own `ObjDirPtr`, optionally offset/rotated by mutating every
  `RndTransformable`'s WorldXfm, then attached with
  `baseScene->AppendSubDir(sd)` followed by another `SyncObjects()`.
- **Character discovery** (lines 129–152): `ObjDirItr<Character>(baseScene,
  true)` recursive iteration; falls back to checking whether a subdir itself
  *is* a `Character` (dirs can be Character subclasses) or contains one.
- **FileMerger outfit path** (`LoadFileMerger`, lines 157–251, used with
  `--char-setup`): finds the `char.fm` `FileMerger` inside the base character
  milo, `fm->Select("outfit", outfitFp, false)` / `fm->Select("viseme", …)`,
  then a synchronous `fm->StartLoad(false)` merge, then `SyncObjects()` again
  (which auto-wires CharFaceServo/CharEyes/CharLipSyncDriver). This is the
  DC3 character-assembly model; RB3 characters are assembled differently
  (BandCharDesc/CharCache/prefabs), so treat this section as *reference-only*.

### Draw loop (`ViewerScene::DrawAllMeshes`, lines 699–721)

The viewer does **not** use `RndDir::DrawShowing` on the whole dir; it walks
meshes explicitly so it can filter:

```cpp
void ViewerScene::DrawAllMeshes(const ViewerConfig& cfg) const {
    RndEnviron* env = FindEnvironment();
    Vector3 origin(0,0,0);
    RndEnvironTracker tracker(env, &origin);      // select lighting env (RAII)

    ObjDirItr<RndMesh> meshIt(baseScene, true);   // recursive
    while (meshIt) {
        if (!ShouldHideMesh(meshIt, cfg))          // --hide substring filters
            meshIt->DrawShowing();                 // virtual -> backend DrawMesh
        ++meshIt;
    }
    for (auto& sd : subdirs) { /* same, plus HasUnresolvedTexture() skip */ }
}
```

Per-frame skeleton (all three modes share it —
`ViewerCapture.cpp:237-245, 373-378, 482-496`):

```cpp
scene.PollMovies(seconds);      // TexMovie decode (virtual-time in headless)
gOrbitCam.Update(cam);          // write cam world xfm + viewProj
TheRnd.BeginDrawing();          // engine: begin pass, clear, scene uniforms
scene.DrawAllMeshes(cfg);
scene.DrawMovieOverlay();       // optional --movie fullscreen quad
TheRnd.EndDrawing();            // submit (+ present when windowed)
```

- `FindEnvironment()` (lines 261–270): `rndScene->GetEnv()` on the base dir,
  else first subdir's env. If none and synthetic lights were requested, one is
  created (`SetupSyntheticLights`, lines 365–426 — also supports `--light
  dir|point x y z r g b [intensity]` and `--ambient r g b` CLI lights added to
  the env via `env->AddLight(light)`).
- Cameras found *in* the scene are ignored on purpose (`PrintSummary`,
  lines 317–323: "found scene camera …, using orbit cam anyway"). RB3's
  render-mesh harness likewise prefers a synthesized cam unless
  `RB3_USE_SCENE_CAM=1` (`rb3_render_mesh.cpp:378-388`).
- **Mesh-visibility heuristics** (`ResolveMeshVisibility`, lines 326–363):
  hide `*_lod*` / `*_wrinkle*` meshes; depth-bias a combined mesh when a
  `.1.mesh` split sibling exists. These are DC3-asset-shaped heuristics; RB3
  will need its own (RB3's harness instead has the `RB3_ONLY_SHOWING` toggle
  because RB3 milos often keep geometry on hidden template meshes,
  `rb3_render_mesh.cpp:134-141`).

## 3. Camera control / auto-framing

Files: `ViewerCamera.cpp` (167 lines) + `ViewerScene::AutoFrameCamera`
(`ViewerScene.cpp:428-566`).

- `OrbitCamera gOrbitCam` — azimuth/elevation/distance/target spherical rig,
  Z-up Milo convention. `OrbitCamera::Update(RndCam*)`
  (`ViewerCamera.cpp:33-109`) builds the camera world transform (Milo rows:
  `m.x=right, m.y=forward, m.z=up`) via `cam->SetLocalXfm(xfm)` **and** builds
  the row-major view-proj by hand and pushes it with `cam->SetViewProj(vp)` —
  a workaround because DC3's `RndCam::UpdateLocal` is stubbed natively.
  *RB3 difference*: BandRnd computes viewProj from the cam itself
  (`Rnd_Wgpu_RB3.h` notes "RndCam without GetViewProjectXfms"); RB3's
  `SynthesizeCamera` only sets `SetWorldXfm` + `SetFrustum` + `SetScreenRect`
  and lets `BandRnd::BeginFrame(cam)` derive the matrices
  (`rb3_render_mesh.cpp:161-213`). An rb3-viewer orbit cam should therefore
  only need to recompute the world transform per frame, not the matrix.
- **Auto-fit** (`AutoFrameCamera`): brute-force AABB over every *showing*
  mesh's world-transformed vertices — both uncompressed `Verts(i).pos` and
  big-endian compressed verts (bswap + memcpy, lines 459–477). Then
  `distance = max(2*maxAxis, 1.5*extent, 3.0)`, elevation 0.3, azimuth 0.4,
  and far-plane rescale `SetFrustum(far*0.001, dist*5, 0.6024, 1.0)`.
  If a Character was found it re-centers on `bone_pelvis.mesh`'s world pos at
  fixed distance 120 (lines 514–526).
  *RB3 improvement to keep*: `rb3_render_mesh.cpp`'s `Bounds` struct
  (lines 72–105) uses **median center + 90th-percentile radius** instead of raw
  AABB, because outlier verts blew up the naive AABB — keep the RB3 version.
- CLI overrides: `--azimuth/--elevation/--distance` and direct
  `--eye X Y Z [--lookat X Y Z]` (converted back to spherical, lines 549–565).
- Interactive controls: GLFW cursor/button/scroll callbacks
  (`ViewerCamera.cpp:114-167`), ImGui gets first refusal via
  `ImGuiBackend::WantCaptureMouse()`.

## 4. Headless capture (screenshot + video)

File: `ViewerCapture.cpp` (505 lines). Mode selection is a `std::variant`
(`ViewerMode = ScreenshotMode | VideoMode | InteractiveMode`,
`SelectMode()` lines 51–67; dispatched via `std::visit` in
`milo_viewer.cpp:479-483`).

### Screenshot one-shot (`RunScreenshot`, lines 73–282)

1. Optionally set anim frame / advance char anim to a target beat (see §5).
2. **Warmup render loop** — `m.warmupFrames` (default small; `--frames N`
   overrides) full frames so GPU resources (lazy texture uploads, pipelines)
   settle before readback:
   ```cpp
   for (int frame = 0; frame < m.warmupFrames; frame++) {
       scene.PollMovies((float)frame / 30.0f);
       gOrbitCam.Update(cam);
       TheRnd.BeginDrawing();
       scene.DrawAllMeshes(cfg);
       scene.DrawMovieOverlay();
       TheRnd.EndDrawing();
   }
   ```
3. Readback + PNG (lines 263–281):
   ```cpp
   int w = gWgpuRnd->Gpu().WindowWidth(), h = gWgpuRnd->Gpu().WindowHeight();
   uint8_t* pixels = malloc((size_t)w*h*4);
   gWgpuRnd->Gpu().ReadbackHeadlessFrame(pixels, pixelSize);  // RGBA8
   WriteScreenshot(cfg.screenshotPath, pixels, w, h);          // PNG
   ```

- `GpuDevice::ReadbackHeadlessFrame(uint8_t*, size_t)` and the
  `AcquireHeadlessFrame()/HeadlessTex()` offscreen path live in the **shared
  engine** (`/home/free/code/milohax/milo-native-engine/src/gfx/GpuDevice.h:80-83`)
  — identical for RB3. The PNG writers are also engine-shared:
  `gfx/Screenshot.h` — `WritePNG / WritePPM / WritePNGToMemory /
  WriteScreenshot`. RB3's harness already uses `WritePNG`
  (`rb3_render_mesh.cpp:460`).
- Headless requires the `MILO_HEADLESS=1` env at GPU init (RGBA8 offscreen
  target; `Rnd_Wgpu_RB3.cpp:1025-1026` keeps RGBA8 specifically so native PNG
  output is unchanged). On RB3 the equivalent trigger is the `headless`
  argument to `gBandRnd.InitGpu(W, H, headless)`.
- CLI for a one-shot headless capture:
  `milo-viewer scene.milo_xbox --screenshot out.png [--frames N] [--width/--height …]`.
- Extra screenshot-mode goodies worth cloning: `--pose-dump file.json`
  (`ViewerPoseDump.cpp` — dumps every `RndTransformable`'s local/world
  transforms as JSON, filterable by `--pose-dump-bones` CSV; this was the
  workhorse of the DC3 feet-in-floor investigation) and `--test-bone name angle
  [axis]` (manually rotate one bone from T-pose, lines 92–114 — directly useful
  for wig/hair debugging).

### Video (`RunVideo`, lines 288–398)

Fixed-timestep loop (`--video out.mp4 --duration s --fps n`): per frame,
re-pose the clip at `beat = seconds*(bpm/60)`, draw, `ReadbackHeadlessFrame`,
`VideoEncoder::WriteFrame` (engine `gfx/VideoEncoder.{h,cpp}` — pipes to
ffmpeg). Smooth pelvis tracking + optional `--camera auto-orbit`.

### Sanity metric worth copying

`render_test_main.cpp:359-366` counts non-black pixels post-readback;
`rb3_render_mesh.cpp:443-454` refines this to "non-clear-color pixels" +
prints the center pixel. Keep RB3's version — it distinguishes "empty frustum"
from "rendered black geometry".

## 5. Animation support (ViewerAnimation)

Files: `ViewerAnimation.{h,cpp}` (77 + 272 lines) + the clip-wiring block in
`milo_viewer.cpp:265-358`.

Two separate systems:

1. **`AnimState gAnim` — prop animation** (`RndAnimatable`: TransAnim,
   PropAnim, MatAnim…). `ScanScene()` (ViewerAnimation.cpp:59-118) iterates
   `ObjDirItr<RndAnimatable>` **non-recursively** (recursion into merged
   subdirs hit corrupt/looping Dir↔Group `EndFrame()` cycles — it explicitly
   skips `ObjectDir`/`RndGroup` containers), collects everything with
   `EndFrame() > StartFrame()`, then per frame calls `a->SetFrame(f, 1.0f)`
   with wrap-around. Frame rate assumed 30 fps.
2. **`CharAnimState` — character clip playback** (the part relevant to a
   wig/hair viewer). Wiring for `--clips clips.milo_xbox [--clip name]`
   (`milo_viewer.cpp:269-358`):
   ```cpp
   scene.clipsDir.LoadFile(clipsFp, false, false, kLoadFront, false);
   CharDriver* driver = charObj->Driver();
   if (!driver) {                       // synthesize the drive chain if absent
       charObj->New<CharDriver>("main.drv");
       driver = charObj->Driver();
       CharServoBone* servo = charObj->Find<CharServoBone>("bone.servo", false)
                           ?: charObj->New<CharServoBone>("bone.servo");
       driver->SetBones(servo);
   }
   driver->SetClips(clipsDirPtr);
   // pick clip (named or first), then:
   ObjDirItr<CharClip> allClips(clipsDirPtr, true);
   while (allClips) { allClips->StuffBones(*servo); ++allClips; }  // union bones
   driver->Enter();
   driver->Play(clipToPlay, CharClip::kPlayNow | CharClip::kPlayLoop, -1.0f, 1e30f, 0.0f);
   ```
   Advancing uses the **engine's own dependency-sorted poll path** rather than
   hand-posing (`CharAnimState::AdvanceBeat`, ViewerAnimation.cpp:167-188):
   ```cpp
   // steps of 0.1 beat so IK/twist/cloth converge like in-game
   TheTaskMgr.SetSecondsAndBeat(lastSeconds, lastBeat, false);
   character->Poll();     // -> RndDir::Poll -> CharPollGroup dependency order:
                          // CharDriver -> CharServoBone -> IK -> twists -> hair
   ```
   This is the crucial pattern for the wig bug: **`Character::Poll()` under a
   manually-driven `TheTaskMgr` clock is what makes CharHair simulate**
   without any game loop. There is also a `DirectPose` alternative
   (`PoseMeshesWithFacing`, lines 215–272: a cached `CharBonesMeshes` +
   `clip->ScaleDown/ScaleAdd` + `PoseMeshes()`, plus bone_facing rotation and a
   ground-clamp) used with `--direct-pose` for pose-dump reproducibility.
3. Face extras (viseme clips → `CharFaceServo`, procedural `BlinkState`,
   `CharEyes::ForceBlink`) — nice-to-have, skip for rb3-viewer v1.

## 6. DC3-specific vs portable to RB3

| Piece | DC3-specific? | RB3 story |
|---|---|---|
| Engine init spine (`InitMakeString` → `SystemPreInit(cfg)` → renderer PreInit → `SystemInit(cfg)` → renderer Init) | Portable pattern; DTA filenames differ | Already exists: `rb3_render_mesh.cpp:522-535` uses `band_preinit_keep.dta`/`band_keep.dta` + `gBandRnd.PreInitRender()` |
| `TheRnd.PreInit()/Init()` (DC3 `WgpuRnd : NgRnd`, global `gWgpuRnd`) | **DC3 backend** (`MILO_ENGINE_GPU_BACKEND=dc3`) | RB3 uses **`BandRnd : Rnd`** (`gBandRnd`, engine `src/platform/Rnd_Wgpu_RB3.cpp`, backend=rb3). Init = `gBandRnd.SetClearColor(...)` + `gBandRnd.InitGpu(W,H,headless)` + `gBandRnd.InitScreenshots()` + `gBandRnd.PreInitRender()`. **Order gotcha**: InitGpu BEFORE `chdir(RB3_DATA)` (Dawn adapter enumeration wants clean cwd — `rb3_render_mesh.cpp:502-511`, `main_native.cpp:669-690`) |
| `TheRnd.BeginDrawing()/EndDrawing()` frame calls | DC3 path | RB3 equivalent: `gBandRnd.BeginFrame(cam)` / `gBandRnd.EndFrame()` (`Rnd_Wgpu_RB3.h:103-108`); `DrawShowing()` on a mesh dispatches into `BandRnd::DrawMesh` |
| `FlowInit/HamInit`, `hamobj/*`, `FileMerger char.fm` outfit merge, `aubrey01_head.mesh` probes | **DC3-only** | RB3: `CharInit()` exists and registers CharHair etc. (`rb3/src/system/char/Char.cpp:125-183`); game-object factories via `RB3RegisterGameObjectFactories()` (`rb3_game_object_factories.cpp`) + `RB3RegisterLegacyRndAliases()` + `InjectTypeDefStubs()` (`rb3_render_mesh.cpp:221-243` — "Tex"/"Dir"/"Text" legacy short names). RB3 char assembly = BandCharacter/BandCharDesc/CharCache/prefabs, not FileMerger |
| `GpuDevice`, `ReadbackHeadlessFrame`, `Screenshot.h` (WritePNG), `VideoEncoder`, `ImGuiBackend`, `PipelineManager` | **Shared engine** (`milo-native-engine/src/gfx/`) | Use as-is |
| `GameRenderHook` auto-register TU | Pattern portable | RB3 already has `rb3_render_hook.cpp` |
| Orbit camera math (Z-up, m.x/m.y/m.z rows) | Portable, **except** the manual `SetViewProj` (works around DC3's stubbed `RndCam::UpdateLocal`) | RB3: set world xfm + frustum only; `BandRnd::BeginFrame(cam)` derives matrices (proven by `SynthesizeCamera`) |
| `ObjDirPtr::LoadFile(fp,false,false,kLoadFront,false)`, `DirLoader::GetDirClass`, `SyncObjects`, `AppendSubDir`, `ObjDirItr` | Portable (same API in RB3 fork: `rb3/src/system/obj/Dir.h:63`, `DirLoader.h:58-60`) | Use either; `DirLoader::LoadObjects` already proven in RB3 |
| `TheTaskMgr.SetSecondsAndBeat` + `Character::Poll()` anim drive | Portable (obj/Task.h + char/ are shared engine areas) | Should work; RB3 `Character` also polled per-frame in-game. RB3 extra: `BandCharacter::Poll()` contains the native skeleton-rebind fixes (`RebindOutfitBonesToOwnSkeleton`) — for wig debugging prefer loading via BandCharacter if the asset is a full band char |
| Mesh visibility heuristics (`_lod`, `_wrinkle`, `.1.mesh` splits) | DC3-asset-shaped | RB3 needs its own: hidden template meshes (`RB3_ONLY_SHOWING` logic), `_lod`* still applies |
| Platform: `kPlatformXBox` for LoadMgr | n/a (DC3 is Xbox-native) | **Required on RB3**: `TheLoadMgr.mPlatform = kPlatformXBox` before any load (assets are 360-ARK big-endian; `rb3_render_mesh.cpp:519`) |
| DC3 asset paths (`.milo_xbox` under orig-assets/extracted) | Layout differs | RB3 data root: `RB3_DATA` env, default `/home/free/code/milohax/rb3/orig-assets/extracted` |

**The two real porting deltas** are (a) DC3-renderer calls → BandRnd calls
(mechanical, table above), and (b) character assembly: DC3 loads one
self-contained HamCharacter milo + FileMerger outfits; RB3 band characters are
assembled from prefab + outfit part milos (`char/…` dirs) by CharCache/
PrefabMgr in-game. For the wig bug, v1 should load the character/outfit milo
directly and draw its meshes (no CharCache), then add `Character::Poll()`
driving for hair sim; assembling a *full* game-accurate band member in the
viewer is a v2 concern (options: replicate the minimal
`BandCharDesc`→CharCache path, or capture/serialize an assembled char dir from
the running game).

## 7. Minimal file set + CMake pattern for an RB3 clone

### DC3 viewer file inventory (for reference)

```
native/src/viewer/milo_viewer.cpp    490  main: init, clip wiring, mode dispatch
native/src/viewer/ViewerArgs.{h,cpp} 270  CLI parse + help (plain struct)
native/src/viewer/ViewerCamera.{h,cpp}198 OrbitCamera + GLFW callbacks
native/src/viewer/ViewerScene.{h,cpp}789  load/subdirs/env/lights/autoframe/draw
native/src/viewer/ViewerAnimation.{h,cpp}349 AnimState/CharAnimState/blink
native/src/viewer/ViewerCapture.{h,cpp}555 mode structs + Screenshot/Video/Interactive runners
native/src/viewer/ViewerPoseDump.{h,cpp}143 JSON pose dump
native/src/viewer/ViewerDebugUI.{h,cpp}445 ImGui panel + light gizmos (optional)
```

CMake target (`dc3-decomp/native/CMakeLists.txt:1604-1663`): the 8 viewer TUs +
`dc3_render_hook.cpp` + `DebugPanel.cpp` + `${DC3_ENGINE_SOURCES}` (decomp fork)
+ `${DC3_NATIVE_CORE_SOURCES_ENGINE}` (platform glue minus what graduated into
libmilo-engine) + `${DC3_EXPORT_SOURCES}`; defines `HX_NATIVE=1 MILO_VIEWER=1
HX_IMGUI=1 MILO_DEBUG=1 _DEBUG=1`; links `milo-engine`, platform libs, imgui,
optional ffmpeg/ncnn; shares the decomp PCH.

### Recommended rb3-viewer CMake (the `rb3-tests` trick)

`rb3/native/CMakeLists.txt:678-692` already demonstrates the cheap way to make
a second executable with the full rb3-native surface (all factories via
static-init, BandRnd backend, char/bandobj/world TUs, link stubs):

```cmake
# rb3-viewer — standalone .milo asset renderer (mirrors dc3 milo-viewer)
get_target_property(_RB3_NATIVE_SRCS rb3-native SOURCES)
list(REMOVE_ITEM _RB3_NATIVE_SRCS ${CMAKE_SOURCE_DIR}/src/main_native.cpp)
add_executable(rb3-viewer
    ${CMAKE_SOURCE_DIR}/src/viewer/rb3_viewer_main.cpp
    ${CMAKE_SOURCE_DIR}/src/viewer/ViewerArgs.cpp      # ported from dc3
    ${CMAKE_SOURCE_DIR}/src/viewer/ViewerCamera.cpp    # minus SetViewProj hack
    ${CMAKE_SOURCE_DIR}/src/viewer/ViewerScene.cpp     # BandRnd draw loop
    ${CMAKE_SOURCE_DIR}/src/viewer/ViewerAnimation.cpp # CharClip/Poll drive
    ${CMAKE_SOURCE_DIR}/src/viewer/ViewerCapture.cpp   # screenshot/interactive
    ${_RB3_NATIVE_SRCS}
)
rb3_configure_target(rb3-viewer)   # flags/includes/milo-engine link, line 369
# + the two set_source_files_properties MS-compat-OFF overrides are inherited
#   automatically because they are per-source, not per-target.
```

Caveats:
- rb3-native's frame-loop glue (`rb3_http_server.cpp` etc.) rides along
  harmlessly — RB3_HTTP is opt-in and never started outside `App::Run`.
  If link time matters, a curated subset is possible later; start with the
  full-set clone because every removed TU risks a missing factory/vtable.
- Keep the RB3-side idioms, in this order, in `rb3_viewer_main.cpp`:
  1. signal handlers, `setbuf`; parse args.
  2. `gBandRnd.SetClearColor(...)`; `gBandRnd.InitGpu(W, H, headless)`
     (**before chdir**); `gBandRnd.InitScreenshots()`.
  3. `RB3RegisterLegacyRndAliases()`; `chdir(RB3_DATA)`;
     `TheLoadMgr.mPlatform = kPlatformXBox`; `SetSystemArgs(argc, argv)`.
  4. `SystemPreInit("config/band_preinit_keep.dta")`;
     `SystemInit("config/band_keep.dta")`; `InjectTypeDefStubs()`
     (copy from rb3_render_mesh.cpp:221 or export it);
     `gBandRnd.PreInitRender()`; `RB3RegisterGameObjectFactories()`;
     **`CharInit()`** (new vs render-mesh — needed for Character/CharHair/
     CharClip factories; watch its `PreloadSharedSubdirs("char")` +
     `CharBoneDir::Init()` calls, which will try to load
     `char/char_bones.milo`-type shared assets from the data dir — verify they
     resolve under RB3_DATA or gate them).
  5. Load milo (`DirLoader::LoadObjects` or `ObjDirPtr::LoadFile`), `SyncObjects`.
  6. Bounds (median/percentile version from rb3_render_mesh.cpp:72) → orbit cam
     (world-xfm only) → warmup frames `{ TheTaskMgr.SetSecondsAndBeat; chr->Poll();
     gBandRnd.BeginFrame(cam); walk meshes DrawShowing/DrawMesh; gBandRnd.EndFrame(); }`
     → `ReadbackHeadlessFrame` → `WritePNG` → `_exit(rc)`.
- Build: `cmake --build native/build-native --target rb3-viewer` (same tree as
  rb3-native; ~3s incremental).
- A "GPU sandbox" run needs Vulkan ICD access — same exit-code-2 guard +
  `dangerouslyDisableSandbox` note as DC3 (`milo_viewer.cpp:193-204`).

### Suggested v1 CLI (subset of DC3's, wig-bug-focused)

```
rb3-viewer <path.milo_xbox> --screenshot out.png [--frames N]
           [--subdir x.milo_xbox]... [--hide pat]... [--only-showing]
           [--azimuth d --elevation d --distance u | --eye X Y Z --lookat X Y Z]
           [--clips clips.milo --clip name --bpm 120 --beat B]   # v1.5
           [--pose-dump out.json] [--verbose] [--width/--height]
```

## 8. Cross-references

- RB3 existing harness modes (read these first):
  `/home/free/code/milohax/rb3/native/src/rb3_render_mesh.cpp` (542 lines —
  the seed of the viewer), `/home/free/code/milohax/rb3/native/src/main_native.cpp`
  `RunGame()` lines 625–755 (full-boot ordering incl. BandRnd shutdown exit
  callback `RB3RegisterBandRndShutdown()` — the viewer should register it too
  if it doesn't `_exit`).
- RB3 backend: `/home/free/code/milohax/milo-native-engine/src/platform/Rnd_Wgpu_RB3.{h,cpp}`
  (`BandRnd::PreInitRender` at .cpp:295, `BeginFrame`:1514, `EndFrame`:1670,
  `InitScreenshots`:180; also `WarmGpuForDir` — useful to pre-upload all
  textures/meshes before the first capture frame).
- Engine capture utilities: `/home/free/code/milohax/milo-native-engine/src/gfx/Screenshot.h`,
  `GpuDevice.h:79-83` (headless), `VideoEncoder.h`.
- DC3 minimal-scene reference (programmatic scenes, no milo):
  `/home/free/code/milohax/dc3-decomp/native/src/render_test/render_test_main.cpp`
  + `test_scene.cpp` — the pattern for synthetic repro scenes (e.g. a
  hand-built hair-strip mesh) if the wig bug needs isolation below asset level.
- CharInit factory list (what becomes loadable):
  `/home/free/code/milohax/rb3/src/system/char/Char.cpp:125-183` — includes
  `CharHair::Init()` (the wig system) and `CharMeshHide`, `CharClip`,
  `CharServoBone`, `FileMerger`.

## 9. Risks / open questions for the implementation agent

1. **`CharInit()` side effects under a minimal boot**: `PreloadSharedSubdirs("char")`
   + `CharBoneDir::Init()` + `TheCharDebug.Init()` (MILO_DEBUG overlay) run at
   init; confirm they behave with RB3_DATA extracted assets (RunGame exercises
   them daily via App ctor, but under the *full* config — the viewer's curated
   band_keep.dta path is less traveled).
2. **RB3 character milos may not be RndDir at top level** and their hair/cloth
   sim (`CharHair`) needs `Character::Poll()` + several settle frames; if the
   wig asset is an outfit *part* milo it has no Character at all — the viewer
   must render it statically (fine for material/texture bugs, insufficient for
   sim bugs).
3. **Draw parity**: DrawShowing on isolated meshes bypasses group/draw-order
   logic (`RndDir::DrawShowing`, PostProc, per-mat z-sorting). If the wig bug
   is a transparency-sort artifact, the viewer should offer a
   `--draw-dir` mode that calls the dir's own `DrawShowing()` instead of the
   mesh walk, to reproduce in-game ordering.
4. Interactive/windowed mode needs GLFW callbacks — GpuDevice::Window() exists
   natively (`GpuDevice.h:61`); ImGui is optional (engine `ImGuiBackend`), skip
   in v1.
