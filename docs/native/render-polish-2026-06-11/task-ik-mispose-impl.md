# task-ik-mispose — C8 residual: the "IK mispose" is actually a RAW-POSE fling (IK-independent)

Render-polish wave 4 (IK follow-up to the C8 land). Ports 9001-9009.
Engine worktree `wt-task-ik-mispose` (`/home/free/code/milohax/milo-native-engine-worktrees/task-ik-mispose`),
rb3 worktree `wt-task-ik-mispose` (`.claude/worktrees/task-ik-mispose`).

**STATUS: partial — complete diagnosis, premise REFUTED, no safe land this wave.**
**verified: false** (no fix attempted; this is a diagnosis deliverable, as the task allows).

---

## TL;DR (the surprising result)

The wave-3/C8 framing — "the residual is a **left-limb IK** mispose" — is **WRONG**.
Per-bone, per-vertex attribution (engine `C8_PROBE` + a new `IK_SHARD_VERT` probe)
shows the residual garment guard-drops are caused by the **RAW ANIMATED POSE** flinging
extremity bones to absurd world coordinates (a finger bone reaches world **Y=123**, a
foot reaches **Y=-108** while the hip is at **Y=+78** — a 186-unit leg). This is
**IK-independent**: with `RB3_NO_IK=1` the very same bones land at the **same** flung
world positions (frame-121 jump from `(17.5,1.8,32.2)` → `(2.4,123.7,36.1)` is
byte-identical IK-on vs IK-off). IK only *amplifies* the spread (~1313 vs 27
frames with bone|Y|>60 over a burst), so disabling IK reduces drops — but the
pose was already broken before IK runs.

This is the **same engine class as the DC3 "feet-in-floor" bug** (`[[dc3-feet-in-floor-anim]]`),
but worse: DC3 *sinks* the foot ~4u; RB3 *flings* the whole leg/finger chain 100-186u.
Both are the **gameplay move/idle pose itself** coming out geometrically wrong on native,
NOT the IK effector apply and NOT an L/R-handedness error in the IK solver.

The C8 character-space rest-bake offset is mathematically **correct** (re-derived below);
it cannot fix a bone whose *world pose* is wrong — the smear is downstream of the rebind,
in the pose pipeline (`CharBones`/`CharBonesMeshes::PoseMeshes` → skeleton WorldXfm).

---

## 1. SYMPTOM (repro)

After the C8 land (master `491288ec`), the residual `char-render` drops are band garment
meshes whose verts weight to leg/finger bones: default guard-on band drop attribution
(`SHARD_DBG=1`, full song burst, port 9006):
```
25619  fingernails_resource.mesh        (#1 — hand/finger, matches verify-c8 §6)
18712  femrockboots_resource.mesh
12830  skinnyjeans_resource.mesh
11589  leatherplaid_skin.2.mesh
10840  drivinggloves_resource.mesh
 ...   thighboots / wrestlingboots / bondagepants / suitpants / hotpants_socks ...
```
The guard (V24 ratio test, world-AABB / bind-AABB > 2.0x) drops these → band members
render **partially dressed** (missing boots / pants / fingernails / gloves). Visible
symptom = the documented char-render residual. (Hero closeups are camera-rare in the
harness; per verify-c8-land the drop-rate is the authoritative garment gate.)

## 2. ROOT CAUSE — the raw animated pose flings extremity bones; IK is near-inert

### 2a. The bone WORLD itself flings (not the offset, not the IK)

`C8_PROBE` time-series of `bone_R-middlefinger03` (engine prints bone `WorldXfm().v`),
gameplay burst, `C8_EVERY=5`:

| frame | IK ON `w=` | IK OFF (`RB3_NO_IK=1`) `w=` |
|---|---|---|
| 15-115 (load rest) | `(17.5, 1.8, 32.2)` | `(17.5, 1.8, 35.9)` |
| **121** (anim starts) | **`(2.4, 123.7, 36.1)`** | **`(2.4, 123.7, 36.1)`** |
| 121 (other members) | `(21.3,118.2,…)`,`(-6.6,186.1,…)` | `(-15.3,155.1,…)`,`(20.9,118.2,…)` |
| 171-393 | oscillates Y≈84-192 | oscillates Y≈84-192 |

