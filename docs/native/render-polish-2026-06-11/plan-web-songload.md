# Plan — web-songload: confirm all 83 songs load+play in the BROWSER web build

**Status: tractable = `yes` (likely a build + in-browser verify; NO server.py/manifest code fix expected).**

The wrap-up item asks to confirm the full 83-song set (proven on native by wave-7
chart-wiring) also loads+plays in the **browser** web build, and to fix any
web-specific breakage. After a read-only end-to-end audit of the web delivery path
(server.py + the engine's `WebAssets.cpp` fetch + `native_file.cpp` open shim +
the committed `SongParser` fix + a live HTTP probe of the server endpoints), the
delivery path is **already correct**. The single real gap is operational, not a
code bug: **the deployed web wasm is STALE** — it predates the `SongParser`
chart-fix commit, so an in-browser test against the current `native/web/build/`
would crash/hold exactly as native did pre-fix. The fix is therefore: **rebuild
the web build, then run an in-browser verification of specific previously-dead
songs.** Keep a small server.py change in reserve only if the live browser run
surfaces a path-resolution or compression problem (none predicted — see Risk).

---

## Root cause / current-state analysis

### What "web song-load" depends on, and the state of each link

1. **The `.mid` charts exist under the loader root and resolve.**
   `find -L orig-assets/extracted/songs -iname '*.mid' | wc -l` = **83**, all
   symlinks, **0 broken** (`-xtype l` = 0). Targets point into
   `…/extracted-xbox-full/songs/<id>/<id>.mid` (machine-local; gitignored — reproduce
   with the campaign one-liner). The 80 wired + 3 dev all present. ✅

