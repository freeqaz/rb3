# C follow-ups — sharpen-fetch hardening + rb3-dta link fix

**Status:** plan authored by Fable 2026-07-02, immediately after C shipped default-ON
(research/13 §RESULTS, rb3 166f5356). Two follow-ups from the integration's honest
misses. Both scoped from live code reads (line numbers verified 2026-07-02).

## Lane A — rb3-dta link failure (pre-existing, telemetry wave)

`rb3-dta` fails to link: `Task.cpp:394-396` (`gRB3TraceFrame`, `RB3ReplayDtForFrame`),
`Task.cpp:43` (`RB3ReplayFixedClock`, `RB3ReplayActive`), `Debug.cpp:34,52`
(`gRB3TraceActive`, `RB3RecordLog`), `System.cpp:394,397` (`RB3ReplaySeed`,
`RB3TraceSetSeed`). The session-telemetry wave added HX_NATIVE hooks to those shared
TUs; the definitions live in `native/src/rb3_session_trace.cpp` + `rb3_replay.cpp`,
which rb3-dta (CMakeLists ~415: main_dta + DTA_FORK_SOURCES + DTA_LEXER +
NATIVE_SHIMS) does not compile. rb3-native/rb3-tests/rb3-web are unaffected.

**Fix preference order:** (1) add the real TUs to rb3-dta's source list IF they are
dependency-light (no HTTP server / GLFW / renderer pulls — verify includes); else
(2) extend the existing stub mechanism (`src/dta_link_stubs.s` is already in the
link line — a C++ stub TU is fine too) with inert definitions (trace inactive,
replay inactive, no-op log). MUST NOT weaken rb3-native/rb3-tests (their real TUs
stay). Gate: rb3-dta links AND still parses the 138 songs (its milestone-a check);
rb3-native + rb3-tests still build.

## Lane B — sharpen sidecar fetch: real yield-to-audio (research/13 risk §, review advisory #1)

**Today:** `rb3_texsharpen_native.cpp:156` kicks `WebAssetsEnsureResidentAsync(rel)`
→ one plain `emscripten_fetch` of the whole ~5.4MB sidecar, same lane as mogg Range
streaming. Empirically harmless (T2: 1.5Mbps ON=635 < OFF=861 underruns) but the
"low priority" comment is aspirational; on other links/timings the single ~29s
(at 1.5Mbps) transfer could compete with a mogg refill.

**Design (chunked, mogg-yielding, driver-paced):**
- Engine adds ONE accessor: `int WebAssetsRangeInFlightCount()` — count of
  `sRangeRequests` entries with `!done && !abandoned` (WebAssets.cpp registry,
  ~:750). Nothing else engine-side for the fetch.
- Driver (`rb3_texsharpen_native.cpp`, `#ifdef __EMSCRIPTEN__` arm) replaces the
  single EnsureResidentAsync with a per-frame chunk pump driven from the existing
  RB3TexSharpenPoll:
  - Chunk size default 256KB (`RB3_SHARPEN_CHUNK_KB`, 0 = legacy single-fetch).
  - At most ONE chunk in flight. Before kicking chunk N: if
    `WebAssetsRangeInFlightCount() > 0` (i.e. mogg is fetching — our own chunk is
    never in flight at check time), skip this frame — strict yield. Bounded
    interference = one 256KB chunk (~1.4s at 1.5Mbps) if mogg starts mid-chunk.
  - Total size: prefer `WebAssetsManifestSize(rel)` (manifest oracle, WebAssets.cpp
    :606). VERIFY the manifest actually lists `.sharpen` sidecars in the downscale
    tree; if absent, fall back to short-read EOF detection (request 256KB, got
    < requested ⇒ final chunk; a 416 on an exact-multiple boundary also terminates
    — treat 416-after-progress as EOF, real error otherwise) or fall back to the
    legacy single fetch. Do not guess: verify with the live server.
  - Assemble chunks in memory; on completion write the file to MEMFS at the same
    rel path (create parent dirs — reuse WebAssets' dir-ensure helper if exposed,
    else mkdir -p via stdio) so `WebAssetsIsResident` flips true and the existing
    RB3SharpenLoadSidecar path runs UNCHANGED.
  - `WebAssetsRangeDone` collect + detach per the native_file.cpp:996 caller
    pattern (see the detach comment at WebAssets.cpp:820 — don't leak registry
    entries).
- **Fold-in (review advisory #3):** RB3TexSharpen.cpp `RB3SharpenStep` currently
  marks `e.sharpened=true` even when `RB3SharpenReuploadTex` returns false
  (!mGpuReady) — change to: on recreate==false, do NOT mark done and do NOT count
  against the per-frame budget; retry next frame. Keep a small per-entry retry cap
  (~120 frames) so a permanently-not-ready state can't spin forever; on cap, mark
  done with a one-line RB3_SHARPEN_DBG note.
- Native (non-web) path unchanged (local file read).

**Gate (empirical, throttled web A/B — reuse `scripts/web/_sharpen_audio_throttle.mjs`):**
1.5Mbps sharpen-ON with chunked fetch: underruns must be ≤ the T2 single-fetch ON
baseline (635 events / 7.06% padded; OFF baseline was 861) AND sharpen must still
reach COMPLETE 15/15 (completion may take longer — log the sharpen-window length,
it is cosmetic). 4Mbps ON sanity (T2 baseline: 3 events). rb3-tests all green
(sharpen gtests exercise the manager, not the web fetch — must stay green).
Wii untouched (no matched-TU edits in this lane at all).

**Invariants:** engine web-only code `#ifdef __EMSCRIPTEN__` never HX_WEB; engine
change → single MILO_ENGINE_PIN bump at integration; generated sidecars/tree stay
gitignored; concurrent agents' uncommitted files (engine FxSendNative.cpp, rb3
main_web.cpp/server.py shell-SFX work) untouched.

