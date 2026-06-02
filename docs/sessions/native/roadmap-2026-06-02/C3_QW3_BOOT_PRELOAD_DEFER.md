# C3 (QW-3) — Defer the boot crowd + colorpalettes preload burst

## TL;DR
The QW-3 premise — that a synchronous boot preload of crowd meshes + the 20 MB
`colorpalettes.milo` blocks the boot critical path — **does not hold for the
current native build**. The earlier QW-1 budgeted `LoadMgr::Poll` already keeps
those heavy char/colorpalettes loads off the pre-server path. I implemented the
faithful QW-3 defer anyway (queue the `preload_subdirs` entries async instead of
draining each synchronously), env-gated and verified safe end-to-end, but it is
**behavior-neutral at boot** (~1925 ms both ways). The real boot cost is
`TheUI.Init` (~0.8 s), which is explicitly out of QW-3 scope.

## Exact call site found
- `PreloadSharedSubdirs(Symbol)` in `src/system/obj/Dir.cpp:1021` is the boot
  preload. It iterates `SystemConfig("preload_subdirs")` for a category symbol and
  calls `ObjDirPtr<ObjectDir>::LoadFile(fp, /*async=*/false, /*share=*/true,
  kLoadFront, false)` per entry. With `async=false`, `LoadFile` (Dir.h:63) calls
  `PostLoad(0)` → `TheLoadMgr.PollUntilLoaded` — a synchronous blocking drain per
  file. The `band` category (`Band.cpp:134 BandInit` → `PreloadSharedSubdirs("band")`)
  includes `char/main/shared/colorpalettes.milo` (20,838,030 bytes, confirmed).
- The preload list is `orig-assets/extracted/config/preload_subdirs.dta`.
- IMPORTANT: the crowd_male01..04 / extras char burst the doc attributes to this is
  NOT sourced here. It is venue/world char setup that runs in the frame loop
  (frames 7-8); my patch does not move it (verified: burst position identical
  ON/OFF). The only thing in `preload_subdirs[band]` related to crowd is
  `char/crowd/anim/shared_clips.milo`.

## Method
- Isolated CoW worktree `tools/setup-worktree.sh c3qw3` →
  `/home/free/code/milohax/rb3/.claude/worktrees/c3qw3` (branch `wt-c3qw3`).
  Configured native build there with `Dawn_DIR` +
  `MILO_ENGINE_GPU_BACKEND=rb3`; built `rb3-native`. Never touched the main build.
- Headless harness (mirrors `scripts/native/*.py`): launch with
  `RB3_GAME=1 RB3_HTTP=1 RB3_HTTP_PORT=<auto> MILO_HEADLESS=1
  RB3_DATA=<repo>/orig-assets/extracted`, time process-start → first `/api/health`
  200, and grep the engine log for the char/crowd/colorpalettes burst + `Skinned
  mesh needs to be re-exported` NOTIFYs.
- Added an env-gated native boot-phase timer (`RB3_BOOT_TIMING=1`, reusing the
  `WEB_BOOT_MARK` macro that already brackets every App-ctor phase) to attribute
  the boot cost per phase. This is the decisive measurement.

## Measurements
| Phase / metric | sync (DEFER=0, Wii-identical) | async (DEFER=1, default) |
|---|---|---|
| boot → /api/health (first cold/loaded run) | 4491 ms (cold cache + load avg ~25) | 1973 ms |
| boot → /api/health (fair low-load A/B) | 1925 ms | 1924-1927 ms (×3) |
| BandInit done (PreloadSharedSubdirs band) | @707.0 ms (~1.2 ms cost) | @700.8 ms |
| TheUI.Init done | @1573.7 ms (~862 ms) | @1517.7 ms |
| PollUntilEmpty | ~0 ms | ~0 ms |
| crowd_male01 burst | frame 7-8 | frame 7-8 (unchanged) |
| colorpalettes merge NOTIFY | frame 16 | frame 16 |

The 4491 ms the doc cites was a cold-cache + high-concurrent-load artifact. Under
matched low load the sync path is already ~1925 ms, and the async defer changes
boot time by < 3 ms (noise). BandInit's preload costs ~1 ms in BOTH paths because
QW-1's budgeted `LoadMgr::Poll` already trickles the heavy loads into the frame
loop (the queue is essentially empty by `PollUntilEmpty` at App.cpp:427).

## Why the patch is still correct / included
- It is the faithful realization of QW-3: under HX_NATIVE we queue the preloads
  async (`LoadFile(async=true)`) so they cannot ever block boot, instead of
  synchronously draining each. `gPreloaded[]` only keeps a ref; nothing derefs it
  synchronously after the loop, and every consumer reaches these shared dirs by
  name via `DirLoader::Find` once resident.
- It re-protects boot if a future change reintroduces a synchronous preload drain
  or if QW-1's budget is removed/raised.
- Reversible + env-gated: `RB3_DEFER_CROWD_PRELOAD=0` restores Wii-identical
  blocking behavior.

## Regression / safety
- `scripts/native/song-end-test.py` against the patched binary (defer ON): boot →
  menus → song → `game_screen` (songMs advances) → `{game jump 600000}` →
  game-over. **PASS (rc=0).** Crowd/colorpalettes lazy-load fine; no crash, no
  missing-asset failure. No speed-for-crash trade.

## Patch shape (disjoint from concurrent-avoid set)
Two files, both additive, both outside the avoid set
(`src/system/obj/Dir.cpp` ≠ the avoid-listed `src/system/rndobj/Dir.cpp`;
`src/App.cpp` ≠ avoid-listed `native/src/main_native.cpp`):
1. `src/system/obj/Dir.cpp` — `PreloadSharedSubdirs`: `#ifdef HX_NATIVE` arm
   queues async (env-gated `RB3_DEFER_CROWD_PRELOAD`, default on); `#else` is the
   byte-for-byte original loop, so the Wii MWCC build is unaffected.
2. `src/App.cpp` — `WEB_BOOT_MARK` gains an `#elif defined(HX_NATIVE)` arm
   (env-gated `RB3_BOOT_TIMING`) that prints per-phase boot ms; no-op unless the
   env is set, so a normal run is unchanged. Wii build untouched.

Build: clean (both .o compiled, rb3-native linked, exit 0). `getenv` was already
available transitively in both TUs.

## Recommendation
- Apply the patch (it is the correct QW-3 shape and harmless) but **do not expect
  a boot-time win** — QW-1 already owns this. If boot speed is the goal, the next
  target is `TheUI.Init` (~0.8 s of UI panel deserialization), which is a separate
  area from the loader spec.
- The `RB3_BOOT_TIMING=1` instrument is a useful permanent diagnostic for any
  future boot/loader work.

## Files
- Patched (worktree): `/home/free/code/milohax/rb3/.claude/worktrees/c3qw3/src/system/obj/Dir.cpp`, `/home/free/code/milohax/rb3/.claude/worktrees/c3qw3/src/App.cpp`
- Spec: `/home/free/code/milohax/rb3/docs/sessions/native/roadmap-2026-06-02/loader-performance.md` (QW-3, §1.3, §3 Phase QW-3)
- Preload list: `/home/free/code/milohax/rb3/orig-assets/extracted/config/preload_subdirs.dta`
- Final diff: `/tmp/c3qw3_final.diff`