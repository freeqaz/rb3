# Incremental-Load Performance — Synthesis & Plan

**Date:** 2026-06-10. Synthesis of the six research docs in
`docs/native/incremental-load-perf/research/` (01-sync-stall-census,
02-async-architecture, 03-web-io-redesign, 04-frame-budget-mechanics,
05-web-phase-waterfall [measured], 06-native-vs-web-stall [measured]).
Measurement (05/06) trumps theory (01-04) wherever they conflicted; conflicts
are called out explicitly in §1.3.

> **Adversarially verified 2026-06-10** by three independent Opus lenses
> (code-correctness, measurement-validity, risk/feasibility) — verdicts in the
> workflow record. All corrections are folded into this document; the notable
> ones: (1) Q7 was refuted as written (MetaMusic already passes async=true; the
> blocker is the explicit `PostLoad`) — task rewritten; (2) the preview freeze
> is in `SongPreview::PrepareSong` → `TheSynth->NewStream` sync open, NOT
> `MoggClip::EnsureLoaded` — T3 retargeted; (3) engine-file web arms must be
> gated `#ifdef __EMSCRIPTEN__`, never `HX_WEB` (milo-engine is compiled with
> HX_NATIVE only; HX_WEB exists only on the rb3-web target); (4) T6 must also
> give `src/system/utl/ChunkStream.cpp` a pending-file arm; (5) the named CPU
> ms in §1.1 come from a `-O0` debug profile (boots 1.67× slower than release)
> — treat them as *rankings*, not release-savings estimates.

---

## 1. Executive summary

### 1.1 Where the seconds actually go (measured, release `-O2`, localhost)