At frame ~121 the venue idle choreography starts and **every** band member's finger/hand
bones jump to world **Y≈100-186** (the character body is at Y≈24). The jump value is
**identical IK-on vs IK-off** → IK does not cause it.

Whole-skeleton confirmation (`C8_SLOT`, frame 285, the exploded `player0` skeleton):
```
bone_pelvis    Y=78.6    bone_R-thigh   Y=78.9    bone_R-knee  Y=64.1
bone_R-ankle   Y=-97.3   bone_R-toe     Y=-104.0  bone_L-ankle Y=-108.6  bone_L-toe Y=-114.9
```
The leg spans **Y=+78 (hip) → Y=-108 (foot) = 186 units**; Z (height) stays ~37 throughout.
A standing-idle leg is ~30u. The pose is geometrically impossible → corrupt, not extreme.

### 2b. The smear is a real fling, attributed to the leg/finger bones

`IK_SHARD_VERT` (new probe — finds the single worst-deviating skinned vertex and its
dominant bone) on the smeared frames:
```
mesh='thighboots'   wext=402 worstBone='bone_R-thigh'  boneWorld.v=(11.6,181.6,41.2)
mesh='suitpants'    wext=248 worstBone='bone_pelvis'   boneWorld.v=(20.5,103.1,35.9)
mesh='fingernails'  wext=350 worstBone='bone_R-middlefinger03' boneWorld.v=(2.4,123.7,36.1)
mesh='wrestlingboots' bindExt=25  worldExt=381  ratio=15.2  DROP   (SHARD_RATIO_DBG)
```
A 25u boot smears to a 381u world AABB (15x) because its toe/ankle bones are flung to
Y≈-100..-150 while its thigh bone is at Y≈+180. The V24 guard correctly refuses to draw it.

Important negative result: `REBIND_DRAW_SKINPOS` (per-bone-ORIGIN |skin.v − boneWorld.v|)
reports **max 46u, ZERO flings >120u** — the bone *origins* compose cleanly. The fling is
in **far-from-origin verts** riding a bone whose rotation+translation is in a broken pose.
This is why origin-only metrics (REBIND_DRAW_SKINPOS, C8_SLOT skin.v) looked fine while the
mesh still smeared — you must measure the worst *vertex* (IK_SHARD_VERT does).

### 2c. The C8 rest-bake offset is CORRECT — re-derived

`NativeCharSpaceRestXfm` returns `rest = Multiply(worldRest, inv(rootWorld))`. The Milo
convention is `Multiply(child,parent,out) ⇒ world = local-applied-first` (confirmed:
`Trans.cpp:138 Multiply(mLocalXfm, mParent->WorldXfm(), mWorldXfm)`), so
`rest == L_rest_local` (bone-relative-to-member-root). The bake is
`offset = Multiply(meshWorld(=I), inv(rest))` (BandCharacter.cpp:1270). Composing at draw:
```
skin = offset · boneWorld(t)
     = inv(L_rest_local) · (rootWorld · L_local(t))      [boneWorld = rootWorld∘L_local]
     = rootWorld · inv(L_rest_local) · L_local(t)        [placement cancels]
```
Placement-independent and correct **for any L_local(t), including IK output**. ⇒ the
offset is not the bug. The bug is that `L_local(t)` (the bone-relative-to-root pose the
clip produces) is itself broken — pelvis→foot 186u.

### 2d. Why IK still reduces the drop COUNT (the wave-3 measurement was real but mis-attributed)

`RB3_NO_IK=1` does drop the band guard-drop rate (verify-c8: 20.4→4.9; reviewer 18→7.6)
— but NOT because IK *causes* the fling. The bone WORLD is identical IK-on/off; IK adds a
*little* extra spread on top of the already-broken pose (e.g. fingernails worstVert dev
18.4 IK-on → 11.1 IK-off; bone|Y|>60 frames 1313 IK-on → 27 IK-off). So disabling IK
nudges a borderline mesh back under the 2.0x ratio threshold for *some* frames, but the
underlying pose corruption remains. The "RB3_NO_IK ⇒ −80% drops" reading led the prior
waves to call this "an IK mispose"; the per-bone-world A/B refutes that.

