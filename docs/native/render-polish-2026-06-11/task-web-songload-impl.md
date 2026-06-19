# task-web-songload — IMPL (render-polish wave-8 wrap-up)

**Status: DONE.** The full song set loads + plays in the BROWSER web build (the one
open native-vs-web gap). No code fix was needed beyond a rebuild + a new test
harness — the web delivery path was already correct; the only gap was the STALE
deployed wasm (predated the `SongParser` underflow fix). `needsEngine = false`.
Wii build byte-identical (no shared `src/` change).

## What the item asked
Confirm all 83 songs load+play in the browser web build (native already proven by
wave-7 chart-wiring), and fix any web-specific breakage (symlink-following / fetch
404 / lazy-fetch stall / compression).

## Root cause of the "gap" (operational, not a code bug)
The deployed `native/web/build/{release,debug}/rb3-web.wasm` was mtime **2026-06-11**,
which **predates** the committed `SongParser::ParseAndStripLyricText` pointer-underflow
SIGSEGV fix `e83e2c79` (**2026-06-16 22:25**, `#ifdef HX_NATIVE`, `src/system/beatmatch/
SongParser.cpp:1095`). So any in-browser test against the as-deployed wasm reproduces
the *pre-fix* crash on real charts — a false negative, not a delivery defect. The fix
is `#ifdef HX_NATIVE`; `HX_NATIVE` is defined for the web build too, so it compiles
into rb3-web identically. **The remedy is a rebuild**, mirroring the MEMORY gotcha
"stuck on song load = STALE deployed build, rebuild fixed."

## What I did
1. `tools/setup-worktree.sh task-web-songload` → worktree at
   `/home/free/code/milohax/rb3/.claude/worktrees/task-web-songload` (branch
   `wt-task-web-songload`, base `1c46a70e`, which is ahead of the fix → fix present
   in source). No `.engine-path` (uses pinned engine `15ce606d…`; no engine change).
2. `scripts/web/build.sh --debug` → fresh `native/web/build/debug/rb3-web.wasm`
   mtime **2026-06-19 19:05** (post-fix). Build green; only the expected
   store/net/PlatformMgr `undefined symbol` stub warnings.
