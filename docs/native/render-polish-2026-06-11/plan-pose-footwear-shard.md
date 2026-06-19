# plan — pose-footwear-shard (render-polish wrap-up)

**Item:** Eliminate the last character residual — footwear/gloves still guard-drop on a
normal pose curl. Fix the GUARD, not the pose (the pose is correct).

**Planner verdict:** `tractable = yes`. Engine-only, native-only, Wii byte-identical by
construction. Small, surgical, fully discriminable on evidence I measured below.

**Scope decisions up front:**
- needsEngine = **true** (engine `Rnd_Wgpu_RB3.cpp` V24 shard-guard only; no rb3-src change).
- This is the V24 ratio-guard threshold at `Rnd_Wgpu_RB3.cpp:4915`. The wave-5 WorldXfm fix
  (the recompose pre-pass at ~L4131) is NOT touched and stays as the upstream cause-fix.
- Out of scope (must STILL drop, see evidence): crowd/extras meshes (`char/extras/*.milo`),
  the `guitar_brain_strings`/instrument mesh (`dir='instrument'`), and `scrollbar_bg`
  (`dir='scrollbar'`, a UI mesh that incidentally trips the skinned guard).

---

## 1. ROOT CAUSE (confirmed by my own measurement, not just the handoff docs)

The wave-5 fix (`15ce606`, the stale-leaf-WorldXfm recompose) repaired the *pose*: the band
garment bones now read SANE world positions. I confirmed this directly with `C8_PROBE` on a
dropped boot:

```
[C8_SLOT] mesh='timberlandboots_resource.mesh' b=0 'bone_R-ankle.mesh'
   bfile='char/char/main/skeleton_unshared.milo' root='player2'
   w=(3.6,1.0,4.1)   skin.v=(-0.0,-4.2,0.8)   bind=(3.7,4.5,4.2)   skinDet=1.000
   ... (knee w=(3.6,2.1,22.6), toe w=(3.6,6.4,-0.2) — all sane, at-floor, det=1)
```

The pose is correct (sane bone worlds, orthonormal skin, tiny skin.v). The DROP is a
**false positive in the V24 ratio guard** at `Rnd_Wgpu_RB3.cpp:4915`:

```cpp
bool degenerate = (wext > 15.f) && (lext > 0.001f) && (wext > 2.0f * lext);
```

A small-bind-extent garment (a boot/glove/legwear whose authored bind AABB is ~12-25u)
legitimately exceeds **2.0x its tiny bind extent** when the limb it skins curls hard, *without
the world extent ever becoming geometrically impossible*. The fixed `2.0` ratio cannot tell a
50u-bind shard (true tear) from a 18u-bind boot whose foot curls to a 37u world span (a real
pose). The guard's own header (L4899-4914) already documents that "small held-prop slivers"
slip/over-fire and that a full fix "needs the CharServo skeleton-math root-cause" — which the
wave-5 fix has now delivered for the band. So the guard is now over-broad for the band class.

### The decisive separation (my SHARD_DBG + SHARD_RATIO_DBG run, port 9821, hard, 20-burst)

24,173 guard drops over the run. Per-mesh bind/world/ratio + owning skeleton:

| mesh (dropped) | bone skeleton file | bind | world | ratio | class |
|---|---|---|---|---|---|
| femaledestroyedchucks_resource | `skeleton_unshared.milo` (band) | 18.45 | 36.9–64.7 | 2.0–3.5 | **FALSE+ (keep)** |
| fingernails_resource | `skeleton_unshared.milo` (band) | 35.93 | 71.9–85.5 | 2.0–2.4 | **FALSE+ (keep)** |
| miniskirt_resource | `skeleton_unshared.milo` (band) | 18.85 | 37.7–56.8 | 2.0–3.0 | **FALSE+ (keep)** |
| rolledjeans_skin.2 | `skeleton_unshared.milo` (band) | 12.52 | 25.0–37.0 | 2.0–3.0 | **FALSE+ (keep)** |
| timberlandboots_resource | `skeleton_unshared.milo` (band) | 21.34 | 42.7–52.3 | 2.0–2.5 | **FALSE+ (keep)** |
| wrestlingboots_resource | `skeleton_unshared.milo` (band) | 25.15 | 50.3–51.3 | 2.0 | **FALSE+ (keep)** |
| shortdress_resource.1 | `skeleton_unshared.milo` (band) | 20.66 | 41.3–45.7 | 2.0–2.2 | **FALSE+ (keep)** |
| male_extras_hair02 | `char/extras/male_extras02.milo` | 14.62 | 35.3–37.5 | 2.4–2.6 | TRUE shard (drop) |
| male_extras_eyebrows11 | `char/extras/male_extras11.milo` | 4.94 | 23.2 | 4.7 | TRUE shard (drop) |
| guitar_brain_strings | `dir='instrument'` | 29.35 | 128.7–154.2 | 4.4–5.3 | out of scope (drop) |
| scrollbar_bg | `dir='scrollbar'` | 80.80 | 324.1 | 4.0 | out of scope (drop) |

