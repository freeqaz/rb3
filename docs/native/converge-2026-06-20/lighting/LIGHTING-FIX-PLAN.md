# LIGHTING-FIX-PLAN — arena-dark (GAP 2) + big_club-white-crowd (GAP 3)

**Synthesis agent (Opus). RESEARCH ONLY — synthesizes the three investigation docs in
this dir (`arena-dark.md`, `bigclub-white.md`, `ground-truth.md`). No native run, no
build, no commit. Every engine anchor below re-verified by reading the source at the
pinned engine SHA `5cbe8556` (== `MILO_ENGINE_PIN` in `native/CMakeLists.txt:74`).**

---

## 0. Verdict in one line

GAP 2 and GAP 3 are **TWO DISTINCT BUGS in the SAME design family** — "a character
draw is lit by the wrong source instead of its authored `RndEnviron::sCurrent`." They
do **not** share one root cause and **cannot** be closed by one edit. Both are genuine
divergences from RB3 intent (band must be lit, not black; crowd must be dim, not white)
per `ground-truth.md`. Fix them as **two ordered, independent engine changes behind one
pin bump**, each with its own venue A/B.

---

## 1. ROOT CAUSE — one family, two distinct triggers

The shared design issue: per-environ character lighting in the native renderer is built
around `world.cam`-gated venue uploads, and a hard distance falloff. Two different
characters hit two different failure points of that machinery.

### GAP 2 (arena band dark) — root cause: point-light falloff is a HARD CUTOFF
- The band IS drawn under `world.cam`, so it **does** get the venue-light path
  (`Rnd_Wgpu_RB3.cpp:1267` matches; `char.env` has 4 live white `*_silhouette.lit`
  type-0 point lights so the grey fallback at `:1337` does **not** fire — verified the
  selection loop `:1302-1336` uploads exactly those points).
- The band gets no key because the **shader** falloff at
  `standard_wgsl.inc:522-525` is `falloff = saturate(1 - d/range)²`, which is exactly
  0 at `d >= range`. arena_02's per-station spots are `range=55` but sit **70-103u**
  from the band roots (band-root probe in `arena-dark.md` §3) → falloff 0.00-0.02.
  The ground-truth Wii GX model (`src/system/rndwii/Lit.cpp:36-44`,
  `GXInitLightAttn(.., k0=1, k1=1/range, k2=0)` = `1/(1+d/range)`) is 0.50 at d=range
  with a long tail → the same spots deliver **0.59-0.86** white key.
- **Decisive probe:** `RB3_VENUE_LIGHT_OFF=1` ~doubles band-region luma (43.7→87.3,
  56.2→101.9) — proves the *venue path* delivers near-zero key, and the off-path
  (single white directional, no falloff) is brighter only because it sidesteps the
  broken curve. (`arena-dark.md` §1 A/B table.)
- **Why festival survives the same code:** festival's `char.env` adds bright
  *directionals* (`char_bounce` 0.5/1.0/0.64, `main_rear`, `sun` range=3000) which
  have **no distance falloff in either model** — so the broken point curve is masked.
  Arena's directional fill is authored near-black (`char_bounce`=(0,0.03,0.24)), so the
  band depends entirely on the tight point spots → it goes dark. (`ground-truth.md`
  §3a; `arena-dark.md` §4.)

### GAP 3 (big_club crowd white) — root cause: impostor-crowd cam is NOT `world.cam`
- The 3D crowd is re-rendered each frame into an off-screen RT through
  `gImpostorCamera`, which is **created unnamed** (`Crowd.cpp:88`,
  `cam->Name()==""`) and `Select()`-ed at `Crowd.cpp:550`.
- Both venue-light gates require the cam name to be exactly `"world.cam"`
  (`Rnd_Wgpu_RB3.cpp:1267` and the per-environ re-write at `:3505-3507`, both
  re-verified). The unnamed cam matches neither → falls through to the **default else
  branch** `:1352-1357`: one full-white directional `(1,1,1)` + `0.45` grey ambient.
  Baked into the crowd impostor texture → stark-white figures.
