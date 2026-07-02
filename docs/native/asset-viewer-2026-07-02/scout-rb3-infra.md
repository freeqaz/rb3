# Scout: RB3 native infra adaptation for rb3-viewer (Task B)

Date: 2026-07-02. Scope: what an `rb3-viewer` standalone .milo asset renderer needs from
`rb3/native/`. Research only; no source modified. Companion scouts: DC3 milo-viewer
(scout-dc3-viewer.md), wig bug (scout-wig-bug.md).

---

## 0. HEADLINE FINDING — the proto-viewer already exists and renders hair TODAY

`rb3-native` already has a mode that is ~80% of rb3-viewer:
**`RB3_RENDER_MESH=1`** → `RunRenderMesh()` in
`/home/free/code/milohax/rb3/native/src/rb3_render_mesh.cpp` (542 lines).
It boots the real config, stands up BandRnd (WGPU, headless), loads ONE milo via
`DirLoader::LoadObjects`, synthesizes a framing camera from robust scene bounds,
draws every `RndMesh` through `BandRnd::DrawMesh`, reads back the headless frame,
and writes a PNG.

Verified live during this scout (no code changes, existing binary):

```bash
cd /home/free/code/milohax/rb3
RB3_RENDER_MESH=1 MILO_HEADLESS=1 RB3_RENDER_MESH_PNG=/tmp/hair.png \
  native/build-native/rb3-native \
  char/main/hair/female/gen/female_hair_long_resource.milo_xbox
# -> "dir 'femalehairlong_resource' [RndDir] — 35 objects, 1 drawable meshes, 0 cams"
# -> "non-clear pixels=68187 / 307200 (22.2%)"  -> wrote /tmp/hair.png
```

The output (saved next to this doc as `probe-female-hair-long-render-mesh.png`)
shows the LONG FEMALE HAIR rendered standalone — pale white/pink, textured but
untinted. Note for the wig-bug agent: in-game hair color is applied by
`OutfitConfig::MatSwap::Compose` (RTT two-color tint via `BandRnd::DrawRect`), and
this probe ran with `OutfitConfig`/`CharHair` factories MISSING (see §3.4) — i.e.
the standalone whitish render is exactly the "no tint composite" state. The viewer
work is largely: add the char-class factories + an optional Poll/sim step + a
nicer CLI around this existing mode.

Loader NOTIFY lines from the probe (each is a to-do item for the viewer):

```
Character not registered, defaulting to RndDir        <- root dir class
Can't make AmbientOcclusion
Can't make OutfitConfig
Can't make CharHair
torso_femalehairlong.mat couldn't find hair_shared_spec.tex ...    <- cross-milo ref
bone_hair_*.mesh couldn't find bone_hair.mesh ...                  <- cross-milo ref
--->Arvin/Diana: Skinned mesh needs to be re-exported: femalehairlong_resource.mesh
```

---

## 1. CMake anatomy — `/home/free/code/milohax/rb3/native/CMakeLists.txt` (1133 lines)

### 1.1 Targets

| target | lines | what |
|---|---|---|
| `rb3-dta` | 415–421 | milestone (a) DTA harness; obj/utl/os/math fork subset only |
| `rb3-native` | 429–630 | full-engine boot harness — the thing to clone/extend |
| `rb3-tests` | 670–705 | gtest suite; **DRY pattern to copy** (see §1.5) |
| `rb3-web` | 928+ | Emscripten (inside `else() # EMSCRIPTEN` at line 707) — irrelevant here |

A new native target goes inside the `if(NOT EMSCRIPTEN)` block (line 408), next to
rb3-tests.

### 1.2 Key variables / source sets

- `DECOMP_FLAGS` (lines 23–54): clang MWCC-compat (`-fms-compatibility`,
  `-fdelayed-template-parsing`, atomic-builtin defines, force-include
  `src/mwcc_compat.h`). Applied per target by `rb3_configure_target`.
- `MILO_ENGINE_PATH` = `../../milo-native-engine` (line 72); soft pin
  `MILO_ENGINE_PIN` = `77eb428b1c8a05c8abcead3a1062dfbcbca49973` (line 74; warn-only).
  Consumed via `add_subdirectory` (line 224).