## Execution

Ultracode wave, Opus implementers: [A ‖ B-impl] → B adversarial review → conditional
fix → single integrator (builds rb3-dta/rb3-native/rb3-tests/rb3-web, runs the
throttled A/B gate, pin bump, commits, updates research/13+14 RESULTS).

## RESULTS (INTEGRATED 2026-07-02 — hardening landed; chunked default NOT shipped)

**Verdict: LAND the hardening, ship `RB3_SHARPEN_CHUNK_KB` default = 0 (legacy single
fetch); chunked stays OPT-IN.** The chunked pump works exactly as designed (COMPLETE
15/15 at every throttle, byte-exact assembly, all safety arms exercised) but MEASURES
WORSE than the legacy fetch on the gate metric at 1.5 Mbps — the strict yield halves
peak audio starvation yet stretches the mogg-contention window ~2.2x, netting ~1.6x
more total padded quanta. Honest miss, honest default. Full detail:
`handoffs/14-integrator.md`; implementation: `handoffs/14-laneA-dta.md`,
`handoffs/14-laneB-throttle.md`; adversarial review (1 blocking finding B1, FIXED
`ecb1c8f7`; advisories A1-A7 open, non-blocking): `handoffs/14-laneB-review.md`.

- **Commits:** Lane A rb3 `0ea167ec` (rb3-dta link fix — real telemetry TUs + one weak
  stub). Lane B engine `3e02cea` (`WebAssetsRangeInFlightCount` + `RB3SharpenStep`
  retry-on-not-ready) + rb3 `9df9fd9b` (chunk pump driver) + `ecb1c8f7` (B1:
  Range-ignoring-server detection). Integration: pin requirement satisfied by
  `fadd179a` (a concurrent commit that moved `MILO_ENGINE_PIN` to `04c8e1c`, whose
  ancestry includes Lane B's `3e02cea` — same pattern as the C wave) + this wave's
  default flip to legacy (rb3_texsharpen_native.cpp).
- **Lane A gate:** rb3-dta links + parses the 138 songs (exit 0); rb3-native +
  rb3-tests unaffected.
- **Builds:** rb3-native clean; **rb3-tests 53/53** (incl. new `RetriesWhenGpuNotReady`
  + `RetryCapMarksDoneEventually`); rb3-web dual build deployed (re-built after the
  default flip).

### Throttled A/B (T2 protocol: u0 at game_screen+2.5 s, 20 s window; cold cache,
`RB3_WEB_DOWNSCALE=1`, 20thcenturyboy/small_club_01, release build, harness
`scripts/web/_sharpen_gate14.mjs`, runs serialized under flock; n=1 per arm)

| arm | 20 s window delta | full transfer phase | sharpen |
|---|---|---|---|
| 1.5 Mbps chunked (256 KB) | **1375 ev / 6920 q = 19.87%** | 20,657 ev / 71,795 q = **28.8%** over **206.7 s** | COMPLETE 15/15, 21 chunks = 5,374,546 B exact |
| 1.5 Mbps legacy (`=0`, same build, back-to-back) | **669 ev / 6920 q = 9.67%** | 13,023 ev / 32,524 q = **40.0%** over **92.9 s** | COMPLETE 15/15, single fetch |
| 4 Mbps chunked | **0 ev** (T2 baseline ~3) | 0 ev through COMPLETE | COMPLETE 15/15 at +48.7 s (vs ~11 s single) |
| unthrottled, `=0` (flag regression) | 4 ev (localhost noise) | — | COMPLETE 15/15, 0 chunk lines (legacy arm genuinely exercised) |
| unthrottled, NO env (shipped-default smoke, post-flip build) | 0 ev | — | COMPLETE 15/15 at +3.9 s, 0 chunk lines (default = legacy verified) |

- The same-day legacy control (669 / 9.67%) sits right in the T2-era band (ON=635 /
  7.06%, OFF=861), so conditions had NOT drifted: chunked genuinely measures ~2.05x
  worse on the gate window and ~1.6x worse on total padded quanta. Gate rule
  "chunked <= 635" therefore **FAILS**, and per the integration decision rule the
  default ships as legacy.
- WHY (measured, not speculated): peak starvation DID drop (28.8% vs 40.0% while
  transferring — the yield works), but at 1.5 Mbps the mogg re-opens a Range fetch
  almost immediately after each gap, so every 256 KB chunk still rides a contended
  wire, and one-chunk-per-frame pacing + per-chunk RTT stretch the transfer to
  206.7 s vs 92.9 s. Total events = rate x duration, and duration won.
- 4 Mbps: chunked is harmless (0 ev, == legacy-era 3-ev baseline) — the regression is
  specific to links where the mogg has no slack.
- The manifest still does not list sidecars (`manifest size -1` live) — short-read EOF
  was the terminator on every chunked run, as Lane B measured.
- **What still ships from Lane B regardless of the default:** engine
  `WebAssetsRangeInFlightCount()` accessor; `RB3SharpenStep` retry-on-not-ready (both
  paths); the legacy lane's `WebAssetsEnsureStatus==2` dead-fetch no-op (fixes
  poll-forever on sidecar-less venues); B1 Range-ignore detection + 64 MB assembly cap
  (chunked arm); the two new gtests; rb3-dta link fix (Lane A).
- **Re-open trigger:** chunked default becomes attractive again if the pump gains
  idle-gating by AUDIO RING DEPTH (only fetch when the worklet ring is comfortably
  full) or a real fetch-priority lane — either would cut the duration cost without
  the peak cost. Tracked as follow-up, not this wave.
