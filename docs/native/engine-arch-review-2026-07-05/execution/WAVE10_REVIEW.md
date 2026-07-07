# Wave 10 — Pre-dispatch Review (Fable adversarial pass)

**Reviewer:** Fable subagent. **Input:** `WAVE10_KICKOFF.md` (draft), `README.md` Waves 8–9 +
Wave-10 menu, `W2.8d/STATUS.md` (full), `WHITE-fix/STATUS.md` + `staged-patch-scene-side.md`,
`REBASELINE/STATUS.md` §(b), plus source spot-checks in `rb3/src/system/bandobj/BandCharacter.cpp`,
`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` / `Rnd_Wgpu.cpp`, `src/gfx/UniformStructs.h`,
and `rb3/native/tests/test_farvert_rotation_oracle.cpp`. Engine pin `10a9ca6`.

## VERDICT: **dispatch-with-amendments**

The lane structure is right (the two designed paths are the correct survivors; the deferred list is
correct; the process rules carry). But the draft contains one **impossible-as-written hard exit**
(A2), one **instrument-validity gap** the wave's own W2.8d.S2 record predicts (A3), and one
**collision-control answer** (A1) where "disjoint regions in one TU" does not actually address the
three real hazards. Eight amendments; none require redesigning a lane.

---

## Amendments

### A1 (answers R-A) — Do NOT dual-write `Rnd_Wgpu_RB3.cpp`. Pre-land Lane B's engine hunks as a serialized step 0.

**Verified:** the two regions are genuinely disjoint — venue-upload site at
`Rnd_Wgpu_RB3.cpp:1430–1620` (accessor cluster `:1167–1225`), dualskin probe block at
`:4375–4560`, ~2,900 lines apart, different functions, no shared statics (the probe's
`sBindWorld`/`sBindFrame`/`sWorstSeen` are block-local; the venue site uses the file-scope
`sVenue*` accessors the probe never touches). **But textual-hunk disjointness is the only hazard it
neutralizes.** The three real hazards are untouched: (1) `git add src/platform/Rnd_Wgpu_RB3.cpp`
stages the *whole file* — under concurrent lanes it captures the sibling's uncommitted,
possibly-broken hunks into your commit; hard rule 2 ("stage only your own files") is unenforceable
at sub-file granularity. (2) Both lanes build the ONE shared engine working tree
(`add_subdirectory` — per-lane build dirs isolate objects, not source); a mid-edit syntactically
broken TU breaks the sibling's build nondeterministically. (3) Edit-tool read-modify-write races on
the same file. These are exactly why Waves 3–9 enforced single-writer, and Wave 9 explicitly made
Lane B stage rather than land for this reason. Note also the staged patch's own header assigns the
land to "the coordinator / Lane A" — the kickoff's Lane-B ownership contradicts its input document.

**Fix:** the engine side of the WHITE patch is 3 files, ~25 lines, default-OFF inert, with all 8
anchors pre-verified against HEAD (`staged-patch-scene-side.md` §S2). Land it as a serialized
**step 0** (coordinator, or Lane B as a barrier-ordered first task that COMMITS before Lane A's
first engine edit), gated by one `drawlog 792` + `milo-engine-tests` run. After step 0, Lane B is
engine-**read-only** (reproducers, gates, flip recommendation) and Lane A is the TU's single writer
for the wave. This costs ~nothing and removes the whole question.

### A2 — The S2 hard exit "RealPathFixture GREEN flag-ON" is impossible as literally written; spell out the two-fixture re-capture protocol AND fix the probe's capture gate.

**Verified in source:** `test_farvert_rotation_oracle.cpp:457–498` — `RealPathFixture` computes
`max|asDrawn − ref|` over the **committed static file** `goldens/w2.8-farvert/live_pose.txt`
(threshold 20u). A code fix cannot turn a committed capture GREEN: re-running the gtest re-reads
the same RED numbers forever. The gate only makes sense as *re-capture* semantics: (i) the
committed flag-OFF fixture stays RED (the anti-revert instrument), (ii) a **fresh flag-ON capture
through the same dualskin probe** reads <20u. Worse, the probe cannot produce that capture: its
entry condition is `getenv("RB3_DUALSKIN_PROBE") && wext > 60.f && owner`
(`Rnd_Wgpu_RB3.cpp:4391`) — **a working fix drops wext below 60 and the instrument never fires**,
so success suppresses the evidence. S1's metric work must also parameterize the capture gate
(e.g. `RB3_DUALSKIN_MINWEXT`, probe-flag-scoped, inert otherwise) so a flag-ON GREEN capture is
mechanically possible. Amend the S2 exit to: "committed flag-OFF fixture stays RED; flag-ON
re-capture (corrected reference per A3, lowered capture gate) reads <20u; both files recorded."

