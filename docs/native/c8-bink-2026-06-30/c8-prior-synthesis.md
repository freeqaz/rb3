# C8 rotation-basis / feet-in-floor — definitive prior synthesis (2026-06-30)

**Agent:** C8-prior-synthesis (Opus). Read-heavy, no native runs (fresh measurement
is the measurement agent's job). Engine pin at read time = `998b8734`.

**TL;DR for the orchestrator.** After `491288ec` (the rotation-basis rest-bake), the
two RB3 char symptoms are **ONE root problem expressed in two layers**, but the
historically-"blocked" ground truth is **NO LONGER BLOCKED** — DC3 captured live Xbox
foot telemetry (Xenia/RSP) and GREEN'd its own feet gate on 2026-06-11. The single
most tractable next step for RB3 is to **port DC3's deterministic post-poll foot-plant
(Wave-6 Lane A) into RB3's `App::RunOneFrame`** — RB3 already has the identical
`CharIKFoot::DoFSM` + `TheTaskMgr.Poll()` seam and currently has NO feet gate at all.
What genuinely still needs an emulator is the **thin-geo rotation-basis (symptom B)
"correct local pose at deep-flex beats"** number — and even that can be sourced from
**rb3-xenon** (which exists) the same way DC3 used Xenia, rather than being a true
hard blocker.

---

## 1. Feet-in-floor (symptom A): root-cause state after `491288ec`

`491288ec` is the **C8 rotation-basis rest-bake** fix, NOT a feet fix. It lives in RB3
`BandCharacter.cpp` as `NativeCharSpaceRestXfm` (src lines 864–878): it captures the
per-member bone's rest WorldXfm **relative to the member root** (`rest = world_rest *
inv(rootWorld)`) so the inverse-bind offset is placement-independent and survives
animation. It fixed the head/hands/torso *bind basis*; it did not (and cannot) fix a
bone whose *world pose* is geometrically wrong downstream. Two RB3 docs establish this
cleanly:

- `docs/native/render-polish-2026-06-11/task-ik-mispose-impl.md` §2c **re-derives the
  C8 offset and proves it mathematically correct** (`skin = rootWorld · inv(L_rest_local)
  · L_local(t)`, placement cancels for *any* `L_local(t)`). So the residual smear is
  downstream of the rebind, in the pose pipeline.
- The same doc §2a-2b is the decisive RB3 measurement: with `C8_PROBE`/`IK_SHARD_VERT`,
  the **raw animated pose flings extremity bones** (pelvis Y+78 → ankle Y-97 → toe
  Y-104 = a 186-unit leg) and this is **byte-identical IK-on vs `RB3_NO_IK=1`** (frame-121
  jump `(17.5,1.8,32.2)→(2.4,123.7,36.1)` identical). IK only *amplifies* the spread
  (so disabling it reduces V24 guard-drops), which is why earlier waves mislabeled it
  "left-limb IK mispose."

**RB3's own feet state is essentially UNINSTRUMENTED.** RB3 has no
`FeetNotBelowFloorDuringGameplay`-style gate or `toeZ/footDataValid` telemetry (grep
empty). The RB3 feet evidence is the *band garment guard-drop* proxy (V24 ratio test
drops boots/pants/gloves whose toe/ankle bones fling), measured via
`band-closeup-capture.py` + `SHARD_RATIO_DBG`. So "the RB3 feet sink ~4u" is largely an
**inference from the shared DC3 root cause + the RB3 fling proxy**, not a direct RB3
toe-Z measurement. That is itself a gap a measurement agent should close.

### What the shared DC3 trail establishes (the real ground truth)

The DC3 feet-in-floor saga (`dc3-decomp/docs/sessions/2026-06-08-feet-reverify-data.md`,
`2026-06-09-xenia-xbox-foot-truth.md`) is the authoritative root-cause work for the
shared engine, and it is now **RESOLVED with Xbox ground truth**:

- **Xbox ground truth captured** (Xenia headless + GDB-RSP read of `mWorldXfm.v`, with
  the VMX128 `+0x78` offset fix): **Xbox plants the toe** (Z ∈ [0.006, 0.53], never
  negative), ankle ~4–5 above floor, pelvis ~36–39. **Native sinks the toe to ~−4.2**,
  ankle ~0. So the bug is a **REAL native divergence, not "Xbox sinks too"**
  (`2026-06-09-xenia-xbox-foot-truth.md`, "DECISIVE — Xbox TOE captured").
- **Frame-matched localization** (Wave-6 Lane A, `0f83a3de`): at the **shallow** pose
  (pelvis 39–41) native and Xbox MATCH (knee ~−20°, toe ~0). At the **deep crouch**
  (pelvis 33–35) native knee under-bends to **−32° (Xbox −57°)** AND the ankle
  under-rotates to **+12° (Xbox +35°)** → the native ankle drops ~4 and the rigid foot's
  toe sinks to −4. The over-extension is a **CLIP/anim-layer QUAT under-bend at the
  deep-crouch beats** — consistent with Push-12h's "the runtime-played clips carry
  `bone_footik.pos = (0,0,0)`; native plays clips lacking the build-time `analyze_footik`
  foot-plant bake."

### What is REFUTED (do not re-chase — each with a citation)

| Claim | Status | Source |
|---|---|---|
| "It's a left-limb **IK** mispose" | **REFUTED** — pose is byte-identical IK-on/off | task-ik-mispose-impl.md §2a, §2d |
| "**Toe-channel** LP64 decode bug" | **REFUTED** — toe tracks ankle by correct rest offset; toeLocal vertical = 0 | feet-reverify §"DECISIVE"; ClipPoseFixture drift 0.000 |
| "Leg/pelvis bone **LOCAL lengths change** rest→live" (the prior "NEW LEAD") | **REFUTED — RED HERRING** | feet-reverify FIRM #2: the two columns are `neutral.iks` vs the rendered char skeleton = *different instances*; femur LOCAL is **constant 17.7 across all 150 gameplay frames** on Xbox |
| "Discarded IK is the **sinker**" | **REFUTED** — IK-skip is a no-op (IK already inert) | feet-reverify Push 2 |
| "**CharLocalIKScope** re-root fixes it" | **REFUTED** — empirically a no-op | feet-reverify Push 4 |
| "Engine **math** (IK/transform/decode) is the bug" | **REFUTED** — 8 independent verifications faithful-to-Xbox-asm | feet-reverify Push 3–6 |
| "**Poll-order** (IK-before-pose) is THE fix lever / is nondeterministic in the sort" | **REFUTED** as a clean lever — `CharPollableSorter::Sort` is already name-deterministic; the matched in-engine IK *diverges* on the native bone rest-frame; the residual is the anim's intended root-crouch | Wave-5 Lane A verdict |
| "Bone-length / femur shrink" | **REFUTED** (red herring) | xenia-xbox-foot-truth Push 9 |

### The standing NEW LEAD (corrected)

The old RB3-memory "NEW LEAD = leg/pelvis bone local-length rest→live" is **dead** (red
herring above). The **current** standing lead, from frame-matched Xbox data, is:
**the gameplay clip's deep-crouch knee+ankle QUAT under-bends ~25° vs Xbox because the
runtime clips lack the `analyze_footik` foot-plant bake (`bone_footik.pos = 0` → the
foot-plant FSM never locks).** DC3 chose NOT to chase the clip-bake (content-pipeline
territory) and instead **asserts the Xbox-correct RESULT** — a deterministic post-poll
analytic 2-bone plant that lifts only a below-floor toe.

---

## 2. Are symptom (A) feet-sink and (B) thin-geo rotation-basis ONE problem?

**Verdict: SAME family, DIFFERENT layers — and they need different fixes.** They are
*not* "the per-member skeleton basis is wrong rest→live" as a single shared mechanism.
Argued from the code + priors:

**(A) feet-sink** is a **pose-pipeline output** error: the `CharBonesMeshes::PoseMeshes`
→ skeleton WorldXfm pose itself is geometrically wrong at deep-flex beats (knee/ankle
QUAT under-bend). The *bind* is fine; the *animated local pose* is under-flexed because
the foot-plant data/IK that Xbox applies is absent/inert on native. This is downstream
of the C8 rest-bake (task-ik-mispose §2c proves the C8 offset is correct), and it is the
**leg/foot chain specifically**.

**(B) thin-geo rotation-basis** is a **bind-basis** error on the *rebind* of long-thin
meshes (footwear `_skin.2`, guitar `_strings`, hair, fingernails, gloves) onto the
member's own per-member skeleton. RB3 `BandCharacter.cpp:1158-1172` names it exactly:
"the authored offset was baked against the magnet basis … a vertex at radius R from the
bone with a rotation error θ flings by ~R·sin(θ) — so LONG-THIN geometry shards while
compact torso geometry survives." The torso rebind (`RebindOutfitBonesToOwnSkeleton`)
works for compact geometry; the head/hands `RebindHeadHandsAtRest` rest-bakes an exact
inverse-bind; but the `RB3_BOUND_REBAKE` experiment (1190-1201) PROVED a bind-side bake
**cannot** repair the divergence — far-from-bone verts persistently smear to 200–460u,
so the V24 guard correctly drops them.

**Why they share a *family* but not a *mechanism*:**
- Both are "the native per-member skeleton's **live posed orientation** diverges from
  what the authored/Xbox data expects." For (A) the consequence is a sunk foot; for (B)
  the consequence is a sheared thin mesh. The common denominator is the **live LOCAL pose
  basis at flex/deep-bend** — the same QUAT under-bend that sinks the foot is a
  rotation-basis error that shears a boot/string skinned across that joint.
- BUT (A)'s fix is **make the leg pose reach the floor (a result-assertion plant, or the
  footik clip bake)**, which is a *pose* fix. (B)'s fix is **make the rebind's bind basis
  match the live pose basis**, which is a *bind* fix — and the docs prove a bind-only bake
  fails while the underlying pose basis is wrong. So **(B) is genuinely gated on (A)/C8
  being solved**: `char-rebake-scope.md` states this directly ("The real blocker behind
  BOTH items is C8 … Until C8 is solved, every thin-geo / rotating-accessory rebake
  reintroduces a rotation-flung slab. When C8 lands, reopen footwear + extras +
  fingernails + gloves together as one thin-geo batch").

**Conclusion:** treat (A) as the **primary** problem (the pose-pipeline basis), and (B)
as **downstream of (A)**. Fixing (A) is what unlocks (B). They are not two independent
bugs and they are not literally one mechanism — (B) is the thin-geo amplifier of the same
deep-flex pose error that sinks the feet.

---

## 3. The SINGLE most-tractable next code lead vs what needs emulator ground truth

### Most-promising tractable lead (measurable WITHOUT an emulator): port DC3's deterministic post-poll foot-plant into RB3

DC3 GREEN'd its feet gate (`0f83a3de`, Wave-6 Lane A) with `Dc3RunPostPollFootPlant` —
an analytic 2-bone clean plant run from `App::RunWithoutDebugging` **after**
`TheTaskMgr.Poll()` (all servo/facing + the final pelvis crouch are done) and **before**
Sample/Draw, so it is **order-independent** (sidesteps the entire poll-order rabbit hole
that consumed Pushes 7–16). It lifts ONLY a below-floor toe (one-directional, strict
improvement, reverts on divergence), default-ON, all `#ifdef HX_NATIVE`.

This is directly portable to RB3 because:
- **RB3 has the same `CharIKFoot::DoFSM`** (`src/system/char/CharIKFoot.cpp:26`) with an
  existing `#ifdef HX_NATIVE` branch (line 87) — the same class DC3 hooked.
- **RB3 has the same seam**: `App::RunOneFrame` calls `TheTaskMgr.Poll()` (App.cpp:561)
  before Draw, exactly where DC3 lands the plant.
- **RB3 has NO feet gate yet**, so step 1 is cheap-and-high-value: add a
  `band-closeup`-driven feet/toe-Z telemetry (drummer/feet framing via
  `{rb3_force_shot}`) to get a *direct* RB3 toe-Z number (today RB3 only has the V24
  garment-drop proxy). This both (a) confirms the RB3 sink magnitude and (b) gives a
  pass/fail gate to verify the port against — entirely in native, no emulator.

Why this is the best lead: it is the **already-proven shared-engine fix**, the
infrastructure (CharIKFoot, the post-poll seam, the deterministic harness) is all present
in RB3, and it requires zero emulator data to *implement and verify the result* (the gate
is "toe ≥ floor"). It also de-risks (B): a correctly-planted leg pose is the prerequisite
the thin-geo rebake needs.

Secondary tractable lead (if a measurement agent wants a faithfulness check first): run
the band-closeup harness with the **drummer/feet framing** + `IK_SHARD_VERT`/`C8_SLOT`
to confirm RB3's deep-flex leg matches DC3's −32°/−4 signature, establishing the symptoms
are the same shared-engine bug before porting the fix.

### What GENUINELY needs emulator ground truth — and why it is NOT actually blocked

The one thing a pure-native effort cannot self-validate is **whether a given fix is
FAITHFUL to the original vs merely "looks right"** — specifically the **correct LOCAL
knee+ankle QUAT at the deep-crouch beats** (DC3's Xbox −57° / +35° numbers). DC3's
post-poll plant deliberately *asserts a result* rather than reproducing the faithful clip
pose, precisely because the faithful number required Xbox capture. For RB3 the equivalent
faithful target (the band's idle/move leg pose, and the thin-geo bone's live LOCAL basis
for symptom B) needs a ground-truth pose dump.

**Crucially, this is NOT a hard blocker for RB3:** `rb3-xenon` **exists**
(`/home/free/code/milohax/rb3-xenon`) — the Xbox-360 RB3 port — which is the exact analog
of the Xenia path DC3 used. The same GDB-RSP / IK-telemetry rig pattern
(`2026-06-09-xenia-xbox-foot-truth.md` "Reproduction" + the VMX128 `+0x78` bone-layout
fix) is reusable to read RB3's rendered `bone_*-toe/ankle/knee.mesh` world Z and the
thin-geo bone's LOCAL basis. So "needs emulator ground truth" should be reframed as **a
known, DC3-proven capture procedure to re-run against rb3-xenon**, not a months-long
blocker. The prior RB3 docs (task-ik-mispose §4, char-rebake-scope) call this a P0 hard
blocker because they predate DC3's Xenia breakthrough and the recognition that rb3-xenon
gives RB3 its own ground-truth path.

### Recommended sequencing

1. **(native, cheap)** Add RB3 feet/toe-Z telemetry to the band-closeup harness (feet
   framing). Confirm the RB3 sink directly (close the inference gap).
2. **(native, the win)** Port `Dc3RunPostPollFootPlant`-style deterministic post-poll
   2-bone plant into RB3 `App::RunOneFrame` (after `TheTaskMgr.Poll()`), HX_NATIVE +
   opt-out, lift-only-below-floor. Gate on the step-1 telemetry. This is the symptom-A win
   and the prerequisite for (B).
3. **(optional, faithfulness)** Re-run the DC3 Xenia capture procedure against
   **rb3-xenon** to get the faithful deep-flex leg LOCAL (and the thin-geo bone basis),
   to validate the plant's result and to unlock the symptom-(B) thin-geo rebake batch.
4. **(deferred)** Reopen footwear/strings/hair/fingernails/gloves as one thin-geo batch
   only after the leg pose is correct (per `char-rebake-scope.md`).

---

## Appendix — primary sources read

RB3: `docs/native/c7c8-ik-mispose-findings-2026-06-20.md`;
`docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md` (full);
`docs/native/render-polish-2026-06-11/task-ik-mispose-impl.md` (the decisive RB3 doc);
`docs/native/converge-2026-06-20/deferred/char-rebake-scope.md`;
`src/system/bandobj/BandCharacter.cpp` (`NativeCharSpaceRestXfm` 864, `RebindOutfitBonesToOwnSkeleton`
1021, `RebindHeadHandsAtRest` 1173, `RebindInstStringsToRestBasis` 1463);
`src/system/char/CharIKFoot.cpp`; `src/App.cpp` (RunOneFrame/TheTaskMgr.Poll).

DC3: `dc3-decomp/docs/sessions/2026-06-08-feet-reverify-data.md`;
`2026-06-09-xenia-xbox-foot-truth.md` (Pushes 9–16 + Wave-5/Wave-6 Lane A — the
resolution, `0f83a3de`).

Tooling present in RB3: `scripts/native/band-closeup-capture.py` +
`{rb3_force_shot}`/`{rb3_director_disable}`/`{rb3_cur_shot}` (deterministic closeup pin);
`IK_SHARD_VERT`/`C8_PROBE`/`C8_SLOT`/`BONE_PROBE_MINFRAME` engine probes;
`RebindCrowdCharBonesToOwnSkeleton` (Crowd.cpp:911). `/home/free/code/milohax/rb3-xenon`
exists (the RB3 ground-truth path).
