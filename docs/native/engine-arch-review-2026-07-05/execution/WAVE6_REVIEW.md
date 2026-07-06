# Wave 6 Kickoff — Pre-Dispatch Review (Fable)

**Reviewing:** `WAVE6_KICKOFF.md` (coordinator draft, engine pin `8e7eddd`).
**Reviewer basis:** README.md Wave-5 results + HELD decision + backlog; `W2.1-flip/dolphin-ab/README.md`;
`W2.1-flip/STATUS.md`; `W2.1/STATUS.md`; source spot-checks (appendix).

## VERDICT: dispatch-with-amendments

The lane structure is sound (flip-blocker leads, single-writer respected, Lane B conditional, Lane C
disjoint), and the wave correctly refuses to flip until the wash is characterized. But the draft's
**prime-suspect mechanism for the wash is contradicted by the source** (the halo capture is
`game.cam`-gated and cannot see crowd draws), its **N≥8 binary protocol is statistically underpowered**
for the decision it must make, the **wash detector as specified is not well-posed** (captures are not
time-pinned, so venue lighting animation legitimately varies luma), and the **flip checklist misses two
A4-pattern breakages** (classification.json rows go stale; every harness that uses "no env" as the OFF
arm inverts semantics post-flip). None of this requires redesign — amend and dispatch.

---

## Amendments

### A1 — (Q A) N≥8-per-state binary wash-rate comparison is underpowered; use continuous scoring + a sequential design, and score the evidence that already exists

**Evidence verified:** The hold was based on 1/2 flag-ON vs 0/2 flag-OFF blow-outs
(README "COORDINATOR FLIP DECISION: HELD"). But the recorded evidence base already spans both flag
states: `W2.1-flip/STATUS.md:152-159` logs mean luma **OFF_1=95.2, OFF_2=23.7 (near-black anomaly),
ON_1=202.5 (blow-out), ON_2=122.6** — a continuum, with an anomalous *dark* tail in flag-OFF — and the
earlier, independent W2.1.S3 verify run (`W2.1/STATUS.md:304-311`) observed the wash **"off1 heavy /
off2 moderate; on1 none / on2 moderate"** — i.e. heavy wash in flag-OFF and a clean flag-ON, the
*mirror* of the Wave-5 sample. The asymmetry motivating the hold is already contradicted across
experiments.

**Statistics:** with a binary wash/no-wash score at N=8 per state, Fisher's exact on 8-vs-8 only
reaches p<0.05 at ≥4/8 vs 0/8. Even if the true flag-ON wash rate were 50% and flag-OFF 0%, the
probability of observing ≥4/8 is ~64% (P(k≥4|Binom(8,0.5))); at a true 25% rate, power is ~11%. So
"N≥8, binary" will most likely return "indistinguishable" *regardless of ground truth* — and the S2
decision rule "(i) statistically indistinguishable → pre-existing" then converts low power into a ship
decision. Amend:

1. **Score continuously** (per-capture mean luma + %pixels above/below thresholds, per A2) and compare
   distributions (Mann-Whitney), not binarized rates — far more power at the same N.
2. **Sequential, interleaved protocol:** capture in alternating OFF/ON pairs; stop early when either
   (a) a wash-class capture appears in flag-OFF (existence proof → A/A-variable, decided) or
   (b) a rank test hits p<0.05 with all wash mass in flag-ON. Cap at N=16 per state.
3. **Batch 0 is free:** run the detector over the 4 committed `dolphin-ab/cap_*.png` first, and cite
   the W2.1.S3 verify record (`W2.1/STATUS.md:304-311`) as prior flag-OFF wash evidence. Given that
   record, S2 may legitimately converge on A/A-variable within one or two batches.

### A2 — (Q A) The numeric wash detector is not well-posed unless captures are time-pinned (songMs), and it must score both tails + hue

**Evidence verified:** `w21flip-dolphin-ab.py` screenshots after `is_playing` plus **wall-clock**
settle sleeps (`time.sleep(0.5)` poll loop, `time.sleep(0.2)` settle — script lines ~108-138). Under
`RB3_FIXED_CLOCK` the sim advances per frame, but the number of frames elapsed before the screenshot
depends on wall-clock load latency — so each boot captures a **different songMs**, and venue lighting
keyframes/light animation legitimately change frame luma across boots. A mean-luma detector on such
captures conflates authored lighting animation with the wash. Amend the S1 protocol:

1. **Pin capture time:** poll songMs (`/api/health`) and screenshot inside a fixed target window
   (e.g. songMs 21000±250, matching the Wave-4/5 captures), recording songMs per capture.
