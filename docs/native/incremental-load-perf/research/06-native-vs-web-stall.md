# 06 — Native vs Web incremental-load stall decomposition (measured)

**Date:** 2026-06-10
**Build:** `native/build-native/rb3-native` (milo-engine pin `9f635b7c`, Clang 22),
release-equivalent native sync file I/O, headless.
**Goal:** isolate how much of the web build's incremental-load freezes is
**web-only** (blocking sync XHR of song assets) vs **shared loader architecture**
(sync `PollUntilLoaded`/`PollUntilEmpty` drains + parse/decode cost), by measuring
the *same* boot→menu→preview scenarios on the native build, which runs the
identical shared engine with native sync file I/O (no network).

## TL;DR

- **Sync drains (`lpu`) contribute 0.0 ms on native across every scenario and
  every loader budget.** The `PollUntilLoaded`/`PollUntilEmpty` call sites are
  reached but finish instantly because the file is already in the OS page cache —
  so the cooperative `Poll()` budget had already completed the loader before the
  drain ran. The sync-drain architecture is **not** the native stall source.
- **Song-preview hover has NO visible stall on native.** The mogg `NewStream`
  frame (30 MB encrypted read + decrypt + vorbis prime, all inline on one frame)
  costs **~10 ms**; the worst frame anywhere in the hover window is **≤ 26 ms**.
  The web build's 2–5 s preview freeze is therefore **~100% web-only** (sync XHR
  of a ~30 MB mogg over the network).
- The **only** non-trivial native stall is a **single ~160–206 ms frame on the
  splash→main_hub transition**, and it is **non-loader** work
  (`nonLoader ≈ 160–180 ms`, `lp` ≤ 27 ms, `lpu` = 0) — i.e. first-time scene
  instantiation + WebGPU render-pipeline build for the hub. This is *shared* GPU
  cost, not sync-XHR and not the loader.

## Method / exact commands

Frame trace is the env-gated JSONL tracer (`native/src/rb3_frame_trace.cpp`,
`RB3_FRAME_TRACE=<path>`), wired in `src/App.cpp:773-811`. Per-frame fields:
`dt` = wall ms for the whole `RunOneFrame`; `lp` = ms in budgeted background
`TheLoadMgr.Poll()`; `lpu` = ms in `PollUntilLoaded`+`PollUntilEmpty` sync drains
(attribution in `src/system/utl/Loader.cpp:305,327-329`); `ld` = loaders added
this frame; `st` = audio streams opened this frame (`Synth::NewStream` hook,
`src/system/synth/Synth.cpp:542`); `pend` = pending loaders.

Drivers (created for this task, under `/tmp`, no tracked-file edits):

```bash
# Boot -> title/splash -> main_hub -> song_select -> 3 preview hovers, with trace:
python3 /tmp/native_stall_probe.py --budget 8  --hover-settle 3.5 \
        --trace /tmp/rb3-native-b8.jsonl          # markers -> stdout JSON
python3 /tmp/analyze_trace.py /tmp/rb3-native-b8.jsonl /tmp/markers-b8.json

# Budget sensitivity:
for B in 4 8 16; do
  python3 /tmp/native_stall_probe.py --budget $B --trace /tmp/rb3-native-bud$B.jsonl \
          > /tmp/markers-bud$B.json
done

# Syscall-level mogg read timing during preview hovers:
strace -f -tt -T -e trace=openat,read,pread64 -o <out> rb3-native ...   # (/tmp/strace_preview3.py)
```

The probe sets `RB3_GAME=1 RB3_HTTP=1 MILO_HEADLESS=1 RB3_DATA=orig-assets/extracted`
and `RB3_GAME_INPUT="@10:start,@30:confirm,@140:select:pn_quickplay.btn,@220:select:qp_quickplay.btn"`
(auto-navs boot→song_select), then injects `down` over `/api/input` to hover
songs and dwells `--hover-settle` s so `SongPreview::Start` fires the mogg load.

## (a) Per-transition frame-time trace (budget 8)

