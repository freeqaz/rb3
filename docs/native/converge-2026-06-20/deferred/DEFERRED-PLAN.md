# Deferred convergence backlog — REFRESHED RANKED PLAN

**Synthesis agent (Opus). Research-only — no code/engine changes, nothing committed.**
Engine pin `20dba55` (all landed convergence fixes: STEP 1 GX falloff `a360e3c`,
GAP B(a) crowd-dim `ada6e56`, GAP A1 watermark `b8f3cfa`). STEP 2 (`bae1aae`, tag
`converge-step2-crowd-wip`) confirmed **NOT** in the pin (HELD). Binary rebuilt clean
(`ninja: no work to do`).

Inputs: the three measured deferred docs in this dir —
[`festival-screenmask.md`](festival-screenmask.md),
[`char-rebake-scope.md`](char-rebake-scope.md),
[`step2-reeval-exposure.md`](step2-reeval-exposure.md).

---

## TL;DR — the backlog is mostly DONE or unfixable-without-C8. Exactly ONE thing to land.

The deferred backlog re-assessed cleanly: **4 of 5 items are ACCEPT or CLOSE_OBSOLETE**,
and only **1 is a real, visible, tractable bug worth implementing** — the festival
`*_screenmask` white-blank, via a ~10-line RB3-only fallback. Everything else is either
already fixed by a landed fix, off-frame and unfixable without the C8 pose-pipeline
root-cause, or made redundant/conflicting by GAP B(a).

| # | Item | Verdict | Visible | Effort | One-liner |
|---|---|---|---|---|---|
| 1 | **Festival `*_screenmask` white blank** | **IMPLEMENT** | **visible** | **small** | Real bug: unpainted movie-RT → 1×1 white fallback blanks the festival crowd shots. Option-A empty-RT skip is ~10 lines, RB3-only, DC3-safe. |
| 2 | Footwear `_skin.2` thin-skin fling | **ACCEPT** | off-frame | medium | Real geo but transient (~3% frames, ~1-in-4 boots), off-frame in 100% of closeups; a rigid anchor makes it a WORSE rotation-flung slab (C8). Required gate ("drop ratio under cap") is unmeetable. |
| 3 | Crowd/extras servo shards | **ACCEPT** | invisible | medium | Premise was wrong — crowd bodies ALREADY rebaked (`RebindCrowdCharBonesToOwnSkeleton`). Residual = 3 tiny accessory meshes on ≤3 of ~292 instances, masked/distant, dropping <2%. Fix = whole-292 blast radius for zero visible gain. |
| 4 | STEP 2 impostor-cam env gate | **CLOSE_OBSOLETE** | n/a | — | B(a) alone fixes the visible big_club white crowd (8–9% → 0% white). STEP 2 doesn't move the visible crowd (already measured) and would DOUBLE-DIM → near-black if stacked on B(a). |
| 5 | STEP 1 arena/venue exposure | **ACCEPT** | visible (landed) | — | GX falloff is a clear landed win (near-black silhouette → readable spotlit band, closeup black% 62→24). `sVenuePointExposure=0.70` is moderate/not-flooded. No tuning needed; `RB3_VENUE_POINT_EXPOSURE` is the future knob. |

**Net: implement item 1; accept items 2/3/5; close item 4.** A short IMPLEMENT list is
the correct outcome here — the convergence campaign has already harvested the
high-value visible wins, and the residual backlog is genuinely either invisible,
already-handled, or C8-blocked.

---

## Per-item rationale (condensed; full measurements in the source docs)

### 1 — Festival `*_screenmask` white blank → IMPLEMENT (visible / small)

`coop_crowd_mass*_screenmask` renders a flat near-white field (mean luma ~200,
white% ~83) over **all** frames (not a 1-frame flash), hiding the mass crowd, while the
venue's DIRECT crowd shots (`coop_dir_crowd00`/`crowdb`, luma ~65, white% 0.4) render the
intended B/W comic-poster backdrop fine. **Root cause is NOT the original GAP-4
blend/alpha hypothesis** — the blend math is fine. The screenmask material's diffuse
(`crowd_mass.tex`) is a **render target** painted only by a `TexMovie` Bink movie
(`crowd_mass*.tmov`, inline in the milo). On `HX_NATIVE` there is no in-world movie
decoder (the native shim handles fullscreen cinematics only), so the RT is never painted,
`BandRnd::DrawRect` hits its `if (!hasTex) texView = mWhiteView` 1×1-white fallback
(`Rnd_Wgpu_RB3.cpp:3326`), modulated by the screenmask's default-white `mColor` → full
screen white. Asset proves intent (a movie-fed crowd backdrop, not an authored white
quad). **Verdict: BUG**, visible whenever the auto-director picks a `*_screenmask` shot
(10+ authored). Option A (skip the unpainted-RT screenmask quad instead of blitting white)
is ~10 lines in an RB3-only TU. This is the only deferred item that is both visible AND
tractable, so it leads the IMPLEMENT plan below.

### 2 — Footwear `_skin.2` fling → ACCEPT (off-frame / medium)

