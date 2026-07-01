# Independent verify: `crowd-venues` (wrap-up) — VERDICT: CONFIRM_WITH_RESIDUALS

Independent reviewer (Opus adversarial), 2026-06-19. Reviewed the implementer's
result for the deferred crowd-venues wrap-up: **Fix C** (venue bridge honors
MetaPerformer override) SHIPPED + **Fix B** (2D imposter crowd) PLAN-ONLY.

- **Worktree reviewed:** `/home/free/code/milohax/rb3/.claude/worktrees/task-crowd-venues`
  @ `1859def9` (rb3 branch `wt-task-crowd-venues`).
- **Engine:** clean at pin `15ce606` (no engine change — confirmed: engine worktree
  `git status` empty, HEAD == `15ce606`). No `MILO_ENGINE_PIN` bump. Matches the
  impl's claim.
- **Built it myself** in the worktree (cmake clang + the worktree's own
  `.engine-path`): `rb3-native` links clean.
- **Ran it myself** on reviewer ports 9838-9841. Evidence under
  `/tmp/rp8rev-crowd-venues/` (my own captures; the impl's `/tmp/rp8-crowd-venues/`
  screenshots were NOT trusted).

## Verdict: CONFIRM_WITH_RESIDUALS

Fix C does exactly what it claims, is byte-identical on Wii, regresses nothing
adjacent, and arena loads cleanly + visibly. Fix B is a sound, accurately-grounded,
correctly-scoped plan-only deferral. ONE residual: Fix C *unlocks* other venues,
and at least `festival_01` SIGSEGVs natively during load — a downstream
venue-asset/native-render gap that is firmly Fix-B-and-beyond territory (NOT a Fix C
logic bug), but should be tracked.

---

## Fix C — INDEPENDENTLY VERIFIED

### 1. Wii byte-identity (re-verified myself)

Built `BandDirector.o` and ran objdiff on `EnterVenue__12BandDirectorFv` in the
worktree's own objdiff infra:

```
symbol: EnterVenue__12BandDirectorFv   target_size: 520  base_size: 520
fuzzy=100.0  normalized=100.0  raw=100.0   diff_score: 0 / 13000
```

(`/tmp/rp8rev-crowd-venues/entervenue_diff.json`.) Confirmed by code read: the
entire change is inside the existing `#ifdef HX_NATIVE` block (BandDirector.cpp
opened at :15, closed at :43; the two new `#include`s `obj/DataUtl.h` + `obj/Msg.h`
sit at :23-24 INSIDE that block; the venue-symbol resolution is inside the existing
`if (!mVenue.Dir() && GetWorld())` native force-load block at ~:632-665). Wii
compilation sees zero new lines. The impl's "520==520, diff 0" is accurate.
(Note: symbol is `__12BandDirectorFv`, not `__13` — confirmed against
`symbols.txt` / the map.)

### 2. Default (no override) unchanged — byte-for-byte path (port 9838)

```
[rev] NO override; current='no_venue_override'
VENUE_DBG: EnterVenue force-loading venue='small_club_01'
VENUE_DBG: ... venueName='small_club_01'   LoadCharacters('small_club_01')
```

NO "honoring" line emitted. Reached `game_screen`, song played (`is_playing=1`),
zero crashes. The handler returns the `no_venue_override` sentinel → the
`if (venueSym.Null())` fallback runs → the original small_club_01 path. Confirmed.

### 3. Arena override honored — a non-small_club venue loads (port 9839, re-run 9841)

```
[rev] override before='no_venue_override' set='0' after='arena_06'
VENUE_DBG: EnterVenue honoring MetaPerformer venue override='arena_06'
VENUE_DBG: EnterVenue force-loading venue='arena_06'
VENUE_DBG: ... venueDir=0x...  venueName='arena_06'  LoadCharacters('arena_06')
```

honoring-count=1, force-loading venue='arena_06', venueDir non-null, reached
`game_screen`, `is_playing=1`, ZERO crashes. **Reproducible** — a second arena_06
run on port 9841 was identically clean.

### 4. Visual — the different venue (MY OWN screenshots, the required deliverable)

- `/tmp/rp8rev-crowd-venues/baseline/{game_00,crowd_shot}.png` — small_club: dark
  intimate club, "CORK/CAIGH" signage, close pink/purple checkered walls.
- `/tmp/rp8rev-crowd-venues/arena/{game_00,crowd_shot}.png` — arena_06:
  unmistakably a larger arena bowl — overhead light banks/lamps, big-arena
  geometry, raised stage, distinct lighting. **Different venue confirmed.**

The white humanoid figures in the arena shots are the broken 2D-imposter crowd
(venue geometry + lighting render, but no proper camera-facing textured crowd rows)
— i.e. the exact Fix B symptom, as documented. NOT a Fix C defect.