| segment | frames | total ms | frozen ms¹ | longest frame | `lp` (Poll) | `lpu` (sync drain) | frames >50ms | streams |
|---|--:|--:|--:|--:|--:|--:|--:|--:|
| boot → main_hub (incl intro+splash) | 88 | 2983 | 1642 | **174.0 ms** @f23 | 460 | **0.0** | 5 | 0 |
| &nbsp;&nbsp;splash → main_hub | 64 | 2512 | 1447 | 71.0 ms | 225 | **0.0** | 3 | 0 |
| main_hub → song_select | 171 | 2421 | 135 | 51.8 ms | 137 | **0.0** | 1 | 0 |
| song_select entry (settle) | 202 | 2016 | 0 | 14.7 ms | 0 | **0.0** | 0 | 0 |
| preview hover 0 | 377 | 3513 | 0 | 12.7 ms | 0 | **0.0** | 0 | 0 |
| preview hover 1 | 396 | 3512 | 12 | **26.5 ms** | 0 | **0.0** | 0 | 1 |
| preview hover 2 | 397 | 3507 | 1 | 17.0 ms | 0 | **0.0** | 0 | 1 |

¹ `frozen ms` = Σ(frame_dt − 16.7) over frames above a 60fps budget. (Some "frozen"
in boot/splash is the headless loop simply running faster than 60fps work-per-frame;
the meaningful number is the *longest single frame*, the user-visible hitch.)

**Whole trace:** total 18 425 ms, `lp` = 598 ms, **`lpu` = 0.0 ms (0.00 % of wall).**

All frames > 50 ms (note `lpu` = 0 on every one; the big ones are `nonLoader`):

| frame | dt | lp | lpu | nonLoader | screen |
|--:|--:|--:|--:|--:|---|
| 23 | **174.0** | 10.2 | 0.0 | **163.8** | splash_screen |
| 24 | 71.0 | 13.7 | 0.0 | 57.3 | splash_screen |
| 85 | 53.0 | 0.0 | 0.0 | 53.0 | main_hub_screen |
| 2 | 52.8 | 14.5 | 0.0 | 38.3 | intro_movie_screen |
| 250 | 51.8 | 0.0 | 0.0 | 51.8 | song_select_screen (`ld`=2) |

## (b) Preview hover — is there any stall? Attribution.

**No visible stall.** The mogg-open (`st:1`) frame and its neighbours, three runs:

| run | st-frame dt | neighbouring frames | longest in window |
|---|--:|---|--:|
| budget 8  | 9.6 / 10.3 ms | 7.7→8.0→**9.6**→11.8→16.9→17.2 | 26.5 ms |
| budget 8 (rerun) | 7.9 / 7.9 ms | — | 23.2 ms |
| budget 16 | 10.9 / 9.7 ms | — | 26.4 ms |

`lp` = `lpu` = 0 on the stream-open frame: the preview path
(`SongPreview::Start` → `TheSynth->NewStream` → `StandardStream(NewFile…)`,
`src/system/meta/SongPreview.cpp:297,301`) does a **direct `NewFile`/fopen read**,
not a queued loader, so neither bucket is even involved. `MoggClip::EnsureLoaded`
(`MoggClip.cpp:196`) *can* call `PollUntilLoaded`, but only if its `kLoadFront`
loader isn't already done — on native it always is (instant read), so that drain
is a no-op too.

**Syscall-level (strace) attribution of the read** (page-cache-warm):

| asset | bytes | total in-kernel read time | openat |
|---|--:|--:|--:|
| `songs/25or6to4/25or6to4.mogg` | 31.96 MB (read 34.24 MB w/ re-reads) | **11.76 ms** | 0.019 ms |
| `songs/20thcenturyboy/20thcenturyboy.mogg` | 35.62 MB | ~12 ms | 0.019 ms |

(`orig-assets/extracted` is a CoW layer over `extracted-xbox-full`; the real
moggs are 30 MB-class. Bytes are encrypted on disk — decrypt happens in-engine
in the same `NewStream` frame, included in the ~10 ms.)

So the native preview cost ≈ **~12 ms file read + a few ms decrypt/vorbis-prime,
all on one ~10 ms frame.** File-read, decrypt, and vorbis-prime are each cheap;
`PollUntilLoaded` drain = 0.

## (c) Shared-vs-web-only decomposition

**Real RB3 mogg payload** (what a web preview hover must fetch): across 83 songs
`min 11.6 MB · median 29.6 MB · mean 30.3 MB · max 59.1 MB`.