**The clean discriminator is the bone's owning skeleton, not the ratio or world extent.**
worldExt alone does NOT separate the classes — `male_extras_hair` (true shard) sits at
world 35–37, dead in the band-boot band (25–85). Ratio alone does NOT separate them either
(band fingernails 2.0–2.4 vs crowd hair 2.4–2.6 overlap). What separates cleanly is:

- **Band false-positives:** every bone resolves to `char/char/main/skeleton_unshared.milo`
  (the static shared band skeleton; root = a `playerN` member). This is the SAME band-member
  detector the wave-6 rebake already uses at L4042
  (`strstr(wdir->mStoredFile.c_str(), "skeleton_unshared.milo")`).
- **Crowd/extras true shards:** bones resolve to `char/extras/*.milo` / `char/crowd/*` and
  their bone WORLD is genuinely flung (crowd `skin.v=(-208.3,68.7,121.0)` is ~200u off its
  bind `(-0.0,0.3,65.4)` — a real cross-instance/torn smear, NOT a curl).

So the fix is: **for band-member meshes (skeleton_unshared.milo) only, relax the ratio guard
to a bind-extent-aware threshold + an absolute world-extent sanity cap; leave the crowd /
extras / instrument / UI guard at the existing 2.0x exactly as-is.**

---

## 2. THE FIX (exact file + approach)

