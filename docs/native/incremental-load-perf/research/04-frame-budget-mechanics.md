# 04 — Frame-budget mechanics: non-network cost centers during incremental loads

Read-only investigation, 2026-06-10. Scope: everything OTHER than network I/O
that blows the frame budget while content loads on the web build — parse,
decrypt, decode, GPU upload — plus the instrumentation needed to attribute a
single dropped frame. Companion to 02-boot-sync-read.md / 03-diagnosis-results.md
(prior wave) and the ground-truth sync-drain inventory in this wave's brief.

All file:line references verified against the tree on 2026-06-10.

---

## (a) CPU cost centers after the bytes arrive

### a.1 The loader state machine: what is sliced, what is monolithic

`LoadMgr::CheckSplit()` (src/system/utl/Loader.h:74-76) is a wall-clock budget
test (`mTimer.Split() > unk1c`). The web `Poll()` arm re-arms it per frame with
`RB3_LOADER_BUDGET_MS` (default 8, Loader.cpp:396) — so anything that *calls*
CheckSplit is sliced; anything between two CheckSplit calls is monolithic.

**Honors CheckSplit (sliced):**

| Stage | Where | Granularity |
|---|---|---|
| File read into buffer | `FileLoader::LoadStream` (Loader.cpp:731-765) | per Eof-wait / chunk read |
| Object instantiation | `DirLoader::CreateObjects` (DirLoader.cpp:516-575, check at :567) | **per object** (after factory NewObject + SetName) |
| Per-object deserialize | `DirLoader::LoadObjs` (DirLoader.cpp:644-687, check at :685) | **per object** — but one object's PreLoad+PostLoad pair is atomic |
| DirUnloader | DirUnloader.cpp:30 | per object |
| Movie loader | Movie.cpp:415 | per state step |

**Does NOT honor CheckSplit (monolithic within one slice / one frame):**

1. **A single object's `PreLoad()`+`PostLoad()`** — DirLoader.cpp:666-673. The
   split check is *between* objects. A 1MB embedded-bitmap `RndTex::PostLoad`
   (Tex.cpp:340-455, `mBitmap.Load(bs2)` at :438) or a fat `RndMesh`/`CharClip`
   deserializes in one go. Same for the *directory object itself*:
   `DirLoader::LoadDir` runs `mDir->PreLoad` + `mDir->PostLoad` back-to-back
   with no split check between them (DirLoader.cpp:631-645) — and ObjectDir
   PostLoad recurses into inline subdirs synchronously.
2. **DTA/DTB parse** — `DataLoaderThreadObj::ThreadStart` (DataFile.cpp:854-876)
   parses the *whole file* (`DataReadStream`/`ReadCacheStream`/`LoadDtz`) in one
   call. On Wii/desktop this runs on the ThreadCall worker thread
   (milo-native-engine src/platform/ThreadCall_Native.cpp:39-61, pthread). **On
   Emscripten there is no worker: `ThreadCallPoll()` runs the entire job
   synchronously on the main thread** (ThreadCall_Native.cpp:115-:140,
   `#ifdef __EMSCRIPTEN__` "run one pending job synchronously per poll"). A big
   .dta/.dtb (songs metadata, locale) = one main-thread parse, zero slicing.
3. **Chunk decompression** — `ChunkStream::PollDecompressionWorker`
   (ChunkStream.cpp:384-393) pops one `DecompressTask` and calls
   `DecompressChunk` *inline on the calling thread* (the "worker" is a no-op
   queue). Each task is one zlib inflate of one milo chunk — bounded by chunk
   size (typically ≤512KB → low-single-digit ms each), naturally amortized by
   the loader pump, but several can run inside one PollUntilLoaded drain.
4. **Mogg decrypt** — NOT whole-file. `VorbisReader::DoFileRead`
   (VorbisReader.cpp:203-250) reads 0x4000 (16KB) at a time and `::Decrypt`
   (VorbisReader.cpp:164-200) AES-CTR-decrypts + HMXA-scans just that chunk.
   Cheap per call; the cost is *who pumps it* (see a.3).