- Engine flavor (lines 181–188, **critical**):
  `MILO_ENGINE_BUILD_GFX=ON` + `MILO_ENGINE_GPU_BACKEND=rb3` (both FORCE-cached) →
  builds the rndobj-FREE gfx core (GpuDevice/PipelineManager/Screenshot) PLUS the
  engine-side `BandRnd : Rnd` backend `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`.
  The DC3 `WgpuRnd : NgRnd` flavor + its 6 rndobj-coupled TUs stay OFF (RB3's
  2010-era rndobj can't compile them).
- `Dawn_DIR` default: `${REPO_ROOT}/../dc3-decomp-deps/dawn/lib/cmake/Dawn` (line 194).
- `MILO_ENGINE_DECOMP_INCLUDE_DIRS` (lines 92–99): rb3 matched-fork headers injected
  into the engine build. `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE` (lines 153–179): the
  DC3-shaped engine platform TUs excluded for RB3 (File/Memory/Joypad/Synth/... —
  each replaced by an rb3 shim in `native/src/`).
- Matched-fork globs (lines 246–295): `NATIVE_FORK_SOURCES` = system
  obj/utl/os/math/rndobj/synth/ui/world/track/beatmatch/midi/meta/movie/bandobj/**char**
  + band3 meta_band/game/net_band/tour/bandtrack + `src/App.cpp` + dsp/VibratoDetector.
  Filtered by `_FORK_EXCLUDE_REGEX_*` (platform `_Wii/_Xbox` TUs, GX/GPU backends,
  Wii SDK pulls, dotfile permuter scratch) at lines 300–325, then
  `_NATIVE_FORK_EXCLUDE` (line 333: ~7 still-clang-dirty TUs, weak-stubbed in
  `band3_link_stubs.s`).
- `NATIVE_SHIMS` (lines 354–359): `rvl_shims.cpp`, `native_link_glue.cpp`,
  `native_file.cpp` (stdio File backend), `dta_link_stubs.s`.
- `DTA_LEXER` = `src/system/obj/DataFlex.c` (line 350).

### 1.3 `rb3_configure_target(tgt)` (lines 369–406) — apply to any new target

- compile opts = `DECOMP_FLAGS`; defines = `HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1`
- includes: engine `include/` + `native/include` (vendored httplib) SYSTEM-BEFORE;
  then `native/src`, `src`, `src/system`, `src/band3`, `src/network`,
  `src/system/oggvorbis`, `src/system/synth/tomcrypt`
- links `milo-engine`, Threads, vorbis/vorbisfile/ogg, ZLIB
- `-Wl,--allow-multiple-definition` (header-defined non-inline globals in the fork)

Special-flag TUs (lines 649–656): `rb3_http_server.cpp` and `rb3_replay_api.cpp`
compile with MS-compat **OFF** (httplib/RTTI ABI clash). Any new HTTP-touching TU
must follow the same split: httplib-only TU (no decomp headers) vs handlers TU
(decomp headers, no httplib) — see the long comment at lines 632–648.

### 1.4 rb3-native's per-TU glue in `native/src/` — boot-specific vs reusable

Reusable for a viewer (the render spine):

| TU | role |
|---|---|
| `rb3_render_mesh.cpp` | **the proto-viewer** (load+walk+frame+PNG); exports `LoadMiloAndWalk`/`RenderFrame`/`RenderToPng` via `rb3_render_mesh.h` (shared with the web boot machine — don't fork it, extend it) |
| `rb3_render_tri.cpp` | GPU smoke triangle |
| `rb3_render_hook.cpp` | engine `GameRenderHook` no-op impl, self-registers at static-init; harmless, keep |
| `rb3_game_object_factories.cpp` | `RB3RegisterGameObjectFactories()` — bandobj/track/world/ui Dir + leaf factory registration (211 lines; the W2 desync fix — see §3.3) |
| `rb3_native_settings.cpp` | env-seeded live-tunable settings (`TheNativeSettings()`) |
| `rb3_http_server.cpp` / `rb3_http_handlers.cpp` | HTTP debug API incl. `/api/screenshot` (only if the viewer wants interactive mode; needs its own poll loop — §2.3) |
| `native_file.cpp`, `rvl_shims.cpp`, `native_link_glue.cpp`, `dta_link_stubs.s`, `rndobj_synth_link_stubs.s`, `band3_link_stubs.s` | platform shims + weak no-op stubs — required for ANY link of the fork |

Game-boot-specific (a viewer does NOT need these, but they ride along harmlessly if
the viewer reuses rb3-native's full source list): `rb3_gamewarm_native.cpp` (loading
-vignette GPU warm driver), `rb3_joypad_native.cpp`, `rb3_game_input.cpp`,
`rb3_guestprofile_native.cpp`, `rb3_netsession_native.cpp`, `rb3_server_native.cpp`,
`rb3_save_native.cpp`, `rb3_movie_native.cpp`, `rb3_session_trace.cpp` +
`rb3_trace_taps.cpp`, `rb3_replay*.cpp`, `rb3_synth_native.cpp` +
`rb3_stream_receiver_native.cpp` + `rb3_sampleinst_native.cpp` +
`rb3_keychain_native.cpp` (audio), `rb3_platform_native.cpp`,
`rb3_waitinguser_gate_native.cpp`, `rb3_crowd_imposter_native.cpp`,
`rb3_disc_label_classes.cpp`, `rb3_heap_maint_native.cpp`,
`rb3_prefetch_native.cpp`, `rb3_texsharpen_native.cpp`.

NOTE: `rb3_band_rnd.cpp` **no longer exists** — the BandRnd backend GRADUATED into
the engine as `src/platform/Rnd_Wgpu_RB3.cpp` (CMakeLists comment lines 435–441).

### 1.5 The DRY pattern for a separate executable (copy from rb3-tests, lines 670–692)

```cmake
get_target_property(_RB3_NATIVE_SRCS rb3-native SOURCES)
list(REMOVE_ITEM _RB3_NATIVE_SRCS ${CMAKE_SOURCE_DIR}/src/main_native.cpp)
add_executable(rb3-viewer src/main_viewer.cpp ${_RB3_NATIVE_SRCS})
rb3_configure_target(rb3-viewer)
```

Two load-bearing properties of this pattern (per the rb3-tests comment block):
1. compiling the same OBJECT set keeps **static-init factory registration live** —
   a static library would drop unreferenced factory TUs;
2. both targets always share the exact same engine/glue surface.
Cost: a second ~117 MB link. See §6 for the cheaper Option A.

---

## 2. Renderer bring-up + headless frame + PNG

### 2.1 BandRnd (engine side)

`milo-native-engine/src/platform/Rnd_Wgpu_RB3.{h,cpp}` (header 443 lines, impl 6692).
Global instance `gBandRnd` (extern at Rnd_Wgpu_RB3.h:442); in RB3_GAME mode it IS
`TheRnd`. Viewer-relevant API (header line refs):

- `PreInitRender()` (h:74, cpp:295–316) — registers the rndobj factories
  (Trans/Cam/Mesh/Environ/Mat/Tex/Light/MultiMesh/Group/Dir) + legacy short-name
  aliases `Tex`/`Text`/`Dir` (`RB3RegisterLegacyRndAliases`, cpp:328). Idempotent.
  Reads gSystemConfig → **call AFTER SystemInit**.
- `InitGpu(w, h, headless)` (h:80) — blocking GpuDevice + pipelines + rings +
  default textures (native path; web uses the StartGpuInit/InitGpuResources split).
- `BeginFrame(RndCam*)` / `EndFrame()` (h:101–103) — the synthetic-harness frame
  (used by rb3_render_mesh.cpp). The game path instead uses the `Rnd` virtual
  overrides `BeginDrawing()`/`EndDrawing()` (h:147–149).
- `DrawMesh(RndMesh*)` (h:106) — also invoked by the engine's strong-def
  `RndMesh::DrawShowing`.
- `DrawRect(...)` (h:177) — used by `OutfitConfig::MatSwap::Compose` for the
  hair/outfit two-color tint RTT; `BeginDrawTarget`/`EndDrawTarget` (h:131) is the
  RTT redirect. **Implication: the GPU must be up before any OutfitConfig Compose
  runs, or hair tinting silently does nothing.**
- `WarmGpuForDir(ObjectDir*, budgetMs)` (h:118) — optional pre-upload sweep.
- `InitScreenshots()` (h:143) — env-driven auto-PNG (`MILO_SCREENSHOT_DIR/_FRAMES/_NAMES`).
- `Shutdown()` (h:97) — must run before libc static dtors (Vulkan ICD unmap;
  `RB3RegisterBandRndShutdown()` exit callback in RB3_GAME mode).

### 2.2 Headless surface + readback (what the viewer's PNG capture reuses)

`GpuDevice` (engine `gfx/GpuDevice.h`) with `desc.headless=true` renders into a
single persistent offscreen texture (`mHeadlessTex`); no window/swapchain.
Readback = `gpu.ReadbackHeadlessFrame(pixels, size)`; PNG = `WritePNG(path, ...)` /
`WritePNGToMemory` from engine `gfx/Screenshot.h`. Exact recipe:
`rb3_render_mesh.cpp:430–474` (`RenderToPng`). Headless auto-detect used everywhere:
`getenv("MILO_HEADLESS") || !getenv("DISPLAY")`.

`RenderToPng` deliberately ends with `_exit(rc)` — RB3 ObjectDir vs Dawn-device
static-destructor teardown races otherwise (comment cpp:468–472). Keep that in the
viewer, or wire the `Shutdown()` exit-callback path RB3_GAME uses.

### 2.3 HTTP screenshot path (only for an interactive viewer)

`native/src/rb3_http_server.cpp` (listener thread + command queue; MS-compat-OFF TU)
+ `rb3_http_handlers.cpp` (main-thread handlers). `/api/screenshot` →
`HandleScreenshot` (handlers.cpp:150–180): render-fresh-frame capture hygiene
(`RB3RenderFreshHeadlessFrame`, :127–147 — re-renders one `BeginDrawing`/
`TheUI.Draw`/`EndDrawing` under the sigsetjmp draw guard so the readback isn't
1–2 frames stale) → `ReadbackHeadlessFrame` → `WritePNGToMemory`.
The queue is drained by `RB3HttpServerPoll()` / `RB3HttpServerPollScreenshots()`
**from App.cpp's frame loop** — a viewer wanting HTTP must call these from its own
loop (and note the capture-hygiene helper draws `TheUI`, which a viewer doesn't
have — it would need a viewer-specific fresh-frame callback or to skip hygiene).
For v1, the one-shot PNG path (§2.2) is simpler and already proven.

---

## 3. Milo loading

### 3.1 Load path + platform resolution

- `TheLoadMgr.mPlatform = kPlatformXBox;` — extracted assets are 360-ARK; drives
  BOTH the filename rewrite and stream endianness.
- Path rewrite: `DirLoader::CachedPath` (`src/system/obj/DirLoader.cpp:200–213`):
  `foo/bar.milo` → `foo/gen/bar.milo_xbox` (`"%s/gen/%s.milo_%s"` with
  `PlatformSymbol(TheLoadMgr.GetPlatform())`).
- All engine paths are RELATIVE — the harness `chdir()`s to the data root
  (`RB3_DATA`, default `/home/free/code/milohax/rb3/orig-assets/extracted`). File
  I/O = `native/src/native_file.cpp` (NativeStdioFile, HX_NATIVE strong def).
- Entry point: `DirLoader::LoadObjects(FilePath(path), nullptr, nullptr)` —
  synchronous pump via `LoadMgr::PollUntilLoaded`. Endianness auto-resolve in
  ChunkStream (`.milo_xbox` chunk header LE, body BE — see the corrected endian
  footnote in `rb3_game_object_factories.cpp:34–47`).

### 3.2 Config prerequisites (before loading anything real)

From `RunRenderMesh` / `RunBoot` / `EnsureEngineInit`:

1. `SetSystemArgs(argc, argv)`
2. `SystemPreInit("config/band_preinit_keep.dta")`
3. `SystemInit("config/band_keep.dta")` — populates `gSystemConfig`; gives
   `SystemConfig("objects")` the per-class type-defs property-sync needs (227
   entries). Both are HX_NATIVE-gated curated boots in `src/system/os/System.cpp`.
4. `InjectTypeDefStubs()` (`rb3_render_mesh.cpp:221–243`) — inserts empty
   `(RndTex (types)) (RndDir (types)) (RndText (types))` into the objects config
   (the decomp's OBJ_CLASSNAMEs differ from the on-disc short names; without the
   stubs `OBJ_SET_TYPE` MILO_FAILs).

### 3.3 Factory registration + what breaks when one is missing (the W2 finding)

`Hmx::Object::NewObject(className)` per serialized object. Missing factory:

- **Leaf class** → `"Can't make <Class>"` WARN; loader ReadDead-skips to the next
  `0xADDEADDE` marker. Recoverable (object simply absent).
- **Dir subclass** → the nested directory's byte extent cannot be ReadDead-skipped
  (inner objects have their own dead markers) → parent stream desync → next
  object's PreLoad reads garbage as a `std::vector<Viewport>` count → runaway
  resize → SIGSEGV (native) / wasm OOM (web). Full writeup:
  `native/src/rb3_game_object_factories.cpp:4–32`.
- **Root dir class** → special case: `"<Class> not registered, defaulting to
  RndDir"` — harmless for static rendering (observed with `Character` in §0).

Registration layers available today:

1. `BandRnd::PreInitRender()` — rndobj base set + `Tex/Text/Dir` aliases.
2. `RB3RegisterGameObjectFactories()` (`rb3_game_object_factories.cpp:123–211`) —
   bandobj HUD/track Dirs, TrackDir/TrackWidget/WorldDir, rndobj anim/particle
   leaves, ui/ containers + widgets, UILabelDir/RndFont. NOTE its deliberate
   skip-rule (lines 195–202): register Dir CONTAINERS always; skip leaf widgets
   whose PostLoad needs runtime subsystem state (LabelShrinkWrapper precedent).
3. `RegisterCommonFactories()` (`main_native.cpp:134–157`) — obj/synth leaves.

### 3.4 Factories a CHAR/HAIR viewer must add (none registered by the above)

Observed missing on the hair probe + head-milo contents (test_charload5b):

- `CharHair` (`char/CharHair.h`, OBJ_CLASSNAME `CharHair`) — the hair sim object.
- `OutfitConfig` (`bandobj/OutfitConfig.h`) — **`OutfitConfig::Init()`
  (OutfitConfig.cpp:421) also news the static `sMat`/`sCam`/`sBandCharDesc`** used
  by MatSwap Compose; call Init(), not bare Register().
- `RndAmbientOcclusion` (`rndobj/AmbientOcclusion.h`, OBJ_CLASSNAME
  `AmbientOcclusion`) — in the full game it's registered by the real
  `Rnd::PreInit` (Rnd.cpp:336) which the synthetic harness doesn't run.
- For head/skeleton/clip milos, the `test_charload5b.cpp:123–162`
  `RegisterCharLoadFactories()` set: RndMeshDeform, RndTexBlender,
  RndTexBlendController, CharClipSet, CharClip, CharCollide, CharLipSync,
  CharInterest, CharFaceServo, CharWeightSetter, CharServoBone, BandFaceDeform.
- The FULL char list is `CharInit()` (`src/system/char/Char.cpp:125–180`, ~45
  classes) and the full bandobj list is `BandInit()`
  (`src/system/bandobj/Band.cpp:77–135`) — but both carry traps (§5); register
  targeted subsets instead, mirroring test_charload5b.

**`Character` trap** (`test_charload5b.cpp:141–148`): `Character::Character`
unconditionally `new CharacterTest(this)` whose ctor does
`RndOverlay::Find("char_test", /*fail=*/true)` (CharacterTest.cpp:38). In a
minimal boot no overlays exist (`RndOverlay::Init` is only called by the real
`Rnd::PreInit`, Rnd.cpp:363, reading `SystemConfig("rnd")("overlays")` — and the
extracted `config/rnd.dta` doesn't even list `char_test`). On native a MILO_FAIL is
*survivable* (`PlatformDebugBreak` is a weak no-op stub — `dta_link_stubs.s:174` —
so `Debug::Fail` logs "APP FAILED" and returns; Find then returns 0) but any
subsequent overlay deref is a null-deref risk. Options for the viewer, in order of
preference:
  a) don't register `Character` — root defaults to RndDir; meshes/materials still
     load (proven §0). Sufficient for static hair rendering.
  b) register it AND pre-seed overlay stubs: inject
     `(char_test (lines 1)(showing FALSE))` (+ `char_debug`, `rate`, `heap`,
     `stats`, `timers` if calling the real Rnd::PreInit) into
     `SystemConfig("rnd")("overlays")` before `RndOverlay::Init()` — same
     DataReadString+InsertNodes pattern as `InjectTypeDefStubs`.
  Needed only when the viewer wants live `Character::Poll()` (hair sim / bone
  animation) rather than a static draw.

### 3.5 Cross-milo references (the remaining NOTIFYs)

`torso_femalehairlong.mat couldn't find hair_shared_spec.tex` /
`bone_hair.mesh` — char resource milos reference objects merged in at runtime from
shared milos via `FileMerger` (see BandCharacter.cpp:3098 comment: "each char
RESOURCE milo ... lists" shared subdir merges). For a static viewer these are
non-fatal (mat falls back; bone meshes have no geometry of their own). To resolve
them faithfully, pre-load `char/main/shared/gen/char_shared.milo_xbox` (5 shared
mats) and the relevant shared tex/skeleton milos into a parent dir, or accept the
fallback. Escalate only if the wig bug turns out to live in one of these refs.

---

## 4. Asset layout + character composition

Data root (`RB3_DATA` default): `/home/free/code/milohax/rb3/orig-assets/extracted/`
→ `char/ config/ midiinstruments/ patchcreator/ sfx/ songs/ ui/ world/`.

### 4.1 char/ tree

```
char/anim_genres.dta  char/anim_groups.dta  char/char_objects.dta
char/crowd/  char/extras/  char/shared/
char/main/<category>/...           categories:
  anim bass drum earrings eyebrows facehair feet gen glasses guitar hair hands
  head keyboard legs mic piercings prefab rigging rings shared torso wrist
```

- Hair: `char/main/hair/{female,male}/gen/<gender>_hair_<style>_resource.milo_xbox`
  (81 female files; also `_hat_*` combos). Probe target:
  `char/main/hair/female/gen/female_hair_long_resource.milo_xbox`.
- A hair resource milo's contents (header dump): root class **`Character`**,
  37 objects = 3 `Tex` (diff/diff_output/norm) + 1 `Mat` + ~30 `Trans` hair bones
  (`bone_hair_*`) + 1 `Mesh` (`femalehairlong_resource.mesh`) + `hair.ao`
  [AmbientOcclusion] + `hair.cfg` [OutfitConfig] + `hair_female_longhair.hair`
  [CharHair].
- Prefab descriptors: `char/main/shared/gen/prefabs.milo_xbox` → 21 **BandCharDesc**
  objects (`prefab_female01`, `naked_girl`, ...). (The per-prefab
  `char/main/prefab/gen/prefab_*.milo_xbox` files are near-empty `ObjectDir`s —
  portrait/closet placeholders, not the descriptor source.)
- Shared: `char/main/shared/gen/` — `char_shared.milo` (5 skin/naked mats),
  `head_{male,female}.milo` (+`_clips`), `viseme_*`, `deform.milo`,
  `colorpalettes.milo`, `female_skinny.milo`, `expensive_prefabs.milo`.
- Rigging: `char/main/rigging/gen/{drum,guitar_rh,keyboard,vocal}.milo_xbox`.

### 4.2 How a full character is composed (game path)

`BandCharacter::OnSetFileMerger` (`src/system/bandobj/BandCharacter.cpp:3225–3290`):
one `FileMerger` selects, per slot:
- `char/main/prefab/<prefab>.milo`
- 13 body parts via `BandCharDesc::MakeOutfitPath`
  (`src/system/bandobj/BandCharDesc.cpp:506–523`):
  `char/main/<part>/<gender>/<piece>.milo` for
  head/eyebrows/torso/legs/hands/wrist/rings/feet/**hair**/facehair/earrings/glasses/piercings
- instruments via `MakeInstrumentPath` (:525–539): `char/main/<inst>/<piece>.milo`
  (drums add `_<kit>` suffix)
- anim milos: `char/main/anim/<inst>/body/<gender>/{realtime_<genre>,<tempo>_<genre>}.milo` etc.

So "a full character" = prefab BandCharDesc (from `shared/gen/prefabs.milo`)
driving piece names → hair piece name like `female_hair_long_resource` → the milo
we probed. **The viewer does NOT need FileMerger for v1** — loading one resource
milo directly is exactly the isolation the wig-bug hunt wants. A v2 "compose whole
character" mode would either drive a real `BandCharacter` (heavy: needs CharInit +
BandInit + skeleton rebind glue, cf memory [[project_char_skinning_deform]]) or
manually merge 2–3 dirs.

