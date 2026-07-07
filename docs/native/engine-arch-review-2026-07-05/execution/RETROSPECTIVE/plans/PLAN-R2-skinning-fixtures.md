# PLAN R2 — Skinning oracle fixtures + oracle-validation harness

**Item:** ROADMAP.md R2. **Author:** Fable planner, 2026-07-07 (read-only research pass; every
file:line below was opened/grepped in this pass, not inherited from the record).
**Executor:** one Opus implementation lane (Wave 17 "Lane S" per OPTIONS.md §5).

---

## 1. OBJECTIVE + non-goals

**Objective.** Promote the HANDS-ADJUDICATION arm-W/arm-S methodology into permanent,
gender/mesh-split, matrix-relative gtest fixtures in the existing `rb3-tests` target, AND build
the oracle-validation harness (REPORT.md:276 — "fire each proposed oracle on known-GOOD and
known-BAD frames, report separation") so that **no metric may gate a skinning wave without a
demonstrated known-good/known-bad separation**. The suite must contain an animated-pose
inter-bone (multi-bone-blend) check that reads RED on the Wave-16 `RB3_HANDS_AUTHORED_REPOINT`
spike-fan palette — the configuration every existing numeric gate (Tier-1, Tier-2) passed while
the screen showed torn fingers (HANDS-FIX/STATUS.md gate table: Tier-1 3.1° count=0 both
genders, Tier-2 0.34u, VISUAL FAIL).

Three deliverable layers:
1. **Fixtures** — committed palette+vertex captures for known-good and two known-bad
   configurations, per gender per mesh (hands_naked M/F, gloves, fingernails, body reference).
2. **Harness** — `ValidateMetric(metric, good, bad) → SeparationReport` with a registry rule:
   gate-eligible metrics must have a green `OracleValidation.*` test asserting VALID separation.
3. **Regression pins** — (a) the Wave-15 verdict table (arm-W male 0.1°/arm-S male 3.1° PASS,
   female 28.9° FAIL-pre-fix) reproduced from committed fixtures; (b) the Wave-16 spike-fan as a
   permanent red test (a blend-tear metric that separates it from known-good); (c) the *proven
   blindnesses* pinned as EXPECT-BLIND tests (Tier-1/Tier-2 blind to the tear; `wext` not an
   oracle — VERDICT.md §6.3/§6.4) so a future wave cannot re-trust them by accident.

**Non-goals.**
- NO fix attempts, NO flag flips, NO default changes. The 8 dead cells stay dead
  (HANDS-FIX/STATUS.md disposition); this item builds the instrument that grades the *next*
  attempt (R5 engine reskin, W2.4 BandPatchMesh).
- NO Dolphin/ground-truth work (that is R1). This suite is native-internal consistency — it can
  prove a palette is *incoherent* (shard) but not that a coherent palette is *Wii-faithful*.
  State this limit in the suite header; R1 owns faithfulness.
- NO image/SSIM gates (OPTIONS.md §3(b) — killed as specified).
- NO replacement of the E1 human visual gate. The suite narrows what reaches E1; it does not
  retire it (Wave 16 is the proof one should be humble here).

---

## 2. CURRENT STATE (verified 2026-07-07)

### 2.1 The gtest target and its patterns (all exist, all verified)

- `rb3-tests` target: `native/CMakeLists.txt:690-745`. Compiles the full rb3-native source set
  minus `main_native.cpp` (`get_target_property(_RB3_NATIVE_SRCS rb3-native SOURCES)` at
  `:707-708`), links GTest, `gtest_discover_tests(... DISCOVERY_MODE PRE_TEST
  RESOURCE_LOCK "rb3_engine_singleton" TIMEOUT 180)` at `:738-744`. Binary exists and is
  current (`native/build-native/rb3-tests`, built Jul 7). New test TUs are added by appending
  one `add_executable` entry (`:709-727` pattern).
- `EngineTestFixture` / `EnsureEngineInit` (`native/tests/test_helpers.h:144-152`,
  `test_helpers.cpp:70-97`): headless SystemPreInit/SystemInit boot off
  `RB3_DATA` (default `/home/free/code/milohax/rb3/orig-assets/extracted` — **verified
  present**, contains `char/ config/ songs/ ui/ world/ ...`), GTEST_SKIP when unavailable.
  NOTE: this plan's fixture tests do NOT need the engine boot (they parse committed files +
  call engine math free functions); only the optional live-gate arm mentions it.