5. **Vorbis decode** — `ogg`/`vorbis` synthesis inside
   `StandardStream::PollStream`/`StuffChannels`; per-call work is bounded, but
   the priming loop batches hundreds of calls (a.3).

### a.2 Texture decode + GPU upload: lazy, but bursty, and always CPU-decode

- At **load time** RB3 does *no* texture conversion: `RndTex::PresyncBitmap`/
  `SyncBitmap` are **no-op inlines on the RB3 native fork** (Tex.cpp:146-147 —
  "RB3 native uses the BandRnd backend" instead of DC3's Tex_Wgpu presync).
  `RndTex::PostLoad` just copies the bitmap bytes from the stream.
- At **first draw**, `UploadRndTexIfNeeded` (milo-native-engine
  src/platform/Rnd_Wgpu_RB3.cpp:565-665) does, per texture:
  - **always chooses RGBA8** — "Choose format: always RGBA8Unorm (CPU-decompress
    DXT)" (:592). It never uses the BC1/2/3 native-upload path even when the
    adapter supports it (unlike DC3's `TextureConvert::MapBitmapFormat`,
    TextureConvert.cpp:369-388, which uploads BC blocks directly when
    `gpu.HasBCCompression()`).
  - copy + `ByteSwapDXT16` of the whole pixel buffer, then `DecompressDXT1/3/5`
    on the CPU (:597-607) — a 1024² DXT5 is a 1MB→4MB expand, ~ms each.
  - one `WriteTexture` upload.
  Cost shape: **a burst of N first-draws on the first frame a new screen
  renders** (menus: dozens of UI textures; song select hover: album art; venue
  load: 100+). This lands on the *first rendered frame after* the load drain —
  i.e. a second stutter right after the loader stall.
- Re-upload guard is cheap: `TexFingerprint` samples 8 bytes (:454-459), so
  steady-state per-draw cost is negligible.

### a.3 Audio stream priming: a deliberate monolithic pump

`StandardStream::Play()` HX_NATIVE arm (StandardStream.cpp:478-499):

```cpp
// On native there's no decode thread, so we must pump the reader until kReady
if (mState == kInit && mRdr) {
    for (int i = 0; i < 500 && !IsReady(); i++) PollStream();
}
...
// Pre-fill ring buffers before the audio device starts consuming
for (int i = 0; i < 20; i++) PollStream();
```

Each `PollStream` → `VorbisReader::Poll` → `DoFileRead` (16KB read + AES-CTR
decrypt + HMXA scan) + ogg page sync + vorbis header/PCM decode +
`StuffChannels` ring fill. Up to 520 iterations decode-priming `mBufSecs` of
multi-channel PCM **inside one frame**. For song preview this stacks on top of
the `MoggClip::EnsureLoaded` PollUntilLoaded drain (MoggClip.cpp:196-208) that
just sync-fetched the multi-MB mogg. So the preview freeze is
fetch-drain → decrypt+decode prime → first-draw texture burst, serialized in
one or two frames.

### a.4 Mesh upload + per-draw unpack

`BandRnd::DrawMesh` (Rnd_Wgpu_RB3.cpp:3030-3290):

- Per-mesh GPU cache (`sMeshGpu`, :3057-3060): VB/IB `CreateBuffer`+write only
  on first draw / dirty — so mesh upload is **lazy + cached**; cost is a
  first-draw burst per new scene (venue: hundreds of `MeshVB` creates;
  `sMeshBufCreatesThisFrame` already counts it, :3286).