---

## 5. Existing harness patterns + lighter-boot precedents

- **`RB3_RENDER_MESH` one-shot CLI** — the model for viewer v1. Env matrix:
  `RB3_DATA`, `MILO_HEADLESS=1`, `MILO_WIDTH/HEIGHT` (default 640×480),
  `RB3_RENDER_MESH_PNG=<out>`, `RB3_CAM_DIR=x,y,z` (view direction override),
  `RB3_USE_SCENE_CAM=1`, `RB3_ONLY_SHOWING=1`, `RB3_MESH_VERBOSE=1`,
  `RB3_CLEAR_COLOR=r,g,b`. Camera synthesis (`SynthesizeCamera`,
  rb3_render_mesh.cpp:161–213) uses median center + 90th-percentile radius
  (outlier-robust) and Milo camera-local axes X=right, Y=forward, Z=up.
- **`rb3-tests` `EngineTestFixture`** (`native/tests/test_helpers.cpp:70–97`,
  `EnsureEngineInit`): chdir data dir → `kPlatformXBox` → `SetSystemArgs` →
  `SystemPreInit`/`SystemInit` → `RegisterCommonFactories`. That is the WHOLE
  "RunBoot" — no GPU, no App, no UI; boots in seconds. `test_charload5b.cpp` on
  top of it is the proven char-milo load recipe (+ the Debug modal-callback
  first-failure capture trick, :82–118, worth reusing for viewer diagnostics —
  note MILO_TRY/MILO_CATCH is BROKEN on LP64, longjmp truncates the msg pointer).
