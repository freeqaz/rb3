# Wave 6 Re-baseline — incremental-load perf on current master (2026-06-23)

**Purpose:** the incload thread paused at Wave 5 on 2026-06-11; the repo moved on
12 days (engine pin `8fb669d` → `20dba552`, a "framestall"/render-polish thread
landed a venue-aware GPU prewarm). This re-measures the gates on current master to
confirm (a) whether the framestall prewarm actually cut the game_screen
first-frame hitch Wave 5 left open, and (b) that the Wave-5 byte/journey numbers
still hold.

**Provenance:** measured by a background agent that built+prewarmed current master
(web deploy 2026-06-23 02:01, master ≈ `33434ce6`, engine pin `20dba552`) and ran
the A/B + netmatrix passes, then died on a server-side rate-limit before writing
this doc. Numbers below are transcribed from the salvaged artifacts in
`/tmp/rb3perf-w6/` (`ff-{on,off}-r{1,2}/result.json`, `nat-{on,off}{,2}.jsonl.reveal.json`,
`nm-c{1,4}/result.json`). STEP 4 (L1 steady-state) did not complete — see below.

---

## 1. HEADLINE — game_screen first-frame: FIXED on current master (−77% web)

Clean A/B on the same build: warm **ON** (default) vs **OFF**
(`RB3_GAMEWARM_OFF=1` + `RB3_TEX_PREWARM_OFF=1`). The reveal frame is the
game_screen ENTER frame (via frame-trace, not a screenshot — the ~25 s intro
cinematic runs at songMs=0).

### Web (release)

| Arm | reveal dt | lpu (sync drain) | objMs | texMs / texN | meshMs |
|---|---|---|---|---|---|
| **Warm ON (default)** | **95.7 / 97.5 ms** | 53.5 / 52.6 | 44.1 | 24.6 / 89 | 1.5 |
| Warm OFF | 419.5 / 406.5 ms | 371.9 / 363.8 | — | — / 97 | — |

**Reveal frame ~410 ms → ~96 ms (−77%).** The win is the **sync drain (`lpu`)
collapsing 368 ms → 53 ms** — exactly the reveal-drain the venue-aware prewarm
targets. `texN` drops 97 → 89 (venue textures pre-uploaded during the dwell). The
ON arm **meets the Wave-5 plan's 120 ms target** (research/09).

### Native (noisier; sync I/O makes the drain cheaper, so the gap is smaller)

| Arm | reveal dt | lpu |
|---|---|---|
| Warm ON | 97.7 / 125.4 ms | 53.8 / 66.4 |
| Warm OFF | 139.9 / 125.5 ms | 76.9 / 67.3 |

### Verdict

The Wave-5 first-frame miss (the deferred **L2 venue warm-sweep**, which shipped a
default-OFF Enter-state no-op) is **effectively resolved** by the framestall
thread's venue-aware prewarm (`86312dcf`: `ComputeVenueMiloPath()` now mirrors
`BandDirector::EnterVenue`'s resolution — override → World `venue` prop →
`small_club_01` — instead of reading the never-loaded `MetaPerformer::GetVenue()`).
This is a large, real win on web, where the sync fetch-drain is expensive.

**Gate maintenance needed:** `scripts/web/firstframe-gate.mjs` still asserts the
stale Wave-5 baseline band `[400, 1200] ms` and therefore mislabels the fixed ON
arm (95.7 ms) as `FAIL (baseline)`. The OFF arm (~410 ms) is what now sits in that
band. Update the gate to assert the ON target (`revealDtMaxMs: 120`, already in
`result.json.targets`) as the PASS condition and treat OFF as the self-test.

---

## 2. Byte / journey gates — Wave-5 numbers HOLD (no regression)

### c4 — 4 Mbps / 150 ms (primary), cold IDB

| Metric | Wave 5 | Current master | Verdict |
|---|---|---|---|
| Wire total | 115.5 MB | **114.9 MB** (114,931,085 B) | holds |
| game_screen reached | 248 s | **251.1 s** | holds (noise) |
| milo bytes | 75 MB | 75.0 MB (109 req) | holds |
| mogg bytes | 14.7 MB | 14.8 MB (15 Range req) | holds |
| bundle bytes | 15 MB | 15.0 MB (4 req) | holds |
| SFX ogg + misc | ~10 MB | ~10.1 MB (total − milo − mogg − bundle) | holds (ogg ≈ 8.5 MB) |
| Cold hovers (×3) | frozen 0 | **frozen 0, over100 0** (longest 66–68 ms) | clean |
| splash→hub | — | longest 75 ms, over100 0 | clean |

### c1 — 20 Mbps / 40 ms (backstop), cold IDB

appBooted 19.4 s, game_screen @ 88.6 s; splash→hub longest **63.4 ms, over100 0**;
canvas never frozen. No regression. (Raw `totalBytes` reads higher than c4 because
this run issued more requests / different hover set — the c4 census is the
canonical Wave-5-methodology number.)

**Verdict:** the Wave-5 bytes wave holds on current master — −36% vs the
pre-Wave-5 ~181 MB baseline is intact, and the canvas stays freeze-free at 4 and
20 Mbps.

---

## 3. Not completed / carried forward

- **L1 steady-state gameplay win (Wave-5 claimed web p50 18.99 → <17 ms).** The
  agent built `native-steady.py` but died before producing a result. **Still
  UNVERIFIED.** Re-run: frame-trace ~10 s of gameplay (songMs>0, scrolling track)
  with `RB3_UNPACK_CACHE_OFF=1` vs default, compare p50/p95 + `unpackMs`/`unpackN`.
- **1.5 Mbps/300 ms** not retried this pass (Wave 5: reaches part_difficulty,
  nav-cadence-bound short of game_screen). Throughput-bound; only A4 content
  downscale moves it.

## 4. Open levers after this re-baseline

1. **A4 milo-aware texture downscale** — the only remaining 1.5 Mbps lever.
   Feasibility (Opus, 2026-06-23): textures live INSIDE the milos (separate
   texture files = 0.2 MB); journey venue `small_club_01.milo_xbox` = 18.5 MB raw
   / ≈11.7 MB q11 wire. Needs a planned wave (RndTex downscale + visual quality
   gate), not a freelance. See `SESSION_HANDOFF_2026-06-11.md` §3.
2. **L1 steady-state verification** (cheap, above).
3. **firstframe-gate.mjs band update** (cheap gate hygiene, §1).