**File:** `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, `BandRnd::DrawMesh`, the V24
degenerate test at **L4915** (inside the `if (skinned && ...)` block opened at L4788).
Engine-only, native-only file (`MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`, not in the Wii image,
not compiled by DC3) → Wii byte-identical + DC3-inert by construction.

> Engine edits go in the PAIRED engine worktree only — edit `<wt>/.engine-path`'s checkout,
> never `/home/free/code/milohax/milo-native-engine` directly. Build with
> `-DMILO_ENGINE_PATH="$(cat .engine-path)"`. Land via a new engine commit on a `wt-` branch,
> then bump `MILO_ENGINE_PIN` in rb3 `native/CMakeLists.txt` in a matching rb3 commit.

### 2a. Detect band membership cheaply at the guard site (reuse the existing detector)

Before the `degenerate` test, determine whether this mesh is a band-member garment by the
SAME mechanism the rebake uses (L4040-4043): walk this mesh's bones (`owner->BoneTransAt(b)`)
and test the bone's owning dir stored file for `"skeleton_unshared.milo"`. `owner`, `numBones`,
and `mesh` are all in scope at the guard. To keep it cheap, test bone 0 first and fall through
to the first non-null bone; a single `strstr` on `bt->Dir()->mStoredFile` is enough (the band's
outfit meshes are 100% band-skeleton-bound — every C8_SLOT line confirmed it).

```cpp
// band-member garment = its skin bones resolve to the static shared band skeleton
// (char/char/main/skeleton_unshared.milo). Crowd/extras bind char/extras|crowd/*.milo,
// instruments bind prop dirs — none match. Same detector as the wave-6 rebake (L4042).
bool bandMember = false;
for (int b = 0; b < numBones && !bandMember; b++) {
    RndTransformable* bt = owner ? owner->BoneTransAt(b) : nullptr;
    ObjectDir* bd = bt ? bt->Dir() : nullptr;
    if (bd && !bd->mStoredFile.empty() &&
        strstr(bd->mStoredFile.c_str(), "skeleton_unshared.milo"))
        bandMember = true;
}
```

### 2b. The discriminating predicate (band-only relaxation)

Replace the single `degenerate` line with a class-aware predicate. For crowd/extras/instrument/
UI (`!bandMember`) keep the EXISTING `2.0x` test verbatim — that is the proven crowd-safe value
and the campaign accepts crowd thin-extras hair/eyebrow drops. For `bandMember`, a deep limb
curl on a small-bind garment is legitimate; gate on TWO conditions that the data shows separate
cleanly from a true band tear (e.g. the wave-4 below-floor leg fling at world ~Z=-33, or a
cross-instance smear):

```cpp
bool degenerate;
if (bandMember) {
    // small-bind garments (boots/gloves/legwear: bind ~12-25u, fingernails ~36u)
    // legitimately span 2-3.5x bind when a limb curls hard, but the WORLD extent
    // stays bounded (measured 25-85u across all band footwear/gloves/legwear). A
    // genuine band tear (wave-4 leg fling) produces a far larger world span and/or
    // a non-finite/below-floor bone, which the per-bone composed-skin guard at L4203
    // already rejects. So for a band garment: keep ONLY if (a) the world extent is
    // within a sane garment span AND (b) the ratio is within the deep-curl envelope.
    // Above EITHER cap it is a real shard -> drop.
    const float kBandWorldCap = 110.f;   // > any measured band garment (max 85u) + margin
    const float kBandRatioCap = 4.0f;    // deep curl tops out ~3.5x; true tears jump >4.4x
    degenerate = (wext > 15.f) && (lext > 0.001f) &&
                 (wext > kBandRatioCap * lext || wext > kBandWorldCap);
} else {
    degenerate = (wext > 15.f) && (lext > 0.001f) && (wext > 2.0f * lext); // unchanged
}
```

**Why these two caps and not one:**
- The bind-extent-scaled ratio cap (4.0x) is the primary lever the orchestrator asked for —
  it tolerates the small-bind boot at 2.0–3.5x while still dropping the band-class true shard
  (the wave-4 leg fling was a >2x AABB jump to a *below-floor* world; an unrecomposed leaf or a
  cross-instance smear blows world span well past 4x of a 12–25u bind).
- The absolute world-extent cap (110u) is a backstop so relaxing the ratio cannot let a truly
  exploded band mesh through: every measured legit band garment is ≤85u world; a real fling is
  hundreds of u (`guitar_brain_strings` 128–154u even though it is an instrument, and the
  wave-5 BEFORE leg fling reached far past that). 110u sits cleanly between (85u top of legit,
  hundreds for a tear). This is the "verts within N units" sanity check the item describes,
  expressed as a world-extent cap (cheaper + already computed than per-vert-vs-bind distance).

Both constants are derived from the measured envelope (max legit band world = 85u, max legit
band ratio = 3.5x; nearest true-shard ratio in-class = 4.7x for crowd eyebrows; nearest large
world = 128u). The margins (110u vs 85u; 4.0x vs 3.5x) are deliberately *narrow* so a future
regression that re-introduces a real fling is still caught.

**Make both caps env-tunable for A/B** (mirroring the file's existing convention): read
`RB3_BAND_SHARD_WORLDCAP` / `RB3_BAND_SHARD_RATIOCAP` once (static, default 110/4.0) so the
reviewer can sweep the threshold without a rebuild. Keep `SHARD_GUARD_OFF` / `SHARD_RATIO_DBG`
exactly as-is.

### 2c. Optional refinement (only if 2b proves insufficient on review)

If a particular member/frame still false-drops a glove at >4x of a *very* tiny bind (e.g. a
single-finger glove with bind <10u), add the third signal the verify doc named: **bone0 at
sane body height**. The SHARD_DBG line already computes `bone0=(bx,by,bz)`; a band garment's
bone0 sits at body height (Y 14–151 in my data) and within the stage X/Z, whereas a torn
crowd mesh flings bone0 to Y≈598 or X≈−226 (off-stage). This is a *secondary* discriminator;
prefer 2b's world-cap, which already excludes the off-stage crowd by class. Do NOT ship 2c
speculatively — only if a measured residual band glove survives 2b.

---

## 3. RISK + MATCH-NEUTRALITY

- **Match-neutrality:** the only file touched is `Rnd_Wgpu_RB3.cpp`, which is native-only
  (`MILO_ENGINE_GPU_PLATFORM_SOURCES_RB3`) — not in the Wii DOL, not compiled by the decomp
  build, not compiled by DC3. rb3 `src/` diff is EMPTY → **Wii byte-identical by construction**
  (no rebuild/objdiff needed for correctness, but the implementer SHOULD still confirm the rb3
  `git diff src/` is empty and that only `native/CMakeLists.txt`'s pin line changed).
- **DC3-inert:** DC3 dancers are matched-gender named chars that never hit the mixed-gender
  band path; the new `bandMember` branch only fires when a bone resolves to
  `skeleton_unshared.milo`, which DC3 does not load.
- **Primary risk — neutering the guard.** The whole point of the guard is to stop
  screen-crossing teal shards (crowd servo poses, NaN, cross-instance palette). The fix MUST
  NOT relax the guard for those. Mitigated structurally: the relaxation is gated on
  `bandMember` (a positive skeleton-file match), so crowd/extras/instrument/UI keep the
  unchanged 2.0x test. The mandatory verification (§4) A/Bs the crowd-extras + any-NaN drop
  count to prove it did not move.
- **Secondary risk — a real band fling slips through.** Bounded by the absolute 110u world cap
  (§2b) which is far below any measured real fling. The wave-5 BEFORE state (RB3_NO_SKEL_WORLDFIX
  =1, leg at world Z=-33) is the in-class negative control: it MUST still drop with this fix.
- **Cost:** the `bandMember` scan adds one bone-loop with a `strstr` per skinned mesh per draw.
  The loop short-circuits on the first match (`!bandMember`), and band garments match at bone 0.
  Crowd meshes scan all bones but they are already being ratio-tested in the same block. Net cost
  is negligible (≤ numBones strstr, deduped by early-out); no per-frame allocation.
- **Conflict risk:** low. The hunk is the `degenerate` line + a small pre-block, self-contained
  inside the existing `if (skinned && ...)` guard. It does not touch the wave-5 WorldXfm recompose
  (L4131), the per-bone composed-skin guard (L4203), the rebake (L3997), the bloom/halo composite,
  or the lighting soft-clip. Re-anchor on `bool degenerate = (wext > 15.f) ...` if it has moved.

---

## 4. VERIFICATION PLAN (per-symptom; implementer + reviewer use the SAME procedure)

Harness: `scripts/native/keyboard-to-gameplay.py --diff hard --game-burst N`. Engine stderr
goes to `/tmp/rb3-kbd2game-<port>.log` (NOT the harness's own stdout — read THAT file for the
`[SHARD_*]` lines). Use ports in the assigned 9821-9824 range. Evidence under
`/tmp/rp8-pose-footwear-shard/`. Kill ONLY rb3-native PIDs whose `/proc/<pid>/environ`
`RB3_HTTP_PORT` is in your range.

### Build (engine + pin)
```bash
cmake -B native/build-native -S native -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn \
  -DMILO_ENGINE_PATH="$(cat .engine-path)"