- **Committed-fixture oracle precedent** — `test_farvert_rotation_oracle.cpp` +
  `goldens/w2.8-farvert/live_pose.txt` (real captured live-pose data parsed by a gtest;
  synthetic arms always run; `RealPathFixture` arm reads the golden). Its header (`:14-22`)
  documents exactly the gate-blindness class this plan generalizes.
- **Env-pointed live-gate precedent** — `test_crowd_bone_oracle.cpp:15-22`:
  `RB3_CROWD_BONE_BASELINE`/`RB3_CROWD_BONE_CANDIDATE` point at fresh capture dirs, test SKIPs
  when unset, turns RED/GREEN on real captures. R2's Suite D copies this shape.
- **Bind-pose oracle precedent** — `test_hands_bind_oracle.cpp` (production math free functions
  `Multiply`/`Invert` linked via `_RB3_NATIVE_SRCS`; env fail-red `HANDS_BIND_ORACLE_PERTURB`;
  self-contained `PerturbationIsDetected` guard at `:250-270`; host-libm trig because
  `TrigTableInit` never runs in the gtest process — `:80-86`, a gotcha the new TU must copy).

### 2.2 The instruments being consolidated

- **Engine HANDS_ATTACH probe** (the Tier-1/Tier-2 source; engine commit `4c93608`, verified
  live at pin `51640ff` == current engine HEAD == `MILO_ENGINE_PIN` in
  `native/CMakeLists.txt:74`): `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:4736-4875`.
  Tier-1 rest-coherence sweep `:4813-4825` (pointer-freshness recapture), Tier-2 parent/child
  joint-attach on the uploaded palette `:4826-4848`, Tier-2-only fail-red
  `RB3_HANDS_ATTACH_PERTURB` `:4779-4789`. Mesh scope = substr match on
  `hands_naked|finger|glove` `:4766-4768`.
- **Instrument-B** (`RB3_HANDS_INSTR_B`, `:4872+`): proves per-vertex access at the probe site —
  `skinnedView[i]` (`const std::vector<GpuVertexSkinned>&`, `:2786`) yields `pos`,
  `boneIndices[4]`, `boneWeights[4]` (`src/gfx/VertexFormats.h:19`); `owner->BoneOffsetAt(b)`,
  `owner->BoneTransAt(b)`, `bt->TransParent()` all available. **This means the palette+vertex
  dump probe in §3.2 is an assembly of parts that already exist at this exact site.**
- **Analysis-side parser** — `execution/HANDS-FIX/evidence/parse_hands_attach.py` (gender split
  by nb=38/40, gate verdicts). Stays as the engine-log summarizer; the new suite supersedes it
  for *gating*.
- **The derivation script** — `HANDS-ADJUDICATION/evidence/offset_basis_derivation.py`
  (matrix-relative `angle(B·inv(R))=87.2°` closure from committed matrices; the numeric method
  the C++ metrics replicate).

### 2.3 The data that does NOT exist yet (the gap this plan fills)

- The committed evidence logs (`HANDS-FIX/evidence/arm{OFF,ON}_hands_attach.log`, 11k lines
  each) contain **summary lines only** (worst/count per block). Verified by reading them: on the
  Wave-16 flag-ON arm every hands_naked block reads `TIER1 worst=3.1deg count(>5deg)=0`,
  `TIER2 ... exactWorst=0.34u` — i.e. **the committed data is itself blind to the tear**. No
  full palette, no verts/weights. A tear-sensitive fixture therefore requires a NEW capture with
  a NEW dump probe; nothing can be salvaged from the logs for that purpose. (The logs ARE
  sufficient for the Wave-15 verdict-table numbers via `arm_summaries.txt`, but the plan
  re-captures those arms too so the regression pin runs on the same fixture format.)
- Both known-bad configurations are reproducible on today's tree: BAD-ceiling = shipped default
  (arm-C protocol), BAD-torn = `RB3_HANDS_AUTHORED_REPOINT=1` — flag verified in-tree,
  `src/system/bandobj/BandCharacter.cpp:1374` (default-OFF, kept as the documented 8th dead
  cell).
