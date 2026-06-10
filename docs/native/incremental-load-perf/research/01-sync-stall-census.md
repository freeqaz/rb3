# Sync-Stall Census — frame-blocking load paths on the web build

**Wave:** incremental-load-perf, research pass 01 (2026-06-10, read-only).
**Scope:** every call site that can suspend the game frame loop inside one
`RunOneFrame` waiting on asset I/O: `PollUntilLoaded` / `PollUntilEmpty` /
`ForceGetLoader` drains, plus the web-only blocking primitives underneath them
(`WebAssetsFetchSync` sync XHR, JSPI `emscripten_sleep(0)` round trips).

## The two-layer stall model (read this first)

There are **two independent blocking layers**, and most user-visible stalls are
the *product* of both:

1. **Loader-graph layer (shared code):** `LoadMgr::PollUntilLoaded` /
   `PollUntilEmpty` / `ForceGetLoader` are synchronous-contract drains. On web
   they slice + `emscripten_sleep(0)` every ~16 ms (`RB3_LOADER_MIN_YIELD_MS`,
   `src/system/utl/Loader.cpp:176-308`) so the **tab** stays alive, but the
   **game frame loop** is suspended — canvas frozen. Each JSPI suspend costs
   ~4-16 ms of event-loop round trip.
2. **File-I/O layer (web-only):** every `fopen` that misses MEMFS becomes a
   **blocking synchronous XHR on the main thread**
   (`native/src/native_file.cpp:258` → engine
   `milo-native-engine/src/platform/WebAssets.cpp:309`), followed by a
   **per-byte `charCodeAt` string→Uint8Array conversion loop**
   (WebAssets.cpp:~352) — O(n) JS work that is itself substantial for multi-MB
   files. This blocks the frame **even inside the "budgeted" background
   `LoadMgr::Poll()`**: the 8 ms budget (`RB3_LOADER_BUDGET_MS`) is checked
   *between* loader-state slices, but a single `DirLoader::OpenFile` /
   `FileLoader` open that triggers a fetch blocks un-sliceably for the full
   network RTT + transfer + conversion.

**Consequence:** the UI screen-transition machinery is *already async*
(see §State machine below) and panels load `kLoadBack` through the budgeted
poll — yet title→main_hub still stalls 5-10 s, because the budgeted poll's
individual iterations balloon to hundreds of ms each on cache-cold fetches, and
because object `PostLoad`s *nested inside the parse* re-enter
`PollUntilLoaded` synchronously (Dir.h:125 family).

Only `.dta/.dtb` (`/api/bundle`) and boot-critical milos (`/api/bundle/boot`)
are prefetched at boot (`native/web/server.py`); everything else —
panel milos, fonts/icon resource milos, textures, moggs, XMA sidecars —
lazy-sync-fetches on first open. A JS-side IDB cache
(`cacheTryHit`/`cachePutAfterFetch`, native_file.cpp:90-160) makes warm
sessions skip the network but does nothing for first play.

---

## Census table

Legend — Convertibility: how feasibly the call site itself could become
`kLoadBack` + `Loader::Callback` / poll-next-frame. **Independent of that**,
almost every site also benefits from fixing layer 2 (async/prefetched bytes).