### 5. Fix A small_club 3D crowd NOT regressed (port 9840, SHARD_DBG)

```
crowd_body SHARD_GUARD drops: 0          (Fix A target: ~0, NOT ~63k+)
crowd_body SHARD_RATIO sample: 0.94 / 1.02 / 1.18 / 1.13 / 1.08 / 1.08  (≈1.0)
crowd_body DROPs: 0
```

Crowd-body palettes are self-consistent (ratio ≈ 1.0); the pose-shard guard passes
the bodies and still functions (genuinely-torn meshes would still drop above
threshold — the guard logic is untouched, only the input is now clean). Fix A
(`dcad5834`) intact. Crowd.cpp is pristine in this branch (no Fix B probe residue).

### 6. Adjacency (menu / song_select)

Every run navigated splash → main_hub → song_select → part_difficulty → gameplay
cleanly. No adjacent-scene regression for this venue-only change.

---

## Fix B — PLAN-ONLY assessment: SOUND + accurately grounded

I independently re-verified every load-bearing source claim in the plan/impl:

- **`GetSharedTex` weak-stub returns null** — `native/src/band3_link_stubs.s:667-668`
  (`.weak _ZN6WiiRnd12GetSharedTex... .set ... __hmx_band3_noop_stub`). CONFIRMED.
- **`BeginDrawTarget` rejects w/h≤0 + bails on nested target** —
  `if (mRtActiveTex) return;` + `if (w <= 0 || h <= 0) return;`
  (engine `Rnd_Wgpu_RB3.cpp:1803-1815`). CONFIRMED — this is the real per-archetype
  RT close-cycle verification risk the plan flags.
- **Portable `RndMultiMesh::DrawShowing` has NO billboard** — just
  `mMesh->SetWorldXfm(it->mXfm)` (`src/system/rndobj/MultiMesh.cpp:162-168`).
  CONFIRMED — Gap 2 is real.
- **NEW de-risking claim (skinned-RTT pipeline variant already handled)** —
  VERIFIED at pin `15ce606`, `Rnd_Wgpu_RB3.cpp:~5384-5410`:
  `key.layout = skinned ? Skinned : Static;` then under
  `bool rtPass = (mRtActiveTex != nullptr); if (rtPass){ key.targetFormat=mRtFmt;
  key.hasDepth=false; } key.alphaWrite = rtPass;`. So a skinned char rendering into
  the RT *does* select a valid RT-compatible skinned pipeline variant — the scout's
  flagged "biggest unknown" (plan piece 4) is indeed already covered. This is a
  genuine, accurate finding that lowers Fix B risk.

The scope call (plan-only) is correct: Fix B is a multi-piece RTT + billboard
pipeline bring-up spanning `native/src` (new GetSharedTex) + `src/system/rndobj`
(billboard) + possibly engine (aspect) + an unverified multi-archetype shared-tex
mid-frame RT close-cycle, with **no retail ground-truth** to gate "the bowl crowd
renders correctly." Correctly deferred behind an opt-in env, default OFF. The exact
hooks + the attempt/stop gate are all specified. Not a contained one-liner — do NOT
force a half-feature. PLAN_ONLY for Fix B is the right answer.

---

## RESIDUAL (track, do not block)

**`festival_01` SIGSEGVs natively during song-load (before EnterVenue prints).**
With `{meta_performer set_venue_override festival_01}` the load path crashed:
`RB3 Native: caught SIGSEGV (signal 11) at 0x136`, a deep recursive DTA backtrace,
BEFORE any VENUE_DBG line — i.e. it dies in the festival venue's load/script flow,
not in the Fix C bridge. arena_06 is reproducibly clean (2 runs), so this is a
**festival-venue-specific native asset/render gap**, not a Fix C logic bug. Fix C
merely makes it *reachable* (without Fix C, festival_01 is ignored → small_club
loads safely). This is squarely Fix-B-and-beyond territory (per-venue native
bring-up). The impl only claimed/verified arena, which is honest; festival/big_club
stability is out of this wrap-up's scope but should be tracked for any future
venue-variety work. big_club not tested (time).

## Evidence
`/tmp/rp8rev-crowd-venues/` — `rev_venue_test.py` (my harness), `entervenue_diff.json`
(my objdiff), `rb3-rev-{9838,9839,9840,9841}.evidence.txt` (distilled logs),
`{baseline,arena}/{game_00,crowd_shot}.png` (my before/after).

## Notes for the orchestrator
- Fix C is landable as-is: `1859def9`, one file, HX_NATIVE-only, Wii byte-identical.
- Fix B stays plan-only; the impl's design doc + de-risking finding are accurate and
  ready for a future attempt-gated wave.
- Add festival/big_club native-load stability to the future Fix-B/venue-variety
  backlog (currently only arena is proven stable).