2. **Score both tails:** the anomaly presents as blow-out (ON_1, luma 202.5) *and* near-black
   (OFF_2, luma 23.7). Detector = %pixels with luma>0.95 AND %pixels<0.05, plus mean luma.
3. **Score hue:** the historical description is a *pink* bloom/exposure wash (`W2.1/STATUS.md:307`),
   and W0.5 recorded a "giant pink limb/head mass … broken-env class" (`W0.5/STATUS.md:300`). Pink
   (missing-texture/broken-env class) vs white (exposure/bloom class) may be two different phenomena;
   a pink-fraction channel separates them for free.
4. Side benefit: if time-pinning makes the wash deterministic or makes it vanish, that is itself the
   diagnosis (capture-timing/asset-residency, not render code).

### A3 — (Q B) The S3 prime suspect is contradicted by source: the halo capture CANNOT see crowd draws. Demote it; rank the alternative mechanisms

**Evidence verified in source:**

- The bloom-halo capture in `DrawMesh` fires only under
  `strcmp(RndCam::sCurrent->Name(), "game.cam") == 0 && IsHaloSourceMat(mat)`
  (`Rnd_Wgpu_RB3.cpp:4403-4404`). The A/B captures pin a **venue** shot
  (`rb3_director_disable` + `rb3_force_shot` wide establishing shot, e.g. `coop_all_n00.shot`);
  crowd geometry draws under that venue camera, never under `game.cam`. The capture path is
  mechanically unreachable for crowd emissive draws.
- `IsHaloSourceMat` (`RB3HaloPass.cpp:68-81`) additionally requires `mEmissiveMap != null &&
  mEmissiveMultiplier > 0` plus the game-hook name exclusions (`rb3_render_hook.cpp:265-279`,
  `QueryHaloPolicy`: "surface", "gem_smasher_glow").