- **HTTP-driven captures** — `scripts/native/song-select-capture.py`,
  `band-closeup-capture.py` (+ shared nav in `keyboard-to-gameplay.py`): spawn
  `RB3_GAME=1 RB3_HTTP=1`, poll `/api/health`, drive `/api/input` + `/api/dta/eval`
  verbs, fetch `/api/screenshot`. Relevant if rb3-viewer grows an interactive
  mode; overkill for v1.
- Build loop: `cmake --build /home/free/code/milohax/rb3/native/build-native
  --target rb3-native` (~3s incremental). `tools/ninja-locked` is for the Wii
  decomp build only — irrelevant to the CMake native build.

---

## 6. Recommendation

### Option A (recommended v1, smallest diff): a `RB3_VIEWER=1` mode inside rb3-native

Add one TU `native/src/rb3_viewer.cpp` to rb3-native's source list + a mode branch
in `main_native.cpp` (alongside RB3_RENDER_MESH, main_native.cpp:807). Zero new
CMake surface, no second 117 MB link, and it reuses `LoadMiloAndWalk`/`RenderFrame`
/`RenderToPng` from `rb3_render_mesh.h` directly. The mode differs from
RB3_RENDER_MESH only by:

1. after `PreInitRender()` + `RB3RegisterGameObjectFactories()`, register the char
   set (§3.4): `CharHair::Init(); OutfitConfig::Init(); RndAmbientOcclusion::Init();`
   + the test_charload5b `RegisterCharLoadFactories` list; `Character` per §3.4
   option (a) initially.