```
preview stall  =  ~12 ms shared            +  Y ms web-only
                  (read + decrypt + prime)    (sync XHR of ~30 MB mogg)
```

- **Shared part (drain + decrypt + prime): ~10–12 ms.** Negligible. The sync-drain
  *architecture* costs essentially nothing because the bytes are already resident.
- **Web-only part (sync XHR): the entire 2–5 s freeze.** On web the same ~30 MB
  read is `WebAssetsFetchSync` → `xhr.open(GET, url, false)` (blocking sync XHR on
  the main thread, `native/src/native_file.cpp` → engine `WebAssets.cpp:339`),
  i.e. a 30 MB network download that blocks `RunOneFrame`. Agent 05 measures the
  web side directly (port 8431); this native run isolates the shared remainder to
  ~10 ms, so **Y ≈ (web preview freeze) − ~10 ms ≈ the whole thing.**

The **menu transitions** (boot→title, title→hub, hub→song_select) are partly
shared: native shows a single ~160–206 ms hub-scene-build spike (GPU pipeline +
scene instantiation, `nonLoader`), plus ~130–460 ms of `lp` budgeted background
loading spread across many small frames. On web the *transition asset* milos for
those screens are boot-prefetched (`/api/bundle/boot`), so the network component
is smaller there than for song moggs — but the ~170 ms shared scene-build spike
**will reproduce on web** as WebGPU pipeline compilation, and is **not** fixed by
touching the loader.

## (d) Loader-budget sensitivity (RB3_LOADER_BUDGET_MS = 4 / 8 / 16)

| budget | boot→hub longest | hub→song_select longest | worst preview frame | `lpu` (all) |
|--:|--:|--:|--:|--:|
| 4  | 162.4 ms (nonLoader 158) | 46.3 ms | 23.3 ms | **0.0** |
| 8  | 174.9 ms (nonLoader 162) | 45.2 ms | 23.2 ms | **0.0** |
| 16 | 206.4 ms (nonLoader 179) | 50.2 ms | 26.4 ms | **0.0** |

The budget knob trades **spike height for spike count** on the boot/splash
transition (b4: 162 ms × fewer; b16: 206 ms × more `>50ms` frames) but does **not**
change total transition stall and is **irrelevant to `lpu`** (always 0) and to
preview hover (≤ 26 ms regardless). Tuning `RB3_LOADER_BUDGET_MS` is **not** the
lever for the user-visible freezes.

## Implications — where the fix must live

1. **Preview-hover freeze = 100 % web-only sync XHR.** Fix belongs in the **web
   I/O layer**, not the shared loader: make song-asset fetch **async** (the
   loader is already a cooperative state machine — feed it bytes from an async
   `fetch()`/streamed XHR off the main thread, or prefetch the hovered song's
   mogg via the boot-bundle mechanism on highlight, before `SongPreview` blocks).
   The shared `PollUntilLoaded`/`NewStream` path is already cheap (~10 ms) and
   does not need rearchitecting for *correctness* — only the byte source does.
2. **Sync-drain rearchitecting is NOT where the win is.** `lpu` = 0 on native at
   every budget proves the 17 sync-drain call sites are not the stall on a system
   with resident bytes. They only *appear* expensive on web because each one
   synchronously triggers a blocking XHR. Eliminate the blocking fetch and the
   drains become free there too — converting drains to async is a *consequence*
   of async I/O, not an independent fix.
3. **The ~170 ms hub-transition spike is shared GPU/scene-build**, reproduces on
   web as WebGPU pipeline compilation, and is a *separate* workstream (pipeline
   pre-warm / scene pre-instantiation), independent of the loader and of XHR.
4. **Loader budget** is a minor smoothness knob (spike-height vs count); leave at
   8. Not the fix for any user-visible freeze.

## Artifacts

- Traces: `/tmp/rb3-native-b8.jsonl`, `/tmp/rb3-native-bud{4,8,16}.jsonl`
- Drivers: `/tmp/native_stall_probe.py`, `/tmp/analyze_trace.py`, `/tmp/strace_preview3.py`
- strace: `/tmp/strace3-*.strace` (mogg read timing)
