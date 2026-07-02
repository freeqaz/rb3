# Scout C — White-wig / long-hair rendering bug (2026-07-02)

**TL;DR — root cause found with target-asm proof, no fix applied (research only).**
The "white wig / long hair renders wrong" artifact is CharHair strand physics
collapsing on native. The proximate cause is a **control-flow divergence in our
decomp of `CharHair::SimulateInternal`** (`src/system/char/CharHair.cpp`): the
per-point bone-update tail (`Scale/Cross/Normalize → bone->SetWorldXfm →
force/friction/inertia → t100.v = pos`) is nested **inside**
`if (thisPoint.collides.size() != 0)`, while the **original Wii binary runs that
tail for EVERY point** (its empty-collides `beq` lands exactly at the tail, target
asm label `.L_806D002C`). DC3's ~complete decomp of the same engine confirms the
correct structure. Native runs this wrong source; the Wii-side objdiff shows it as
an innocuous-looking 99.6% "diff_arg branch target" mismatch — it is actually a
real CFG/behavior bug. Fixing the brace scope is a portable one-block move that
should *raise* the Wii match toward 100% **and** fix native hair.

---

## 1. Reproduction on native (band-closeup harness)

Build: `cmake --build native/build-native --target rb3-native` (clean).
Harness: `scripts/native/band-closeup-capture.py` (pinned venue shots; prefab
lineup rotates per launch — 3 runs captured).

```
RB3_HEADMAT_DBG=1 SHARD_DBG=1 SHARD_RATIO_DBG=1 \
python3 scripts/native/band-closeup-capture.py --member all --frames 2 \
    --frame-dt 600 --out /tmp/wig-bug/run1 --tag r1        # + run2, run3-iso
```

Captures + verdicts (kept on disk):

| Run | Out dir | Engine log | Verdict |
|---|---|---|---|
| run1 | `/tmp/wig-bug/run1/` | `/tmp/rb3-bandcloseup-r1-46477.log` | PASS (34/34 pinned, 0 band drops) |
| run2 | `/tmp/wig-bug/run2/` | `/tmp/rb3-bandcloseup-r2-45869.log` | FAIL (375 band drops — all `lowtopsneaks_skin.2.mesh`, the KNOWN deferred footwear issue, not hair) |
| run3-iso | `/tmp/wig-bug/run3-iso/` | `/tmp/rb3-bandcloseup-iso-*.log` | isolation run — see gotcha in §5 |

### What the artifact looks like (exact images)

- **`/tmp/wig-bug/run1/r1_coop_g_b_0.png` — the money shot.** The guitarist
  (prefab **Duke**: round goggles + chops sideburns) has his mohawk
  (`crazyhawk`) rendered as a cluster of **pale/whitish flat ribbon strips
  draped down the left side of his face** — a crumpled-fan "white wig" hanging
  over temple/cheek. The top of the head is bare where the upright hawk fan
  should stand. This matches the user's "white wig (long hair)" description:
  it *reads* as a white stringy wig.
- `/tmp/wig-bug/run1/r1_coop_g_n01_0.png` — same character, same pale strands
  hanging to the chin around the face.
- **`/tmp/wig-bug/run1/r1_coop_b_cg_1.png` / `r1_coop_b_cg_0.png` /
  `r1_coop_b_n01_0.png`** — the bassist (prefab **male05**: `ziggymullet` hair +
  `greenvisor` hat) has the long mullet rendered as a **dark flat slab hanging
  straight down THROUGH the middle of his face** (brow to below chin, thin
  vertical strand divisions visible). Eyes render above/through it.
- Hair that is pulled tight to the head renders fine: run1 drummer
  (`forbdrums` prefab, `pigtails`) and vocalist (`female01`, `shortspikes`)
  look correct (`r1_coop_d_n01_0.png`); run2's `tam`+`dreadpony` bassist and
  `powermullet` members show heads with hats/short hair intact
  (`/tmp/wig-bug/run2/r2_coop_b_cg_0.png`).

So: **not missing, not untextured, not shard-dropped — the long free-hanging
strand geometry is in the WRONG POSE (collapsed/draped), and the pale color
makes it read as a white wig.** The head.mesh keep-mesh-data fix (`26c5684d`)
and skin composite (`372baf7b`) are unrelated and working (faces present,
`*_output.tex hasTex=1 isRT=1` for all hair mats — see census below).

### Log evidence (run1, `RB3_HEADMAT_DBG` census)

Band (dir=`outfit`) hair meshes ARE drawn, with painted RT composites:

```
[HEADMAT] mesh='crazyhawk_resource.mesh'  dir='outfit' mat='hair_crazyhawk.mat'  diffuse='crazyhawk_solid_output.tex' hasTex=1 isRT=1
[HEADMAT] mesh='crazyhawk_buzz_resource.mesh' dir='outfit' mat='hair_crazyhawk_buzzed.mat' diffuse='crazyhawk_buzz_output.tex' hasTex=1 isRT=1
[HEADMAT] mesh='ziggymullet_resource.mesh' dir='outfit' mat='hair_ziggymullet.mat' diffuse='ziggymullet_output.tex' hasTex=1 isRT=1
[HEADMAT] mesh='pigtails_resource.mesh'   dir='outfit' mat='hair_pigtails.mat'   diffuse='pigtails_output.tex' hasTex=1 isRT=1
```

- **No `[SHARD_GUARD]` drops for any band hair mesh** (the only hair drop is
  crowd `male_extras_hair02.mesh`, class `other`). The V24 guard is NOT hiding
  band hair. (Band hair also emits no `[SHARD_RATIO]` lines — the probe only
  logs `wext > 8.f`; hair extents are smaller.)
- `NOTIFY: Skinned mesh needs to be re-exported: <hair>.mesh` (from
  `src/system/rndobj/Mesh.cpp:903`) fires for every loaded hair asset on both
  runs — benign authoring notify, fires on Wii-original assets too.
- No `[HEADMAT] EMPTY` for hair (only venue props). Geometry is present.

---

## 2. Exact assets involved

Band lineup ↔ hair asset mapping was recovered by inflating
`orig-assets/extracted/char/main/shared/gen/prefabs.milo_xbox` (the
`BandCharDesc` prefab dir — `prefabs_path` in
`orig-assets/extracted/char/char_objects.dta:9`; loaded by
`BandCharDesc::ReloadPrefabs()`, `src/system/bandobj/BandCharDesc.cpp:29`)
with `dc3-decomp/scripts/milo/inflate_milo.py`. NOTE: the per-prefab
`char/main/prefab/gen/prefab_*.milo_xbox` files are 440-byte empty ObjectDir
stubs — the real descs all live in the shared `prefabs.milo`.

Prefab → hair (`BandCharDesc::Outfit::mHair`):

| Prefab | Hair/hat asset (all under `orig-assets/extracted/char/main/hair/`) | Seen in |
|---|---|---|
| `prefab_duke` | `male/gen/male_hair_crazyhawk_resource.milo_xbox` (+ facehair chops) | run1 guitarist — **"white wig" artifact** |
| `prefab_male05` | `male/gen/male_hat_ziggymullet_greenvisor_resource.milo_xbox` | run1 bassist — **face slab artifact** |
| `prefab_forbdrums` | `female/gen/female_hair_pigtails_resource.milo_xbox` | run1 drummer — OK |
| `prefab_female01` | `female/gen/female_hair_shortspikes_resource.milo_xbox` | run1 vocalist — OK |
| `prefab_male01` | `male/gen/male_hair_powermullet_resource.milo_xbox` | run2 — mostly OK |
| `prefab_reggae_female02` | `female/gen/female_hat_tam_dreadpony_resource.milo_xbox` | run2 — OK (hat-bound) |

The user's "white wig" character is **whichever long free-strand style the
random lineup rolled** — in our captures, `male_hair_crazyhawk` (pale ribbons =
white wig look) and `male_hat_ziggymullet_greenvisor` (dark slab). Long styles
like `male_hair_long/longmop/robertplant/youngozzy`, `female_hair_long/
longstraight/longwavy` will all misrender the same way — anything with
CharHair-simulated free strands and few/no collide hookups (§3).
`PrefabMgr::AssignPrefabsToSlots` (`src/band3/meta_band/PrefabMgr.cpp:137`)
randomizes male/female/male/female per launch, which is why the wig char
"comes and goes".

(Also loaded-but-not-drawn every run: `male_hair_youngozzy`, `male_hair_mohawk`,
`male_hair_bedhead`, `female_hair_blownback`, `male_facehair_lemmy` — a constant
preload set, not the on-stage members; not the bug.)

---

## 3. ROOT CAUSE (asm-proven): SimulateInternal tail wrongly gated on collides

### The code

`src/system/char/CharHair.cpp`, `CharHair::SimulateInternal(float)` (starts
line 511). Per strand point, after the spring/length constraint, the code does:

```cpp
if (thisPoint.collides.size() != 0) {          // line ~583
    ...collision resolution loop...
    Scale(m128.y, rsa, t100.m.y);              // <-- TAIL: builds the bone frame
    Cross(t100.m.y, m128.z, t100.m.x);
    ...
    if (thisPoint.bone)
        thisPoint.bone->SetWorldXfm(t100);     // <-- writes the strand bone
    Subtract(v158, thisPoint.pos, thisPoint.force);  // spring-back force
    ...friction, inertia...
    t100.v = thisPoint.pos;                    // <-- advances the chain frame
}                                              // line ~691
```

**Everything from `Scale(...)` down must be OUTSIDE the `if`.** Two independent
proofs:

1. **Target binary.** `build/SZBE69_B8/asm/system/char/CharHair.s`,
   `SimulateInternal__8CharHairFf`: the `size()`/`cmpwi`/`beq` for the collides
   check is at `806CFE14` → **`beq .L_806D002C`**, and `.L_806D002C` is exactly
   the `Scale → Cross → RecipSqrt → Normalize → Cross → lastZ → (bone ?
   SetWorldXfm) → Subtract(force)...` tail. The original skips ONLY the
   collision loop; the tail runs unconditionally for every point.
   `mcp run_objdiff SimulateInternal__8CharHairFf` = 99.6% with the giveaway
   mismatch: `[257] diff_arg: beq 0x346c vs beq 0x4ad8` — ours branches past
   the tail to the loop end. **A 99.6% match hiding a real CFG bug.**
2. **DC3 reference** (`dc3-decomp/src/system/char/CharHair.cpp:310-411`): the
   `if (pt.collides.size() != 0) { ... }` closes at the end of the collision
   loop (line 381); `Scale/.../SetWorldXfm/.../t100.v = pt.pos` follow outside.

### Why this collapses hair into a draped "wig"

For any point whose `collides` list is empty:

- `t100.v = thisPoint.pos` never runs → every point in the strand constrains
  its length against the **root** frame instead of the previous point → the
  chain collapses onto a radius-`length` shell around the root;
- gravity (`vec134.z += gravTerm`, applied unconditionally at the top) keeps
  integrating, settling points **straight down from the strand root** → long
  strands drape down over the face (bassist slab) / down the side (guitarist
  ribbon fan);
- the spring-back force toward `idealPos` (`Subtract(v158, pos, force)`) is
  never recomputed → no stiffness restoring the authored direction;
- `bone->SetWorldXfm` never runs in `SimulateLoops`, but
  `CharHair::SimulateZeroTime()` (line 701, runs whenever
  `TheTaskMgr.DeltaSeconds()==0`) **unconditionally** stamps bones from the
  collapsed `pt.pos` — so the broken point state does reach the rendered bones.