2. optional `--frames N` / orbit (re-`SynthesizeCamera` per frame with rotated
   `RB3_CAM_DIR`) and optional `TheTaskMgr`/`Character::Poll` stepping for hair
   sim once Character registration is enabled.
3. multi-milo: load a shared-deps milo first (e.g. `char_shared.milo`) then the
   subject, so cross-milo tex refs resolve.

### Option B (if a standalone binary is required): `rb3-viewer` target via the rb3-tests DRY pattern

CMake exactly as §1.5 with `src/main_viewer.cpp`; `rb3_configure_target(rb3-viewer)`.
main_viewer.cpp = argv parsing + the §6-A init path. Everything else identical.

### Smallest init path (either option), in order

```
gBandRnd.SetClearColor(...);                       // before InitGpu
gBandRnd.InitGpu(W, H, /*headless=*/true);         // BEFORE chdir — Dawn wants clean cwd
chdir(RB3_DATA);
TheLoadMgr.mPlatform = kPlatformXBox;
SetSystemArgs(argc, argv);
SystemPreInit("config/band_preinit_keep.dta");
SystemInit("config/band_keep.dta");
InjectTypeDefStubs();                              // rb3_render_mesh.cpp:221
gBandRnd.PreInitRender();
RB3RegisterGameObjectFactories();
<char factory set — §3.4>;
WalkResult w = LoadMiloAndWalk(absPath);           // resolves relative→RB3_DATA
RenderToPng(w);                                    // ends in _exit(rc) — intentional
```