- **The CPU vertex unpack runs UNCONDITIONALLY every draw, every frame**
  (:3081-3085: "NB: the unpack runs UNCONDITIONALLY (not gated on needUpload)
  ... Caching the unpacked verts CPU-side to skip this too is the tracked
  follow-up perf win."). Every Xbox-compressed venue mesh re-converts all its
  verts (BE float swizzle + Dec4n decode) per frame. This is a *steady* tax
  that lowers headroom, making any load-time spike more likely to drop a frame.

### a.5 WebGPU pipeline / bind-group creation

- Pipelines: keyed cache (`PipelineManager::GetPipeline`,
  milo-native-engine src/gfx/PipelineManager.cpp:479-489); shader modules
  cached (:160-174). Misses cluster on the first frames of a new screen (new
  blend/zmode/format combos) — each `CreateRenderPipeline` is a synchronous
  WGSL→backend compile on the JS thread (ms-scale on first encounter; Dawn/
  Chrome caches help on revisit). RB3 also creates one-off pipelines outside
  the manager (quad/post/halo, Rnd_Wgpu_RB3.cpp:2474, :2859, :1830, :4762) —
  all lazily on first use.
- Bind groups: cached per mesh-slot (`slot.objBG` :3373, material BG via
  `MakeMaterialBindGroupCached` :1375-1394 rebuilt only when views change), so
  steady state is fine; first-draw frame creates hundreds at ~µs each —
  secondary to texture decode.

### a.6 The sync-drain frame itself

(Ground truth, confirmed.) 17 `PollUntilLoaded`/`ForceGetLoader` sites block
RunOneFrame: e.g. `UIPanel::OnLoad` (UIPanel.cpp:372), `UI::Poll` script load
(UI.cpp:259), `TrackPanel` (TrackPanel.cpp:277), `MoggClip::EnsureLoaded`
(MoggClip.cpp:200). Inside the drain, all of a.1-a.3's work runs back-to-back;
`emscripten_sleep(0)` every `RB3_LOADER_MIN_YIELD_MS` keeps the *tab* alive but
the canvas is frozen (Loader.cpp:176-300). Everything in this doc still
matters under future async I/O: the same CPU work would then run inside the
budgeted `Poll()`, where any single monolithic item > ~8ms still drops frames.

---

## (b) Top monolithic chunks and slice/defer/move feasibility

Ranked by expected contribution to the user-visible stutters:

1. **Audio prime pump (preview hover, song start)** — StandardStream.cpp:482-498.
   - *Slice:* trivial-by-construction: the pump is already an iteration loop.
     Convert "prime fully inside Play()" into a `kPriming` state advanced from
     `SynthPoll` N iterations/frame (or wall-clock budget), starting playback
     when ready. The stream object already has the state machine (`kInit`→
     `kReady`→`kPlaying`). Risk: callers that assert `IsReady()` right after
     Play (MILO_ASSERT at :500) and preview-fade logic that assumes immediate
     start. A capped per-frame budget (e.g. 4ms of PollStream) preserves the
     contract within a few frames.
   - *Move:* decrypt+vorbis in a Web Worker needs the mogg bytes + grinder
     state off-thread; with the existing SAB infra it's feasible (DC3-era
     design had a decode thread), but the sliced-pump option is 10× cheaper.

2. **First-draw texture decode burst** — Rnd_Wgpu_RB3.cpp:592-607.
   - *Cheapest win:* stop CPU-decompressing when BC is available — the DC3
     `TextureConvert` path (TextureConvert.cpp:369-388) already does
     BC1/2/3-native upload + per-mip handling; RB3's `UploadRndTexIfNeeded`
     hardcodes RGBA8. BC upload = memcpy of the *compressed* bytes (4-8×
     smaller, no decode loop). WebGPU `texture-compression-bc` is available on
     desktop Chrome; feature must be requested at device creation
     (GpuDevice has `HasBCCompression()` already).
   - *Slice/defer:* uploads are already lazy per texture; a per-frame upload
     budget (defer remaining textures to next frame, draw mWhiteView
     meanwhile) is easy here since the caller already falls back to
     `mWhiteView` on empty view — visual pop-in tradeoff.
   - *Warm:* after a DirLoader completes, walk new RndTex objects and upload K
     per frame from `Poll()` (a "presync queue"), so the first rendered frame
     isn't the decode frame. DC3's load-time `PresyncBitmap` (Tex_Wgpu.cpp:166)
     is the precedent: it rides DirLoader's per-object CheckSplit slicing.