Which points have empty `collides`? `CharHair::Hookup(collides)` (line 758)
only attaches a `CharCollide` when `curStrand.mHookupFlags & col->mFlags` —
strands authored with sparse hookup flags (or venue/char dirs where matching
`CharCollide`s aren't reachable) get none. That's why tight-to-head styles
(pigtails/shortspikes, and points that DO hook the head-sphere collide) look
fine while free-hanging mohawk/mullet strands collapse.

`CharHair::Poll()` DOES run on native (`src/system/char/CharHair.cpp:460`;
only gated by the `RB3_NO_FACE` env), `DoReset` (line 419) resets point
positions then calls `SimulateLoops(reset, ...)` — which immediately re-breaks
collide-less points via the same gated tail. The native-only substitutions in
this TU were checked and are NOT the cause: `StrandMultiply` →
`Multiply(a,b,out)` (line 235) matches the paired-single asm's row-vector
`out = a*b` convention (verified against `Rot.cpp:624` and the psq block at
lines 100-233), and the missing-return shims (line 872+) are benign.

### Renderer interaction (why the collapsed pose draws instead of being hidden)

- Band hair meshes are non-torso, so `BandCharacter::RebindHeadHandsAtRest()`
  (`src/system/bandobj/BandCharacter.cpp:1174`, runs pre-`Character::Poll`)
  rest-bakes them and sets `mNativeBonesRebound` → the engine fling-clamp and
  the V24 shard guard **exempt** them
  (`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:4666,5204`) → the live
  (collapsed) strand-bone transforms draw as-is. The guard exemption is
  *correct* behavior — the bones really are where the sim put them; the sim is
  what's wrong.
- The torso-scoped `RebindOutfitBonesToOwnSkeleton()`
  (`BandCharacter.cpp:1021`) never touches hair (name filter
  trackjacket/vestdenim/plaidshirt/shred only).

### Whiteness

Secondary/cosmetic: the hair color composites ARE painted (census `hasTex=1
isRT=1` on `crazyhawk_solid_output.tex` etc.), so "white" is plausibly Duke's
authored pale-blond hawk color; collapsed ribbons show the strands' broad flat
faces (normally edge-on when upright), amplifying the pale look. Do NOT chase a
texture bug first — re-judge color only after the pose fix.

---

## 4. Hypotheses, ranked

1. **H1 (root cause, CONFIRMED by target asm + DC3): mis-scoped brace in
   `CharHair::SimulateInternal`** — bone/force/chain tail gated on
   `collides.size()!=0`, original runs it unconditionally. Fix = move the tail
   out of the `if` (lines ~668-691 region: from `Scale(m128.y, rsa, t100.m.y);`
   through `t100.v = thisPoint.pos;` — note the RB3 (2010) tail legitimately
   lacks DC3's later wind block). Expected: Wii match goes 99.6% → ~100% (the
   `beq` target and the two `lfs 0x50/0x54` deletes at [259-262] are in the same
   region), native hair stands back up. Verify: `mcp run_objdiff
   SimulateInternal__8CharHairFf` == 100%, then band-closeup A/B on a lineup
   with crazyhawk/ziggymullet (re-roll until `[HEADMAT] ... crazyhawk` or
   `ziggymullet` appears in the log; `--song-downs 4` club venue).
2. **H2 (secondary, verify after H1): empty `collides` on native even for
   flagged strands** — if `CharCollide` objects (head/shoulder spheres, loaded
   with the char) aren't reachable from `Dir()` at `Hookup()` time on native,
   even flagged points lose collision AND (post-H1-fix) hair will hang through
   the skull (no collision push-out), though no longer collapsed. Cheap probe:
   log per-strand `points-with-collides / points` in `Hookup` under an env
   gate.
3. **H3 (unlikely primary): hair strand roots not following the animated
   head** — refuted as primary: draped strands track the head position across
   frames/shots in the captures (root `WorldXfm` is read live each
   `SimulateInternal`), and heads themselves render/animate post-`26c5684d`.
   Re-check only if post-fix hair floats offset from the scalp.
4. **H4 (cosmetic follow-up): hair color/lighting** — pale-white ribbons may
   partly be flat/unlit shading on the strand ribbons (same family as the
   known "over-bright flat face shading" open item in
   `docs/native/c8-ground-truth-2026-07-01/RESOLUTION.md`). Assess against
   Dolphin ground truth AFTER the pose fix.

## 5. Notes for the rb3-viewer campaign + tooling gotchas

- **This bug did not need the viewer to find, but is the viewer's ideal first
  regression asset**: load `char/main/hair/male/gen/male_hair_crazyhawk_resource.milo_xbox`
  + a head, tick `CharHair::Poll`, and the collapse repros in isolation;
  after the H1 fix the hawk should hold its authored fan under gravity.
- **`RB3_ISOLATE_MESH=hair` gotcha**: substring-matches `chair.mesh` (venue
  chairs!) and MISSES all real hair meshes — band hair meshes are named by
  style (`crazyhawk_resource.mesh`, `ziggymullet_resource.mesh`,
  `pigtails_resource.mesh`), no "hair" substring. run3-iso rendered a stack of
  chairs (`/tmp/wig-bug/run3-iso/iso_coop_g_b_0.png`). Use style names.
- Shard-guard verdict lines: run2's FAIL is `lowtopsneaks_skin.2.mesh` (known
  deferred footwear, `project_converge_venue_lighting`), unrelated to hair.
- The prefab lineup rotates per launch: to A/B a specific hair, re-roll runs
  until the census shows the target mesh, or drive the closet/customize path.

## 6. Concrete recommendation to the implementation agent

1. In `src/system/char/CharHair.cpp::SimulateInternal`, close the
   `if (thisPoint.collides.size() != 0)` block right after the collision
   `for` loop (mirror DC3 lines 310-381), leaving the
   `Scale → ... → t100.v = thisPoint.pos` tail unconditional. Keep the
   existing collision-loop contents untouched. This is a shared-code
   (Wii+native) change — per the sub-100 shared-geom rule
   (`feedback_decomp_sweep_native_visual_gate`), gate it with BOTH:
   `run_objdiff` (expect ≥99.6%, likely 100%) AND the native band-closeup
   visual gate.
2. Re-run `band-closeup-capture.py` until a crazyhawk/ziggymullet lineup rolls;
   compare against `/tmp/wig-bug/run1/r1_coop_g_b_0.png` (broken baseline).
3. Then evaluate H2 (collide hookup coverage probe) and H4 (color) as
   follow-ups.
