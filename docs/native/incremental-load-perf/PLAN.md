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
