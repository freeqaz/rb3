# Char-rebake scope — footwear `_skin.2` (item 2) + crowd/extras servo (item 3)

Research-only. Engine pin `20dba55` (all landed convergence fixes). Binary built
clean (`ninja: no work to do`). Measured live via `band-closeup-capture.py`
(small_club, guitar closeup, 4 wardrobe-random boots) + `SHARD_RATIO_DBG`/
`SHARD_DBG` engine logs (`/tmp/rb3-bandcloseup-b{1..4}-*.log`).

**TL;DR verdicts**

| Sub-item | Verdict | One-line reason |
|---|---|---|
| 2 — Footwear `_skin.2` fling | **ACCEPT-as-dropped** | Real geometry, but transient (~3% of frames), off-frame in 100% of closeups, and a rigid anchor cannot fix it — the engine's own `RB3_GUARD_EXEMPT_REBOUND` experiment proved the residual is a *rotation-basis* fling that draws a full-screen slab, not a translation offset an anchor can null. |
| 3 — Crowd/extras servo shards | **ACCEPT-as-dropped** (premise was wrong) | The "no non-Band rebake hook exists" premise is **false** — `Crowd.cpp` already has `RebindCrowdCharBonesToOwnSkeleton` and it works (crowd BODIES render at ratio ~1.0, zero drops). Residual is 3 tiny accessory meshes on ≤3 of ~292 instances, masked/distant, dropping <2% of their draws. |

---

## Item 2 — Footwear thin-skin `_skin.2` fling

### Measured (4 wardrobe-random boots, guitar closeup)

`max_band_ratio` / closest-to-cap per boot (band cap = ratio 4.0× **OR** world 110u,
with a 40u world floor; see `Rnd_Wgpu_RB3.cpp` L5164-5190):

| boot | rolled garment closest to cap | max band ratio | band drops |
|---|---|---|---|
| b1 | `maleslipons2_skin.2.mesh` | 3.30 | 0 |
| b2 | `wovensteppers_skin.2.mesh` | 3.16 | 0 |
| b3 | `gloves_resource.1.mesh` | 3.35 | 0 |
| **b4** | **`saddleshoe_skin.2.mesh`** | **4.78** | **407** |

So 3 of 4 boots: the thin shoe rolls **under** the cap and renders fine. 1 of 4
(saddleshoe): it crosses the cap and gets guard-dropped. Wardrobe-random, as
flagged.

### The saddleshoe drop is a transient false-positive, not a persistent explosion

`saddleshoe_skin.2.mesh` over the song (231 sampled `SHARD_RATIO` lines, b4):
- bind extent **11.21u** (tiny — that is the whole problem; a 4× span = 45u)
- world extent: median **25.3u**, p90 **36.2u**, max **60.6u**
- frames over the 40u floor: **19/231 (8%)**; frames over the 4.0 ratio cap: **7/231 (3%)**
- bone0 height of the drops: median Z=18.9u (ankle/foot region) — confirms it's the foot

The drops fire at world extent **44–60u** / ratio **4.0–5.4**. That is well under
the **110u world cap** and under the **85u "real tear" floor** the engine comment
cites (min measured real band tear = 44.9u, typical 120–400u). It only trips the
**ratio cap** because the bind is so small that a deep ankle curl legitimately
spans 4×. This is **exactly** the small-bind-garment false positive the V24
comment (L5132-5152) anticipates — the shoe is geometrically *sane*, just
flickering off for the worst ~3% of ankle-curl frames.

### Visibility: off-frame in 100% of closeups (verified by eye)

Captured guitar closeups (`coop_g_cg`, `coop_g_n01`, `coop_g_n03`, `coop_g_b`) all
frame **chest/guitar up** — the feet and floor are never in shot. small_club has no
feet-framing band shot. The drop is invisible.

### Can a rigid anchor fix it? NO — it makes it worse