| # | Site (file:line) | Trigger phase | Caller chain (runtime) | Why synchronous | Convertibility | Notes |
|---|---|---|---|---|---|---|
| 1 | `src/system/utl/Loader.cpp:124` (`ForceGetLoader` → `PollUntilLoaded`) | all | primitive used by #6,10,11,14 | by definition — returns a loaded `Loader*` | — | the sync primitive; convert callers, not this |
| 2 | `src/App.cpp:459` `PollUntilEmpty` | boot/App-ctor | `AppInit` → App ctor end | boot barrier: everything queued during init must exist before first frame | HARD | sync by design; already throttled (55c2d8e0). Boot-bundle coverage is the lever |
| 3 | `src/band3/bandtrack/TrackPanel.cpp:277` | into-song (panel Enter), restart | `TrackPanel::Enter()` (:118) → `Reload()` → `PollUntilLoaded(mLoader)` | wants `mTrackPanelDir` live to reset players | **dormant in practice** | guarded by `if (!IsLoaded())` *after* a `MILO_FAIL(!IsLoaded())`; UIManager only Enters after `CheckIsLoaded()` — drain unreachable in the normal flow |
| 4 | `src/system/bandobj/PatchDir.cpp:836` | patch/sticker UI (closet, band logo editor, profile art) | `PatchDir::GetSticker(cat,ix,b=true)` (:806) → `LoadStickerTex(push=false)` | caller wants `sticker->mTex` immediately to draw | **EASY** | the async path already exists in the same function: `push=true` → `mStickersLoading` polled by `PollStickers` (:815). Flip call sites + tolerate 1-frame-late tex |
| 5 | `src/system/beatmatch/BeatMaster.cpp:61` | into-song | `BeatMaster::Load(..., b, ...)` sync arm | caller may need song data parsed now | **dormant** | only call site `Game.cpp:261` passes `b=false` → async `BeatMasterLoader` (kLoadFrontStayBack) + `LoaderPoll`. Already converted by design |
| 6 | `src/system/obj/DataFile.cpp:768` | any (dtor) | `DataLoader::~DataLoader` → `PollUntilLoaded(this)` if thread-obj in flight | correctness: in-flight `DataLoaderThreadObj` owns buffers; must finish before free | HARD (keep) | rare — only deleting a *still-loading* DTA loader (e.g. panel unload mid-load). Not a hot path |
| 7 | `src/system/bandobj/BandDirector.cpp:56` | into-song venue load | `BandDirector::LoadVenue` (:1366) → `VenueLoader::Load(async=mAsyncLoad)` | when `mAsyncLoad==false` result needed for immediate `FinishLoading` | **EASY (mostly done)** | `mAsyncLoad = !LOADMGR_EDITMODE` (:83) → async at runtime. EXCEPT the HX_NATIVE `EnterVenue` force-load (:615-618) flips it false → sync venue drain on web. Fixable: let that path stay async + defer EnterVenue |
| 8 | `src/system/rndobj/CubeTex.cpp:29` | milo parse w/ non-cached cubemaps; propsync | `RndCubeTex::SetBitmap(face,fp)` (:55) → `LoadBitmap` → `ForceGetLoader` | builds `RndBitmap` from buffer inline | MEDIUM | shipped (cached) data embeds bitmaps; loose-file path is editor/propsync-leaning. Low traffic |
| 9 | `src/system/synth/SynthSample.cpp:40` | boot sound banks, lesson banks (non-cached only) | `SynthSample::PostLoad` (`!bs.Cached()` → `Sync(sync0)`) → `ForceGetLoader(mFile)` | sample bytes needed to build `SampleData` before bank usable | MEDIUM | cached banks take sync2/sync3 (inline or PreLoad-queued loader, no drain). Audit which web assets are non-cached before investing |
| 10 | `src/system/synth/MoggClip.cpp:200` | any milo containing a MoggClip + `Play()` | (a) `MoggClip::PostLoad` (:93) mid-parse; (b) `MoggClip::Play` (:96) → `EnsureLoaded` → `PollUntilLoaded(mFileLoader)` | `Play` needs the whole mogg buffer for `NewBufStream` | **MEDIUM** | loader is `kLoadFront` from `LoadFile`; PostLoad-drain mid-parse is the bad arm. Convert: leave loader pending, poll in `Play`/owner Poll (pattern = BinkClip `IsReadyToPlay`) |
| 11 | `src/system/rndobj/Tex.cpp:181` | song_select album art, char portraits, milo parse (non-cached) | (a) `RndTex::PostLoad`:451 (`!Cached()` + platform) → `SetBitmap(mLoader)`; (b) `TexLoadPanel::Poll`/`FinalizeTexturesChunk`, `PrefabChar::PollLoadingPortrait` (CharData.cpp:85) | bitmap bytes needed to create GPU tex now | **EASY for (b), MEDIUM for (a)** | (b) callers ALREADY gate on `mLoader->IsLoaded()` before calling → the inner `PollUntilLoaded` is a no-op there. (a) only fires for non-cached milo textures |
| 12 | `src/system/rndobj/Tex.cpp:222` | DTA propsync (`file_path`, Tex.cpp:602), `set_bitmap` handler | script → `SetBitmap(FilePath)` → `ForceGetLoader` | script semantics: property set = effective now | MEDIUM | script-driven; low frequency in normal play |
| 13 | `src/system/obj/Dir.cpp:171` | milo parse of proxy dirs (`ObjectDir::Load`) | stream `Load()` of a proxy ObjectDir → polls *pre-existing* loader for `mProxyFile` | object must be complete when `Load` returns (caller continues parsing stream) | HARD | data dependency mid-parse. Only drains if a loader is already queued |
| 14 | `src/system/obj/Dir.cpp:503` | into-song vocal track init; closet | `VocalTrackDir` pitch arrows (:881/886/891), `LayerDir.cpp:214`, `ClipCollide.cpp:33`, world `Dir.cpp:275`, propsync `Dir.cpp:1007` → `SetProxyFile(fp,false)` → `new DirLoader(kLoadFront)` + `PollUntilLoaded` | callers `Reset(0)` / use the proxy contents immediately after | **MEDIUM** | pitch-arrow milos are small but fetch-bound on web. Could queue + resolve in owner's Poll before first draw |
| 15 | `src/system/obj/Dir.cpp:903` | DTA propsync of `subdirs` | `PropSyncSubDirs` (:965/:974) → `SyncSubDir` → `ForceGetLoader` | propsync contract | MEDIUM | editor/script path; rare in play |
| 16 | `src/system/synth/BinkClip.cpp:212` | into-song (venue crowd audio), lesson/campaign VO | (a) `BinkClip::PostLoad` (:68) mid-parse; (b) `Play()` (:83) → `EnsureLoaded` | `NewBufStream` needs full buffer | **MEDIUM** | same shape as MoggClip; `IsReadyToPlay()` (:200) already exists as the async predicate — callers just don't use it |
| 17 | `src/system/ui/UIPanel.cpp:373` | script-driven screen loads | DTA `{<panel> load 1}` → `OnLoad` → `PollUntilLoaded(mLoader)` | script asked for blocking load explicitly | **EASY-MEDIUM** | only when the DTA passes the blocking arg; plain `{... load}` is async. Census of which scripts pass it needs the extracted .dtb assets |
| 18 | `src/system/ui/UI.cpp:259` | debug only | `Automator::ToggleAuto` (UI automation record/playback) | loads its script synchronously | N/A | **not the screen-transition path** (common misread). Ignore |
| 19 | `src/system/world/LightHue.cpp:56` | venue milo parse (non-cached only) | `LightHue::PostLoad` (`!bs.Cached()`) → `Sync()` → `PollUntilLoaded(mLoader)` | builds `mKeys` from the .bmp now | EASY | cached venues embed keys inline; loose path likely dormant on shipped data |
| 20 | `src/system/obj/DirLoader.cpp:141` (`DirLoader::LoadObjects` static) | boot (`BandCharDesc.cpp:37/94/115` gPrefabs/gDeforms), char load/closet (`BandHeadShaper.cpp:32/142/161`), patches (`PatchRenderer.cpp:15`, `PatchDir.cpp:127`), DTA `CharUtl.cpp:22`, debug `GamePanel.cpp:448` | function returns `ObjectDir*` to a caller that uses it immediately | MEDIUM-HARD | API is value-returning; converting means restructuring callers into poll states. Boot users are behind the boot barrier anyway; closet/headshaper users cause in-menu hitches |
| 21 | `src/system/obj/Dir.h:125` (`ObjDirPtr<T>::PostLoad`) | **every milo with subdirs/inlined dirs/resources** — title, main_hub, song_select, venue | `DirLoader::LoadObjs` → per-object `PostLoad` → `ObjectDir::PostLoad` (Dir.cpp:397/438/445 subdir+inline resolution), `UIComponent::PostLoad` (UIComponent.cpp:178) + `ResourceFileUpdated(false)` (:392) for font/icon resource milos, `UIProxy::PostLoad` (UIProxy.cpp:61), `WorldInstance::PostLoad` (Instance.cpp:286), `VocalGuidePitch`, `DirectInstrument.cpp:33` | parent parse needs child objects resolvable (cross-dir refs fix up during LoadObjs) | **HARD at the parse layer; EASY at the bytes layer** | THE nested-drain workhorse — defeats the 8 ms budget from inside `PollFrontLoader`. `UIProxy::Poll` (UIProxy.cpp:76-80) proves the async adoption pattern: poll `IsLoaded()` then `PostLoad` |
| 22 | `Dir.h:63` (`ObjDirPtr::LoadFile(async=false)` → PostLoad) | **main menu music start** (`MetaMusic::LoadStreamFx` MetaMusic.cpp:315 — six sequential sync dir loads), `UIComponent::ResourceFileUpdated(b=false)`, `WorldInstance::SetProxyFile` | callers wire FxSends / resources right after | **EASY (MetaMusic)** | MetaMusic already tolerates streams starting late; make the 6 FX dirs `async=true` + finish in `SetScene`/Poll |
| 23 | `native/src/native_file.cpp:258` (`WebAssetsFetchSync` on fopen miss) | **all phases** | any `NewFile`/`fopen` → MEMFS miss → sync XHR + per-byte convert + MEMFS write | fopen is a synchronous API | **MEDIUM — the highest-leverage fix** | options: (a) server-side dependency manifests → prefetch closures per screen/song; (b) `emscripten_fetch` + JSPI suspend (lets RAF/compositor run but still suspends game loop — marginal); (c) Worker+SAB fetch with `Atomics.wait` off-main impossible on main thread — so (a) prefetch is the real fix; (d) extend IDB cache warm-up |
| 24 | `native/src/rb3_xma_sidecar.h:117` | gameplay/menu first play of each XMA SFX | `SidecarPCM TryLoad` ← SFX play → fopen miss → `WebAssetsFetchSync` (19 KB-1.5 MB) | decode needs PCM now | **EASY** | enumerate sidecars per bank at bank load and prefetch async (or add to `/api/bundle/boot` for common bank). One hitch per distinct SFX today |
| 25 | `src/system/synth/Synth.cpp:565` (`NewStreamFile` → `NewFile(*.mogg)`) | **preview hover** (`SongPreview::PrepareSong` SongPreview.cpp:283→301 → `TheSynth->NewStream`), into-song audio, VO streams | `MusicLibrary::CheckSongPreview` (MusicLibrary.cpp:490) → `SongPreview::Poll` → `PrepareSong` → `NewStream` → blocking fopen of multi-MB `.mogg` → sync XHR + convert; then vorbis prime | `NewStream` returns a playing-ready stream | **EASY-MEDIUM** | NOT via LoadMgr at all — pure layer-2 stall. SongPreview is already a state machine with an async wait state (`kMountingSong` for DLC mounts); add a fetch-pending state or prefetch `_prev.mogg` when the 1 s hover debounce *starts* (`StartSongPreview` :483 starts a timer — free prefetch window) |
| 26 | `native/src/rb3_render_mesh.cpp:285` (`LoadMiloDirYielding`) | native/web render harness path | `LoadMiloAndWalk` (:346) | yields to browser but synchronous contract for caller | — | prior art for slice+yield; tool path, not the game UI flow |
| 27 | `milo-native-engine/src/platform/AsyncFile_Native.cpp:40` | — (DC3 only) | — | — | N/A | excluded from the RB3 build (`native/CMakeLists.txt:134,161`); RB3's hook is native_file.cpp |