cmake --build native/build-native --target rb3-native
```

### Symptom 1 — the named garments render (drops → ~0 for them)

Because the visible roster is randomized per run, A/B in ONE run via the env opt-outs rather
than comparing two roster-different runs. Run with the guard fix ON (default) vs OFF
(`RB3_BAND_SHARD_WORLDCAP=0` would force the cap; better: `SHARD_GUARD_OFF=1` for the absolute
floor, and the wave-5 opt-out for the pre-fix baseline). Concretely:

```bash
# AFTER (fix on): count band-garment drops by mesh
env SHARD_DBG=1 python3 scripts/native/keyboard-to-gameplay.py --port 9821 \
    --diff hard --game-burst 24 --out /tmp/rp8-pose-footwear-shard/after
grep -a SHARD_GUARD /tmp/rb3-kbd2game-9821.log \
  | grep -a "skeleton_unshared\|_resource\|_skin\." \
  | sed -E "s/.*mesh='([^']*)'.*/\1/" | sort | uniq -c | sort -rn
```
- PASS: the band garments that bind `skeleton_unshared.milo` (boots/legwear/gloves/fingernails,
  identified by `dir=''` / `dir='outfit'` and the C8_PROBE skeleton check) drop **~0** times
  (a transient single-frame drop at scene entry is acceptable; sustained drops are a fail).
- Confirm membership with `C8_PROBE='<meshname>' C8_EVERY=120` on the dropped names → their
  C8_SLOT `bfile=` must be `char/char/main/skeleton_unshared.milo`. (I confirmed timberlandboots,
  fingernails this way; the implementer re-confirms whatever roster their run draws.)
- Visual: `--game-burst` closeup PNGs — the foreground band members have feet/shoes and hands/
  gloves present (no bare ankles, no missing hand volume). Compare an AFTER closeup to a
  `RB3_NO_SKEL_WORLDFIX=1`-OFF + guard-fix-OFF baseline shot of the same member.

### Symptom 2 — genuinely-broken meshes are STILL dropped (do NOT neuter the guard)

This is the gating safety check. Two negative controls:

**(a) Crowd / cross-instance still drops.**
```bash
grep -a SHARD_GUARD /tmp/rb3-kbd2game-9821.log | grep -aE "extras|crowd" \
  | sed -E "s/.*mesh='([^']*)'.*dir='([^']*)'.*/dir=\2 \1/" | sort | uniq -c | sort -rn