### Known traps (each cost a prior agent a debugging session)

1. **HX_NATIVE calls into DC3-only engine symbols break the rb3 BandRnd-backend
   link** — shim in `native/src` (memory [[project_native_visual_repro_loop]]).
   Worked precedent: `InvalidateGpuMesh` no-op with rationale,
   `rb3_render_mesh.cpp:47–62`.
2. **GpuDevice init must precede `chdir`** (Vulkan layer discovery uses relative
   paths; comment rb3_render_mesh.cpp:502–506).
3. **`_exit` after PNG** — ObjectDir/Dawn static-dtor race (rb3_render_mesh.cpp:468).
   If you need a clean exit instead, use `RB3RegisterBandRndShutdown()` +
   `TheDebug` exit callbacks like RunGame (main_native.cpp:625–643).
4. **Missing Dir-subclass factory = stream desync SIGSEGV**, not a warning (§3.3).
   When a new milo type crashes in `std::vector<Viewport>::resize`, it's a factory
   gap, not endianness.
5. **Character/CharacterTest `char_test` overlay** (§3.4). Related: `CharDebug`
   (`Char.cpp:119–123`, MILO_DEBUG) derefs a `char_debug` overlay — don't call
   `CharInit()` wholesale in a minimal boot.
6. **Static library would silently drop factory registrations** — always compile
   the TUs into the executable (§1.5).