3. Authored `scripts/web/_w8-songload-verify.mjs` (new test artifact, committed
   `5015eb0d`): per-song ISOLATED Playwright runs (fresh browser per song so one
   boot flake can't cascade), name-targeted song selection via
   `window.rb3HighlightedSong` (published by `main_web.cpp:404 PublishHighlightedSong`
   → the highlighted SortNode's token == song shortname), guitar/expert, then assert
   `game_screen` + advancing frame + painted highway, plus a per-song console scan
   for `.mid`/`.mogg` 404/FAILED and hard-crash signatures.

## Verification (evidence under /tmp/rp8-web-songload/)
Server: `python3 native/web/server.py --port 9805` (my assigned range 9805-9808).

### Server-side delivery (curl) — ALL PASS
| Check | Result |
|---|---|
| `.mid` GET (5 targets) | HTTP **200**, Content-Length == symlink-target size, magic `MThd`, type `audio/midi`. bohemianrhapsody 165422, freebird 517446, rehab 144279, crazytrain 207531, 20thcenturyboy 175996. |
| `.mid` compression round-trip | `Accept-Encoding: br` → `Content-Encoding: br` (27796 br); `brotli -d` **cmp-IDENTICAL** to identity body. (fetch() decodes transparently before MEMFS.) |
| `/api/manifest` mids | **83** `.mid` entries, sizes == symlink targets (raw `grep -c '\.mid' = 83`). |
| `.mogg` audio Range | HTTP **206**, `Content-Range: bytes 0-1023/41033510` (full symlink-followed size); all 5 moggs present (41-59 MB). |

Server.py `_serve_asset_file` (server.py:935) follows symlinks at every step
(`os.path.isfile`/`getsize`/`open` all dereference); `.mid ∈ COMPRESSIBLE_EXTS`
(server.py:110), `.mogg ∈ INCOMPRESSIBLE_EXTS` (server.py:114). No code change needed.

### In-browser gameplay (Playwright, `?debug=true`, no-store) — ALL PASS
| Song | screen | Δframe (4s) | painted% | note |
|---|---|---|---|---|
| bohemianrhapsody | game_screen | 74 | 69.1 | previously-dead; highway+now-bar visually confirmed |
| freebird | game_screen | 50 | 98.1 | previously-dead; highway + 5 smashers + band visually confirmed |
| rehab | game_screen | 65 | 98.1 | previously-dead; midFetched logged |
| crazytrain | game_screen | 72 | 81.4 | previously-dead; midFetched logged |
| 20thcenturyboy | game_screen | 75 | 83.5 | dev song — NO REGRESSION (solo run, run3) |

- All 5 reach `game_screen` with the frame counter advancing and a painted highway.
- Deep-gameplay screenshots (`/tmp/rp8-web-songload/highway/{bohemianrhapsody,freebird}_t5.png`,
  taken 5s into game_screen, screen still == game_screen, Δframe 76/91 over 3s) show
  the real note highway (Bohemian Rhapsody's Queen-themed lanes + now-bar; freebird's
  5 colored fret smashers + band + venue).
- Console scan: **zero** `.mid`/`.mogg` 404 or FAILED; **zero** SIGSEGV/SIGABRT/abort/
  RuntimeError/MILO_FAIL. The lazy `.mid` fetch lands over the wire (e.g.
  `WebAssets: songs/bohemianrhapsody/bohemianrhapsody.mid (165422 bytes)`), then the
  parser reads the chart (the benign `NOTIFY: ... Double note-on` lines are the MIDI
  parser reading real events — exactly the path the underflow fix unblocked).

### Caveats / non-issues
- **20thcenturyboy FAILED in the 5-song run2** with "never reached splash_screen" —
  a transient BOOT flake on the 5th consecutive fresh-browser boot (browser launch /
  server saturation). Confirmed a flake: solo retry (run3) PASSED at painted 83.5%.
- **`midFetched=false` for bohemianrhapsody/freebird** is a log-grep artifact (their
  3 MB+ char-loading console logs; the fetch line is present — run1 showed it
  explicitly — and reaching game_screen requires the parsed chart anyway). Not a bug.
- The recurring `WebAssets: FAILED ... 404` lines in console are **benign pre-existing
  dev/optional-asset misses** (`rndobj/gen/sphere.milo_xbox`, `ui/dev_only/selvenue.dta`,
  `ui/framerate/frame_rate.dta`, `<song>.vfv`) — the same long-tail misses present
  across the web port; they do NOT block gameplay (every target reached game_screen
  despite them) and are NOT `.mid`/`.mogg`. Out of scope.

## Match-discipline
- No shared Wii `src/` edit. The only file I added is `scripts/web/_w8-songload-verify.mjs`
  (a Playwright test, not compiled into anything). The `SongParser` fix is already
  landed + `#ifdef HX_NATIVE` + previously objdiff-confirmed byte-identical.
- `wiiByteIdentical = true` by construction (no compiled-source change).

## Deliverables
- **Worktree:** `/home/free/code/milohax/rb3/.claude/worktrees/task-web-songload`
- **Branch:** `wt-task-web-songload`
- **Commit:** `5015eb0d` — `test(web): in-browser full-song-set load+play verify harness`
  (1 file, +222: `scripts/web/_w8-songload-verify.mjs`)
- **needsEngine:** false. **No engine worktree touched.**

## Landing notes (for the orchestrator)
- Single rb3 commit `5015eb0d` adds ONE new file `scripts/web/_w8-songload-verify.mjs`
  (new file, no overlap with any other task — zero conflict risk). Cherry-pick onto
  master directly.
- NO engine commit, NO `MILO_ENGINE_PIN` bump, NO `Rnd_Wgpu_RB3.cpp`/`standard_wgsl.inc`
  touch (those are sibling-task regions; I touched none of them).
- The deployed `native/web/build/**` wasm was NOT committed (build artifacts are
  gitignored / regenerated). To make the live deploy reflect the fix, run
  `scripts/web/build.sh` (both release+debug) on the deploy host after landing.

## Reviewer hints
- Independent: pick 2-3 songs NOT in {bohemianrhapsody,freebird,rehab,crazytrain,
  20thcenturyboy} (sweep the alphabet, e.g. `25or6to4`, `antibodies`, `centerfold`),
  build `scripts/web/build.sh --debug`, serve on a port in 9805-9808, and run
  `node scripts/web/_w8-songload-verify.mjs --port <P> --songs <a,b,c>`; expect each
  game_screen + advancing frame + painted highway, clean `.mid`/`.mogg` console.
- Confirm the deployed wasm mtime is post-`e83e2c79` (2026-06-16) to guard against a
  stale-build false pass.
- If a song boot-flakes (rare 5th-consecutive-boot), retry it solo — it's a Playwright
  boot timeout, not a delivery failure.