```
- PASS: `male_extras_*` / `*_crowd_body*` meshes (bone file `char/extras|crowd/*.milo`) still
  drop at the unchanged rate (in my baseline: `male_extras_hair02` 93, `male_extras_eyebrows11`
  93). The count for these MUST be statistically unchanged vs a guard-fix-OFF run — the
  `bandMember` branch must not fire for them. A drop in their drop-count = the fix leaked into
  the crowd class = FAIL.

**(b) A synthetic in-class band fling still drops.** The wave-5 opt-out re-introduces the real
band leg fling (ankle world Z=−33, below floor) WITHOUT changing the skeleton class — it is the
perfect in-class negative control:
```bash
env SHARD_DBG=1 RB3_NO_SKEL_WORLDFIX=1 python3 scripts/native/keyboard-to-gameplay.py \
    --port 9822 --diff hard --game-burst 16 --out /tmp/rp8-pose-footwear-shard/fling
grep -a SHARD_GUARD /tmp/rb3-kbd2game-9822.log | sed -E "s/.*mesh='([^']*)'.*/\1/" \
  | sort | uniq -c | sort -rn | head
```
- PASS: with the leaf-WorldXfm fix OFF, the band legwear/footwear (now genuinely flung to a
  >110u world span / >4x ratio) STILL drop. If the guard relaxation lets the below-floor leg
  through, the world-cap (110u) is mis-set → FAIL (tighten the cap). This proves the relaxation
  is "tolerant of a SANE pose," not "blind to band meshes."

**(c) No NaN / runaway leaks.** `grep -aci "nan\|inf\|assert\|segv\|abort" /tmp/rb3-kbd2game-*.log`
must be 0; the per-bone composed-skin guard (L4203, unchanged) still rejects non-finite skins
regardless of class.

### Symptom 3 — no scene regression

- `song-end-test.py --require-endgame` reaches `coop_endgame_screen` clean (no new abort).
- Menu / song-select have no band skinned meshes → the new branch is a no-op there (confirm 0
  `bandMember` drops outside gameplay).
- A/B the total non-band drop count (crowd + instrument + UI) before/after the fix — it must be
  unchanged (only the band class moves).

### Reviewer adversarial bar (independent of implementer numbers)
1. Re-run §4 Symptom-1 and Symptom-2(a)/(b) on a fresh boot; confirm band garments → ~0 AND
   crowd/fling drops unchanged, on the reviewer's OWN measurement.
2. Sweep `RB3_BAND_SHARD_RATIOCAP` (e.g. 3.0 / 4.0 / 5.0) and `RB3_BAND_SHARD_WORLDCAP`
   (e.g. 90 / 110 / 150) to confirm the chosen 4.0 / 110 sits in a stable plateau (band drops
   ~0, crowd/fling drops unchanged) and is not a knife-edge.
3. Confirm match-neutrality: rb3 `git diff src/` empty; only the engine file + the pin line
   changed.

---

## 5. EVIDENCE (this planning pass)

- `/tmp/rp8-pose-footwear-shard/shard_evidence_9821.txt` — SHARD_GUARD/SHARD_RATIO drops
  (24,173 total), per-mesh bind/world/ratio/bone0; the per-class table in §1 is derived here.
- `/tmp/rp8-pose-footwear-shard/shard_evidence_9822.txt` — C8_PROBE bone-dir dump proving band
  garments bind `char/char/main/skeleton_unshared.milo` (root=playerN) while crowd extras bind
  `char/extras/*.milo` with a genuine ~200u skin-vs-bind smear.
- `/tmp/rp8-pose-footwear-shard/run{1,2}.harness.log` — both runs reached `game_screen`, hard,
  song playing (PASS).

Key engine reference points (current pin `15ce606`):
- V24 ratio guard: `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:4915` (the `degenerate`
  line); guard block opens at L4788, SHARD_DBG attribution at L4926-4945.
- Reusable band detector: same file L4040-4043 (`strstr(wdir->mStoredFile, "skeleton_unshared.milo")`).
- wave-5 WorldXfm recompose (the cause-fix, untouched): L4104-4156.
- per-bone composed-skin finite guard (untouched backstop): L4203-4236.