3. **Synchronous whole-file DTA parse on web** — ThreadCall_Native.cpp:115
   (+ DataFile.cpp:854). Affects title→main_hub (scripts/locale/songs dta) and
   any screen with an `mAutoPath` script load (UI.cpp:259 drains it).
   - *Slice:* hard (recursive-descent parser, lexer state).
   - *Move:* this is the textbook Worker job — pure bytes→DataArray, no engine
     globals except the Symbol table (which IS a global — the real blocker;
     DataArray interning into the shared symbol table is not thread-safe on
     wasm without the pthread build).
   - *Pragmatic:* most cost is .dtb (binary) `ReadCacheStream`, which is
     allocation-bound; measure first (see (c)) — it may be small next to 1+2.

4. **Single-object PreLoad/PostLoad spikes** — DirLoader.cpp:666-673. Can't
   slice *within* an object without per-class continuation support (the Wii
   never needed it; objects were sized for DVD-era budgets). Mitigation is
   measurement-driven: find the >8ms classes (likely CharClipSet / big
   embedded-bitmap RndTex / SynthSample PCM) and special-case those few.

5. **Per-draw compressed-vert unpack (steady tax)** — Rnd_Wgpu_RB3.cpp:3081.
   Already flagged in-source as the tracked follow-up: cache unpacked verts
   CPU-side per mesh generation. Static meshes don't need re-unpack at all
   (only the skinned shard-guard consumes them per-frame).

ChunkStream inflate (a.1.3) and pipeline creation (a.5) look acceptable as-is;
re-rank after instrumentation.

---

## (c) Instrumentation inventory and the gap

**Have today:**

| Instrument | Where | What it gives |
|---|---|---|
| `RB3_FRAME_TRACE=<path>` JSONL | native/src/rb3_frame_trace.cpp (recorder), counters defined in Loader.cpp:55-57 | per frame: `dt` (frame ms), `lp` (LoadMgr::Poll ms), `lpu` (sync-drain ms), `scr` (UI screen), `ld` (loaders added), `st` (streams opened), `pend` (queue depth) |
| `RB3_FRAME_INSTRUMENT=1` | src/App.cpp:748-810 (dirty file — read only) | LONG-frame log lines with the same lp/lpu attribution |
| `gLoadPollMsThisFrame` / `gLoadPollUntilMsThisFrame` | Loader.cpp:46-47 | the two loader entry-point timers feeding both of the above |
| `RB3_BOOT_IO_STATS` | native/src/native_file.cpp:56-67 + Loader.cpp:21-29 | boot-scope totals: sync-XHR count, loader yields, PollUntilLoaded calls |
| `loadperf-profile.mjs` | scripts/web | V8 .cpuprofile + Long Tasks + rAF gaps + Resource Timing + boot milestones, on one timeline; `--nav` drives to song_select/game |
| `analyze-cpuprofile.mjs` | scripts/web | self-time aggregation (wasm/JS/GC buckets, `--grep`) |
| `lib/core.mjs` harness + smoke-test | scripts/web | headless boot/nav/screenshot plumbing |
| native harnesses | scripts/native/song-select-capture.py, song-end-test.py, frame_profiler.py (consumer of RB3_FRAME_TRACE) | same-engine repro at 3s rebuild |
| server request log | native/web/server.py:808-812 | logs non-200s and /api only — static asset GETs are suppressed, so per-fetch timing must come from Resource Timing |
| MILO_PERF frame budget | engine Rnd_Wgpu.cpp:316,1126-1147 | frame-ms histogram + violations (DC3 path) |
| `sMeshBufCreatesThisFrame` | Rnd_Wgpu_RB3.cpp:3286 | mesh VB-create burst counter (logged, not exported) |

**The gap:** today a dropped frame can be attributed to *"loader work"* (lp/lpu)
vs *"everything else"* — nothing splits that "everything else", and nothing
splits the *inside* of lp/lpu into fetch vs inflate vs object-PostLoad vs
DTA-parse, nor charges the render-side first-draw bursts (tex decode, mesh
upload, pipeline create) which happen *outside* the loader timers entirely. The
V8 profile can post-hoc explain a big stall, but at 1ms sampling it can't
reliably attribute a single 20ms frame, and it can't be left on.

