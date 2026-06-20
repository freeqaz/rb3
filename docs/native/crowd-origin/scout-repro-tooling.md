# Scout 4 — Live repro + tooling pattern (crowd/band-gear at origin)

Date: 2026-06-20. Scope: (A) confirm the symptom live, (B) find the cleanest
pattern for a future "dump object world positions" debug tool. This is a
SCOUT/handoff doc — no engine fix here.

---

## (A) LIVE REPRO — CONFIRMED

Built `rb3-native` with the canonical -O0 Debug recipe (exit 0, binary
`native/build-native/rb3-native`, 115 MB). Drove it headless to gameplay with
the `song-end-test.py` nav pattern and captured a gameplay `/api/screenshot`.

- Engine pin skew (non-blocking warning at configure): engine HEAD
  `884ab17d`, rb3 pins `MILO_ENGINE_PIN = 8fb669d9`. Built against the pinned
  engine; symptom reproduces.
- Reached `game_screen`, `songMs` advancing, `{game is_playing}=1`,
  `{game is_game_over}=0`. Screenshot saved to `/tmp/crowd-gameplay.png`.

**What the screenshot shows (symptom confirmed):**
- The note highway / gem track renders correctly in the foreground (center).
- On the LEFT third of the frame there is a large bright JUMBLED PILE of
  overlapping geometry — character + instrument meshes clumped at a single
  location. This is consistent with "crowd + band gear (incl. drum kit)
  collapsed to one spot (origin)".