Measured 4 wardrobe-random boots: 3/4 roll UNDER the band cap and render fine; 1/4
(saddleshoe) crosses it (ratio 4.78, 407 drops) but only on the worst ~3% of ankle-curl
frames (bind extent 11u, world p90 36u, well under the 110u "real tear" floor) — a
small-bind-garment false-positive the V24 guard anticipates. **Off-frame in 100% of
closeups** (small_club frames chest/guitar-up; feet never in shot). A rigid ankle anchor
was effectively already tried+rejected: `RB3_GUARD_EXEMPT_REBOUND` proved anchoring
translation nulls the origin offset but the **native rotation-basis divergence (C8)
remains** → far-from-bone verts smear to 200–460u and draw as full-screen slabs. The
followup's mandatory gate ("MUST drop max_band_ratio under cap") **cannot be met** without
C8. Correctly handled by the existing drop-when-broken guard. Reopen only with the whole
thin-geo family when C8 lands.

### 3 — Crowd/extras servo shards → ACCEPT (invisible / medium)

The followup premise ("no non-Band rest-rebake hook exists") is **false**:
`Crowd.cpp:911 RebindCrowdCharBonesToOwnSkeleton(Character*)` is exactly that hook and
already ships — it fixes the crowd **bodies** (ratio <2.0, zero drops across all boots).
Residual = only 3 tiny accessory meshes (`male_extras_eyebrows11`/`hair02` on the
separate `extras.fm` path, `clap` on the crowd path) on ≤3 of ~292 instances, masked,
distant, GAP-B(a)-dimmed, dropping <2% of their draws. A new extras-path rebake is a
whole-292-instance blast radius for zero visible gain, and `clap` shares item 2's
unfixable C8 rotation fling. **Doc fix worth doing (non-code):** correct the false "no
hook exists" claim in `NATIVE_FOLLOWUPS.md` / GAP 6 — see the coordinator note below.

### 4 — STEP 2 impostor-cam env gate → CLOSE_OBSOLETE

Fresh A/B on big_club_01: B(a) ON lands the crowd at small_club's correct ~0% white
(crowdL 0.3 / crowdR 0.0); B(a) OFF shows the stark white cut-out figures (7–9% white) =
the original bug. **B(a) alone fixes the visible crowd.** STEP 2 and B(a) act on
different draws in the SAME serial pipeline (STEP 2 dims the off-screen impostor-RT
*bake*; B(a) dims the final composited *billboard* ×0.10) so they **multiply** — and
B(a)'s 0.10 was explicitly calibrated assuming a near-WHITE bake, so STEP 2 + B(a) →
double-dim → near-black crowd. Moreover STEP 2's own verify already measured it does NOT
move the visible crowd (the visible path is the world.cam billboard, not the impostor
cam). So STEP 2 is redundant, conflicting if stacked, and venue-non-additive. **Action:
do not push/pin `bae1aae`; mark the tag superseded.** Keep `RB3_CROWD_DIM_OFF=1` /
`RB3_CROWD_DIM=<f>` as the lever. (Honest caveat: STEP 2's structural insight — the
unnamed impostor cam misses the world.cam venue gate → white-default — is correct and
explains WHY the bake is near-white; if a future need to truly light the impostor RT
arises, revisit it, but then B(a) must be REPLACED, not stacked.)

### 5 — STEP 1 arena/venue exposure → ACCEPT (visible / landed)

A/B on arena_02: GX falloff ON (default) lifts the band closeup from near-black
silhouette (LEGACY black% 53–62) to readable spotlit (black% ~24, region luma 31→61),
moody not flooded. `sVenuePointExposure=0.70` lands moderate; the wide shot stays ~51.
Retail ground-truth (lit performers in `images/retail-screenshots/`) + RB3 Deluxe's
"restored per-pixel lighting" note confirm RB3 venues had a readable spotlit band, not a
flat-dark fallback → GX ON is directionally correct. **No tuning pass needed.**
`RB3_VENUE_POINT_EXPOSURE=<f>` (default 0.70) + `RB3_VENUE_GREY_KEY` are the runtime knobs
if a future retail arena/big_club frame surfaces. (Honest caveat: STEP 1's blast radius is
venue-WIDE, not arena-only — it brightens any point-lit env; consistent with band-lit
intent, masked where directional dominates, not a regression. A per-venue tune would need
per-env exposure scoping — out of scope for "is it fine?": it is.)

---

## ORDERED IMPLEMENTATION PLAN (IMPLEMENT items only)

Only **one** item qualifies. (Plus one zero-code doc correction, listed second because it
is trivial and prevents a future agent re-chasing a non-bug.)

### 1. Festival `*_screenmask` empty-RT fallback (Option A) — visible, small, DC3-safe

**Goal:** stop the full-screen white that blanks the festival mass-crowd shots; let the
band + the festival's own world geometry/lighting show through (far closer to retail than
white).

**Concrete fix (engine — RB3-only TU, DC3-safe by construction):**
In `BandRnd::DrawRect`, `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`
(~L3316–3326, the `if (!hasTex) texView = mWhiteView;` white-fallback site):