## 3. WHY the pose is broken (hypotheses, ranked) — for the wave-5 fix

The defect is in the pose pipeline that writes the skeleton WorldXfms, NOT the rebind and
NOT the IK. The shared decomp char math is highly matched (CharIKFoot::Poll 100%,
CharIKHand::Poll 96.1%, CharIKFingers::Poll 86.7% — and even disabling all of it doesn't
change the fling). Candidate causes, in priority order:

1. **(most likely) Native bakes venue/stage placement into the bone WORLD before the pose
   composes, double-applying or mis-spacing it** — the exact DC3 `CharLocalIKScope` /
   "inputs/space" conclusion (`[[dc3-feet-in-floor-anim]]` Push 3-7: "native bakes
   venue-world into bone worlds before IK; Xbox runs character-local"). RB3's fling is the
   same family, larger. The clean-vs-exploded split (player0 has a `root=''` pelvis at the
   clean `(0,1.6,37.6)` AND a `root='player0'` pelvis at `(18.6,78.6,37)`) shows TWO
   skeleton instances for the member — the animated one is the one whose chain explodes.
   Next step: trace the bone LOCAL translations down the exploded chain (the new
   `BONE_PROBE_MINFRAME` lever delays the one-shot BONE_PROBE past frame 121 to capture
   this — note: in this session it did not fire on the leg meshes because they draw via a
   GeomOwner with NumBones<8 / cache-skip; widen the gate or hook `CharBonesMeshes::PoseMeshes`
   directly).

2. **A `CharBones`/`CharBonesMeshes::PoseMeshes` decode or hierarchy-compose divergence
   specific to the extremity chains** (fingers, toes). DC3 EXONERATED the leg-bone decode
   (rotation-only, stable bind locals) for its *sink*, but RB3's *fling* is far larger and
   may be a different decode path (the finger chains use `CharServoBone`/`CharIKFingers`
   MoveFinger writing `DirtyLocalXfm` from world-space math). Check whether
   `CharUtlFindBoneTrans` resolves the finger/toe bones to the per-member animated instance
   vs the shared magnet (a name-collision could pose the wrong instance).

3. **The member root world CHANGES between the (clip-free) C8 rest capture and draw**
   (placement/teleport applied after load). If `rootWorld_capture ≠ rootWorld_draw`, the
   offset's baked `rootWorld_capture` factor no longer cancels `rootWorld_draw` — a partial
   smear. (Lower probability: the fling is 186u and Z-stable, more consistent with a pose
   than a placement delta; but worth a `rootWorld@capture` vs `@draw` compare.)

## 4. FIX DESIGN (for wave 5 — do NOT force-land now)

This is the **hardest open char issue** and shares a root with a DC3 bug that is STILL
UNFIXED after 7 ultracode pushes. Do not attempt a risky landing. Concrete plan:

1. **Get a ground-truth pose.** The DC3 trail is blocked on Xenia capture (dancer unposed,
   async stall). RB3 may fare better: capture the band idle pose on Xenon/Xbox (rb3-xenon)
   or Dolphin/Wii for `bone_R-thigh`/`bone_R-ankle` LOCAL (relative-to-root) translations
   during the venue idle clip. If Xbox/Wii ALSO produce a 186u leg ⇒ the pose is faithful
   and the SHARD_GUARD threshold is too tight (relax it / per-character-scale it). If Xbox
   produces a sane ~30u leg ⇒ native pose-pipeline bug (proceed to 2).
2. **Localize native vs reference at the LOCAL-pose layer.** Hook `CharBonesMeshes::PoseMeshes`
   / `CharUtlFindBoneTrans` (RB3 src/system/char) and dump each leg/finger bone's LOCAL
   xfm per frame; compare to the reference. A wrong LOCAL translation ⇒ decode/scale; a
   correct LOCAL but wrong WORLD ⇒ a hierarchy/placement (space) bug → port the DC3
   `CharLocalIKScope`-class character-local fix.
3. **Match-neutrality:** any fix is HX_NATIVE-gated in `src/system/char/*` or engine-side
   (`Rnd_Wgpu_RB3.cpp`); the Wii arm stays byte-identical.

NON-fixes (ruled out this session, do not re-try): repointing/rebaking the offset
(C8 already correct); disabling/handedness-flipping the IK solver (IK is near-inert, the
fling is IK-independent); guard-exempting the meshes (`RB3_GUARD_EXEMPT_REBOUND` — drew
full-screen slabs per prior wave, because the poses are genuinely broken).

## 5. VERIFICATION (of the diagnosis)

- **Bone-world fling, IK-independent:** `C8_PROBE` time-series, IK-on vs `RB3_NO_IK=1`,
  bone `bone_R-middlefinger03` jumps to identical `(2.4,123.7,36.1)` at frame 121 in BOTH
  (`/tmp/rb3-kbd2game-9009.log` IK-on, `…-9001.log` IK-off).
- **Whole-leg fling:** `C8_SLOT` f=285, player0 pelvis Y+78 / ankle Y-108 (`…-9003.log`).
- **Worst-vert bone attribution:** `IK_SHARD_VERT` → thigh/pelvis/finger bones at Y±100-180
  (`/tmp/rb3-kbd2game-9007.log`).
- **Offset is correct:** re-derived from `Trans.cpp:138` Multiply convention (§2c).
- **Bone origins compose cleanly:** `REBIND_DRAW_SKINPOS` max 46u, 0 flings >120u
  (`…-9006.log`) — proves the fling is in far verts via a bad pose, not the bind.
- **Default drop attribution** = fingernails/femrockboots/skinnyjeans/drivinggloves
  (`…-9006.log`), matching the verify-c8 residual list.

## 6. WHAT I CHANGED

- **rb3 src: NOTHING.** No `src/` edits ⇒ Wii build byte-identical trivially.
  (`git status --short src/` empty.)
- **engine (native-only `src/platform/Rnd_Wgpu_RB3.cpp`, NOT compiled into the Wii image):**
  two **diagnostic, env-gated, render-inert** probes, committed on `wt-task-ik-mispose`
  @ `3b32d82`:
  - `IK_SHARD_VERT=<mesh-substr|*>` — worst-flung-vertex → dominant-bone attribution
    with composed-skin vs bone-world rotation rows (inside the SHARD_GUARD ratio block;
    requires the block live: default, or `SHARD_GUARD_OFF=1 SHARD_RATIO_DBG=1` to see
    would-drop meshes).
  - `BONE_PROBE_MINFRAME=<N>` — delays the one-shot `BONE_PROBE` past frame N so the leg
    chain is sampled after animation starts.
  - Also cherry-picked the scout-c8 `C8_PROBE` (`6a324be`→`ee99bba`) into this engine
    branch for per-slot attribution.
- Engine pin UNCHANGED (probes are diagnostics; nothing to land for behavior).

## 7. LANDING NOTES (for the orchestrator)

- **Nothing to land for behavior** — this is a diagnosis. The two new probes are optional
  to keep (they're useful for wave-5; if landed, cherry-pick engine `3b32d82` onto engine
  main; it touches only `src/platform/Rnd_Wgpu_RB3.cpp` inside the SHARD_GUARD ratio block
  (~L4605 region) and the BONE_PROBE gate (~L3867 region) — both diagnostic, no pin bump
  needed for the live build since they're env-off by default; but a pin bump IS needed if
  you want them in the composed binary).
- **Conflicts with siblings:** my engine edits are in `Rnd_Wgpu_RB3.cpp` (shared with
  fret-sphere / venue-blowout / menu-fog wave-4 siblings). My two hunks are diagnostic and
  region-isolated (SHARD_GUARD ratio block + BONE_PROBE gate); cherry-pick after the
  behavioral engine fixes to avoid churn. Low conflict risk (additive, gated).
- **No order constraint** — diagnosis only.

## 8. HANDOFF — recommended wave-5 framing

Re-title the follow-up from "left-limb IK mispose" to **"native band idle/move pose flings
extremity bone chains (pelvis→foot 186u)"** — a pose-pipeline space bug, the RB3 sibling of
the DC3 feet-in-floor sink. P0 = a Xbox/Wii ground-truth LOCAL-pose capture of the venue
idle clip to decide faithful-pose (relax guard) vs native pose bug (port DC3 char-local
fix). The C8 fix should stay landed (it's correct + strictly reduces drops); this residual
is independent of it.