- Venue dressing (a dartboard) renders on the right; the rest of the venue is
  dark (a SEPARATE lighting/env issue, not the collapse — don't conflate).

The collapse affecting BOTH crowd AND band gear (incl. the drum kit) points to
a SHARED placement/transform seam rather than a crowd-only bug. The two
placement systems that would have to BOTH break are:
1. **Crowd 3D path** (`WorldCrowd::Draw3D`, see below) — per-member transform
   comes from `m3DChars[i].unk0`.
2. **Band gear / venue items** — placed off their own `RndTransformable`
   WorldXfm (drum kit, etc.), composed through the venue/world dir hierarchy.

A common upstream cause would be a parent/world dir transform resolving to
identity at origin, or the per-member authored transforms reading as zero.

### Repro recipe (reuse this)
- Binary: `native/build-native/rb3-native`
- Env: `RB3_GAME=1 RB3_HTTP=1 RB3_HTTP_PORT=<p> MILO_HEADLESS=1 RB3_DATA=orig-assets/extracted`
  plus `RB3_GAME_INPUT=<NAV>` (the exact nav string is in `song-end-test.py`
  `NAV_SCRIPT` and in the throwaway `/tmp/crowd_repro.py` I used).
- Poll `/api/health` until `songMs > 2000` (gameplay underway), sleep ~2s,
  then `GET /api/screenshot` (PNG). Boot→gameplay ~1900 frames / ~90s here.

---

## (B) TOOLING PATTERN — "dump object world positions"

### The exact code-paths to copy (all already in-tree)

**1. The cleanest env-gated + DTA-registered probe template:**
`native/src/rb3_http_handlers.cpp` → `RB3DtaCharProbe` (lines ~462-498),
registered at ~502 via `DataRegisterFunc(Symbol("rb3_char_probe"), ...)`.
It is callable over HTTP as `POST /api/dta/eval` body `{rb3_char_probe 0}`,
returns a one-line summary string (so the harness gets it directly), AND when
`CHAR_PROBE_DUMP` env is set it also `fprintf(stderr, ...)` a detailed
per-object dump (the harness greps the log). This is the model to mirror — it
already combines BOTH delivery channels (HTTP return value + env-gated stderr).

**2. How to enumerate every positioned object + read its world position:**
`native/src/rb3_render_mesh.cpp` `CountAndBound` (lines ~112-158) is a working
example:
```cpp
for (ObjDirItr<Hmx::Object> it(dir, true); it; ++it) {   // recursive walk
    if (RndMesh* mesh = dynamic_cast<RndMesh*>(o)) {
        const Transform& w = mesh->WorldXfm();            // world pos = w.v
        ...
    }
}
```
`WorldXfm()` lives on `RndTransformable` (`src/system/rndobj/Trans.h:104`); the
world-space position is `WorldXfm().v` (a `Vector3`). `Character` is a
`RndDir : ... : RndTransformable`, so `character->WorldXfm().v` is its placed
position — this is exactly what the crowd path SETS via `SetWorldXfm(spXfm)`.

**3. The dir handles to iterate at runtime:**
- `ObjectDir::sMainDir` — the global root, recursable with `ObjDirItr<T>(dir, true)`
  (used in `rb3_gamewarm_native.cpp:441` and resolved by DTA-eval at
  `rb3_game_input.cpp:611`: `ObjectDir::sMainDir->FindObject(name, true)`).
- For the CROWD members specifically you cannot just iterate drawables — a
  `WorldCrowd` draws N instances from ONE archetype `Character`, so the
  per-member positions are NOT separate objects. They live in
  `WorldCrowd::mCharacters` → `CharData::m3DChars[i].unk0` (a `Transform`,
  `unk0.v` = authored per-member position), composed at draw time in
  `src/system/world/Crowd.cpp` `Draw3D` (lines ~321-410) into `spXfm`, then
  applied with `curChar->SetWorldXfm(spXfm)` (line ~408). To validate the crowd
  DISTRIBUTION you must dump `m3DChars[i].unk0.v` (and/or the final `spXfm.v`),
  not just iterate `Character` objects.
- `WorldCrowd` is reachable by `dynamic_cast<WorldCrowd*>` while walking
  `ObjectDir::sMainDir` (it's a registered object; see `Crowd.h` /
  `WorldDir : PanelDir` in `world/Dir.h`).

**4. The python-harness side (capture + parse):**
`scripts/native/song-end-test.py` is the canonical driver: pure-stdlib HTTP,
`free_port()`, launch with env, `wait_for(/api/health)` predicate, `dta_eval()`
helper that POSTs to `/api/dta/eval` and parses
`{"ok":true,"data":{"type":"...","value":...}}`. The throwaway
`/tmp/crowd_repro.py` I wrote adds the `/api/screenshot` capture. A position-
dump harness = song-end-test.py boot/wait + a `dta_eval(port, "{rb3_pos_dump}")`
call (and, if the dump is large, set the env flag and grep the engine log file
that the harness already redirects stdout/stderr into, e.g. `/tmp/rb3-*.log`).

### RECOMMENDATION — simplest tool that gives per-object world positions

Add ONE DTA func `RB3DtaPosDump` in `native/src/rb3_http_handlers.cpp`,
mirroring `RB3DtaCharProbe` exactly, registered as
`DataRegisterFunc(Symbol("rb3_pos_dump"), RB3DtaPosDump)`. It does:

1. Walk `ObjectDir::sMainDir` with `ObjDirItr<Hmx::Object> it(dir, true)`.
   - `dynamic_cast<WorldCrowd*>` → for each `mCharacters` CharData, for each
     `m3DChars[i]`, `fprintf(stderr, "[POSDUMP] crowd '%s' i=%d pos=%.2f,%.2f,%.2f\n",
     name, i, unk0.v.x, unk0.v.y, unk0.v.z)`. (This is the crowd-distribution
     ground truth — the thing the user actually wants to validate.)
   - `dynamic_cast<RndTransformable*>` (cast last; covers band gear / venue
     items incl. the drum kit) → `WorldXfm().v`; dump name + class + pos.
2. Return a short summary string (count of crowd members, count of placed
   transformables, and how many are within ~1u of origin — the smoking-gun
   metric) so the HTTP caller gets a verdict directly without parsing the log.
3. Gate the verbose per-object stderr lines behind an env flag (mirror
   `CHAR_PROBE_DUMP`, e.g. `POS_DUMP_VERBOSE`) so `{rb3_pos_dump}` is cheap to
   poll and the full dump is opt-in.

Why a DTA func and not a new `/api/positions` endpoint: `/api/dta/eval` already
routes to main-thread DTA funcs with a crash guard
(`DtaEvalCrashHandler`/`gCallStackPtr` save-restore in `rb3_http_handlers.cpp`),
so a registered func is ~15 lines, needs NO new HTTP wiring, is callable from
the existing `dta_eval()` python helper, and is the SAME pattern reviewers
already maintain (`rb3_char_probe`, `rb3_overshell`, `rb3_set`). A bespoke
`/api/positions` endpoint would re-implement the screenshot/handler plumbing in
`rb3_http_server.cpp` for no benefit. Native gating: this is native-only code
in `native/src/` — no `src/system/*` change, so no Wii-codegen risk; if any
helper is ever added to `src/system/world/Crowd.cpp`, it MUST be `#ifdef HX_NATIVE`.

Harness: copy `song-end-test.py`'s boot+wait, then
`print(dta_eval(port, "{rb3_pos_dump}"))` for the verdict, and (with
`POS_DUMP_VERBOSE=1` in the env) grep the redirected engine log for
`[POSDUMP]` lines to get the full per-object distribution.

### Smoking-gun checks the tool should make obvious
- Count of objects whose `WorldXfm().v` length < ~1.0u (collapsed-to-origin).
- Crowd: are `m3DChars[i].unk0.v` all equal / all zero (authored data wrong, or
  not loaded), vs. spread but composed wrong (parent `mPlacementMesh->WorldXfm()`
  identity at origin)? That fork tells the next agent whether the bug is in the
  authored/loaded per-member data or in the world-dir parent transform — and
  since the DRUM KIT (separate system) is also at origin, the parent/world-dir
  transform hypothesis is the stronger lead.

---

## Files / artifacts
- Screenshot: `/tmp/crowd-gameplay.png` (gameplay frame showing the collapse).
- Throwaway repro: `/tmp/crowd_repro.py` (boot→gameplay→screenshot).
- Engine log of the repro run: `/tmp/rb3-crowd-<port>.log`.
- Probe template: `native/src/rb3_http_handlers.cpp:462` (`RB3DtaCharProbe`) +
  registration at `:502`.
- World-pos iteration template: `native/src/rb3_render_mesh.cpp:112`
  (`CountAndBound`, `mesh->WorldXfm()` at `:143`).
- Crowd per-member positions: `src/system/world/Crowd.cpp:321-410`
  (`m3DChars[i].unk0` → `spXfm` → `SetWorldXfm` at `:408`); struct in
  `src/system/world/Crowd.h:39` (`Char3D::unk0` Transform).
- Harness template: `scripts/native/song-end-test.py` (`dta_eval`, `health`,
  `wait_for`, env launch).