- The crowd's authored env **is** correctly `RndEnviron::sCurrent` during the draw
  (`Crowd.cpp:549` `RndEnvironTracker`), and it is dim-by-design
  (`RB3_crowd_mesh.env`: ambient 0.18, **zero lights**). If the path read it, the crowd
  would render dim (the correct small_club/retail look).
- **Decisive probe:** `RB3_VENUE_LIGHT_OFF=1` leaves crowd white% essentially
  unchanged (9.4→9.7, 14.4→18.3) while the venue *backdrop* luma triples — the crowd
  does **not** track the venue-light path at all (it's already on the default branch).
  `RB3_LIGHT_PROBE`: the `''` cam fires WriteSceneUniforms **16,063×** in big_club vs
  **90×** in small_club → big_club just exercises the impostor path far more heavily.
  (`bigclub-white.md` §1, §3, §4.)

### Why this is ONE family but TWO fixes
Both are "character lighting must follow `RndEnviron::sCurrent`, not a hardcoded
falloff radius / cam name." But the two characters break at different points:
- GAP 2: the venue path *runs* but the **falloff curve** extinguishes the only key.
  Fixing the cam gate does nothing here.
- GAP 3: the venue path is **skipped entirely** (wrong cam name). Fixing the falloff
  curve does nothing here (the crowd env has 0 lights — falloff never applies).

So: **distinct root causes, distinct fixes.** A claimed single-edit fix is wrong.

---

## 2. Ground-truth confirmation (both worth fixing)

From `ground-truth.md` (retail closeups + authored asset light data + festival control):
- **GAP 2 is genuine.** RB3's band is *always* lit and readable across venues
  (retail `auw78cT5J-g`/`Kgu03I32IAQ` closeups, in-repo club frames). A *pure-black,
  zero-surface-detail* performer is wrong. **Honesty caveat:** RB3 arenas ARE
  intentionally dark with spotlit bands — the target is **spotlit-dim with form**
  (skin/face/clothing readable, dark stage floor), NOT a flat flood. The fix must
  restore the *key spot reaching the performer*, not brighten the whole stage.
  Corroboration: RB3 Deluxe ships a separate opt-in "Black Venue" — a black stage is a
  special non-default look, so stock arenas are not black-band.
- **GAP 3 is genuine.** Retail audiences are never bright-white; intent is dim/dark and
  unobtrusive (small_club's in-engine crowd at white% ~0.0-0.1 is the proxy
  ground-truth). big_club's crowd at white% 9-18 is the brightest thing in frame.
- **Do NOT "fix":** festival's B&W comic crowd backdrop and video_01's white studio
  backdrop are authored-correct. GAP 3 is specifically the big_club **audience figures**.
- **No retail arena/big_club wide frame was locatable** (galleries 403'd). The band
  intent is venue-independent (same `*_silhouette.lit`+`char_bounce`/`rim` rig per
  venue), so the closeup ground-truth transfers — but see §5 deferral note.

---

## 3. FIX PLAN (ordered, implementable)

All engine edits are in `../milo-native-engine` (separate repo). Land engine first,
then **one** `MILO_ENGINE_PIN` bump in `rb3/native/CMakeLists.txt:74` for both fixes
(they're independent enough to A/B separately but cheap to ship together).

### STEP 1 — GAP 2: GX-faithful point-light falloff (highest confidence, 2 lines)
- **File:** `../milo-native-engine/src/gfx/standard_wgsl.inc:522-525`.
- **Change:** replace the squared-saturate hard cutoff with the GX inverse-linear law:
  ```wgsl
  // GX-faithful point falloff: GXInitLightAttn(.., k0=1, k1=1/range, k2=0) => 1/(1 + d/range).
  // Half-bright at d==range; long tail (matches Wii Lit.cpp:36-44), not a hard cutoff.
  let falloff = 1.0 / (1.0 + lightDistance / max(lightRange, 0.001));
  ```
  Delete the now-unused `rangeAttenuation`/squared lines (522-524).
- **Effect:** the range-55 silhouette spots now reach the band 70-100u away
  (~0.35-0.55 each) → real white key → spotlit-dim performer against a dark stage.
- **Blast radius — this is the riskiest part.** This shader path is shared by EVERY
  point light in EVERY scene (not venue-gated). The GX law never reaches 0 (0.10 at
  d=4·range), so very-distant points retain a tiny tail. Mitigations / guards:
  - The native upload caps at **4 point lights** (`Rnd_Wgpu_RB3.cpp:1325` `pl<4`), so
    bleed is bounded to 4 sources max — negligible in practice.
  - If a far point bleeds onto distant geometry, add a soft far-cut at e.g. `4·range`
    via a `smoothstep(4*range, 3*range, d)` window multiplied into `falloff` — do NOT
    revert to the squared cutoff.
  - Keep the existing `sVenuePointExposure()` (×0.70, `:1328`) lever. After the curve
    fix the band may read slightly hot — tune **exposure**, never re-cripple falloff.
  - `kFakeSpot` (GX type=2) gets a *steeper* GX curve + a spot cone; the native path
    treats all points identically. arena_02's spots are type=0 so inverse-linear is
    correct for THIS gap. A fuller fidelity pass (honor the cone) is out of scope.
- **Verify:** `band-closeup-capture.py` via `/tmp/bch_override.py` on arena_02
  `coop_fs_b_c` / `coop_fs_all_n00` — band-region perf_mean should rise from ~44/56
  into the spotlit range **without** flattening to the OFF look (perf_mean should land
  *below* the VENUE_OFF ~87/102, i.e. moody not flooded). A/B `small_club_01`,
  `festival_01`, gameplay highway (`game.cam`) — confirm no regression (directionals +
  large-range points are essentially unchanged by the curve near d≪range).

### STEP 2 — GAP 3: let the impostor-crowd cam read its scoped environ (env-driven gate)
- **Files:** `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:1267` (the gate) and
  the per-environ re-write at `:3505-3507`.
- **Change:** widen the gate so the impostor cam reads `RndEnviron::sCurrent` instead
  of the hardcoded-white default. Preferred discriminator (cleanest, env-driven):
  - Admit ANY cam when a real authored `RndEnviron::sCurrent` is scoped — i.e. replace
    the `strcmp(camNm,"world.cam")==0` requirement with `world.cam OR
    (venv && venv->mAmbientFogOwner && <env has authored lights or non-default
    ambient>)`. The impostor crowd guarantees `sCurrent` is set (`Crowd.cpp:549`).
  - Apply the IDENTICAL widening to both gates (`:1267` and `:3505-3507`) so the
    per-environ re-write also fires when the impostor cam's env becomes current.
  - Alternative discriminator if env-driven proves too broad: detect
    `cam == gImpostorCamera` (needs the symbol exposed to the engine) or
    `cam->TargetTex() != nullptr` (impostor cam renders to a target). Prefer the
    env-driven form; fall back to cam-identity only if it regresses menu/game cams.
- **Effect:** the crowd impostor reads `RB3_crowd_mesh.env` (ambient 0.18, 0 lights) →
  renders dim/dark (matches small_club + retail).
- **Watch the zero-light fallback:** `RB3_crowd_mesh.env` has **0 lights**, so the
  widened gate would trip the `dl==0 && pl==0` grey-key fallback at `:1337-1349`
  (a `sVenueGreyKey()*sVenueDirExposure()` directional). For a crowd that should be
  ambient-only, that key may still be a touch bright. Guard: **skip the fallback key
  when the scoped env is a crowd env** (let the 0.18 ambient carry it alone), or tune
  `sVenueGreyKey` low for this case. This is the single subtle bit of STEP 2.
- **Blast radius:** confined to draws that scope a real `RndEnviron` through a non-
  world.cam camera — in practice the impostor crowd cam. Must NOT alter `game.cam`
  (highway) or menu cams: those scope the engine's degenerate default env (no
  `mAmbientFogOwner` / near-white ambient), which the gate already excludes — verify
  the widened condition still excludes them. Gate behind the existing
  `RB3_VENUE_LIGHT` opt-out and/or add a new `RB3_CROWD_LIGHT_OFF` for isolated A/B.
- **Verify:** big_club_01 `coop_dir_crowd.shot` — crowd white% should drop from ~9-18
  toward small_club's ~0, mean crowd luma into the 20s, with the **band and highway
  byte-identical** (game.cam untouched). Then re-check arena_02/festival_01 crowds for
  the same improvement and confirm no menu-hub regression.

### STEP 3 — combined verification + pin bump
- A/B both fixes together AND each in isolation (STEP 1 via `RB3_VENUE_POINT_EXPOSURE`/
  curve, STEP 2 via `RB3_CROWD_LIGHT_OFF`) across the matrix:
  **arena_02, big_club_01, festival_01, small_club_01, gameplay highway, menu hub.**
  Objective gates: band perf_mean (arena up, festival/club unchanged), crowd white%
  (big_club down, small_club unchanged), game.cam frame byte-identical.
- Land both engine commits, then bump `MILO_ENGINE_PIN` in
  `rb3/native/CMakeLists.txt:74` to the new engine HEAD in one matching rb3 commit.

---

## 4. Recommended implementation batch

**Implement STEP 1 and STEP 2 in the SAME engine pass, behind ONE pin bump**, but
land/verify them as **separate engine commits** so each can be reverted independently
if a venue regresses:
1. STEP 1 first (point falloff) — it's a 2-line, high-confidence, well-bounded change
   and it's the more visible gap (arena black band). Verify the full venue matrix
   before touching STEP 2; the curve change is global so it earns its own gate.
2. STEP 2 second (impostor-cam env gate) — localized to the crowd path, slightly more
   subtle (the zero-light fallback guard). Verify big_club crowd white% drop with
   game.cam byte-identity.
3. Single `MILO_ENGINE_PIN` bump committing both.

Both are within the existing `RB3_VENUE_LIGHT` opt-out family, so the whole change is
runtime-toggleable for safe rollout.

## 5. Defer / open for more ground-truth
- **No retail arena/big_club wide reference frame exists yet** (`ground-truth.md` §1).
  The band-lit and crowd-dim intents are well-established from closeups + asset data +
  the festival/small_club in-engine controls, so STEPS 1-2 are safe to implement now.
  But the *exact* arena exposure (how bright the spotlit band should read) is a tuning
  judgment — capture a retail arena/big_club gameplay frame (WebSearch) before
  final-tuning `sVenuePointExposure`/`sVenueGreyKey`, and leave those as the last knobs.
- **`kFakeSpot` cone fidelity** (GX type=2 steep curve + spot cone) is deferred — not
  needed for these two gaps (arena spots are type=0), but a future fidelity item if any
  venue authors band keys as spots (`ground-truth.md` §4.1 spot-type audit suggested).

## 6. Anchors (all re-verified at engine pin 5cbe8556)
| claim | anchor |
|---|---|
| point falloff hard cutoff (GAP 2 bug) | `milo-native-engine/src/gfx/standard_wgsl.inc:522-525` |
| Wii GX falloff (ground truth) | `rb3/src/system/rndwii/Lit.cpp:36-44` |
| venue-light gate name==world.cam (GAP 3 bug) | `Rnd_Wgpu_RB3.cpp:1267` |
| per-environ re-write also name-gated | `Rnd_Wgpu_RB3.cpp:3505-3507` |
| default white-directional else branch | `Rnd_Wgpu_RB3.cpp:1352-1357` |
| point upload / range passthrough / 4-cap | `Rnd_Wgpu_RB3.cpp:1325-1334` |
| zero-light grey fallback | `Rnd_Wgpu_RB3.cpp:1337-1349` |
| impostor cam unnamed + Select | `rb3/src/system/world/Crowd.cpp:88,:550` |
| crowd env is sCurrent during draw | `rb3/src/system/world/Crowd.cpp:549` |
| engine pin | `rb3/native/CMakeLists.txt:74` (`5cbe8556`) |