- The W2.1.S2 sentence the kickoff leans on ("flung emissive geometry feeds the bloom pass into a
  full-screen wash") describes a **mid-development bug that was fixed before commit** — the
  identity-fallback-bone cancellation + cached meshWorld (`W2.1/STATUS.md` S2 findings #2-3; the fix
  is in the committed contract comment, `Rnd_Wgpu_RB3.cpp` ~:2893-2900 "those are initialized to
  meshWorld^-1 under the flag"). It is not a live residual mechanism, and if it *were*, it would
  present as torn shards + wash together — S2/S3/verify all recorded "no shards".

S3's planner should therefore start from the alternatives, roughly in this order of prior:

1. **Capture-timing / songMs drift** (A2) — legitimate lighting animation, not a bug at all.
2. **Asset/texture residency at capture** — pink placeholder / broken-env class (W0.5 precedent);
   note the W0.3d part-(b) **async-loader completion-order patch is still staged, not landed**
   (README "New backlog items filed from Wave 5"), so this nondeterminism source is live even under
   `RB3_FIXED_CLOCK` + the SortDraws tie-break.
3. **RB3PostProc venue grade/bloom** — `bloomIntensity` comes from `RndPostProc::Current()`
   (`RB3PostProc.cpp:234-254`), i.e. per-shot/venue-event postproc state at capture time; boot-varying.
4. **P4 per-environ venue-light SceneUniforms rewrite** (`RndEnviron::sCurrent`-driven, default-ON) —
   which environ is current at the pinned shot can vary with timing.

**Cheap attribution beats statistics:** add a flag-isolation matrix to S2 — on a wash-reproducing
configuration, re-capture with `RB3_HIGHWAY_BLOOM_OFF=1`, `RB3_BLOOM_OFF=1` (postproc term only),
`RB3_VENUE_LIGHT_OFF=1`, `RB3_TRACK_LIGHT_OFF=1`. Whichever flag kills the wash names the mechanism
in ~8 captures, independent of the ON/OFF-rate question.

### A4 — (Q A/R-A) A/A-variable verdict may ship the flip — with two conditions

The proposed answer to R-A is correct: the hold reason was asymmetry; if asymmetry is disproven the
wash is an independent pre-existing bug and must not hostage the flip. Two conditions:

1. **E1 must be judged on detector-selected wash-free captures** per flag state (the S4 package should
   include the numeric scores per capture and mark which are wash-affected), so the confound cannot
   re-enter the human sign-off the way it forced the Wave-5 hold.
2. The wash gets its **own backlog item** carrying S2's attribution data (flag matrix + songMs-pinned
   scores), so the characterization work isn't orphaned by the flip shipping.

### A5 — (Q E/R-E) 792 remains the expected count, but measure it flag-ON before the flip commit; two missed A4-pattern breakages

**Evidence verified:** committed golden `native/tests/goldens/drawlog/splash_screen.json` = **888
draws** (checked). The 888→792 delta is entirely the crowd mesh `0xc57f…` draw-count change 211→115
(`W2.1/STATUS.md` B2 section), i.e. a **count/multiset** effect. The W0.3d-fix is a SortDraws
**ordering** tie-break (`src/system/rndobj/Utl.cpp:192-199`, comparator only — cannot add/remove
draws), and the canonical comparator is multiset-based, so the expected flag-ON count is unchanged at
792. However, 792 was only ever *measured* pre-W0.3d-fix (Wave-4 A/A: 792/792/792); the post-fix
verifier measured 888 default and merely *inferred* 792. Amend S4: **run one
`RB3_PLACEMENT_CONTRACT=1 drawlog-golden.py --canonical-order --fixed-clock` before the flip commit**
and treat its measured count as the re-golden target (R-E's own advice, made mandatory).

**Missed breakages the flip itself causes (the Wave-5-review A4 pattern):**

1. **classification.json rows go stale.** `NativeCompatFlags.classification.json:91-92`:
   `RB3_PLACEMENT_CONTRACT` says `"default": "off"` / "not-live … default-OFF pending coordinator
   flip", and `RB3_PLACEMENT_CONTRACT_OFF` says "Effective default-OFF this wave". The flip commit
   must update both rows' text/default (coordinator's single wave-end regen covers `gen.inc`, not the
   row content).
2. **OFF-arm semantics invert in every existing harness/doc.** `w21flip-ui-ab.py`,
   `w21flip-dolphin-ab.py`, and the fail-red demo commands throughout the STATUS files use **no env**
   as the flag-OFF arm and `RB3_PLACEMENT_CONTRACT=1` as ON. Post-flip, "no env" = contract-ON, the
   legacy opt-in is a no-op, and the OFF arm must be `RB3_PLACEMENT_CONTRACT_OFF=1`. Any post-flip
   A/B or fail-red re-run with the current scripts silently compares ON-vs-ON. Add a
   flip-checklist item: sweep the two capture scripts (+ `placement-gate-capture.py` docs) to take an
   explicit `--flag-state {on,off}` mapped to the post-flip envs, in the same review cycle as the flip
   commit.

### A6 — (Q C/R-C) Lane B: keep stop-at-prototype, and pre-declare it — the overlap is near-certain, not conditional

**Evidence verified:** per-environ lighting state is written exclusively in
`BandRnd::WriteSceneUniforms` (`Rnd_Wgpu_RB3.cpp:1176ff` — the only writer of `SceneUniforms` light
fields: `lightDirs`/`lightColors`/`numLights` at ~:1425-1440), and the shading contract lives in
`gfx/UniformStructs.h` (656-byte static_assert) + `standard_wgsl.inc` — shared with DC3 (the W3.1a
zero-blast gate exists precisely because of this). A faithful BoxMap/ambient environ lighting redesign
that never touches WriteSceneUniforms, the WGSL include, or UniformStructs.h is implausible; even a
new TU needs call sites in `Rnd_Wgpu_RB3.cpp`. So do **not** loosen R-C to "landable if file-disjoint
proven" — file-disjointness would have to include the cross-backend WGSL/struct contract, and Lane A's
W3.1b (projLight + fog) is *in those same files this same wave*. Instead, drop the conditional:
declare Lane B **prototype-in-worktree-only for Wave 6** up front, landing in Wave 7 after Lane A
settles. This avoids a mid-wave stop/renegotiate cycle and lets the S1 planner spend its budget on the
lighting-path map instead of an overlap audit with a foregone conclusion.

### A7 — (Q D/R-D) Lane C is game-side actionable, but name the one real collision: `rb3_render_hook.cpp`

Both findings are plausibly game-side: the song_select overlap family is the known MusicLibrary
Text-class stale-slot issue (band3/meta_band — the 360 ARK draws unused slots the Wii hid), and the
main_hub ticker quad is a hide/asset fix. **But** the natural "quick fix" home for name-based
render policy is `rb3/native/src/rb3_render_hook.cpp` (all B1-B13 name branches live there,
`QueryHaloPolicy` at :265) — which is **exactly where Lane A's S3 candidate fix ("exclude
crowd-instance materials from halo capture") would land** if the (demoted, per A3) halo hypothesis
survives. Amend the Lane C brief: implement in band3/UI source only; if a render-hook edit becomes
unavoidable, sequence it behind Lane A S3 or escalate to the coordinator. On R-D:
`/tmp/visdiff-20260702` **still exists today** (verified: `native_choose_diff.png` etc. present), but
the re-derivation requirement stays — /tmp is volatile and the memory summary is not ground truth.

### A8 — (Q F) Process: commit-per-review-cycle is compatible with the HARD RULES; two hygiene additions and two stale anchors

- **No conflict found:** HARD RULES 1-2 already mandate MOVE-xor-CHANGE + commit-early; rule 3 (pin)
  and the coordinator-only flip/re-golden/regen list in the kickoff are consistent; the
  classification.json single-writer (flock + append-only + no lane regen) is preserved.
- **Add:** PLAN.md/docs commits by planners go to the shared rb3 repo — they must use
  `flock /tmp/rb3-git.lock` (rule 4) and stage only their own files, same as code commits.
  Commit-per-review-cycle raises the frequency of concurrent rb3 doc commits, so say this explicitly
  in the kickoff rather than leaving it implied.
- **Stale anchors in the kickoff (agents grep these):** (1) the fog fill is at
  `Rnd_Wgpu_RB3.cpp:1444-1457` (flag-ON writes, `s.fogEnabled = 0` in the else at :1457) — the cited
  ":1429/:1431" lines are the ambient-only grey-key fallback. (2) `projLight*` fields exist in
  `gfx/UniformStructs.h:49-52` but are **never written** in `Rnd_Wgpu_RB3.cpp` — they are only
  implicitly zeroed by `SceneUniforms s{};` at :1176. The W3.1b brief's "hard-zeroed at :1429/:1431"
  is wrong on both counts; substance (fields exist, currently inert) holds.
- One more R-B note (S3 regression gate): the proposed "existing bloom A/B captures + a
  gem-halo-present assertion" is right; make the gem-halo assertion concrete as
  `RB3_HIGHWAY_BLOOM_BLEND=0 vs default` pixel-diff on a gameplay capture (the documented negative
  control in `RB3HaloPass.cpp:30-32`) so "halo still present" is machine-checkable, not eyeballed.

---

## Appendix — what I checked in source

- `../milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` — placement contract arm :2860-2925
  (`kPlacementContractDefaultOn = 0`, opt-out-first read, `placementContractArm` excludes
  scrollbarThumb/hubBarPlacement); halo capture site :4380-4415 (**`game.cam` strcmp guard** at
  :4403); `WriteSceneUniforms` :1176 (`SceneUniforms s{}`), light writes ~:1425-1440, fog fill
  :1444-1457; no `projLight` references anywhere in the TU.
- `../milo-native-engine/src/platform/RB3HaloPass.cpp` — `HighwayBloomEnabled` (:39-46),
  `IsHaloSourceMat` (:68-81, emissive-map + multiplier data test + hook policy), the
  `RB3_HIGHWAY_BLOOM_BLEND=0` negative-control note (:30-32).
- `../milo-native-engine/src/platform/RB3PostProc.cpp` — venue grade bloom term (:210-256,
  `pp->GetBloomIntensity()`, fixed `kBloomThreshold 1.8`, `RB3_BLOOM_OFF` isolation flag).
- `../milo-native-engine/src/platform/NativeCompatFlags.classification.json` — :91-92 (contract +
  opt-out rows, both `"default": "off"`, "pending coordinator flip" text).
- `rb3/native/src/rb3_render_hook.cpp` — `QueryHaloPolicy` :265-279 (surface/gem_smasher_glow name
  exclusions live game-side).
- `rb3/src/system/rndobj/Utl.cpp` — SortDraws material-name tie-break :185-203 (comparator-only,
  gated `RB3FixedClockActive() && !RB3DrawSortDeterministicOff()`).
- `rb3/scripts/native/w21flip-dolphin-ab.py` — capture timing: `is_playing` poll + wall-clock
  sleeps, screenshot not songMs-pinned (~:108-138); OFF arm = no env, ON arm = legacy opt-in env.
- `rb3/scripts/native/drawlog-golden.py` — canonical/multiset comparator + residual-sidecar shape
  (order-insensitive; count is a multiset property).
- `rb3/native/tests/goldens/drawlog/` — `splash_screen.json` = **888 draws** (committed),
  `splash_screen.fixedclock-residual.json` present.
- `/tmp/visdiff-20260702` and `/tmp/wave6-current-state` — both exist today (listed).
- Docs: `W2.1/STATUS.md` (S2 findings #2-3 fallback-bone/cached-meshWorld wash fix; S3-verify wash
  record :304-311 "off1 heavy … on1 none"; deviations #2-3), `W2.1-flip/STATUS.md` (S4 luma spread
  :152-159; verifier "would be 792 if flipped" inference), `dolphin-ab/README.md` (checklist item 3),
  README.md Wave-5 results + HELD + backlog.