The proposed fix (mirror `RebindInstStringsToRestBasis`: anchor the `_skin.2` shoe
to its ankle bone, rebake `offset = meshWorld * inv(ankleWorld)`) was effectively
**already tried and rejected** by the engine team:

1. `RebindOutfitBonesToOwnSkeleton` (the band outfit rebind) is **deliberately
   TORSO-ONLY** (`BandCharacter.cpp` L1063-1086: trackjacket/vestdenim/plaidshirt/
   shred). The header comment (L1163) states the torso rebind *cannot* fix the
   thin head/hands/feet geometry because "long-thin geometry shards under the
   rotation-basis mismatch." Footwear is squarely in that excluded thin-geo class.
2. The `RB3_GUARD_EXEMPT_REBOUND=1` experiment (`Rnd_Wgpu_RB3.cpp` L5007-5017)
   paired a rest-rebake with guard-exemption and found: anchoring **translation**
   does null the origin offset (≤92u, no flings), **but the native rotation-basis
   divergence remains** — far-from-bone verts smear by R·sin(θ) to 200–460u and
   the exempt meshes "drew as full-screen slabs." The conclusion: "The 2.0× ratio
   guard is CORRECT about those poses; keep dropping them by default until the
   pose-pipeline basis root-cause (C8) is fixed."

A rigid single-ankle anchor would convert the shoe from a *transient 3%-of-frames
flicker* (currently dropped, invisible) into a *persistent rotation-flung slab*
that the guard would either (a) still drop (no gain) or (b) — if exempted — draw as
a screen-crossing slab (strict regression). The InstStrings rigid anchor works for
the *guitar neck* because the strings are a near-rigid bar riding one bone; a shoe
deforms across ankle+foot bones with real rotation, so the same trick reintroduces
the rotation fling the torso-only scope was created to avoid.

**HX_NATIVE/Wii-neutral feasibility:** trivially feasible to *write* (it would live
beside the other rebinds in `BandCharacter.cpp`, HX_NATIVE-gated, Wii byte-identical).
But it has no path to *lower* the band ratio under cap without the C8 rotation-basis
fix, so the gate the followup demands ("MUST drop max_band_ratio under cap")
**cannot be met**.

### Item 2 verdict: ACCEPT-as-dropped

Real geometry, off-frame everywhere, transient (~3% of frames on ~1-in-4 wardrobe
rolls), and unfixable by a rigid anchor (the residual is C8 rotation-basis, which
makes an anchored+exempted shoe a *worse* slab). Revisit only if/when C8
(pose-pipeline rotation-basis) is solved — at which point the entire thin-geo
family (feet, hands, fingernails) reopens together, not as a one-off footwear hook.

---

## Item 3 — Crowd/extras servo shards

### The "no hook exists" premise is FALSE

The followup states the faithful fix needs "a NEW non-Band Character rest-rebake
hook (none exists — all 3 existing rebinds are BandCharacter-only)." That is
outdated. There are **four** rebinds, and one is already a non-Band crowd rebind:

- `src/system/world/Crowd.cpp:911` `RebindCrowdCharBonesToOwnSkeleton(Character*)`,
  called per-instance at draw time from `WorldCrowd::Poll`/`Draw3DChars`
  (Crowd.cpp:414), opt-out `RB3_NO_CROWD_REBIND=1`, probe `CROWD_REBIND_PROBE=1`.
  It walks each archetype's GeomOwner bones, resolves them by name inside the
  archetype's own dir, and `SetBone(b, own, calcOffset=true)` to rest-rebake the
  inverse-bind, latched via `mNativeBonesRebound`.

This already fixes the crowd **bodies**: measured `male_crowd_body0N` /
`female_crowd_body0N` produce thousands of `SHARD_RATIO` lines at ratio <2.0 and
appear in **zero** drop records across all 4 boots.

### Measured residual (4 boots aggregated)

Only **3** non-band meshes ever drop, on **≤3 of ~292** crowd/extras instances:

| mesh | drops/4boots | max ratio | max world | owning dir |
|---|---|---|---|---|
| `male_extras_eyebrows11.mesh` | 333 | 4.7 | 23.2u | `male_extras11` |
| `male_extras_hair02.mesh` | 333 | 2.6 | 37.5u | `male_extras02` |
| `clap.mesh` | 108 | 2.2 | 111.7u | `crowd_male04` |

(`scrollbar_bg.mesh` also shows 3196 drops but is the song-select scrollbar
measurement artifact — GAP 1, already gated, not gameplay crowd.)

Context: `clap.mesh` has **5819** total draws but only **108** drops (<2%) — the
clap animation legitimately swings the hands ~106u most frames and only crosses
2.0× on the worst frames. `male_extras_*` are accessory meshes (hair/eyebrows) on
the `extras.fm` FileMerger path (BandDirector.cpp:1360) — a **different path than
WorldCrowd**, which is why the crowd rebind doesn't reach them.

### Visibility: masked / distant (verified)

Gameplay-active background in the captures (e.g. `b4_coop_g_n01_1.png`) shows a
clean, dim, distant crowd — no visible shards. The crowd is GAP-B(a)-dimmed and far
behind the highway; the 3 residual accessory meshes are tiny (eyebrows 23u, hair
37u) and dropped-when-broken, so nothing visible tears.

### Scope of a fix (if pursued) and its risk

To rebake `male_extras_*`/`clap` you'd need a hook on the **extras** path (a Character
rest-rebake similar to the crowd one but reached via `extras.fm`) plus, for `clap`,
the same C8 rotation-basis problem as item 2 (a held-prop arm swing is rotation,
not a fixable static offset). The crowd-body rebind's own comments (Crowd.cpp
L1003-1011) warn the latch is *load-bearing* and re-baking against a mid-animation
pose freezes the mesh — so any extras rebake must capture at a true rest pose,
which the always-clapping extras may never hit cleanly. Blast radius = the whole
292-instance crowd/extras draw path; payoff = un-dropping 3 invisible accessory
meshes. Poor trade.

### Item 3 verdict: ACCEPT-as-dropped

The crowd itself is already FIXED (existing `RebindCrowdCharBonesToOwnSkeleton`).
Residual = 3 tiny accessory meshes on ≤3 of ~292 instances, masked + distant +
dropping only their worst <2% of draws. A new extras-path rebake is high-blast-
radius for zero visible gain, and `clap` shares item 2's unfixable C8 rotation
fling. Do **not** implement.

---

## Notes for the coordinator

- Neither sub-item should land code. Both are correctly handled by the existing V24
  shard guard (drop-when-broken) and the existing crowd rebind.
- The single thing worth a doc-level correction in `NATIVE_FOLLOWUPS.md` / GAP 6:
  the claim "no non-Band rest-rebake hook exists" is wrong — `Crowd.cpp`'s
  `RebindCrowdCharBonesToOwnSkeleton` is exactly that hook and already ships.
- The real blocker behind BOTH items is **C8** (the pose-pipeline rotation-basis
  divergence). Until C8 is solved, every thin-geo / rotating-accessory rebake
  reintroduces a rotation-flung slab. When C8 lands, reopen footwear + extras +
  fingernails + gloves together as one thin-geo batch, not as one-offs.
- Repro for re-verification: `for b in 1 2 3 4; do SHARD_RATIO_DBG=1 SHARD_DBG=1
  python3 scripts/native/band-closeup-capture.py --member guitar --frames 2
  --out /tmp/foot/b$b --tag b$b; done` then read each `verdict.json`
  (`closest_band_to_cap`, `drops_band`) + grep the per-boot engine log
  (`/tmp/rb3-bandcloseup-b$b-*.log`) for `saddleshoe`/`clap`/`male_extras`.
  saddleshoe is the ~1-in-4 wardrobe roll that crosses the band cap.