- When `mat`'s diffuse is **non-null but resolves to no GPU view** AND the diffuse is an
  **unpainted render target** (`diffuse->IsRenderTarget()` and never went through
  `BeginDrawTarget`), `return;` early — **skip the quad** instead of blitting `mWhiteView`.
- Gate `RB3_SCREENMASK_FIX` (default-ON) with opt-out `RB3_SCREENMASK_FALLBACK_OFF=1`.

**Why DC3-safe:** `Rnd_Wgpu_RB3.cpp` is the RB3-only backend TU; DC3 never compiles it. No
shared shader/struct/uniform touched. The early-return triggers ONLY for the
unpainted-RT-diffuse case — the sky-dome `clouds_rnd.tex` RT path IS painted by
`BeginDrawTarget`, so it has a view and is unaffected.

**Verification (must-pass A/B, `/tmp/bch_override.py` = `band-closeup-capture.py` + venue
override, deterministic pins):**
```
SHARD_DBG=1 MILO_HEADLESS=1 python3 /tmp/bch_override.py \
  --override festival_01 --song-downs 0 \
  --shots "coop_crowd_mass01_screenmask,coop_crowd_mass_screenmask,coop_dir_crowd00,coop_dir_crowdb" \
  --frames 2 --frame-dt 600 --out /tmp/fest_smfix --tag smfix
```
- PASS: `coop_crowd_mass*_screenmask` mean luma drops from ~200 into the venue's normal
  ~60–80 range; band/world visible through the (now-skipped) screenmask; white% << 83.
- MUST-NOT-BREAK: `coop_dir_crowd00`/`crowdb` (no screenmask) stay byte-identical (still
  render the B/W comic poster, luma ~65, white% ~0.4).
- DC3 regression guard: build DC3 + a club/no-screenmask RB3 venue, confirm no other
  venue's RT-backed surface (sky dome) regressed (`RB3_RENDER_DBG`).

**Effort:** ~10 lines, one TU, one engine commit + `MILO_ENGINE_PIN` bump in
`native/CMakeLists.txt`. **Do NOT close GAP 4** on landing the engine fix is the faithful
movie path (Option B); Option A converts the artifact from "jarring full white" to
"acceptable band-through-world," which is the deferred-batch win.

**Explicitly NOT in scope (Option B, defer):** a real native `TexMovie` decoder. The movie
bytes are inline in the milo (no `.bik`/`.webm` sidecar — the intro `<video>`-overlay
trick does NOT apply), so it needs an in-engine Bink/transcoded decoder writing a WebGPU
RT + wiring `RB3MovieNativeBegin`/`IsOpen`/`Draw` for non-cinematic TexMovies + a real
`RndTex::MakeDrawTarget`/`FinishDrawTarget` on the RB3 backend (currently no-op). Large;
pays off only festival screenmask shots. Separate lower-priority "native in-world movie"
feature.

### 2. (Zero-code) Doc correction — GAP 6 "no non-Band rebake hook exists" is FALSE

In `docs/native/NATIVE_FOLLOWUPS.md` (GAP 6) and the audit `RANKED-GAPS.md` GAP 6 note,
correct the claim that no non-Band Character rest-rebake hook exists:
`src/system/world/Crowd.cpp:911 RebindCrowdCharBonesToOwnSkeleton` IS that hook and already
ships (it fixes crowd bodies). Mark items 2/3 ACCEPT-as-dropped (C8-blocked) and item 4
CLOSE_OBSOLETE (superseded by B(a)). This prevents a future agent re-chasing a fixed/non-
fixable item. Pure doc edit, no build.

---

## What NOT to do (so a later wave doesn't re-litigate)

- **Do NOT land a footwear or extras rigid rebake.** Both reintroduce the C8 rotation-basis
  fling as a full-screen slab; the guard's drop-when-broken is the correct behavior until
  C8 (pose-pipeline rotation basis) is solved. When C8 lands, reopen footwear + hands +
  fingernails + gloves + clap as ONE thin-geo batch, not one-offs.
- **Do NOT push/pin STEP 2 `bae1aae`.** It is superseded by B(a) and double-dims if stacked.
- **Do NOT run an arena exposure tuning pass.** `sVenuePointExposure=0.70` is moderate and
  retail-consistent; the knob exists (`RB3_VENUE_POINT_EXPOSURE`) for a future ground-truth
  frame, but no change is warranted now.

## Evidence index
- Festival: `deferred/festival-screenmask.md` + `deferred/shots/festival_{screenmask_WHITE,dir_crowd00_INTENDED,dir_crowdb_INTENDED}.png`
- Char rebake: `deferred/char-rebake-scope.md` (4-boot SHARD_RATIO measurements)
- STEP 2 / exposure: `deferred/step2-reeval-exposure.md` (`/tmp/converge_{ba,s1,fest}/` A/B captures)
- Engine sites: `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` — DrawRect white fallback `3326`, B(a) `5513–5584`, STEP 1 falloff `1144–1158`/exposure `1350–1359`
- Crowd pipeline: `src/system/world/Crowd.cpp:547–581` (RT bake → billboard composite), rebind `911`