| Phase | wall (median) | frozen (rAF Σ>33ms) | dominant cost | source |
|---|--:|--:|---|---|
| nav → appBooted | **3.82 s** | 1.24 s | ~1.0 s wasm+bundles+GPU; **~2.83 s App-ctor body**: milo BE→LE swap (~550 ms shape), software DXT decode (~600 ms shape), first-draw `BandRnd::DrawMesh` upload (816 ms shape), vector alloc/teardown, sync-XHR JS glue (354 ms), DTA yylex (299 ms). *All "shape" ms are from the `-O0 -g2` debug profile (boots 6.37 s vs release 3.82 s, 1.67×) — rankings hold; absolute release savings are smaller and non-uniform (the JS glue won't shrink under `-O2`).* | 05§a,b |
| appBooted → intro | 0.14 s | ~0 | instant | 05§a |
| intro → splash (title) | 1.46 s | 0.40 s | CPU: char/venue milo build (19.5 MB milos, served in 72 ms) | 05§b |
| splash → main_hub | 2.19 s | **0.99 s** | CPU: venue shell milos `sv8_a` 6.2 MB / `sv3_a` 5.5 MB (11.8 MB, served 27 ms) + a **~170 ms shared scene-build/pipeline spike** that also reproduces on native | 05§b, 06§a |
| song_select entry | — | 0.19 s (197 ms gap) | 33 UI/venue milos, 12 MB | 05§d |
| **preview hover (cold)** | — | **0.44–0.51 s** localhost | full **32–37 MB mogg** blocking sync XHR + per-byte string convert + MEMFS/IDB copies + decrypt/prime; **on a real network the 30 MB transfer itself blocks the canvas (~13–15 s @ 20 Mbps, arithmetic — UNVERIFIED by direct throttled measurement)** | 05§d, 06§b |
| preview hover (warm, same session) | — | 17–46 ms | MEMFS-resident; zero fetch | 05§d |

**Critical caveat carried from 05:** all network numbers are localhost lower
bounds (52 MB of milos served in 169 ms). On real bandwidth, every byte column
becomes a *blocking, canvas-frozen* sync XHR — the user-reported 5–10 s
title→main_hub vs the measured 2.19 s is plausibly explained by real network
and/or the `-O0` debug build and/or cold IDB on a slower machine
(**UNVERIFIED** which; experiment E3 resolves it).

### 1.2 Architecture diagnosis in plain language

Milo's loader **was built async** and is still async above the `File` class:
FileLoader/DirLoader state machines poll `ReadDone()`, ChunkStream surfaces
`TempEof`, `CheckSplit()` slices parse per object, kLoadFront queue position is
the dependency graph, UI screen transitions already wait across frames
(`UIManager::Poll` kTransitionTo, UI.cpp:514-628; `UIPanel::Load` defaults
`kLoadBack`, UIPanel.cpp:106). The native port collapsed asynchrony at exactly
one seam: **`NativeStdioFile`'s constructor does a blocking synchronous XHR on
MEMFS miss** (`native/src/native_file.cpp:258` →
`milo-native-engine/src/platform/WebAssets.cpp:339-340`,
`xhr.open(GET,url,false)`), and `ReadAsync` reads inline. Two consequences:

1. The 8 ms `LoadMgr::Poll` budget can't preempt a fetch — a single `OpenFile`
   blocks the frame for the full network round trip *plus* a per-byte
   `charCodeAt` string→Uint8Array conversion loop (WebAssets.cpp:341-352).
2. The 17 `PollUntilLoaded`/`ForceGetLoader` drain sites — designed as
   backstops that normally no-op (06 measured **`lpu` = 0.0 ms on native at
   every loader budget**) — become serial chains of blocking 30 MB XHRs on web.

On top of the I/O seam, three **CPU** chunks break the budget even with bytes
local (04, confirmed by 05's profile): software DXT→RGBA8 decode at first draw
(`UploadRndTexIfNeeded` always picks RGBA8Unorm, engine Rnd_Wgpu_RB3.cpp:592),
the BE→LE milo vertex/color swap, and the `StandardStream::Play` 500+20
iteration prime pump (StandardStream.cpp:482-498). Plus the steady per-draw
compressed-vert unpack tax (Rnd_Wgpu_RB3.cpp:3081-3085).

### 1.3 Where research disagreed (resolution)

- **01 ("bytes-layer prefetch is highest leverage / network-bound") vs 05/06
  ("network is NOT the local bottleneck"):** both right at different
  bandwidths. On localhost, CPU dominates; on real networks the sync XHR
  dominates *and* freezes the canvas regardless of CPU. The plan therefore
  needs BOTH the async-I/O seam fix (kills the canvas freeze at any bandwidth)
  and the CPU reductions (kills the residual localhost stalls).
- **06 ("rearchitecting the 17 sync-drain sites is NOT the lever") vs 01's
  per-site conversion ranking:** 06's `lpu=0` proof settles it — convert the
  *File seam*, not the call sites. EXCEPTION: the preview path still needs its
  own treatment, because the freeze is in **synchronous stream construction**:
  `SongPreview::PrepareSong` (SongPreview.cpp:283-301) calls
  `TheSynth->NewStream` → `NewStreamFile` (Synth.cpp:549) → `NewFile(path,2)`
  (Synth.cpp:569) = the blocking sync-XHR open, all BEFORE the existing
  `kPreparingSong`/`IsReady()` poll even starts. (NOT `MoggClip::EnsureLoaded`
  as research doc 05 claimed — MoggClip serves a different consumer; doc 06
  has the correct trace.) `IsReady()` polls vorbis-prime readiness, not the
  fetch — so the fix must ensure bytes are resident *before* constructing the
  stream (prefetch at debounce start), not poll after. Native `lpu=0` holds
  only when bytes are resident.
- **05's cold-hover "freeze is CPU: decrypt + vorbis prime" vs 06's native
  measurement (decrypt+prime ≈ 10 ms):** contradiction. The 0.44–0.51 s
  localhost freeze is most plausibly the **sync-XHR JS glue** (per-byte
  convert of 32–37 MB ≈ 72 MB JS string, MEMFS write, IDB write-queue copy —
  `flushPendingWrites`+`saveResponseAndStatus` already showed 354 ms at boot)
  plus wasm-vs-native slowdown, NOT crypto. Exact split **UNVERIFIED** —
  the T1 counters resolve it.
- **Ground-truth misattribution corrected by 01:** `UI.cpp:259` is the debug
  Automator, not screen transitions; `TrackPanel.cpp:277` and
  `BeatMaster.cpp:61` sync arms are dormant in normal flow.

---

## 2. Ranked lever list

### QUICK WINS (days each, independently landable)

| # | Lever | Impact (measured/estimated) | Effort | Risk | Deps | Evidence |
|---|---|---|---|---|---|---|
| Q1 | **Frame-trace counter extension** — ~7 `g*MsThisFrame` counters (fetchSync, inflate, dtaParse, objLoad+slowest-object, audioPrime, texUpload, meshUpload/pipelineCreate) appended to the existing JSONL record | enables attribution of every dropped frame; resolves the §1.3 hover-CPU split | S | nil | none; do FIRST | 04§c (exact insertion points listed) |
| Q2 | **Preview hover prefetch + deferred stream construction.** Fire an async mogg prefetch (fetch → MEMFS) when the ~1 s preview debounce *starts* (`MusicLibrary::StartSongPreview`, MusicLibrary.cpp:483); in `SongPreview::PrepareSong` (SongPreview.cpp:283-301) do NOT call `TheSynth->NewStream` until the bytes are resident (`memfsResident()` check or prefetch-complete flag) — add a fetch-pending state ahead of the existing `kPreparingSong` poll. The freeze is the synchronous `NewStream`→`NewFile` open, so a Play-side poll alone fixes nothing; prefetch is REQUIRED, not an optimization | kills the worst single stall: 0.45 s localhost, **multi-second→~0 on real networks** | S–M | preview-cancel-on-select must cancel the pending state; mispredicted hover wastes one mogg fetch | none (better with Q3) | 01#25, 02§d-Opt1, 05§d, 06§b + verification |
| Q3 | **Range-backed `.mogg` File** (HTTP 206; server already serves Range, server.py:736-790). Preview touches header+seek-map+~2–4 MB instead of 31–36 MB; gameplay song-start stops paying 36 MB too; kills the 36 MB/song MEMFS+IDB retention | ~10× less preview bytes; song-start stall ↓ | M | chunk-cache correctness; CTR decrypt already chunk-friendly (64 KB read chunks / 16-byte AES blocks, VorbisReader.cpp:189,:208) | manifest size map (from A1, or standalone) | 03§Opt1b |
| Q4 | **BC-native texture upload** — stop CPU-decompressing DXT when `HasBCCompression()`; DC3's `TextureConvert` path exists (TextureConvert.cpp:369-388); RB3's `UploadRndTexIfNeeded` hardcodes RGBA8 (Rnd_Wgpu_RB3.cpp:592) | ~600 ms of boot profile + every screen's first-draw burst; 4–8× less upload bytes | S–M | visual diff (needs `visual_diff.py` gate per mesh-cache lesson); request feature at device creation | none | 04§b2, 05§b |
| Q5 | **Async-arraybuffer sync fallback** — replace the binary-string sync XHR with JSPI-suspended `fetch()`+arraybuffer wherever a blocking fetch survives (~30 lines, WebAssets.cpp) | kills the per-byte convert loop (~354 ms glue at boot; big slice of the 0.45 s hover freeze) | S | none | none | 03§cheap-fix, 05§b |
| Q6 | **IDB pre-warm cap** — `loadAllRows` (rb3_pre.js:185-206) loads every cached row into a JS Map uncapped; ten hovered moggs ≈ 360 MB next boot. Cap/exclude `.mogg`, lazy-load large rows | prevents latent OOM | S | none | none — ship regardless | 03§1 |
| Q7 | **`MetaMusic::LoadStreamFx` deferred PostLoad** — six sequential dir loads at menu-music start are ALREADY issued async (`LoadFile(fp, true, ...)`, MetaMusic.cpp:315); the blocking is the explicit `PostLoad(nullptr)` on :316 (→ PollUntilLoaded) PLUS the caller's immediate 6-way deref `unk70[i]->Find<FxSendEQ>(...)` at :174. Fix = remove the eager PostLoad AND move the FX-wiring loop into a poll state gated on all six `IsLoaded()` — a two-function state-machine change, NOT a one-line flip (original framing refuted by verification) | removes a 6-file sync chain hitting title/main_hub | M | streams start a few frames late (tolerated); FX must not be touched before wiring completes | none | 01#22 + verification |
| Q8 | **XMA sidecar prefetch** — per-bank async prefetch (rb3_xma_sidecar.h:117, 19 KB–1.5 MB blocking XHR per first SFX play) | one hitch per distinct SFX → 0 | S | none | none | 01#24 |
| Q9 | **EnterVenue HX_NATIVE force-sync revert** — BandDirector.cpp:615-618 flips `mAsyncLoad=false`; retail keeps async (`!LOADMGR_EDITMODE`, :83) | into-song venue drain → async | S | verify gameplay-entry gating still holds | none | 01#7 |
| Q10 | **`RB3_PREWARM_SCREENS` default-ON for web** (UIScreen.cpp:205; infra + UAF fix `585ad0f8` already landed; native A/B was neutral, web is the predicted win) | hides next-screen milo fetch+parse during dwell | S | A/B first; pointer-identity ownership rules (02 L6) | better after A1/A2 | 01§rec6, 02 hazard 9 |
| Q11 | **Sliced audio prime** — the 500+20-iteration pump lives INSIDE `Play()` with `MILO_ASSERT(IsReady() \|\| kSuspended)` between the loops (StandardStream.cpp:478-499), so a `kPriming` state cannot live inside Play(): the slice must gate UPSTREAM — don't call Play() until a per-frame-advanced prime state reports ready | bounded; native says prime ≈ few ms, wasm multiple ×; **UNVERIFIED magnitude on web** — gate on Q1 counters | M | callers asserting `IsReady()` right after Play | Q1 first | 04§b1, 06§b + verification |

### ARCHITECTURE (the async-loading rework + engine-better-than-stock track)

| # | Lever | Impact | Effort | Risk | Evidence |
|---|---|---|---|---|---|
| A1 | **Pending-File async open** (restore the Wii ReadAsync contract at the File seam): on MEMFS miss return a `WebPendingFile` that kicks the existing async `WebAssetsFetch` (WebAssets.cpp:118-150), answers `Size()`/`Fail()` from a boot-loaded `/api/manifest` map (server.py:414-435, doubles as 404 oracle), `ReadDone()=false` until bytes land; keep the sync fast path for MEMFS/IDB-resident files. Bonus: start fetches for queued non-front loaders at AddLoader time (network parallelism without touching the single-lane parse) | frame loop keeps running through ALL kLoadBack loads at any bandwidth; sync drains shrink (parallel in-flight fetches vs today's serial chain); converts the whole problem from network-bound to parse-bound | M (~2-3 days core; 1-2 wks incl. FileLoader **and ChunkStream** web arms + validation — `ChunkStream` ctor (src/system/utl/ChunkStream.cpp:69-91) calls NewFile, sets `mFail=!mFile\|\|mFile->Fail()` and issues `ReadAsync(&mChunkInfo,0x810)` immediately, and `DirLoader::OpenFile` checks `mStream->Fail()` on the spot — `Fail()` must answer from the manifest oracle while pending) | Size()-before-ready callers (`FileLoader::OpenFile` calls `Size()` AND `Fail()` synchronously, Loader.cpp:670-673, then `AllocBuffer()` reserves the FULL buffer up front — a pending 36 MB mogg FileLoader holds 36 MB for the whole fetch); Eof position exactness (02 L7); double-completion vs in-flight JSPI suspend | 02§d-Opt2, 03§Opt1 + verification |
| A2 | **Per-screen dependency bundles** — generalize `/api/bundle/boot` (`_emit_bundle` shared) to `main_hub`/`song_select` manifests fired async at transition-start | first-visit transitions go warm-path; 5-10 s real-network freezes → ~CPU-only | S–M | mispredicted prefetch wastes bandwidth only | 03§Opt3, 01§rec1 |
| A3 | **Completion-work slicing** (DC3 L4 residual): Clear() deletes, MergeDirs, first-draw GPU upload budgeted per frame; only where Q1 counters still show hitches | removes the post-load hitch class | M–L | half-merged dirs visible to Find()/draw unless gated | 02§d-Opt3c, 04§b |
| A4 | **Pre-converted assets at extraction** — ship LE-swapped milos / BC-ready textures; kills the ~550 ms BE→LE swap + remaining decode | boot/transition CPU ↓ | M–L, **UNVERIFIED feasibility** (asset pipeline + cache invalidation) | divergence from Wii/360 asset truth | 05§takeaway2 |
| A5 | **Pipeline/scene pre-warm** for the shared ~170 ms hub spike (native nonLoader=164 ms; reproduces on web as WebGPU pipeline compile) | one 170-650 ms hitch class | M | separate workstream, independent of loader | 06§c, 04§a5 |
| A6 | Cache unpacked verts (per-draw unconditional unpack, Rnd_Wgpu_RB3.cpp:3081) | steady headroom ↑ | M | already in-source tracked follow-up | 04§a4 |

**Explicitly rejected** (03): Worker+SAB fetch (main thread can't `Atomics.wait`;
identical poll shape to emscripten_fetch), pthreads I/O build (full recompile,
emdawnwebgpu single-thread restriction, immature JSPI×pthreads — no unique I/O
benefit). Loader-budget tuning (`RB3_LOADER_BUDGET_MS`) is a non-lever — 06
measured spike-height-vs-count trade only; leave at 8.

---

## 3. Recommended architecture direction

**Primary: restore the async ReadAsync contract at the File seam (A1) + Range
moggs (Q3) + per-screen/hover prefetch (A2/Q2), then budget-fit the residual
CPU work (Q4, Q11, A3 where measured).**

Why this over the alternatives:
- *Per-call-site conversion of the 17 drains* (01's framing) attacks symptoms:
  06 proved the drains are free with resident bytes; A1 makes bytes-resident
  the steady state and the drains revert to designed-for no-op backstops.
- *Worker/pthread I/O* adds platform cost with no completion-model improvement
  (03 Options 2/4).
- *Prefetch-only* (no A1) leaves every mispredicted first touch a multi-second
  canvas freeze and the frame budget still un-enforceable below `OpenFile`.
- A1 is the smallest change that reactivates **engine machinery that already
  exists and is tested by 15 years of Wii operation** — `while (Eof()!=NotEof)
  { if (CheckSplit()) return; }` paths are currently dormant dead code (02§a).

**Invariants that must hold** (02 §minimal-correctness-set, distilled):
1. Only the front loader runs CPU parse per slice (FilePathTracker /
   gLoadingProxyFromDisk / rev-stack exclusivity); slice across frames, never
   poll loaders concurrently.
2. kLoadFront queue position = dependency edge; a child spawned during a
   parent's poll completes before the parent resumes (`GetFirstLoading()!=this`).
3. Loaders are created at call time on the caller's heap (`mHeap` captured at
   ctor); defer only the wait.
4. `GetDir`/`GetBuffer`/`Data` only after `IsLoaded()`; exactly one
   access-then-release (`mAccessed` ownership transfer).
5. `FinishLoading` only after SyncObjects; `FailedLoading` on early death.
6. Never adopt/steal a loader you didn't issue (pointer identity, prewarm UAF
   `585ad0f8`).
7. Stream byte positions exact under async delivery; ReadDead must terminate
   (DC3 L7).
8. DESIGN GOAL for A1 steady-state: yield by returning from RunOneFrame rather
   than mid-frame JSPI suspends (4–16 ms each, DC3 L8). NOTE: today's shipping
   loader DOES mid-frame `emscripten_sleep(0)` (Loader.cpp:267, throttled to
   one per 16 ms) and that is the current load-responsiveness mechanism — do
   not remove it until A1 makes the drains short; this invariant governs NEW
   code, not a retrofit.
9. No new sync drain reachable per-interaction from RunOneFrame (DC3 L1).
10. Boot must not regress: keep the sync fast path for MEMFS/IDB-resident
    files; gate with `RB3_BOOT_IO_STATS` + the 05 waterfall harness
    (App-ctor ≤ 3.9 s localhost).

**Staged rollout (house style: default-on after A/B, env opt-out per feature):**
1. Wave 0: Q1 counters + E1 fake-async experiment (de-risks A1 before any web
   code).
2. Wave 1 (independent quick wins): Q2 (`RB3_PREVIEW_PREFETCH_OFF`), Q5, Q6,
   Q7 (`RB3_METAMUSIC_SYNC` to force old), Q8, Q9.
3. Wave 2: A1 behind `RB3_ASYNC_OPEN_OFF` (default ON after the E2/E3 A/B);
   Q3 behind `RB3_MOGG_RANGE_OFF`; Q4 behind `RB3_BC_TEX_OFF` with a
   `visual_diff.py` gate.
4. Wave 3: A2 bundles; Q10 prewarm flip after web A/B; Q11 if Q1 counters
   justify.
5. Wave 4: A3/A5 only where the counters still show >16 ms frames.

---

## 4. Experiment specs (cheapest de-riskers)

**E1 — Fake-async open vs the real loader (de-risks A1, the 90% risk, in an
afternoon).** Dev-only `RB3_FAKE_ASYNC_OPEN_MS=<n>` in `native_file.cpp`: stub
File whose `ReadDone()` stays false for n ms (timer, no network), then
delegates to `NativeStdioFile`. Run the **native** build (3 s rebuild) through
boot→main_hub→song_select→preview→gameplay-start at n=50/200/1000 using
`/tmp/native_stall_probe.py` (06's driver; recreate from 06§method if expired).
**Success:** no deadlock/assert at any n — watch `MILO_ASSERT(t==TempEof)`
(Loader.cpp:735 LoadStream), DirLoader Cleanup, VorbisReader `mFail`, the 17
drain sites, `ForceGetLoader`. Any failure = a named call site to fix before
A1.

**E2 — Counter-attributed re-run of the 05/06 harnesses (after Q1).** Re-run
`/tmp/rb3perf/waterfall.mjs` + `preview-hover.mjs` (05§method) and the native
probe with the new JSONL fields. **Success:** every >50 ms frame decomposes as
`dt ≈ lp + lpu + tex + mesh + pipe + prime + fetchSync + residue` with residue
< 20%; resolves the hover decrypt-vs-glue split (§1.3).

**E3 — Throttled-network web measurement (validates all "real network"
claims, currently arithmetic-only).** Playwright CDP
`Network.emulateNetworkConditions` (e.g. 20 Mbps / 40 ms RTT) wrapped around
the 05 harness. **Success criteria:** cold preview hover freeze scales to
multi-second as predicted (>5 s @ 20 Mbps); title→main_hub approaches the
user-reported 5–10 s. This also produces the BEFORE baseline that Wave 2 must
beat (target: cold hover frozen-time < 100 ms with Q2+Q3+A1; transitions
canvas-live throughout).

**E4 — BC-upload prototype A/B (de-risks Q4).** On native: hack
`UploadRndTexIfNeeded` to the TextureConvert BC path behind `RB3_BC_TEX=1`,
capture `scripts/native/song-select-capture.py` screenshots, gate with
`visual_diff.py` against baseline (mesh-cache lesson: strict visual gate).
**Success:** pixel-identical-or-better screens + measured `gTexUploadMs` drop
on the splash→hub transition.

**E5 — Hover-prefetch-only probe (de-risks Q2 without engine changes).** JS
shim in `rb3_pre.js`-land (or manual console `fetch` + MEMFS write via the
existing bundle unpack helper) that pre-warms the hovered song's mogg, then
run `preview-hover.mjs` cold. **Success:** cold hover freeze collapses to
warm-hover numbers (17–46 ms) — proven mechanism since 05 already showed
MEMFS residency short-circuits the XHR (`memfsResident()`,
native_file.cpp:249).

---

## 5. Handoff decomposition (concurrent Opus agents)

> Shared-file conflict map at the end; schedule tasks in the listed waves.
> NEVER touch `src/App.cpp` (dirty, another agent) or matched Wii code except
> via `#ifdef HX_NATIVE` arms per house style. Engine changes go to
> `../milo-native-engine` first, then bump `MILO_ENGINE_PIN`.

**T1 (Wave 0) — Frame-trace counters.** Goal: per-frame attribution counters
per 04§c. Files: engine `WebAssets.cpp`, `ThreadCall_Native.cpp`,
`Rnd_Wgpu_RB3.cpp`, `PipelineManager.cpp` (**engine TUs gate web-only code on
`#ifdef __EMSCRIPTEN__`, NEVER `HX_WEB` — milo-engine is compiled with
HX_NATIVE only; HX_WEB is defined only on the rb3-web target**); rb3
`native/src/rb3_frame_trace.cpp`, `src/system/utl/ChunkStream.cpp`,
`src/system/obj/DirLoader.cpp` + `src/system/synth/StandardStream.cpp`
(HX_NATIVE-gated timing wraps only — verify match-neutral with objdiff).
Counter pattern: plain globals incremented behind `gFrameTraceActive`, read +
zeroed by the recorder (precedent: Loader.cpp:55-57, :155-158) — no App.cpp
edit needed; note the recorder zeroes AFTER writing each line while App.cpp
zeroes the two legacy counters BEFORE RunOneFrame (different reset point,
workable). Verification: native run shows new JSONL fields; sum ≈ dt on spike
frames. Opt-out: behind the existing `gFrameTraceActive` gate. **Do not edit
App.cpp** (trace wiring there is already landed).

**T2 (Wave 0) — E1 fake-async experiment.** Goal: validate loader tolerance of
multi-frame opens. Files: `native/src/native_file.cpp` only (+ /tmp drivers).
Verification: E1 criteria. Flag: `RB3_FAKE_ASYNC_OPEN_MS` (default off).

**T3 (Wave 1) — Preview prefetch + deferred stream construction.** Goal: kill
the cold-hover freeze. Files: `src/band3/meta_band/MusicLibrary.cpp` /
`src/system/meta/SongPreview.cpp` (HX_NATIVE arms), possibly a native shim in
`native/src/`. **The freeze is `PrepareSong`'s synchronous
`TheSynth->NewStream` → `NewFile` sync-XHR open (SongPreview.cpp:283-301,
Synth.cpp:549/:569) — NOT `MoggClip::EnsureLoaded`; do not touch MoggClip.**
Approach: fire async prefetch at debounce start
(`MusicLibrary::StartSongPreview`, :483); in PrepareSong, defer the
`NewStream` call behind a fetch-pending state until the mogg is MEMFS-resident
(the existing `kPreparingSong`/`IsReady()` poll then handles vorbis prime as
today); cancel pending on selection change. Verification: E5 harness — cold
hover ≤ 100 ms frozen under E3 throttling. Opt-out: `RB3_PREVIEW_PREFETCH_OFF`.

**T4 (Wave 1) — Small sync-site fixes bundle.** Goal: Q7 MetaMusic deferred
PostLoad (NOT an async-flag flip — `LoadFile` is already async; remove the
eager `PostLoad(nullptr)` at MetaMusic.cpp:316 AND defer the 6-way
`Find<FxSendEQ>` wiring at :174 into a poll state gated on all six
`IsLoaded()`), Q9 EnterVenue revert (BandDirector.cpp:615-618 — caution: the
sync load exists to make `TheBandWardrobe` non-null before EnterVenue's
wardrobe path, per the comment at :604-607; the async version must preserve
that ordering), Q8 XMA sidecar prefetch (rb3_xma_sidecar.h). Verification:
native song-end-test + menu-music smoke; frame trace shows the 6-file chain
gone. Opt-outs: `RB3_METAMUSIC_SYNC`, `RB3_VENUE_SYNC`, `RB3_XMA_PREFETCH_OFF`.

**T5 (Wave 1) — Web JS hygiene.** Goal: Q5 arraybuffer sync-fallback
(engine WebAssets.cpp) + Q6 IDB pre-warm cap (`native/web/rb3_pre.js`).
Verification: boot waterfall unchanged-or-better; hover ten moggs, next-boot
JS heap < 100 MB. Opt-out: n/a (strict improvements; keep old XHR path behind
`RB3_SYNC_XHR_LEGACY` for one release).

**T6 (Wave 2) — A1 pending-File async open.** Goal: the seam fix. Files:
`native/src/native_file.cpp`, `native/web/rb3_pre.js` (manifest map), engine
`WebAssets.cpp/h` (in-flight dedupe, `WebAssetsEnsureResidentAsync` —
`__EMSCRIPTEN__`-gated, never HX_WEB), small web arms in
`src/system/utl/Loader.cpp` FileLoader::OpenFile if needed (HX_WEB is fine in
rb3 sources; match-verify), **and `src/system/utl/ChunkStream.cpp`** — its
ctor calls NewFile, derives `mFail` from `mFile->Fail()` immediately and
issues `ReadAsync(&mChunkInfo,0x810)` (ChunkStream.cpp:69-91), and
`DirLoader::OpenFile` checks `mStream->Fail()` on the spot, so `Fail()` must
answer from the manifest oracle while the fetch is pending. Also account for
the eager `AllocBuffer()` full-size reservation at Loader.cpp:670-673 (a
pending 36 MB mogg holds its whole buffer during the fetch). Verification:
E2+E3 — canvas stays live through title→hub at 20 Mbps; boot App-ctor ≤ 3.9 s
localhost (no regression). Opt-out: `RB3_ASYNC_OPEN_OFF`.

**T7 (Wave 2) — Q3 Range-backed mogg File.** Files:
`native/src/native_file.cpp` (WebRangeFile), server.py untouched (Range
exists). Verification: cold preview fetches < 5 MB (server netlog); audio
correctness via `/audio-verify` skill. Opt-out: `RB3_MOGG_RANGE_OFF`.

**T8 (Wave 2) — Q4 BC texture upload.** Files: engine `Rnd_Wgpu_RB3.cpp`
(+ TextureConvert reuse), device-feature request (any web-only branch gated
`__EMSCRIPTEN__`, not HX_WEB). Verification: E4 visual gate + `gTexUploadMs`
drop. Opt-out: `RB3_BC_TEX_OFF`.

**T9 (Wave 3) — A2 per-screen bundles + Q10 prewarm flip.** Files:
`native/web/server.py`, manifest generation (extend
`scripts/web/gen-boot-manifest.mjs`), trigger shim in `native/src/main_web.cpp`
or UIScreen prewarm hook; UIScreen.cpp env default flip. Verification: E3
title→hub time with cold IDB; prewarm A/B on web. Opt-outs:
`RB3_SCREEN_BUNDLES_OFF`, `RB3_PREWARM_SCREENS=0`.

**T10 (Wave 3, conditional on Q1 data) — Q11 sliced audio prime / A3
completion slicing.** Files: `src/system/synth/StandardStream.cpp` (HX_NATIVE
prime state) + the Play() call sites, engine upload-budget seams. The prime
pump is INSIDE `Play()` with `MILO_ASSERT(IsReady() || kSuspended)` between
its two loops (StandardStream.cpp:478-499) — the slice must gate UPSTREAM of
the Play() call (don't enter Play() until primed), not convert Play()'s loop
in place. Verification: frame trace `prime`/`tex` fields < 8 ms/frame during
song start. Opt-out: `RB3_PRIME_SLICE_OFF`.

### Shared-file conflict map (schedule non-overlapping)

| File | Tasks | Rule |
|---|---|---|
| `native/src/native_file.cpp` | T2, T6, T7 | T2 alone in Wave 0; T6+T7 same agent or sequential in Wave 2 |
| `native/web/rb3_pre.js` | T5, T6 | T5 (Wave 1) lands before T6 (Wave 2) |
| engine `WebAssets.cpp` | T1, T5, T6 | T1 Wave 0; T5 Wave 1; T6 Wave 2 — strictly staged |
| engine `Rnd_Wgpu_RB3.cpp` | T1, T8 | T1 first; T8 Wave 2 |
| `src/system/synth/StandardStream.cpp` | T1, T10 | T1 first |
| `src/system/utl/ChunkStream.cpp` | T1, T6 | T1 Wave 0 (counters); T6 Wave 2 (pending-file arm) |
| `native/web/server.py` | 05 harness (reads only), T9 | T9 only writer |
| `src/App.cpp`, `docs/native/audio-perf-loop/STATE.md` | NONE | dirty with another agent's work — hands off |

### Success criteria for the whole effort (re-measure with E2/E3 harnesses)

- Cold preview hover: frozen ≤ 100 ms at 20 Mbps (today: ~0.45 s localhost,
  ~13–15 s estimated real network).
- Title→main_hub: canvas never frozen > 100 ms contiguous at 20 Mbps; total
  ≤ 3 s localhost.
- Boot App-ctor: no regression (≤ 3.9 s localhost release).
- Steady-state main_hub/song_select: no frame > 33 ms attributable to
  load/upload counters.

---

## Implementation results (2026-06-10)

Waves 0–2 landed and integrated. Single engine pin bump
`MILO_ENGINE_PIN → fb23b5e` (rb3 `d71aadc3`). Full release+debug web build
deployed to `native/web/build/`. Gates measured against the deployed release
build on localhost (port 8434, instrumented netlog server) and under CDP
`Network.emulateNetworkConditions` 20 Mbps / 40 ms RTT (E3). All artifacts in
`/tmp/rb3perf-integ/` (waterfall, preview-hover, throttled before/after,
flagcheck, screenshots).

### Per-task landed table

| Task | Lever | Status | Default | Opt-out flag | Commit(s) |
|---|---|---|---|---|---|
| T1 | Frame-trace attribution counters | LANDED | on (under `gFrameTraceActive`) | n/a | engine `e60b4ce`; rb3 `9984e794` |
| T2 | E1 fake-async de-risk probe | LANDED | off | `RB3_FAKE_ASYNC_OPEN_MS` (opt-in) | rb3 `43c7a713` |
| T3 | Preview-hover prefetch + deferred stream construction | LANDED | on | `RB3_PREVIEW_PREFETCH_OFF` | rb3 `91235980` |
| T4 | MetaMusic deferred FX wiring (Q7); XMA per-bank prefetch (Q8); venue-sync flag (Q9) | LANDED | Q7/Q8 on; **Q9 kept SYNC (no-op lever)** | `RB3_METAMUSIC_SYNC`, `RB3_XMA_PREFETCH_OFF`, `RB3_VENUE_SYNC` (=0 opts into experimental async) | rb3 `3eaccc25` |
| T5 | Q5 JSPI async-fetch sync fallback; Q6 IDB pre-warm cap | LANDED | on | `RB3_SYNC_XHR_LEGACY` (one-release legacy XHR) | engine `84a6792`; rb3 `ab42d242` |
| T6 | A1 pending-File async open (manifest oracle + ensure-resident) | LANDED | on | `RB3_ASYNC_OPEN_OFF` | engine `fa89954` (+UAF fix `fb23b5e`); rb3 `79cb7a54` |
| T7 | Q3 Range-backed mogg File (HTTP 206) | LANDED | on | `RB3_MOGG_RANGE_OFF` | engine `fa89954`; rb3 `79cb7a54` |
| T8 | Q4 BC-native DXT texture upload | LANDED | on | `RB3_BC_TEX_OFF` | engine `6c45e96` |
| — | CMakeLists un-wire of untracked `rb3_replay_api.cpp` (T3 contamination) | LANDED | n/a | n/a | rb3 `dd584cdc` |
| — | Single `MILO_ENGINE_PIN` bump to `fb23b5e` | LANDED | n/a | n/a | rb3 `d71aadc3` |

Deferred to Wave 3+ (not in this integration): A2 per-screen bundles + Q10
prewarm flip (T9), Q11 sliced audio prime / A3 completion slicing (T10,
conditional on Q1 counter data), A4/A5/A6.

### Gate results

| Gate | Criterion | Result | Verdict |
|---|---|---|---|
| Wii build | byte-identical, no regression | `tools/ninja-locked`: 31951 funcs / 62.88% (== wave baseline) | PASS |
| Native build | engine+rb3-native+rb3-tests link | clean (built from clean checkout of `d71aadc3` in CoW worktree — main tree is link-blocked only by a *concurrent* agent's uncommitted replay edits, see Known issues) | PASS |
| Native tests | rb3-tests gtest | 13/13 | PASS |
| Native smoke | boot→menu→part_difficulty→gameplay→song-end | `game_screen` reached, song plays, game-over reached | PASS |
| 5a smoke | `smoke-test.mjs` reaches main_hub, song DB populated, no pageerror | main_hub, 83 songs, no pageerror | PASS |
| 5a keyboard→gameplay | reaches `game_screen` by pure keyboard, song playing | `game_screen` (guitar/hard), frame advancing | PASS |
| 5b boot waterfall (localhost, 3-run median) | appBooted ≤ 3.9 s | **3.62 s** (vs 05 baseline 3.82 s) | PASS |
| 5b boot frozen (localhost) | — | boot frozen Σ 806 ms (vs 05 baseline 1.24 s); intro 3.74 s, splash 5.02 s; splash→hub longest single gap **69 ms < 100 ms** | PASS |
| 5c preview hover (localhost, cold×3) | cold freeze ≈ warm, no >100 ms gap | cold longest 20–50 ms, frozen 0–24 ms, **over100 = 0**; mogg = **14 Range (206) reqs, 1 MB chunks**, zero whole-file | PASS |
| 5d throttled boot→hub (20 Mbps) | canvas never frozen >100 ms contiguous | splash→hub **longest 62 ms, over100 = 0** | PASS |
| 5d throttled cold hover (20 Mbps) | cold hover frozen ≤ 100 ms | `beencaughtstealing` cold: **longest 20 ms, frozen 0 ms, over100 = 0**; 4 MB Range (1–12 ms/chunk) | PASS |
| 5e flag A/B | page boots + hover works on each legacy path | `RB3_ASYNC_OPEN_OFF` / `RB3_MOGG_RANGE_OFF` / `RB3_BC_TEX_OFF` / `RB3_PREVIEW_PREFETCH_OFF` — all 4 PASS (main_hub + song_select + hover, no pageerror) | PASS |
| 5f T8 BC visual | no gross texture corruption on web device | main_hub + song_select render clean (BC-ON ≈ BC-OFF; menu chrome identical, no decode artifacts) — Dawn advertises TextureCompressionBC | PASS |

### Before/after headline (E3, 20 Mbps / 40 ms RTT)

The decisive change is the **cold-preview transfer model**, measured directly:

| | Cold preview mogg | Per-request transfer | Canvas |
|---|---|---|---|
| **BEFORE** (`RB3_ASYNC_OPEN_OFF=1` + `RB3_MOGG_RANGE_OFF=1`) | **whole-file 200**, 5.0–8.75 MB each | **7.5–12.0 s** per mogg | sync-XHR open blocks the frame for the full transfer at the File seam |
| **AFTER** (default) | **Range 206**, ~4 MB (4×1 MB chunks) per hover | **1–12 ms** per chunk, async | rAF longest 20 ms, frozen 0 ms, over100 = 0 |

≈ **2–9× fewer bytes per cold hover** and the multi-second blocking transfer is
eliminated; the canvas stays live (no >100 ms freeze) at 20 Mbps. Title→main_hub
at 20 Mbps: longest single rAF gap 62 ms (< 100 ms), titleToHub wall ≈ 3.7 s.

Honest caveat on the BEFORE rAF number: in the BEFORE run the cold-hover rAF
freeze also read ~0 ms *within the measured hover window*, because T3's prefetch
still warms the highlighted song and the harness's measured song was resident by
hover time. The unambiguous before/after delta is therefore in the **bytes +
transfer model** (whole-file 5–8.75 MB @ 10 s vs Range 4 MB @ ms) and the
WebGPU netlog (200 whole-file vs 206 Range), not the single rAF figure — the
mechanism that keeps the canvas live regardless is A1's async File seam (T6),
which is what `RB3_ASYNC_OPEN_OFF=1` disables.

### Known issues / nonblocking findings (carried from wave reviews)

- **Working-tree build contamination (not ours, MUST resolve before any
  `native/build-native` build of the main tree):** a concurrent milo-trace
  agent has uncommitted edits to `native/src/rb3_http_server.cpp/.h` +
  `native/CMakeLists.txt` (wires untracked `rb3_replay_capture.cpp`) and two
  untracked files `rb3_replay_api.cpp` / `rb3_replay_capture.cpp`. The
  `http_server` edits call `RB3ReplayApiEnabled()/Handle()` whose definitions
  live in the *un-wired* `rb3_replay_api.cpp`, so the **main working tree
  link-fails** (`undefined reference to RB3ReplayApiEnabled()`). A clean
  checkout of `master` (the committed state) builds + runs fine — verified in a
  CoW worktree. The milo-trace agent must commit their `replay_api.cpp{,.h}` +
  re-add its CMakeLists ref + their `http_server` edits as one self-contained
  feature.
- **T8 BC (Q4):** no guard against non-multiple-of-4 DXT dimensions; empirically
  never fires (all 360-extracted DXT textures are 4-aligned; zero WebGPU
  validation errors on native + web), but a cheap `if ((w%4)||(h%4)) → CPU path`
  guard is recommended for future odd-size assets. DXN/BC5 deliberately stays on
  the RGBA8 CPU path.
- **T4 Q9 (EnterVenue) is a no-op lever as shipped** — kept default-sync because
  async is unsafe until `BandDirector::Enter()`'s tail is split into a
  multi-frame poll (`TheBandWardrobe`-before-Enter ordering). Delivers zero perf
  win by default; the prerequisite Enter()-split is the future work.
- **T4 Q7 (MetaMusic) is web-only-observable** — `MetaPanel::Load()` skips the
  MetaMusic FILE load on the native 360-ARK extract (no `metamusic_loop` in the
  extract's `synth.dta`), so the "6-file sync chain gone" is confirmable only on
  the full web asset set (`RB3_METAMUSIC_DBG` log hook in `PollFxWiring`).
- **T6 in-flight resource vectors** (`sFetchRequests`, `sRangeRequests`,
  `sEnsureInFlight`) grow without compaction over a long session; bounded by
  Drop for Range but the FetchRequest leak is pre-existing — minor latent growth.
- **T6 Range header buffer** is a single `static thread_local` shared across
  requests; safe today (emscripten_fetch copies headers synchronously) but
  fragile — prefer a per-request buffer.
- **T3 residency probe is MEMFS-only** (`/data/<rel>`); an IDB-warm-but-not-MEMFS
  mogg's first hover fetches from network instead of IDB. Not a regression
  (today's first touch also networks). T6 may want to route prefetch residency
  through the IDB cache (research 03§1 option (b)).
- **A1 eager `AllocBuffer`:** a *pending non-mogg* FileLoader reserves the full
  manifest size up front for the whole fetch (moggs take the Range path so they
  are exempt). Transient, released on loader completion — not a leak, but worth
  watching peak heap under E3.
- **`?env` URL→ENV bridge fix is load-bearing:** T1 seeded `Module.ENV` but
  Emscripten's `getEnvStrings` reads its own internal `ENV`; T6 fixed it in
  `main_web.cpp` (drains `window.__rb3ExtraEnv` via `::setenv` at BOOT_INIT).
  Every `RB3_*` flag set via `?env=` depends on this — verified working for all
  four Wave-2 flags in gate 5e.
- **Range-fetch cancel-mid-fetch (UAF abandon path) is not yet harness-covered.**
  Engine `fb23b5e` fixes the UAF (abandon in-flight on Drop), and it compiles
  under real emcc; a browser cancel-mid-fetch case (hover A → switch to B before
  A's first chunk lands) should be added to `t6t7-async-open-verify.mjs` to
  exercise it live.

### What remains (Wave 3+)

- **T9 — A2 per-screen dependency bundles** (`main_hub`/`song_select` manifests
  fired async at transition-start) + **Q10 `RB3_PREWARM_SCREENS` default-ON for
  web** after a web A/B (infra + UAF fix `585ad0f8` already landed).
- **T10 (conditional on Q1 counter data) — Q11 sliced audio prime / A3
  completion slicing.** Gate on the frame-trace `prime`/`tex` fields; only
  pursue where counters still show >16 ms frames during song start.
- **A4/A5/A6** — pre-converted assets at extraction, pipeline/scene pre-warm for
  the shared ~170 ms hub spike, cached unpacked verts — separate workstreams,
  only where the T1 counters still attribute >16 ms frames.
- **`gDtaParseMsThisFrame` wire-in** (declared + reset-wired by T1 but stays 0):
  RB3 parses DTA inline via `DataReadFile` in `src/system/obj/DataFile.cpp`; a
  2-line timing wrap there finishes the counter (outside T1's file scope).

## Wave 3 results (2026-06-10)

Wave 3 landed **T9** (A2 per-screen dependency bundles + Q10 prewarm
default-flip for web) and **A5** (pipeline pre-warm). Single engine pin bump
`MILO_ENGINE_PIN fb23b5e → a0848b1` (rb3 `ead85adb`) pulls in the A5 engine
commit. Full release+debug web build redeployed to `native/web/build/`. T10
(Q11 sliced audio prime) was **NOT** taken — M1's measurement verdict was
`t10_go=false` (see "what remains"). Gates measured against the deployed
**release** build on a dedicated localhost server, and under CDP
`Network.emulateNetworkConditions` 20 Mbps / 40 ms RTT (E3); native A5 A/B via
`RB3_FRAME_TRACE` headless. Artifacts in `/tmp/rb3perf-w3-integ/`.

> **Measurement-harness blocker found+fixed in this wave (rb3 `5dea9a6e`):** the
> committed T9 verify harnesses navigated to `/?env=...` with no `?debug=true`,
> so they loaded the *feature-less release* build (release didn't contain T9 yet
> at review time) — every arm reported `bundles=NONE`, a non-discriminating A/B.
> Fixed to `/?debug=true&env=...` + a hard end-of-run gate (`exit 2` when an arm
> that should fire a bundle fired none). For the **integration release gate**
> below the bundle feature *is* now in the shipped release wasm
> (`strings release/rb3-web.wasm | grep -c api/bundle/screen` == 1), so the gate
> drives the release build directly (no `?debug`) and the A/B discriminates.

### Per-task landed table

| Task | Lever | Status | Default | Opt-out flag | Commit(s) |
|---|---|---|---|---|---|
| T9 (A2) | Per-screen dependency bundles — `main_hub`/`song_select` manifests fired async at transition-start (`/api/bundle/screen/<name>`, MEMFS-primed) | LANDED | **ON for web** (`RB3_SCREEN_BUNDLES_OFF` default OFF) | `RB3_SCREEN_BUNDLES_OFF` (truthy disables; `=0` forces ON); `RB3_SCREEN_BUNDLE_NEXT="from:to,…"` tunes the current→next map | rb3 `bc651674` |
| T9 (Q10) | `RB3_PREWARM_SCREENS` default-flip — next-screen loader pre-warm | LANDED | **ON for web** (native UNCHANGED: opt-in) | `RB3_PREWARM_SCREENS` (`=0`/`false`/`no`/`off`/`OFF` disable on web) | rb3 `e6fae130` (+ flag-spelling fix `6fa304fc`) |
| A5 | Pipeline pre-warm — `PipelineManager::PreWarm(mainFmt,rtFmt)` sweeps the draw-time key space at the first post-GPU-ready `BeginFrame`, moving the splash→hub pipeline-compile spike off the venue-build frame into the boot dwell | LANDED | ON | `RB3_PIPELINE_PREWARM_OFF` (`=1` restores on-transition compile); `RB3_PREWARM_DBG` prints created-count+ms | engine `a0848b1`; pin bump rb3 `ead85adb` |
| — | T9 verify harnesses must load the DEBUG build (`?debug=true`) + discriminating gate | LANDED | n/a | n/a | rb3 `5dea9a6e` |
| — | `RB3_PREWARM_SCREENS` `off`/`OFF` now disables as documented (was checking only `0/f/F/n/N`) | LANDED | n/a | n/a | rb3 `6fa304fc` |
| — | Single `MILO_ENGINE_PIN` bump `fb23b5e → a0848b1` (A5) | LANDED | n/a | n/a | rb3 `ead85adb` |

### Gate results

| Gate | Criterion | Result | Verdict |
|---|---|---|---|
| Wii build | byte-identical, no regression | `tools/ninja-locked`: **31951 funcs / 62.884%** (== wave-2 baseline); UIScreen.o rebuilds SHA `7bd85a37` (no-op) | PASS |
| Native build | engine(`a0848b1`)+rb3-native+rb3-tests link clean | `PipelineManager::PreWarm` symbol present; "no work to do" (already built against engine HEAD) | PASS |
| Native tests | `rb3-tests` gtest | **13/13** | PASS |
| Native smoke | boot→game_screen→song-end | `game_screen` reached, song plays, `{game is_game_over}==1` after jump | PASS |
| 5a smoke (release) | `smoke-test.mjs` main_hub + song DB + no pageerror | main_hub, **83 songs**, no pageerror | PASS |
| 5a keyboard→gameplay (release) | reaches `game_screen` by pure keyboard | `game_screen`, guitar/hard, frame advancing | PASS |
| 5b E3 boot→hub (20 Mbps) | longest rAF gap, over100 (wave-2: 62 ms, over100=0) | **longest 68 ms, over100=0** | PASS |
| 5b E3 cold preview hover (20 Mbps) | cold freeze ≤ 100 ms, Range model intact (wave-2: longest 20 ms, frozen 0 ms, ~4 MB) | **longest 37 ms, over100=0, frozen 20 ms; mogg = 4 Range / 4.00 MB** | PASS |
| 5c T9 bundles A/B (release, cold IDB) | bundles ON fires both manifests + collapses per-file reqs; control fires none | **ON**: main_hub(8.36 MB)+song_select(8.62 MB), splash→hub fileReqs 12→4, hub→select 20→1; **control**: NONE, 12/20; both over100=0; GATE PASSED | PASS |
| 5c Q10 prewarm A/B (release) | shipping default + opt-out both transition clean | **default** (bundles+prewarm ON): hub→select **fileReqs=0**; **prewarmoff** (all OFF): fileReqs=20, both reach song_select | PASS |
| 5e A5 splash→hub worst frame (native A/B) | pipeline spike removed from venue-build frame | **OFF**: f=13 dt=165.3 ms, pipeMs=86.6, pipeN=13 (run-total pipeN=24/167.2 ms). **ON**: f=13 **dt=82.0 ms (−50%), pipeMs=0, pipeN=0** (run-total pipeN=0); splash→hub frozen Σ 274→178 ms | PASS |
| 5e A5 visual no-op (native) | A5 doesn't change pixels | main_hub OFF-vs-ON diff (MAD 27.0 / 81.8 %) **below** the OFF-vs-OFF animation-phase noise floor (MAD 28.0 / 83.7 %); structurally a same-key cache pre-fill (run-total pipeN=0 ON ⇒ every draw is a cache hit) | PASS |
| 5f flag A/B (release, ?env bridge) | each new flag's opt-out boots + transitions clean | `RB3_PIPELINE_PREWARM_OFF=1`, `RB3_SCREEN_BUNDLES_OFF=1`, `RB3_PREWARM_SCREENS=0/off/OFF`, `RB3_SCREEN_BUNDLE_NEXT=…` — all reach song_select, 0 pageerrors | PASS |

### Before/after headline

- **A5 (native, the decisive win):** the splash→main_hub venue-build frame's
  synchronous pipeline-compile spike (≈14 pipelines, **86.6 ms** of compile on a
  single **165 ms** frame) is **fully removed** — that frame drops to **82 ms**
  (−50 %) with **pipeMs=0 / pipeN=0**, every pipeline-create absorbed by the
  boot-dwell pre-warm. Run-total draw-time pipeline-create cost **167 ms → 0 ms**;
  zero recorded draw-time `GetPipeline` cache misses ⇒ the swept key set is a
  verified superset. On web the same compile is async (≈4 ms dispatch) so the win
  there is the removed residue, not a recorded frame.
- **T9 (web, cold IDB, the request-collapse win):** firing the screen's whole
  dependency bundle async at transition-start replaces the per-file on-demand
  read storm: **splash→hub 12 → 4** file requests, **hub→select 20 → 1** (and
  **→ 0** with Q10 prewarm also on). At 20 Mbps this also recovers wall time on
  the cold hub→select transition (default ≈16 s vs all-off ≈24 s). Canvas stays
  live throughout (over100 = 0 on every arm).

### What remains (Wave 4+)

- **T10 — Q11 sliced audio prime / A3 completion slicing: NOT TAKEN
  (`t10_go=false`).** M1's fresh per-frame counter trace showed no residual
  >16 ms song-start frames attributable to audio-prime work once waves 0–2 +
  A5 are in — the `primeMs`/`texMs` fields did not surface a spike worth a
  slicing lever. Revisit only if the T1 counters show >16 ms `prime`/`tex`
  frames during song start on the full asset set.
- **A4 — pre-converted assets at extraction time** (offline DXT→BC / vert-unpack
  bake) and **A6 — cached unpacked verts**: separate offline-tooling
  workstreams, pursue only where the T1 counters still attribute >16 ms frames
  after A5. A5 (`a5_go=true`, landed) already removed the dominant shared
  splash→hub spike that A4/A6 were also aimed at, so their priority drops.
- **`gDtaParseMsThisFrame` wire-in** (still 0 — see Wave-2 note; 2-line timing
  wrap in `DataFile.cpp::DataReadFile`).
- **Non-blocking carry-overs from the wave-3 reviews:** (a) A5 native pre-warm is
  a real ~0.64 s synchronous main-thread burst (`created 240 pipelines in
  ~642 ms`) during the idle boot dwell — produces no *recorded* frame spike
  (lands before the tracer's f=0) but is a one-time boot-dwell hitch; a future
  cleanup could chunk it across frames. (b) `PipelineKeyHash` omits
  `k.stencil`/`k.alphaWrite` (pre-existing; `operator==` still disambiguates, so
  correct but a slightly degraded bucket) — out of A5 scope. (c) A5's swept
  blend/zMode ranges are hardcoded magic literals against the `WgpuBlend`/
  `WgpuZMode` enums; a shared `kMaxBlend`/`kMaxZMode` or `static_assert` would
  couple them so a future new key can't silently miss the warm. (d) the
  T9/A2 server allowlist + traversal guards (`..%2f` → 400, unknown screen → 200
  empty 4-byte count=0) were independently re-verified contamination-free.

## Wave 4 results (2026-06-10) — network fixes

Wave 4 implements the ranked fixes from the **network-restricted benchmark
matrix** (`research/07-network-matrix.md`): the matrix proved that the
shipped 20 Mbps/40 ms gate (c1) **cannot see** the user's real symptom —
~85 MB of asset fetches run **100 % serially** (0/38 big-milo overlaps), so
screen transitions wall-clock-hang for tens of seconds at 4–8 Mbps and the
journey DNFs at 1.5 Mbps. The canvas never freezes (the wave-1 yield gate holds
at every condition); the problem is **fetch pipelining**, not yield. No engine
pin bump (engine HEAD == pin `a0848b1`); all four fixes are rb3-side. Gates run
against the freshly-redeployed **release** build, at the **new harsh
conditions** the matrix identified (8 Mbps/80 ms = cheapest condition that
exposes the wall; 4 Mbps/150 ms = the user-symptom regime). Harness:
`scripts/web/_netmatrix.mjs`; artifacts in `/tmp/rb3perf-w4-integ/`; baselines
in `/tmp/rb3perf-netmatrix/c{1,3,4}-run1/`.

### Per-task landed table

| Task | Lever | Status | Default | Opt-out flag | Commit(s) |
|---|---|---|---|---|---|
| N1 (matrix #1) | **Loader-queue fetch read-ahead** — kick `WebAssetsEnsureResidentAsync` for the next N (default 6) queued loaders' files so file k+1…k+N download while file k downloads+parses, turning the 85 MB serial chain into a pipelined one (engine fetch machinery already async + deduped via `sEnsureInFlight`) | LANDED | **ON for web** (k=6) | `RB3_LOADER_READAHEAD=0` (disable; any N sets depth); `RB3_READAHEAD_DEBUG` (queue-depth/kick diagnostic) | rb3 `f07a365c` |
| N2 (matrix #2) | **Mogg Range read-ahead slot + cross-open chunk cache** — `WebRangeFile` gains a 2nd in-flight slot to prefetch chunk N+1 (VorbisReader reads strictly forward), and a process-wide `MoggChunkCache` so gameplay's fresh stream reuses chunks the preview already fetched (kills the chunk 0–1 re-download) | LANDED | ON | `RB3_MOGG_READAHEAD_OFF=1` (single-slot); `RB3_MOGG_CACHE_MB=0` (no cross-open cache; default 24); `RB3_MOGG_CACHE_DBG` | rb3 `654ac73e` |
| N3 (matrix #3) | **Yield in `StandardStream::Resync` ReadDone wait** — the zero-yield `while(!ReadDone)` spin would deadlock the web tab if a seek/jump fired over a stalled WebRangeFile (the fetch callback needs an event-loop turn); now routes through the same `emscripten_sleep(0)` JSPI yield PollUntilLoaded/Play use, with a 2M-iter watchdog cap | LANDED | ON | `RB3_RESYNC_YIELD_OFF=1` (bare spin); `RB3_LOADER_MIN_YIELD_MS` tunes yield cadence | rb3 `06777103` |
| N4 (matrix #4) | **Land `rb3_frame_trace.cpp` into the web build + parent-dir mkdir guard** — the per-frame fetch/dta/obj/prime/tex/mesh/pipe attribution counters were unreadable on web (recorder lived only in `App::RunWithoutDebugging`, which the web loop bypasses); now wired so `?env=RB3_FRAME_TRACE=/trace.jsonl` captures | LANDED | opt-in (`RB3_FRAME_TRACE=<path>`) | n/a (diagnostic) | rb3 `acb306c1` |
| — | No `MILO_ENGINE_PIN` bump (engine HEAD == pin `a0848b1`) | n/a | n/a | n/a | — |

### Gate results (NEW harsh-condition gates)

| Gate | Criterion | Result | Verdict |
|---|---|---|---|
| Wii build | byte-identical, no matched-TU leak | `tools/ninja-locked`: **31951 funcs / 62.88397 %** (== baseline exactly; broad band3 recompile was benign dep-timestamp churn, numbers unchanged) | PASS |
| Native build | engine(`a0848b1`)+rb3-native+rb3-tests link clean | already built against engine HEAD ("no work to do") | PASS |
| Native tests | `rb3-tests` gtest | **13/13** | PASS |
| Native smoke (N3 seek path) | boot→`game_screen`→`{game jump 600000}`→game-over | `{game is_game_over}==1` after jump; **no hang/crash on the Resync seek** | PASS |
| Audio (N2+N3 data path) | `audio_verify --selftest`, then fresh gameplay capture `--rank` | selftest **6/6**; capture (song `beforeiforget`) verdict **MATCH** — chroma 0.81, pitch 1.000×, clip 0.00 %, no distortion (rank winner self-identified, MATCH) | PASS |
| **8 Mbps/80 ms (c3) — N1 overlap** | netlog shows overlapping `/api/file` big-milo fetches (baseline peakConcurrent=2, serial) | **peakConcurrent 2→6, overlappingMilos 2→15** — serial chain now pipelined (read-ahead depth 6 visibly kicking); maxSingleMilo 19.5→17.4 s; journey completes, canvas frozenΣ=0 | PASS |
| **8 Mbps/80 ms (c3) — N2 reuse** | cold-hover preview→gameplay chunk reuse, no chunk re-download | **chunkReDownloads 1→0** (combatbaby chunk-0 cross-open re-download eliminated) | PASS |
| **4 Mbps/150 ms (c4) — completes** | journey reaches `game_screen`; chunk reuse visible | reached `game_screen`; **chunkReDownloads 1→0**; peakConcurrent 2→6, overlappingMilos 2→16; maxSingleMilo **45.2→35.5 s** (~22 % smaller); hovers frozenΣ=0 | PASS (partial win — see note) |
| **20 Mbps/40 ms (c1) — regression** | boot→hub longest rAF gap ≤100 ms, cold hover frozen ~0 (wave-3: 68 / 37 ms) | appBooted 19.62 s (== 19.84 baseline); transitions longest ≤89 ms, over100=0; **hovers longest 20–25 ms, frozenΣ=0**; boot 454 ms over250=1 (RTT-invariant GPU pipeline-compile floor, not network) | PASS (no regression) |
| N4 frame-trace on web | `?env=RB3_FRAME_TRACE=1` → non-empty trace with counters | **5363/7895/4001 records** across the three runs, 23 well-formed counter fields each (was `ERR:No such file` in the matrix build) | PASS |
| Flag A/B opt-out | each new flag's off value boots→`song_select` clean | `RB3_LOADER_READAHEAD=0`, `RB3_MOGG_READAHEAD_OFF=1`, `RB3_MOGG_CACHE_MB=0`, `RB3_RESYNC_YIELD_OFF=1` — **all reach song_select, 83 songs, 0 pageerror/trap** | PASS |
| UAF stress | 10× hover-switch-before-load at 4 Mbps/150 ms — no crash | **10 switches, 0 errors/0 traps, alive** (abandoned kicked fetches + Range-chunk drops + engine UAF fix are switch-safe) | PASS |

### Before/after vs the matrix baselines (8 Mbps/80 ms and 4 Mbps/150 ms)

| Metric | c3 baseline | c3 wave-4 | c4 baseline | c4 wave-4 |
|---|--:|--:|--:|--:|
| big-milo peakConcurrent | 2 (serial) | **6** | 2 (serial) | **6** |
| big-milo overlappingMilos | 2 | **15** | 2 | **16** |
| maxSingleMilo (s) | 19.5 | 17.4 | 45.2 | **35.5** |
| mogg chunk re-downloads | 1 | **0** | 1 | **0** |
| reached gameplay | yes | yes | yes | yes |
| canvas frozenΣ (hovers/transitions) | 0 | 0 | 0 | 0 |

> **Honest result note.** N1 demonstrably pipelines the previously-serial fetch
> chain (concurrency 2→6, overlaps 2→15/16 — the matrix's #1 fix is doing exactly
> what it was designed to). But the **40–50 % wall-clock cut the matrix
> *projected* does NOT fully materialise** at these bandwidth-saturated
> conditions: with 6 concurrent fetches sharing one throttled pipe, each
> individual fetch is slower (bandwidth split N ways), so aggregate transfer
> stays bounded by `total-bytes / bandwidth` regardless of concurrency, and
> `maxSingleMilo` shrinks only ~22 % rather than collapsing. The projection
> assumed parse-time-dominated overlap; at pure bandwidth saturation the wall is
> throughput-bound. This corroborates the N1 reviewer's caveat ("did not
> reproduce the 40–50 % cut; queue depth shallow"). The wins that *are* robust:
> (a) the canvas-never-freezes property holds at every condition, (b) the journey
> completes at 4 Mbps (DNF remains only at 1.5 Mbps, same as baseline — that
> condition is throughput-unplayable regardless of any client fix short of
> smaller assets), (c) mogg cross-open re-downloads are eliminated, and (d) N3
> removes a latent web tab-hang on seek. `miloSerialΣ` (sum of durations) is **no
> longer a valid wall-clock proxy once fetches overlap** — it inflates with
> concurrency (c3 96.7→167.8 s, c4 185.7→337.1 s) precisely *because* fetches now
> run in parallel; peakConcurrent / overlappingMilos are the correct N1 signals.

### Future perf-gate policy

Per matrix ranked fix #5: **future incremental-load perf work gates at ≥2
network points** — **8 Mbps/80 ms** (cheapest condition that exposes the
serial-fetch wall; the 20 Mbps/40 ms gate provably cannot see it) and
**4 Mbps/150 ms** (the user-symptom regime). 20 Mbps/40 ms is retained only as a
regression backstop. Use `scripts/web/_netmatrix.mjs` (cold-IDB per arm) +
`/tmp/rb3perf-w4-integ/analyze_net.py` for the overlap/chunk-reuse signals;
`peakConcurrent` and `chunkReDownloads`, not `miloSerialΣ`, are the trustworthy
columns once any pipelining is active.

---

## Wave 5 results

Wave 4 established the pipe is **bandwidth-saturated** at 4–8 Mbps (wall ≈
totalBytes / bandwidth), so Wave 5 attacked the two remaining levers it named:
**(1) BYTES on the wire** (research/08) and **(2) the game_screen first-frame
reveal hitch** (research/09). Integration bumped `MILO_ENGINE_PIN`
f75339a→**8fb669d** (one commit, `38c5ca7e`).

### Landed (per task)

| Task | What | Repo / commit | Status | Flag (default ON) |
|---|---|---|---|---|
| W5-T1 | Server q11 offline pre-warm + close compression coverage holes (.pcm/.png_xbox/.mid/manifest) | rb3 `2b9d8e64` | landed | `--no-encode` off-switch; `--prewarm`; `prewarm_encode_cache.py` |
| W5-T2 | Vorbis SFX sidecars — replace ~59 MB raw PCM wire with vorbis `.ogg` | rb3 `e5728fc0` | landed | `RB3_SFX_OGG_OFF`; converter `--no-ogg` / `RB3_OGG_QUALITY` |
| W5-T3 | Wire-byte census tool (`_netbytes.py`) + integrated byte gates | rb3 `78e4d074`, `b104b383` | landed | — |
| T1 | Engine: L1 vertex-unpack cache + `WarmGpuForDir` API | engine `8fb669d`, rb3 trace `dbaa7b41` | landed | `RB3_UNPACK_CACHE_OFF` |
| T2 | rb3: vignette-dwell GPU-warm driver + reveal-drain pre-kick | rb3 `2850b9b1`, `9cde0dba` | landed-partial (L2 sweep default-OFF) | `RB3_GAMEWARM_OFF` (+ `_HOLD`/`_DBG`/tuning) |
| T3 | Web `lp`/`lpu` timing wiring + `firstframe-gate.mjs` | rb3 `c816bb6d` | landed | — |
| pin | Engine pin bump | rb3 `38c5ca7e` | landed | — |

### Gates (cold per arm, release build, own server :8446 after full q11 prewarm)

| Gate | Condition | Result | Verdict |
|---|---|---|---|
| Wii byte-identical | `report.json` | 31951 / 41254 funcs @ 62.88397% == baseline | **PASS** |
| Native | build + `rb3-tests` + song-end | tests 13/13; song-end reaches game_screen→game-over | **PASS** |
| Audio (W5-T2 SFX path) | `audio_verify --selftest`; gameplay `--rank` | selftest 6/6; rank WINNER `20thcenturyboy` chroma 0.961 (+0.104 margin) = MATCH | **PASS** |
| G-c4 (primary) | 4 Mbps / 150 ms | journey **COMPLETES** (game_screen @ 248 s); **wire 115.49 MB** vs ~181 MB W4 baseline (−36 %) | **PASS** |
| G-c3 | 8 Mbps / 80 ms | game_screen @ 151 s; 123.0 MB; no freeze regression (boot frozenΣ 323) | **PASS** |
| G-c1 (backstop) | 20 Mbps / 40 ms | game_screen @ 91 s; splash→hub longest 33 ms; **3 cold hovers frozenΣ=0** | **PASS** |
| G-c5 (retry) | 1.5 Mbps / 300 ms | **boot + song_select + 3 cold hovers (frozenΣ=0) + part_difficulty reached** (was DNF at W4); game_screen NOT reached within harness window (nav-cadence at 300 ms RTT, not a hang) | **partial — major improvement over W4 DNF** |
| G-firstframe (release) | unthrottled reveal | reveal dt **600.4 ms** (in baseline band [400,1200]); attributed objMs 67.7 + texMs 34.5 + meshMs 1.6 + **unpackMs 1.2** (one-time, T1 cache proven); residue ~495 ms | **at-baseline (120 ms target needs deferred L2/L3)** |
| Flag A/B | all 3 new opt-outs | `RB3_SFX_OGG_OFF` / `RB3_UNPACK_CACHE_OFF` / `RB3_GAMEWARM_OFF`: boot+song_select on web release via `?env=` bridge (env-echo confirmed); reach game_screen+game-over on native | **PASS** |
| Visual | gameplay @ songMs 4284 | highway/gems/now-bar/venue render clean (small_club, score 185) | **PASS** |

### Byte census (c4 4 Mbps/150 ms cold journey, `_netbytes.py`)

| Category | W5 wire | vs baseline | note |
|---|---|---|---|
| milo | 75.13 MB | 85.4 → q11-weighted (~0.89×) | brotli-q11; `Content-Encoding: br`, decodes to exact source size (verified 11.67 MB→19.43 MB on `small_club_01`) |
| SFX (ogg) | **8.50 MB** | ~59 MB raw PCM → ogg (W5-T2) | **the 10× lever**; well under the 25 MB gate (677/677 pairs, ch=1) |
| mogg | 14.69 MB | Range chunks (unchanged) | — |
| bundle | 14.99 MB | 17.9 raw → compressed | — |
| misc | 2.18 MB | png/mid/txt/manifest now compressed | — |
| **TOTAL** | **115.49 MB** | **~181 MB → −36 %** | matches P1 projection (~116.6 MB) |

maxSingleMilo = `small_club_01` at **11.67 MB** wire (was 12.88 MB q5; matches P1).

### Honest misses / what remains

- **First-frame reveal still ~600 ms** (release). T1 (L1 vertex-unpack cache) and
  T3 (lp/lpu attribution) landed and are *proven* (steady `unpackMs`≈1.2 ms, one-
  time; `lpu` now correctly attributed). But T2's **L2 warm-sweep is default-OFF**
  (documented harmful on native; kept behind `RB3_GAMEWARM_HOLD=1` for a future
  resident-dwell flow) and the **L3 in-frame-drain removal did not land**, so the
  ~495 ms first-draw/pipeline residue persists. The 120 ms `firstframe-gate`
  target is the *post-full-stack* aspiration; today's reveal sits at the
  documented baseline band, no regression. The byte-reduction wave (P1) is the
  landed Wave-5 win.
- **q11 artifacts are not durable** under on-demand q5 serve traffic (last-writer-
  wins at the cache path). Gates were run **after a full prewarm-to-completion**
  (4455/4455 milo_xbox.br at br11). Operationally: re-run `prewarm_encode_cache.py
  --level 11` after any q5-warming session before measuring. A sticky-q11 mitigation
  (on-demand q5 writer skips when a fresh higher-level artifact exists) is suggested
  but not yet implemented.
- **1.5 Mbps remains throughput-constrained**: dramatically better than W4 DNF
  (now playable through song_select + hovers + part_difficulty, all freeze-free),
  but full game_screen entry didn't complete within the harness at 300 ms RTT.
  Smaller assets (further byte reduction) are the only lever left for this regime.
- 1 "failed" request per journey is the intro-cinematic `.webm` Range being
  abandoned on nav-skip (by-design UAF-fix abandon), not a real failure.

Artifacts: `/tmp/rb3perf-w5-integ/` (c1/c3/c4/c5 `result.json`+`net.ndjson`,
firstframe-release `result.json`+`trace.jsonl`, gameplay screenshot, prewarm log).

## Wave 6 re-baseline (2026-06-23)

Re-measured on current master (web deploy 02:01, ≈`33434ce6`, engine pin
`20dba552`) after a 12-day gap. Full results + provenance:
`research/10-wave6-rebaseline.md`. Salvaged from an interrupted measurement run
(`/tmp/rb3perf-w6/`); STEP 4 (L1 steady-state) did not complete.

**Headline — game_screen first-frame is FIXED (the Wave-5 deferred L2).** Clean
web A/B, same build: warm ON (default) **95.7 / 97.5 ms** vs OFF
(`RB3_GAMEWARM_OFF=1` `RB3_TEX_PREWARM_OFF=1`) **419.5 / 406.5 ms** —
**~410 → ~96 ms (−77%)**, the sync drain (`lpu`) collapsing 368 → 53 ms,
`texN` 97 → 89. ON **meets the research/09 120 ms target.** Native ON ~98–125 ms
vs OFF ~125–140 ms (sync I/O → smaller gap). Credit the framestall venue-aware
prewarm (`86312dcf`), which mirrors `BandDirector::EnterVenue` resolution instead
of the never-loaded `MetaPerformer::GetVenue()` — the exact reason Wave 5's warm
was an Enter-state no-op.

**Bytes / journey gates HOLD.** c4 4 Mbps/150 ms: **114.9 MB** wire (W5 115.5),
game_screen @ 251 s (W5 248), milo 75.0 / mogg 14.8 / bundle 15.0 MB, cold hovers
frozen 0 / over100 0. c1 20 Mbps backstop: game_screen @ 88.6 s, splash→hub
63 ms / over100 0, freeze-free. The −36% bytes win is intact.

**Carried forward:** L1 steady-state gameplay win (web p50 18.99 → <17 ms) still
UNVERIFIED; `firstframe-gate.mjs` baseline band `[400,1200]` is stale and
mislabels the fixed ON arm FAIL — update it to assert the 120 ms target;
1.5 Mbps still throughput-bound (A4 downscale the only lever).