**Smallest sufficient addition** (one pattern, ~7 counters, mirrors the existing
`gLoadPollMsThisFrame` convention — engine + rb3 fork, all behind the existing
`gFrameTraceActive` gate, zeroed by `RB3FrameTraceRecord`):

- `gFetchSyncMs/Count` — around `WebAssetsFetchSync` (WebAssets.cpp:339) [the
  I/O baseline the rest is judged against]
- `gInflateMs` — `ChunkStream::DecompressChunk` (ChunkStream.cpp:404)
- `gDtaParseMs` — the `__EMSCRIPTEN__` job body in `ThreadCallPoll`
  (ThreadCall_Native.cpp:115)
- `gObjLoadMs` + **slowest object** (class+name+ms, one slot/frame) — wrap the
  PreLoad/PostLoad pair in `DirLoader::LoadObjs` (DirLoader.cpp:660-676)
- `gAudioPrimeMs` — the two pump loops in `StandardStream::Play`
  (StandardStream.cpp:482-498)
- `gTexUploadMs/Count` — `UploadRndTexIfNeeded` body (Rnd_Wgpu_RB3.cpp:565)
- `gMeshUploadMs/Count` + `gPipelineCreateMs/Count` — the `needUpload` block
  (:3219) and `PipelineManager::GetPipeline` miss (PipelineManager.cpp:482)

…then append them as fields to the existing JSONL record (rb3_frame_trace.cpp:88)
and teach frame_profiler.py to print per-spike breakdowns. That single change
makes every dropped frame self-attributing: `dt ≈ lp + lpu + tex + mesh + pipe +
prime + residue`, with the slowest-object slot naming the (4)-class offenders.

---

## (d) DC3 / shared-engine prior art

- **Load-time texture presync** — DC3's `RndTex::PresyncBitmap`
  (engine src/platform/Tex_Wgpu.cpp:166-230) uploads during object load, so
  texture cost rides DirLoader's per-object CheckSplit slicing instead of
  bursting at first draw; and `TextureConvert::CreateFromBitmap`
  (src/gfx/TextureConvert.cpp:394+) uploads BC1/2/3 natively when supported
  (CPU decode only for DXN/fallback). RB3's BandRnd path uses neither.
- **No upload slicing/budgeting exists anywhere in the engine** — grep for
  budget/slice/defer in src/platform + src/gfx finds only the transparent-draw
  ordering queue (TransparentQueue.cpp) and MILO_PERF reporting. A per-frame
  upload budget would be new code.
- **ThreadCall** is a real pthread worker on desktop (ThreadCall_Native.cpp:39)
  but degenerates to synchronous-on-poll under `__EMSCRIPTEN__` — any "move it
  off-thread" plan on web means a real Worker + message protocol (or the
  pthread/Worker Emscripten build), not ThreadCall.
- **DC3 loading docs** (dc3-decomp docs/native/LOADING_ARCHITECTURE.md,
  FILEMERGER_CONVERGENCE.md, docs/sessions/convergence/05-filemerger-async-
  pipeline.md): DC3 converged on *forcing the real async loader path* (removing
  native sync shortcuts, Step 1 "Force async loading on native") rather than
  parallelism — consistent with this doc's conclusion that Milo's loader is
  already an async-shaped state machine and the wins are (i) stop draining it
  synchronously, (ii) make the per-item work fit the slice. DC3 "still
  stutters" because it kept the monolithic per-item costs (single-object
  PostLoad, sync DTA parse, presync of large textures still one object = one
  slice).

## Bottom line

Even with perfect async I/O, three monolithic CPU/GPU chunks will keep
breaking the 16ms budget: (1) the StandardStream::Play 500+20-iteration
decrypt+decode prime, (2) the first-draw DXT→RGBA CPU decode burst (made ~10×
worse than necessary by ignoring BC upload), (3) synchronous whole-file DTA
parse via the Emscripten ThreadCallPoll fallback — plus a steady per-frame tax
from the unconditional compressed-vert unpack that shrinks the budget
everything else must fit in. All three have natural slicing/deferral seams
already present in the code. The blocking instrumentation gap is that neither
the inside of the loader drains nor the render-side bursts are attributed;
the ~7-counter frame-trace extension above closes it.