### A3 — S1's "like-space metric fix" must extend to the fixture's *reference*, and the corrected fixture must be re-proven RED before S2 starts; add the placement-independent wext exit.

W2.8d.S2 documented the ΔR confound (char-space `inverse(off)` vs world-space `bw` ≈ placement
yaw) but asserted "worstSep … is real" without checking the same term there. **Checked in
source:** the shipped rebake bakes `off = meshWorld·inv(rest_char)` with `rest_char =
restWorld·inv(rootWorld)` (`NativeCharSpaceRestXfm`, `BandCharacter.cpp:932–946`;
`Multiply(mesh->WorldXfm(), invRest, mesh->BoneOffsetAt(b))` at `:1539`), and skinned-mesh
`meshWorld == I`, so `asDrawn = v·rootWorld·inv(restWorld)·W` — **placement-anchored** — while
the probe's coherent ref is `v·inv(bw)·W` with `bw` captured at first probe encounter
(`Rnd_Wgpu_RB3.cpp:4400–4434`, and the block's own caveat: "rest == the first steady frame this
mesh is drawn") — **origin/first-capture-anchored**. The 32.8u sep therefore plausibly carries a
rigid placement term (member placements measured 27–60u — the same order), i.e. the committed RED
may be partly the identical confound S2 caught in ΔR. Required: S1 re-derives the coherent
reference in like-space, re-captures + re-commits the fixture, and **re-proves it RED (>20u) on
the current build before S2 begins**. If the corrected fixture is no longer RED, STOP and escalate
— the candidate-(b) instrument story changes and S2's premise dies with it (this is a legitimate
5th-class-stop outcome, not a blocker to route around). Independently: add the
**placement-independent physical metric** as a mandatory S2 exit — `hands_naked` wext A/B
(RED baseline 105–107u, the exact metric that refuted S2's world-space attempt) with a
pre-registered target (e.g. p90 into the ≤70u clean-limb envelope, and strictly no regression) —
this gate cannot be gamed by fixture rewrites. Also fold the "corrected like-space ΔR collapsing
toward 0" exit into the S1 premise check: it is only a valid S2 gate if S1 first shows the
corrected ΔR is **measurably nonzero pre-fix and consistent with the like-space sep via
R·2sin(ΔR/2)** — otherwise it is a gate that cannot fail red.

### A4 (answers R-B) — Extraction is feasible with EXISTING machinery (no new milo parsing), but the kickoff's premise wording contradicts the repo's documented ground truth; S1 must define the extraction target operationally.

Three routes, none needing new parsing: (1) a pre-deform capture hook — at milo load the bone
`RndTransformable`s hold the authored local xfms until `SetDeformation`'s deform clip poses them
(the existing `NativeCaptureRestPoseAfterDeform` deliberately captures *after*; the authored value
is available strictly *before*); (2) a scratch-`ObjectDir` side-load of `char/main/skeleton.milo`
and `skeleton_unshared.milo` via the existing `DirLoader` (the same machinery that shares them
today — `BandCharacter.cpp:3735–3738`); (3) offline via `rb3-viewer --pose-dump` (shipped, memory
`project_rb3_viewer_wig_fix`). **However:** the kickoff's "per-member `skeleton_unshared.milo`
AUTHORED bind rotations" conflicts with the codebase's own documented ground truth at
`BandCharacter.cpp:3748–3749`: *"skeleton_unshared.milo is itself male-bind; the gender pose comes
from the outfit/clip"*, and the per-member instances are fresh loads of the SAME file — so a
*per-member* authored bind may not exist as asset data at all, and the per-member ΔR difference
(87.3° vs 68.8°) may be entirely the placement-yaw artifact S2 already named. S1 must state what
it extracts (authored local chain composed to char space; optionally ⊕ the gender deform's
finger-bone rest) and treat "asset ΔR ≈ 0 / identical across members" as a live premise-death
branch under the stop rule — an honest outcome, not a failure to route around. Related integration
requirement (kickoff is silent): the rebake must slot into the **existing
`RB3_APPENDAGE_REST_ROT` site inside `RebindHeadHandsAtRest`** (replace the rest ROTATION for
appendage meshes at `BandCharacter.cpp:1522–1539`), NOT a second pass — the once-latch
(`mNativeHeadReboundOnce`) + `mNativeBonesRebound` flags make a post-pass double-bake, and the
default rebake **destroys the pristine authored offset** (needed if S1 compares against
`inverse(offA)`; capture-before-mutate precedent exists at `:1278–1300` and `:2100`). And the
composition must live in ONE space end-to-end — mixing an asset-space rotation with a captured
translation is the exact frame-mixing that killed W2.8c (80u→2600u).

### A5 (answers R-C) — DC3 zero-init VERIFIED; accept behavioral-zero-blast.

Checked exhaustively: `SceneUniforms` is constructed in exactly two places in the engine —
DC3's `Rnd_Wgpu.cpp:1235` `SceneUniforms scene{};` and RB3's `Rnd_Wgpu_RB3.cpp:1323`
`SceneUniforms s{};` — both value-initialized (all pads zero); **no code anywhere writes
`_padPL`**; the WGSL declares `_padPL2/_padPL3` (`standard_wgsl.inc:81–82`) and never reads them;
`static_assert(sizeof(SceneUniforms)==656)` holds under a pure pad-rename
(`UniformStructs.h:41,56`); the W1.1 Dawn shader-validation gtest catches any WGSL breakage.
The "behavioral-zero-blast" claim is sound and the `pointFalloffMode` precedent (an identical
venue-only gate on the same struct, already shipped shared) applies. Recommend the coordinator
accept option (a) and record the acceptance on the kickoff.

### A6 (answers R-D) — Do not hard-gate the W2.8e land on the forearm triage; DO require the triage verdict in any flip package; and add a third hypothesis the draft is missing.

Wave 10 lands S2 default-OFF; the flip is a later coordinator decision — gating the *land* on the
triage would couple an independent cheap diagnosis to the wave's hardest item for no benefit. But
the Wave-5/6 lesson cuts both ways: an unexplained band-character visual anomaly must be
*characterized* before a hands flip is signed (Wave 5 held on an uncharacterized anomaly; Wave 6
refuted the hold **by measurement**, not by ignoring it). So: S3 stays in-wave and cheap; its
verdict is a mandatory exhibit in the eventual flip package; "H1 confirmed AND fix flag-ON does
not collapse it" is recorded as evidence of incompleteness (blocks the flip recommendation, not
the land). Add **H3**: the *rest of the character's meshes* guard-DROPped, leaving one legitimate
forearm drawn — the documented mixed-palette V24 drop family ("invisible legs/feet/hands — the
dominant only-teeth/eyes-render symptom", `BandCharacter.cpp:1416–1428`). A draw-log at the
sighting separates H1/H2/H3 in one capture (H1: forearm mesh present with detached bone worlds;
H2: torso draws present but occluded/masked; H3: sibling skin draws absent). Sequence note: the
"collapses with the S2 flag" check needs a flag-OFF baseline of the same sighting captured FIRST.

### A7 — Lane B's binary "WHITE class eliminated" gate is statistically underpowered; make the continuous paired deltas primary and pre-register thresholds.

The flood reproducer's strict-WHITE base rate is 2/5 (STATUS §1a); "0/N vs 2/5" at N=5 is Fisher
p≈0.44 — the exact trap the Wave-6 review caught in the Wave-5 flip hold. The deterministic
strength of the flood is its **continuous** signature (5/5 over-exposed, hi_frac 19–30%,
WHITE-class mid_sat 0.045): primary gates must be paired continuous deltas (`hi_frac` ↓,
`mid_sat` ↑) with N≥6/arm and numeric thresholds written into the lane brief before dispatch;
binary WHITE rate is secondary color. Include the `eng_hot` engaged arm (its WHITE keeps mid_sat
0.256 — there the luma-preserving path has real chroma to save, the flood is the degenerate
zero-chroma case). And pre-register the "authored bright moments preserved" gate concretely
(which SP-overlay/strobe scenes, which metric, what threshold) — as drafted it cannot fail red
because nothing defines failure.

### A8 — Minor: independence, tiers, and one wording fix.

(i) B.S2 "independent verify" must be a **distinct agent** from B.S1, stated explicitly (the
workflow makes this free; the draft's lane phrasing leaves it implicit). (ii) Model tiers are
acceptable (Opus both lanes; Lane A unquestionably). If A1's step-0 pre-land is adopted, Lane B
becomes measurement-only and B.S1 could drop to Sonnet *only if* A7's thresholds are fully
pre-registered; otherwise keep Opus. S3 is Sonnet-capable but sits inside sequential Lane A —
fine as-is. (iii) The kickoff describes both lanes' TU work as "both additive"; Lane A's S1 is a
**modification** of the existing probe block (metric + capture-gate changes), not additive — the
flag-OFF drawlog-792 gate must be re-run after that edit (it will pass — the block is
`getenv`-gated — but it must be *run*, not assumed). (iv) The stop rule ("5th-class refutations
must stop here") is prose inside S2's brief and thus unenforceable as written: make S1 write
`verdict: MATCH|NO_MATCH` (with the A3-pre-registered tolerances) to
`/tmp/wave10-checkpoints/A-S1.json`, and make S2's charter line 1 = read that checkpoint and, on
NO_MATCH, write the honest-negative STATUS and exit **with no code**. Same checkpoint-first
discipline CLAUDE.md already mandates for workflow resume.

---

## Direct answers to the kickoff's R-questions

- **R-A:** Serialize — but the cheap form: pre-land Lane B's tiny pre-verified engine patch as
  step 0, then single-writer holds naturally (A1). Dual-writing one TU is not acceptable even with
  disjoint regions, because the failure modes are git-staging/build/race, not textual merge.
- **R-B:** Feasible with existing loaders/tools — pre-deform hook, scratch DirLoader side-load, or
  `rb3-viewer --pose-dump`; no new milo parsing. But the "per-member authored bind" premise needs
  an operational definition and carries a documented risk of being vacuous (male-bind file, gender
  pose from outfit/clip) — the stop rule must own that branch (A4).
- **R-C:** Verified true (A5). Accept behavioral-zero-blast.
- **R-D:** No hard gate on the land; mandatory exhibit in the flip package; add H3 (A6).

## The extra checks requested

- **Does W2.8d's own data already contain the asset-level ΔR confirmation?** **No.** S1 explicitly
  deferred it ("Open confirmation deferred to A.S2 (not required for this verdict)") and A.S2 spent
  its budget on the world-space fix attempt instead. The kickoff's now-mandatory S1 is new work,
  not re-measurement — correct as drafted (subject to A3/A4's metric + definition fixes).
- **Is the stop rule enforceable as written?** Not as prose; yes with the A8(iv) checkpoint
  mechanism.
- **Gates that cannot fail red:** three found — "corrected ΔR collapsing toward 0" (A3),
  "WHITE class eliminated" at N=5 (A7), "authored bright moments preserved" without pre-registered
  scenes/metric (A7). Plus one gate that cannot pass green as written: RealPathFixture (A2).
- **Model tiers / sequencing:** tiers fine (A8); sequencing fine once step 0 is inserted; Lane A
  S1→S2→S3 order is right, with S3's flag-OFF baseline capture noted (A6).

---

## Appendix — what I checked in source

- `rb3/src/system/bandobj/BandCharacter.cpp` — `NativeCharSpaceRestXfm` (`:932–946`, char-space =
  world·inv(rootWorld)); Poll ordering `RebindHeadHandsAtRest` pre-`Character::Poll()` (`:526`),
  torso rebind + `NativeRepinHandsRigid`/`NativeConjHandsPerFrame` post (`:574–597`); the
  `RB3_APPENDAGE_REST_ROT` refuted-experiment slot (`:1248–1266, :1319–1332, :1398–1413,
  :1522–1539`) incl. the bake `Multiply(mesh->WorldXfm(), invRest, mesh->BoneOffsetAt(b))`;
  the W2.8c pristine-offset mutual-exclusion precedent (`:1278–1300`); the mixed-palette V24
  guard-DROP family (`:1416–1428`); the magnet/share ground truth + "skeleton_unshared.milo is
  itself male-bind; the gender pose comes from the outfit/clip" (`:3729–3751`).
- `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` — venue accessor cluster (`:1167–1225`),
  `SceneUniforms s{}` (`:1323`), engaged-venue upload + `pointFalloffMode` write (`:1430–1478`,
  `:1460`); dualskin probe block (`:4375–4560`): entry gate `wext > 60.f` (`:4391`), first-
  encounter world-space rest capture (`:4400–4434`), ΔR = `angleDeg(invOff.m, restW.m)` and
  fixture write (`:4500–4545`). No shared statics between the two regions.
- `milo-native-engine/src/platform/Rnd_Wgpu.cpp` — `WriteSceneUniforms` `SceneUniforms scene{};`
  (`:1234–1235`) = DC3's only construction; no `_padPL` writes anywhere in `src/`.
- `milo-native-engine/src/gfx/UniformStructs.h` — `_padPL[2]` after `pointFalloffMode` (`:38–41`),
  `static_assert(...== 656)` (`:56`). `standard_wgsl.inc:81–82` — `_padPL2/_padPL3` declared,
  never read.
- `rb3/native/tests/test_farvert_rotation_oracle.cpp` — `RealPathFixture` (`:457–498`): static
  committed-fixture read, `max|asDrawn − ref|`, 20u threshold, SKIP-if-absent; confirms the A2
  re-capture requirement.
- `rb3/native/tests/goldens/w2.8-farvert/live_pose.txt` — present (6,031 bytes, committed).
- `WHITE-fix/staged-patch-scene-side.md` — patch exists, complete (3 files, 8 hunks, anchors
  re-verified at HEAD `30d4f00`, field-offset alignment argued correctly C++ `_padPL[0]` ↔ WGSL
  `_padPL2`), and its header assigns the land to "the coordinator / Lane A" (A1).