7. **Mesh CPU geometry**: `SetKeepMeshData` no-free is HX_NATIVE default (fix
   `26c5684d`, opt-out `RB3_MESH_FREE=1`) — head/face meshes rely on it; don't
   "optimize" frees back in.
8. **Do not rewrite sub-100% shared geom/anim decomp code while here** — needs the
   native visual gate (memory [[feedback_decomp_sweep_native_visual_gate]]).
9. **`BandHeadShaper::Init` hard-gates head-milo loading on HX_NATIVE**
   (`src/system/bandobj/BandHeadShaper.cpp:137`) — historical 5b serialization
   distrust; `test_charload5b` exists to prove/disprove it. If the viewer needs
   full heads, that gate is the blocker to revisit, with the test as evidence.
10. **MILO_TRY/MILO_CATCH is broken on LP64** — use the `SetModalCallback` +
    own-jmp_buf capture pattern (test_charload5b.cpp:19–33) for fail-tolerant loads.
11. zsh note for harness scripts: `echo ===` fails under zsh (`=`-expansion);
    several scout one-liners tripped on it. Quote it.

### Suggested acceptance for the implementation task

1. `rb3-viewer` (or `RB3_VIEWER=1`) renders
   `char/main/hair/female/gen/female_hair_long_resource.milo_xbox` to PNG with
   ZERO "Can't make" NOTIFYs.
2. Same for a torso + a head milo (head may stay factory-partial per trap 9).
3. PNG diff vs `probe-female-hair-long-render-mesh.png` (this doc's baseline) to
   confirm the added factories don't regress the static render.
4. Then hand to the wig-bug agent: toggle OutfitConfig tint composition on/off in
   the viewer and compare against in-game closeup captures.
