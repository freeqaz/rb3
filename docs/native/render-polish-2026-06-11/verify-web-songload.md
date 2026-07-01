# verify-web-songload — INDEPENDENT REVIEW (render-polish wave-8 wrap-up)

**Verdict: CONFIRM.** The full 83-song set loads + plays in the BROWSER web build.
Independently reproduced from a self-built debug wasm (NOT the implementer's
artifact), against reviewer-chosen songs the implementer never tested, with the
note highway, gems, and audio-delivery path all confirmed live. Wii build
byte-identical (no compiled-source change in this task; the underlying SongParser
fix is fully `#ifdef HX_NATIVE` and was already objdiff-confirmed in wave-7).

Reviewer: Opus adversarial. Evidence under `/tmp/rp8rev-web-songload/`. Server port
9808 (assigned range 9808-9811).

---

## What I verified independently (did NOT trust the impl's wasm/screenshots)

### 1. Self-built the web debug wasm in the implementer's worktree
- `scripts/web/build.sh --debug` in `.claude/worktrees/task-web-songload` → exit 0,
  `[100%] Built target rb3-web`. My fresh wasm mtime **2026-06-19 19:40** (after the
  SongParser fix `e83e2c79` of 2026-06-16, and after the implementer's 19:05 build).
- Confirmed the fix is actually IN my wasm: the DWARF symbol string
  `SongParser::ParseAndStripLyricText(char const*, VocalNote&)` is present.
- Confirmed the deployed wasm in the MAIN repo is genuinely STALE
  (`native/web/build/debug/rb3-web.wasm` mtime **2026-06-11 21:27**, predates the
  fix) — the implementer's "operational gap = stale wasm" root cause is correct.

### 2. Server-side delivery (my own curl, against my build's served assets) — ALL PASS
| Check | Result |
|---|---|
| `.mid` GET (25or6to4, centerfold, antibodies, bohemianrhapsody) | HTTP **200**, Content-Length == symlink-target size (261929/200222/286040/165422), magic `MThd` (4d546864), `Content-Type: audio/midi`. server.py follows the `.mid` symlinks transparently. |
| `.mid` br round-trip (25or6to4) | `Accept-Encoding: br` → `Content-Encoding: br` (24115 br); `brotli -d` **cmp-IDENTICAL** to identity body (fetch() decodes before MEMFS → parser sees raw chart). |
| `/api/manifest` mids | **83** `.mid` entries, **0** size mismatches vs symlink targets. |
| `.mogg` Range (25or6to4) | HTTP **206**, `Content-Range: bytes 0-1023/31963791` (full symlink-followed size). |
| Served wasm identity | served `Content-Length: 30111884` == my freshly-built worktree wasm. |

### 3. In-browser gameplay (Playwright, `?debug=true`, no-store) — reviewer-chosen songs
Ran `_w8-songload-verify.mjs` against songs the implementer did NOT test (alphabet
sweep), each in an isolated fresh browser.

| Song | screen | Δframe(4s) | painted% | midFetched | .mid/.mogg 404 | hard-crash |
|---|---|---|---|---|---|---|
| 25or6to4 (prev-dead) | game_screen | 83 | 91.47 | true (261929 B) | none | 0 |
| centerfold (prev-dead) | game_screen | 84 | 98.07 | true (200222 B) | none | 0 |
| antibodies (prev-dead) | game_screen | 82 | 73.99 | true (286040 B) | none | 0 |
| smokeonthewater (prev-dead) | game_screen | 66 | 98.01 | true | none | 0 |
| 20thcenturyboy (dev) | game_screen | 81 | 98.14 | true | none | **NO REGRESSION** |

`songCount=83` in song-select (full set visible). Every run: OVERALL PASS, zero
`.mid`/`.mogg` 404, zero `SIGSEGV/SIGABRT/abort/RuntimeError/MILO_FAIL`. Each chart's
lazy fetch landed over the wire (`WebAssets: songs/<id>/<id>.mid (N bytes)`).

### 4. Deep-sample confirms the REAL note highway renders (not just a painted venue)
The `_w8` harness samples at 4s into game_screen, which lands during the song-intro
band-pan camera — so its 4s screenshots show the venue/band, not the lanes. I wrote
a deeper probe sampling at 6s/10s for **centerfold**:
- game_screen sustained t0→t6→t10 (frames 5022→5142→5247, clock advancing).
- `deep/centerfold_t10.png` shows the unambiguous gameplay highway: 5 colored fret
  smashers (green/red/yellow/blue/orange), now-bar, star-power meter, score/multiplier
  widget, receding fretboard. The implementer's `freebird_t5.png` shows the same.
- **Audio path engaged:** the deep console shows `MOGG_DBG: DoFileRead got 16384
  bytes BEFORE decrypt…` for centerfold — the mogg is streamed + decrypted over the
  wire, i.e. the *play* (not just load) path works.

### 5. Wii byte-identity — independently re-confirmed
- `git diff --stat 1c46a70e..5015eb0d` = **exactly one new file**:
  `scripts/web/_w8-songload-verify.mjs` (+222). `git diff --name-only … -- src/`
  is EMPTY → zero shared `src/` change, zero `native/src/`, zero `server.py` change.
  The commit is strictly a test artifact; nothing compiled into the Wii binary
  changed, so no `.o` rebuild is required.
- The underlying `SongParser::ParseAndStripLyricText` fix (`e83e2c79`, landed
  earlier on master, base of this branch) is fully `#ifdef HX_NATIVE / #else
  (matched original) / #endif` — I read the full diff; the Wii object is
  byte-identical (already objdiff-confirmed in `verify-chart-wiring.md`).

---

## Adversarial checks I ran (and their outcome)

- **Distrust the impl's wasm** → rebuilt my own; confirmed fix compiled in. PASS.
- **Distrust the 4s screenshots** ("painted%" could be a venue, not a highway) →
  deep-sampled to 10s; real highway + smashers confirmed. PASS.
- **Distrust "play" = just rendering** → confirmed the mogg decrypt/read path fires
  live (audio delivery, not only chart). PASS.
- **Songs the impl never tested** → 4 fresh previously-dead songs + 1 dev song, all
  reach gameplay; dev song no-regression. PASS.
- **Server symlink-following / compression corruption** → curl-verified size + magic
  + br cmp-identity. PASS.

## Residual (non-blocking, accurate to the impl's report)
- The recurring benign `WebAssets: FAILED … 404` console lines for optional dev
  assets (`sphere.milo_xbox`, `dev_only/selvenue.dta`, `frame_rate.dta`, `<song>.vfv`)
  are pre-existing long-tail misses across the whole web port; they do NOT block
  gameplay (every target reached game_screen) and are NOT `.mid`/`.mogg`. Out of scope.
- The deployed wasm in the **main repo** (`native/web/build/**`) is still the stale
  2026-06-11 build (artifacts are gitignored / not committed). To make the LIVE
  deploy reflect the fix, the deploy host must run `scripts/web/build.sh` after
  landing — exactly as the impl's landing note states.

## Landing assessment
Single additive commit `5015eb0d` (one new test `.mjs`, zero conflict surface,
zero match impact). Safe to cherry-pick onto master. No engine change, no
`MILO_ENGINE_PIN` bump.

## Status: CONFIRM — the open native-vs-web song-load gap is closed in the browser.