2. **server.py resolves the symlinks transparently.** `/api/file/<path>`
   (`_serve_asset_file`, server.py:935) does `os.path.join(ASSETS_DIR, safe)` then
   `os.path.isfile()` + `open(...,'rb')` — Python follows symlinks at every step.
   **Verified LIVE** (server on :9801): `GET /api/file/songs/bohemianrhapsody/
   bohemianrhapsody.mid` → 200, **165422 bytes** (= the symlink target's size),
   `MThd` magic, `Content-Type: audio/midi`. ✅ No symlink-following gap.

3. **`.mid` is served compressed AND round-trips byte-identically.** `.mid ∈
   COMPRESSIBLE_EXTS` (server.py:110), so a browser advertising `Accept-Encoding:
   br` gets a `Content-Encoding: br` body (24554 br → 165422 raw). The engine's
   fetch path (`WebAssets.cpp` `webAssetsAsyncFetchToMemfs`, EM_ASYNC_JS) uses
   `await fetch()` + `arrayBuffer()`, which **transparently decodes `Content-
   Encoding` before the bytes reach MEMFS** — so the C++ `MidiReader` sees the
   original raw chart. **Verified**: `brotli -d` of the br body `cmp`-equals the
   identity body. ✅ No compression-corruption risk.

4. **`.mid` is in `/api/manifest` with the correct (symlink-followed) size.**
   `_serve_manifest` walks `ASSETS_DIR` with `os.walk` (lists symlink files) and
   `os.path.getsize` (follows them). **Verified LIVE**: manifest `.mid` count =
   **83**, sizes correct. This matters because the engine's async-open oracle
   (`native_file.cpp:1365` `WebAssetsManifestSize`) keys the WebPendingFile path on
   a non-negative size. With the mids in the manifest, the chart takes the async
   WebPendingFile path; **even on a manifest MISS the code falls back to the legacy
   sync path** (`native_file.cpp:1366-1367` comment: a -1 is *not* a definitive
   404 → legacy sync `NativeStdioFile` → `WebAssetsFetchSync` → `GET /api/file/…`
   → server resolves the symlink). **Both branches succeed.** ✅

5. **The chart-open goes through the standard web shim.** `SongParser` opens the
   chart via `MidiReader(*mFile, …)` (`SongParser.cpp:135`) where `mFile` is a
   normal `NewFile`/`NativeStdioFile` (`native_file.cpp:272`). On an `__EMSCRIPTEN__`
   read-miss it runs `WebAssetsFetchSync("/data/songs/<id>/<id>.mid")` and re-`fopen`s
   — the exact path proven in (2). The wave-6 no-chart guard (`Game.cpp:319`,
   `BeatMatcher.cpp:23`, `SongData.h`) only fires when the `.mid` is genuinely
   absent; with symlinks present it does not engage. ✅

6. **The `SongParser` SIGSEGV fix compiles into rb3-web identically.** The
   committed fix `e83e2c79` (`ParseAndStripLyricText`, on master, latest commit on
   that file) is `#ifdef HX_NATIVE` (`SongParser.cpp:1095`). **HX_NATIVE is defined
   for BOTH the native and web builds** (web is the native port's browser target —
   same shared `src/` + engine), so the underflow fix is active in rb3-web. ✅ No
   web-specific code divergence.

### The one real gap: the deployed wasm is STALE
- `native/web/build/release/rb3-web.wasm` mtime **2026-06-11 21:26**;
  `…/debug/rb3-web.wasm` **2026-06-11 21:27**.
- The `SongParser` chart-fix `e83e2c79` landed **2026-06-16 22:25** — *after* the
  deployed wasm was built.
- So the currently-deployed web build does **not** contain the fix. An in-browser
  test against `native/web/build/` as-is would reproduce the *pre-fix* crash/hold
  on real charts (e.g. bohemianrhapsody) — a false negative, not a delivery bug.
- This mirrors the MEMORY gotcha "stuck on song load = STALE deployed build,
  rebuild fixed" (incremental-load-perf note).

**Conclusion:** the web delivery path (server.py symlink-following + lazy `.mid`
fetch + compression + manifest oracle + the committed parser fix) is correct as
written. The task is overwhelmingly a **rebuild + in-browser verification**, not a
server.py/manifest fix. Set `tractable: yes` on that basis, with a contingency in
the plan if the live run surprises us.

---

## Exact files + approach

### Step 0 — (no code) reproduce the machine-local symlinks if absent
Already present on this machine (verified). On a fresh checkout, run the campaign
one-liner from `CAMPAIGN_SUMMARY.md:64-68`. (Mids + moggs are gitignored.)

### Step 1 — rebuild the web build (debug, fast loop)
```bash
scripts/web/build.sh --debug      # -O0 -g2, gzip only (no multi-minute brotli q11)
```
Produces `native/web/build/debug/rb3-web.{js,wasm,gz}`. This is the only required
build for verification (served at `?debug=true`, no-store → no cache staleness).
Build a release too (`scripts/web/build.sh` with no flag = both) only if a
release-path A/B is wanted; not required for the headline.

### Step 2 — verify in-browser (NO code change if it passes)
Drive the freshly-built debug build with the Playwright harness
(`scripts/web/lib/core.mjs`), selecting **specific previously-dead songs by name**
(not the harness's default "one ArrowDown then Enter", which picks an arbitrary
song). The lever for name-targeted selection already exists:
`engineState(page).highlightedSong` (published by `main_web.cpp:417`
`window.rb3HighlightedSong`). The verification script:
1. `launchBrowser(port, { query: 'debug=true' })`, `waitForBoot`, `navigateTo(…,
   SCREENS.SONG_SELECT)`.
2. For each target song: press `ArrowDown`/`ArrowUp`, polling `highlightedSong`
   until it equals the target shortname (cap the scroll, e.g. ≤ 250 presses); then
   `Enter` → `part_difficulty_screen`; pick guitar/expert + nofail + autohit via
   the existing key sequence; `Enter` → wait `game_screen`.
3. Assert `screen === 'game_screen'` and `frame`/songMs advancing; `screenshot`
   the canvas (expect highway + 5 smashers + gems + HUD). Optionally read a score
   widget if a web dta-eval bridge exists; otherwise the rendered highway +
   advancing frame + non-silent audio is the PLAY signal.
4. Scan the captured browser console for `WebAssets: FAILED`, `404`, `SIGSEGV`/
   `abort`/`RuntimeError`, or a hold at `tv3_b_screen`/the loading vignette.

Author this as a new harness script under `scripts/web/` (e.g.
`_w8-songload-verify.mjs`, leading-underscore convention like the other `_w*`
scripts) reusing `core.mjs`. **Targets: bohemianrhapsody, freebird, rehab, +1
(e.g. crazytrain or smokeonthewater)** per the item; add 1-2 alphabet-spread songs
for breadth. This is a test artifact, not a port change.

### Step 3 — ONLY IF the live run reveals a web-specific break
Contingency edits, in priority order (none predicted):
- **A 404 / wrong-path on a `.mid`** → fix path resolution in `server.py`
  `_serve_asset_file` (or add a fallback root). Lowest-likelihood: the live probe
  already shows 200 + correct bytes.
- **A compression-corruption symptom** (chart parses garbage only over the wire) →
  move `.mid` from `COMPRESSIBLE_EXTS` to `INCOMPRESSIBLE_EXTS` in `server.py`
  (`110`/`112`). Predicted unnecessary — br round-trip is byte-identical and
  fetch() decodes transparently.
- **A lazy-fetch stall** (chart never lands; WebPendingFile spins) → inspect the
  manifest oracle / `WebAssetsEnsureStatus` flow in `WebAssets.cpp` +
  `native_file.cpp`. The async path already has a sync fallback, so a hard stall
  would be a genuine engine bug; if found, fix in the **paired engine worktree**
  only and bump `MILO_ENGINE_PIN`.
- **A mogg/audio-only failure** (highway renders, no audio) → the `.mogg` Range
  path (`WebRangeFile`, `native_file.cpp:1374`); moggs are also symlinks and serve
  via `/api/file` Range. Treat as a separate audio-delivery sub-issue if it occurs.

All Step-3 edits are server.py (Python, not match-relevant) or `native/src/` /
engine-worktree (`#ifdef`/native-only). No shared Wii `src/` change is anticipated;
if one were ever needed it would have to be `#ifdef HX_NATIVE` + rebuilt-`.o` +
objdiff-confirmed byte-identical.

---

## Match-neutrality

- **No Wii decomp impact.** server.py and the Playwright harness are not compiled
  into the Wii binary. The `SongParser` fix is already landed (`e83e2c79`,
  `#ifdef HX_NATIVE`, independently verified byte-identical in
  `verify-chart-wiring.md` §(d): fuzzy 90.77109 unchanged). This task adds no new
  `src/` edit in the expected path.
- If the Step-3 contingency requires an engine fix, edit ONLY the paired engine
  worktree (`<wt>/.engine-path`) and bump `MILO_ENGINE_PIN` — never the engine repo
  directly. Engine code is not part of the Wii match surface.

---

## Per-symptom VERIFICATION plan (implementer + reviewer)

Run from repo root. Use a server port in YOUR assigned range; kill ONLY your own
rb3-native/server PIDs. Evidence lean, under `/tmp/rp8-web-songload/`.

| Symptom to confirm | Procedure | PASS criteria |
|---|---|---|
| **Rebuild contains the fix** | `scripts/web/build.sh --debug`; check `native/web/build/debug/rb3-web.wasm` mtime is NEWER than `e83e2c79` (2026-06-16 22:25). | wasm rebuilt; build green (the `undefined symbol` store/net stubs are expected, not errors). |
| **Server serves the chart** | `python3 native/web/server.py --port <P>`; `curl -s -D - -o /tmp/.../x.mid http://localhost:<P>/api/file/songs/bohemianrhapsody/bohemianrhapsody.mid` | HTTP 200, `Content-Length: 165422`, body starts `MThd`. (Pre-confirmed live this session.) |
| **Compression transparent** | `curl -H 'Accept-Encoding: br' …x.mid`; `brotli -d` the body; `cmp` vs identity. | `Content-Encoding: br`; decoded `cmp`-IDENTICAL to identity body. (Pre-confirmed.) |
| **Manifest oracle has mids** | `curl -s http://localhost:<P>/api/manifest \| python3 -c '…count .mid…'` | `.mid` count = 83, sizes match symlink targets. (Pre-confirmed.) |
| **bohemianrhapsody reaches gameplay (browser)** | Harness: `navigateTo(SCREENS.SONG_SELECT)`, scroll `highlightedSong=='bohemianrhapsody'`, select, guitar/expert/nofail/autohit, wait `game_screen`. | `engineState.screen === 'game_screen'`, `frame` advancing; canvas screenshot shows highway + 5 smashers + HUD; console has NO `WebAssets: FAILED`/`404`/`abort`/`SIGSEGV`; no hold at `tv3_b_screen`. |
| **freebird reaches gameplay** | same, target `freebird` | same; freebird has guitar gems early (native showed score climbing), so expect visible gems + non-silent audio. |
| **rehab reaches gameplay** | same, target `rehab` | same. |
| **+1 (crazytrain or smokeonthewater) reaches gameplay** | same | same. |
| **No regression: a dev song still plays** | same, target `20thcenturyboy` | reaches `game_screen`, no crash. |
| **No new crash class** | grep ALL captured browser-console logs across the run | zero `SIGSEGV\|abort\|RuntimeError\|MILO_FAIL\|WebAssets: FAILED\|404` on a `.mid`/`.mogg`. |

**Reviewer (independent) additions:** pick 2-3 songs the implementer did NOT test
(sweep the alphabet), run the same harness against the implementer's deployed
build, and confirm gameplay + a clean console. Confirm the deployed wasm mtime is
post-`e83e2c79` (guards against an accidental stale-build false pass). If any
server.py change was made, re-run the `curl` chart + compression checks against the
edited server.

---

## Risk

- **LOW overall.** Every server-side link is pre-verified live this session; the
  parser fix is committed and match-neutral; the only moving part is a rebuild +
  browser drive, both well-trodden (the web port has shipped through W5 and the
  song-preview-audio harness already navigates song select in-browser).
- **Residual unknowns the live run will close:**
  - *Browser name-targeted selection robustness* — the song-select scroll may need
    a press-count cap and ArrowUp wrap; `highlightedSong` polling de-risks it.
  - *First-visit lazy-fetch latency for a big chart over the wire* — charts are
    tiny (38.6 MB total; freebird 517 KB) and br-compress to ~8%, so the
    JSPI-suspending fetch is sub-second; not expected to stall, but watch for a
    WebPendingFile spin (the sync fallback backstops it).
  - *Audio over the wire* — gameplay also Range-streams the `.mogg` (41-59 MB);
    that path is W4/W5-shipped, but a previously-dead song exercises it on a fresh
    mogg. If audio is silent while the highway renders, that's a separate
    mogg-delivery sub-issue, not a chart problem.
- **Not a blocker for the campaign** (this is the non-blocking wrap-up); native is
  already proven. If the rebuild+verify passes (expected), the item closes with no
  code change beyond the new test harness script.

---

## Scope decision

**tractable = `yes`** — implement fully: rebuild the debug web build, author the
name-targeted in-browser verification harness, run it against bohemianrhapsody /
freebird / rehab / +1 (plus a dev-song no-regression check), and close the item.
Apply a server.py/engine contingency edit only if the live run surfaces one (none
predicted). `needsEngine` is **false** unless the live run reveals a lazy-fetch
stall traceable to `WebAssets.cpp` (low likelihood).