### Sites checked and found NOT to be stalls

- **`src/system/ui/UI.cpp:259`** — debug Automator, not screen transitions (#18).
- **`TexLoadPanel` / `PrefabChar`** — both gate `SetBitmap` on `IsLoaded()`
  first (TexLoadPanel.cpp:109, CharData.cpp:79-85): existing async prior art.
- **`BeatMaster`** sync arm — dormant (#5). **`TrackPanel::Reload`** drain —
  unreachable in normal flow (#3).
- **`CreditsPanel`** (`src/system/meta/CreditsPanel.cpp:16`) — creates a
  `kLoadFront` DataLoader but overrides `IsLoaded()` and consumes in
  `FinishLoad` → fully async panel-pattern prior art.

---

## The UIScreen/UIPanel transition state machine (it's already async)

`UIManager::Poll` (`src/system/ui/UI.cpp:514-628`):

```
if (mTransitionState == kTransitionTo) {
    if ((!mTransitionScreen || mTransitionScreen->CheckIsLoaded())
        && !mCurrentScreen->Exiting() && !IsBlockingTransition()) {
        ... mTransitionState = kTransitionFrom;
        mCurrentScreen->Enter(mTransitionScreen);
    } else if ((mTransitionScreen && !mTransitionScreen->CheckIsLoaded()) ...
}
```

- `kTransitionTo` **waits across frames** for the new screen's panels:
  `UIPanel::CheckIsLoaded` (UIPanel.cpp:231) → `PollForLoading` (:209) checks
  `mLoader->IsLoaded()` non-blockingly, finishing with `FinishLoad` when ready.
- `UIPanel::Load` (UIPanel.cpp:98) defaults **`LoaderPos pos = kLoadBack`**
  (overridable per-panel in DTA `(file ... <pos>)`) → loads ride the budgeted
  per-frame `LoadMgr::Poll`.
- There is even an opt-in prewarm system (`RB3_PREWARM_SCREENS`, default OFF —
  UIScreen.cpp:144+, adoption in UIPanel.cpp:130-175) that pre-issues
  DirLoaders for the *next* screen's milos while the user dwells.

**So the native/web fork does NOT bypass an async loading state — it exists and
runs.** The 5-10 s title→main_hub freeze is the budgeted poll being defeated
from *inside*: each `PollFrontLoader` slice can hit (a) a MEMFS-miss fopen →
blocking sync XHR (#23) and (b) a nested `ObjDirPtr::PostLoad` →
`PollUntilLoaded` drain of a child milo chain (#21), which itself fetches more
files synchronously. The frame returns only after the whole nested chain
completes.

---

## Per-phase attribution

### Boot / wasm-start → intro (~5.4 s App-ctor)
Dominated by #2 (`PollUntilEmpty` barrier) + #23 (sync fetches not covered by
`/api/bundle/boot`) + #20 (gPrefabs/gDeforms `LoadObjects` during
CharCache/PrefabMgr init) + #9 (sound bank). Already heavily optimized
(yield throttle 55c2d8e0); remaining lever = boot-bundle coverage & IDB warm.

### Intro cinematic → title (2-5 s)
Title screen milo + font/icon **resource milos** via #21/#22
(`UIComponent::ResourceFileUpdated` chains) with per-file #23 fetches.
Movie itself is the JS `<video>` shim (no Bink decode load).

### Title → main_hub (5-10 s — worst screen transition)
The async transition machine waits in `kTransitionTo` while main_hub's many
panels (kLoadBack) drain through the budgeted Poll, but per-frame cost explodes
via: #23 (dozens of cache-cold milo/tex fetches, serial, each blocking),
#21 (nested subdir/proxy/resource PostLoad drains inside the parse),
#22 (`MetaMusic::LoadStreamFx` — six *sequential synchronous* dir loads when
menu music starts). Canvas freezes for whole nested chains despite the budget.

### song_select entry (multi-second)
song_select_panel (~2.8 MB milo per the prewarm notes) + MusicLibrary +
patch/sticker resources (#4, #20 PatchRenderer/sResource) + album art
(#11 — already async-gated, fetch-bound only). Prewarm infra (#RB3_PREWARM_SCREENS)
exists but is default-OFF.

### Preview hover (2-5 s freeze)
**#25 almost exclusively**: `SongPreview::PrepareSong` → `NewStream` →
blocking multi-MB `.mogg` sync XHR + per-byte conversion + vorbis prime, all
inside one frame. (Plus #10 for milo-embedded MoggClips.) The 1 s hover
debounce (`mSongPreviewDelay`) is an unused prefetch window.

### Into-song load
Venue (#7 — async normally, but the HX_NATIVE EnterVenue fallback forces sync),
track system milos, char milos via `WorldInstance::PostLoad` (#21),
vocal pitch arrows (#14), crowd audio BinkClips (#16), song audio stream (#25),
each multiplied by #23. BeatMaster/midi already async (#5).

### During gameplay
#24 (first-play XMA SFX sidecar fetch — one hitch per distinct SFX),
#4 (sticker textures), any deferred #21 proxy resolution.

---

## Recommended attack order (for the planning wave)

1. **Bytes layer first (#23): dependency-closure prefetch.** Server-side
   manifest per screen/song (server.py already bundles boot); fetch closures
   async during dwell/transition-start; keeps ALL loader code untouched.
   Converts every other site's cost from network-RTT to CPU-parse.
2. **#25 preview**: prefetch `_prev.mogg` when the hover debounce starts, or
   add an async-fetch state to SongPreview's existing state machine.
3. **#22 MetaMusic::LoadStreamFx** → `async=true` + finish-in-Poll (one-line
   shape change, removes a 6-file sync chain at menu-music start).
4. **#24 XMA sidecars**: per-bank async prefetch.
5. **#7 EnterVenue HX_NATIVE force-sync** → keep async.
6. Enable/promote **RB3_PREWARM_SCREENS** once #1 makes prewarm cheap.
7. Longer term: #21 (parse-internal PostLoad) only matters for CPU-parse cost
   once bytes are local; revisit then (DC3's FILEMERGER_CONVERGENCE docs cover
   the hazards of going truly async inside the parse).