- Capture protocol exists: `scripts/native/keyboard-to-gameplay.py --song-downs 3
  --game-burst 15 --burst-interval 0.3` with `RB3_FIXED_CLOCK=1` (script verified,
  `--game-burst` loop at `:299-308`) — the exact protocol of the adjudication
  (`arm_summaries.txt` header) and of Wave 16.

### 2.4 Why Tier-1/Tier-2 are structurally blind to the Wave-16 failure (the design driver)

From HANDS-FIX/STATUS.md "Root cause" + the probe source: Tier-1 checks `off_b·restW_b ≈ I`
**per bone at a captured rest** — a static, single-bone check; the repoint cell satisfies it
exactly (authored off vs own's play-time basis, 3.1°). Tier-2 checks that parent and child
palette matrices map the **shared joint point** to the same place — a conjugation-invariant,
origin-anchored check; both the ceiling-hand (rigid per-bone conjugation preserves joints) and
the torn blend (joints attach; far verts tear) pass it. The tear lives in **multi-bone-blended
vertices under animated inter-bone deltas** — visible only to a metric that combines real
verts + real weights + the full uploaded palette at an animated pose. That triple is exactly
what the fixtures must contain.

---

## 3. DESIGN

### 3.1 Fixture inventory

Configurations ("arms", one capture run each, all on the pinned build, fixed-clock protocol):

| arm tag | env | population captured | role |
|---|---|---|---|
| `good-body` | default (all hands flags unset) | 1-2 torso/outfit body meshes per gender + head | known-GOOD (ships correct; band animates since `acd9c19a`) |
| `bad-ceiling` | default | hands_naked M+F, gloves*, fingernails | known-BAD class 1 (rigid displacement; Tier-1 87.3/42.6 modes) |
| `bad-torn` | `RB3_HANDS_AUTHORED_REPOINT=1` | same appendage set | known-BAD class 2 (Wave-16 spike-fan; Tier-1/2 GREEN) |
| `arm-w` | `RB3_NO_HEAD_REBIND=1 RB3_NO_SKIN_CLAMP=1 SHARD_GUARD_OFF=1` | hands_naked M+F | verdict-table pin (male 0.1° / female 28.9°) + BAD class 3 (frozen) |
| `arm-s` | `RB3_HANDS_SHELL_FIX=1` | hands_naked M+F | verdict-table pin (male 3.1° PASS / female 28.9° 40/40 FAIL) |

Per (mesh, member, frame) record — the `PaletteFrame`:
- header: mesh name, owner name + pointer, frame index, `nb` (38=male/40=female — the gender
  key, per WAVE16_REVIEW A5 "per-gender is an analysis-side split on nb"), arm tag, build/pin.
- per bone `b < nb`: bone name, palette-resolved parent index (engine Tier-2's pointer-matched
  resolution, `-1` if chain root), current `BoneOffsetAt(b)` (12 floats), uploaded palette
  matrix `bones.bones[b]` (16 floats, col-major as bound), bone `WorldXfm()` (12 floats).
- vertex block (static per mesh, emitted once per (mesh,ownerPtr) then referenced): for every
  skinned vertex (hands_naked ≈ 2.2k — full dump, no sampling; sampling is how W9's dual-skin
  confound hid): `pos[3]`, `boneIndices[4]`, `boneWeights[4]`.

Frames per arm: 4-6 dumps spaced across the game-burst window (the burst screenshots at the
same frames are captured alongside and committed — the human cross-reference that the BAD
frames really show the ceiling-hand / spike-fan, closing the "did we capture a torn pose?"
loophole). Estimated committed size: ~250 KB per mesh-vertex block + ~15 KB per frame record →
a few MB total under `native/tests/goldens/r2-skinning/<arm>/`. Same repo-goldens precedent as
`w2.8-farvert`.

### 3.2 The dump probe (the only engine edit)

New additive block in `Rnd_Wgpu_RB3.cpp` adjacent to the HANDS_ATTACH probe (~4736 region),
gated `RB3_PALETTE_DUMP=<comma substrs|*>` + `RB3_PALETTE_DUMP_DIR=<dir>` +
`RB3_PALETTE_DUMP_EVERY=<N frames>` (default 60). Writes one `PaletteFrame` text file per
(mesh, ownerPtr, frame) directly to the dir (file-write, not stderr — avoids interleaving with
concurrent probes; format = `#`-commented whitespace floats, the `goldens/w2.2-hands/README.md`
convention). Mesh selector is env-driven (NOT hardcoded to the hands substr list) so `good-body`
arms can dump torso meshes. All ingredients (`skinnedView`, `owner->BoneOffsetAt/BoneTransAt`,
`TransParent` resolution) already used within 150 lines of the insertion point (§2.4 anchors).
Flag-unset behavior: zero getenv-positive branches taken → render-inert, no files, no stderr.

Engine change ⇒ commit engine first, bump `MILO_ENGINE_PIN` (`native/CMakeLists.txt:74`) in the
matching rb3 commit (CLAUDE.md engine-pin rule).

### 3.3 Metrics library + harness — `native/tests/skinning_oracle.h` (header-only, `namespace skinoracle`)

Data + loader:
```cpp
struct BoneRec  { std::string name; int parent; Transform off, world; float palette[16]; };
struct SkinVert { float pos[3]; int idx[4]; float w[4]; };
struct PaletteFrame {
    std::string mesh, owner, armTag; int frame, nb;   // nb: 38=male, 40=female
    std::vector<BoneRec> bones; std::vector<SkinVert> verts;
};
bool LoadPaletteFrame(const std::string& path, PaletteFrame& out);   // strict, rejects short reads
```

Metrics (each `double Metric(const PaletteFrame&)`, per-bone/per-vert detail rows available for
messages; matrix-relative math replicated from `offset_basis_derivation.py` +
production `Multiply`/`Invert` per the hands-bind-oracle pattern):
- `M_Tier1RestCoherence` — `max_b angle(off_b · world_b, I)` in degrees, + `count(>5°)`.
  (Offline analog of the engine Tier-1 xcheck; pose-dependent by design — the dump frames are
  play-time frames where own ≈ B, matching arm-S semantics.)
- `M_Tier2JointAttach` — offline replica of the engine Tier-2 exact-joint metric:
  `max over (b,parent p) || j_b·P[p] − j_b·P[b] ||`, `j_b = inverse(off_b).v`.
- **`M_BlendSpread` (NEW — the tear metric, the plan's centerpiece):** per vertex with ≥2
  active weights (`w_i, w_j ≥ 0.05`): `spread(v) = max_{i,j} || v·P[idx_i] − v·P[idx_j] ||`;
  mesh scalar = p95 over such verts (p95, not max, to shed single-vert decode outliers; max
  reported as detail). Rationale: authored multi-weighted verts sit near shared joints, so a
  coherent palette bounds the spread by joint-local articulation (order of the Tier-2 residual,
  ~2.4u worst on the ON arm); a torn blend drives adjacent composed matrices apart at
  wrist-radius R≈46u by the APD mixed-sign ±6-35° inter-bone deltas →
  `2·R·sin(Δ/2)` ≈ 15-28u. **This size argument is a hypothesis until M1 measures it — M1
  exists to retire exactly this risk.**
- `M_InterBoneRelPose` (diagnostic, non-gating): per adjacent pair, `angle(P_b·inv(P_p))` vs
  the same product on the `arm-w`-authored bind palette — the ROADMAP R1-refined "two-adjacent-
  bone relative pose" quantity, reported for lane forensics and as R1's future native-side
  comparand.
- `M_WorldExtent` — wext recomputed from skinned verts. Included **specifically to be validated
  as NOT-an-oracle** (VERDICT §6.4).

Harness:
```cpp
struct SeparationReport {
    double goodMin, goodMed, goodMax, badMin, badMed, badMax;
    double marginRatio;            // badMin / goodMax (>1 = separated)
    enum Verdict { VALID, MARGINAL, BLIND, INVERTED } verdict;
    // VALID: zero overlap AND marginRatio >= 3.0; MARGINAL: separated but < 3x;
    // BLIND: overlapping; INVERTED: bad reads better than good.
};
SeparationReport ValidateMetric(MetricFn m, const std::vector<PaletteFrame>& good,
                                const std::vector<PaletteFrame>& bad);
```
Population hygiene enforced in code (OPTIONS.md §4.2 as an assert, not a norm): `ValidateMetric`
**fails** (gtest ASSERT) if either population mixes `nb` values or mesh names without an
explicit `AllowMixed` opt-in — an aggregate can motivate, never close.

The **registry rule** (the "no unvalidated oracles" lint §4.3 as code): a small
`GateRegistry()` table maps metric name → the `OracleValidation.*` test that certifies it.
Suite D (live gate) refuses to grade with any metric absent from the registry; adding a metric
to the registry without its green validation test is caught by a registry-completeness test.

### 3.4 Test suites — `native/tests/test_skinning_oracle.cpp`

**Suite A — `OracleValidation.*`** (committed fixtures; the harness firing on known frames):
- `Tier1SeparatesCeilingHand` — M_Tier1 VALID on good-body vs bad-ceiling (≈3° vs 87.3/42.6°).
- `Tier1IsBlindToTornBlend` — **EXPECT verdict BLIND** on good vs bad-torn (3.1° vs 3.1°).
  Pins the Wave-16 lesson; if this ever flips to VALID the pinned fact changed → investigate.
- `Tier2IsBlindToTornBlend` — EXPECT BLIND (0.33u vs 0.34u). Same pinning.
- `BlendSpreadSeparatesTornBlend` — M_BlendSpread VALID on good-body vs bad-torn.
  **This is the permanent red test for the Wave-16 spike-fan**: had this suite existed, the
  HANDS-FIX lane's numeric gates could not all have read GREEN.
- `BlendSpreadOnCeilingHand` — measured, verdict recorded whichever way it lands (the spike
  *webbing* in arm-C bursts suggests RED here too; do not pre-assert — assert the measured
  verdict after M1 and pin it).
- `WextIsNotAnOracle` — EXPECT M_WorldExtent NOT VALID on good-pose vs bad populations
  (arm-S male legit 64.5-104u overlaps shard ranges — `arm_summaries.txt`). Pins §6.4.
- `GenderSplitEnforced` — mixed-nb population without opt-in must ASSERT-fail (fail-red of the
  hygiene rule itself).
- `RegistryComplete` — every gate-registry metric has a validation test and its stored verdict
  is VALID.

**Suite B — `VerdictTable.*`** (committed `arm-w`/`arm-s` fixtures): reproduces the Wave-15
adjudication table from a clean checkout — arm-w male Tier-1 mode ≈0.1° count=0 (authored ≡
bound basis, matrix-level), arm-w female ≈28.9° count 34/40, arm-s male ≈3.1° count=0, arm-s
female ≈28.9° count 40/40 (tolerances ±1.5° on modes, exact on counts=0). This is ROADMAP R2's
named Wave-17 exit ("fixtures reproduce the arm-W verdict table").

**Suite C — `SkinOracleSynthetic.*`** (no fixtures, always runs, Dolphin/GPU/boot-free —
mirrors the farvert-oracle pattern incl. host-libm trig): synthetic coherent palette (off =
inv(rest), rigid animation) GREEN on all metrics; synthetic torn palette (rotate ONE bone's
live pose 0.3 rad, keep its neighbors) → M_BlendSpread RED at seam verts while Tier-1(that
bone's own rest)/Tier-2(conjugation) stay GREEN — the in-vitro replica of the Wave-16
signature; `SKIN_ORACLE_PERTURB=<rad>` env fail-red; `PerturbationIsDetected` permanent guard.

**Suite D — `SkinningLiveGate.*`** (env-pointed, SKIPs when unset — crowd-oracle pattern):
`RB3_SKIN_ORACLE_GOOD_DIR` / `RB3_SKIN_ORACLE_CAND_DIR` point at fresh capture dirs from the
capture script; runs every registry metric, EXPECTs candidate within the good envelope. This is
the one-command grader for any future hands/W2.4/R5-reskin claim.

### 3.5 Capture script — `scripts/native/skinning-fixture-capture.py`

Wraps the adjudication protocol (`keyboard-to-gameplay.py --song-downs 3 --game-burst 15
--burst-interval 0.3`, `RB3_FIXED_CLOCK=1`) once per arm with that arm's env +
`RB3_PALETTE_DUMP`/`_DIR`; collects dumps + matched burst screenshots into
`execution/<KEY>/evidence/<arm>/` (lint §4.7: evidence committed) and curates the fixture
subset into `native/tests/goldens/r2-skinning/<arm>/` with a `MANIFEST.txt` (build SHA, engine
pin, env, frame↔screenshot map). Refresh = re-run script, re-run Suite A/B, only then commit
new goldens.

---

## 4. MILESTONES

**M1 — decisive risk retirement: does any palette-level metric see the tear?** (~0.3 wave)
Implement ONLY the dump probe (§3.2) + a throwaway Python replica of M_BlendSpread /
M_InterBoneRelPose (numpy, ~100 lines, patterned on `offset_basis_derivation.py`). Capture
`good-body`, `bad-ceiling`, `bad-torn` (3 boots). Compute candidate metrics on the raw dumps.
- **GO:** some candidate reads zero-overlap ≥3x separation good vs bad-torn, gender-split →
  proceed to M2 with that metric as the tear gate.
- **NO-GO:** no candidate separates → the tear is not a palette+verts+weights function (would
  falsify §2.4's analysis — e.g. the corruption is GPU-side or vertex-decode-side). Then:
  commit the dumps + a NO-GO note; descope to Tier-1/Tier-2 validation + verdict-table pins
  (still worth ~half the item: locks the 11 defaults and the §6.3/§6.4 lessons); flag the tear
  gate as R1-blocked in ROADMAP. Either way the coordinator learns something true for ~0.3
  wave — this is the cheapest decisive step because the probe is an assembly of existing parts
  and the metric math is 100 lines of numpy on data we've never had.

**M2 — consolidation** (~0.4 wave): `skinning_oracle.h` + `test_skinning_oracle.cpp` Suites
A/B/C; capture `arm-w`/`arm-s`; curate + commit all goldens; CMake wiring; every Suite-A
verdict matches M1's Python numbers (cross-implementation check); fail-red demos run and logged
in STATUS. Exit: `rb3-tests --gtest_filter='OracleValidation.*:VerdictTable.*:SkinOracleSynthetic.*'`
green from a clean build — ROADMAP's Wave-17 Lane-S exit.

**M3 — live gate + docs** (~0.3 wave): Suite D + capture script hardening; goldens README +
refresh protocol; one rehearsal: re-capture `bad-torn` fresh and confirm Suite D flags it RED
against `good-body` (the end-to-end demonstration that a Wave-16-class result can no longer
pass the numeric layer). Register the suite in the Wave-18 kickoff as the mandatory gate for
W2.4 BandPatchMesh and the R5 decision.

---

## 5. GATES (each with its fail-red demonstration)

| # | gate | fail-red demonstration |
|---|---|---|
| G1 | Fixture integrity: every committed `PaletteFrame` loads strictly | truncate a copied fixture → loader rejects (test asserts on the copy) |
| G2 | `VerdictTable.*` reproduces Wave-15 numbers within tolerance | point the arm-s test at the arm-w fixture → RED (wrong-arm control) |
| G3 | `BlendSpreadSeparatesTornBlend` VALID (the permanent Wave-16 red test) | (a) Suite-C synthetic torn palette → metric RED; (b) recompute with weights forced single-bone → verdict degrades to BLIND (proves the metric's discrimination lives in the blend, not in an artifact) |
| G4 | Blindness pins: `Tier1IsBlindToTornBlend`, `Tier2IsBlindToTornBlend`, `WextIsNotAnOracle` all EXPECT their measured non-VALID verdicts | these ARE recorded fail-reds — each is a real gate that really fired GREEN on a really-broken build, now frozen as data |
| G5 | Registry rule: no gate metric without a green validation test | add a dummy metric to the registry with no test → `RegistryComplete` RED |
| G6 | Probe inertness: `RB3_PALETTE_DUMP` unset ⇒ byte-identical behavior | flag-OFF run passes the existing drawlog-792 golden (`test_draw_log_golden.cpp`) + zero files in dump dir; flag-ON run produces files (positive control) |
| G7 | Suite hygiene: full `rb3-tests` green; no default flips; engine pin bumped in lockstep commit | `ctest` on rb3-tests; `git diff` review shows only additive gated engine block + new test/golden/script files |

---

## 6. RISKS (honest)

- **R-a (primary, M1-retired): M_BlendSpread may not separate.** Counter-argument giving
  confidence: the skinned positions the screen shows are a pure CPU-reproducible function of
  (verts, weights, palette) — Instrument-B already recomputes exactly that blend at this site
  (`Rnd_Wgpu_RB3.cpp:4893-4913`) — so a visible tear MUST appear in the recomputation; the risk
  is statistic choice (max/p95/threshold) and frame selection, both cheap to iterate offline on
  the M1 dumps. Mitigation: M1's screenshots pin that dumped frames show the tear.
- **R-b: capture non-determinism (BOOTRNG — R4 not landed).** Committed fixtures are frozen
  files, so CI is deterministic; the risk is only at *refresh* time (distributions shift
  between boots). Mitigation: gates are distribution-shaped (zero-overlap + 3x margin), not
  exact values; refresh protocol re-runs Suite A before committing; arm-C-style protocol-validity
  check (wext window vs `arm_summaries.txt`) in the capture script.
- **R-c: "body meshes are known-good" is an assumption.** Band skinning ships correct
  (`acd9c19a`), but count-in transients exist (walk-on pose memory). Mitigation: GOOD frames
  are mid-gameplay burst frames with committed screenshots; Suite C's synthetic GOOD is
  assumption-free; if body meshes carry a real low-grade incoherence, `ValidateMetric` will
  show a fat good-tail — a finding, not a plan failure.
- **R-d: engine-tree concurrency** (campaign standing issue — concurrent uncommitted engine
  edits, e.g. the FxSendNative.cpp precedent). Mitigation: additive single-block edit; record
  `git -C ../milo-native-engine rev-parse HEAD` + `status` with every capture (WAVE16_REVIEW
  A7 discipline); build in a worktree if the shared tree is dirty (Wave-16 Lane-F precedent).
- **R-e: fixture bloat.** ~few MB text. Acceptable per goldens precedent; if review objects,
  gzip + decompress-in-loader is a contained change.
- **R-f: the suite certifies coherence, not faithfulness.** A future fix could be
  internally-coherent-but-wrong (e.g. freeze everything — coherent!). Mitigation: `arm-w`
  freeze fixtures give a frozen-BAD population (M_InterBoneRelPose ≈ static across frames →
  add a cheap `PaletteAnimates` check to Suite D); E1 stays; R1 owns ground truth.

---

## 7. COST + what it unblocks

**Cost: ~1 lane-wave** (M1 0.3 + M2 0.4 + M3 0.3), consistent with ROADMAP's 0.5-1 estimate;
the 0.5 floor is reachable only if M1's first candidate metric separates immediately.

**Unblocks:**
- **W2.4 BandPatchMesh** re-attempt (memory: that family broke native TWICE with no numeric
  invariant) — Suite D is the pre-registered gate; ROADMAP names R2 as its prerequisite.
- **R5 hands endgame** — the engine true-reskin (or closure) decision gets graded by Suite D +
  R1 instead of another native-only verdict; `M_InterBoneRelPose` is the native-side comparand
  R1's Dolphin probe will diff against.
- **Locks the 11 shipped defaults** — any regression in band skinning turns Suite B/D RED.
- **Executes lint §4.3 permanently** — "no unvalidated oracles as gates" stops being a kickoff
  rule someone must remember and becomes `RegistryComplete`.

## Appendix — source anchors (all opened this pass)

`native/CMakeLists.txt:690-745` (rb3-tests), `:74` (MILO_ENGINE_PIN=51640ff…);
`native/tests/test_helpers.{h:144-152,cpp:70-97}`; `native/tests/test_hands_bind_oracle.cpp`
(:80-86 trig gotcha, :250-270 guard, :300-358 fixture arm); `test_farvert_rotation_oracle.cpp`
+ `goldens/w2.8-farvert/live_pose.txt`; `test_crowd_bone_oracle.cpp:15-22`;
`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp:4736-4875` (HANDS_ATTACH; Tier-1 :4813-4825,
Tier-2 :4826-4848, perturb :4779-4789, scope :4766-4768), `:4872-4913+` (Instrument-B vertex
access), `:2786` (skinnedView), `src/gfx/VertexFormats.h:19` (GpuVertexSkinned);
`src/system/bandobj/BandCharacter.cpp:1374` (RB3_HANDS_AUTHORED_REPOINT, default-OFF);
`scripts/native/keyboard-to-gameplay.py:299-308`; `orig-assets/extracted/` (present);
`execution/HANDS-ADJUDICATION/{VERDICT.md, evidence/{arm_summaries.txt,
offset_basis_derivation.py}}`; `execution/HANDS-FIX/{STATUS.md, evidence/
{arm{OFF,ON}_hands_attach.log (summary-only — verified), parse_hands_attach.py}}`;
`RETROSPECTIVE/{ROADMAP.md, OPTIONS.md §2#2 §4, REPORT.md:276}`; `WAVE16_REVIEW.md` A5.
