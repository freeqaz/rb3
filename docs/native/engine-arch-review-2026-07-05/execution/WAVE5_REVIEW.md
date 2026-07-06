# Wave 5 Kickoff — Fable pre-dispatch review

**Reviewer:** Fable. **Date:** 2026-07-06. **Reviewing:** `execution/WAVE5_KICKOFF.md` (draft).
Evidence tags: **MEASURED** = I read the file/line or ran the check myself; **JUDGMENT** = my call
on measured facts. Engine HEAD/pin verified `609efb7` (W2.3 probe) on top of `6852caa` (W2.1
contract); `Rnd_Wgpu_RB3.cpp` is now 5,002 lines.

## VERDICT: dispatch-with-amendments

The lane skeleton is right and the highest-value call — ship W2.1 this wave — is correct: **the
flip CAN proceed this wave.** But the flip brief as drafted re-litigates a question W2.1's own S3 +
verifier already answered (and answers it with the wrong mechanism), its "one-line commit" is
mechanically not one line (the flag read is presence-truthy, so a default-ON flip with no opt-out
read change ships a fix nobody can turn off), and — the biggest miss — **the flip breaks the
committed splash draw-log golden (888→792 draws, MEASURED in W2.1 S3/VERIFY) and nothing in the
kickoff re-goldens it**, which also couples the flip to W0.3d-fix (both invalidate the same golden
→ they must land in a coordinator-defined order with ONE re-capture). The UI regression gate is
also aimed at the wrong screen for the hub bar: S3 proved the hub-bar mesh is **not observable in
song_select** — the gate needs a main_hub capture. W0.6b's stated exit gate is already green today
(vacuous); W3.1 must be split fog/projLight-first (struct-neutral) with 4→8 lights deferred.

---

## R-A — Subsumption: blocker or proceed? **PROCEED WITH THE FLIP THIS WAVE, hacks co-active and RETAINED. The "prove subsumed / no-op" step must be deleted from the brief — it is a refuted criterion, not an open task. There is no double-apply.**

**MEASURED — where everything lives and how it composes** (engine
`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` @ `609efb7`):

- The name-match + opt-out flag reads for the two UI hacks were relocated by W1.7 to
  **rb3** `native/src/rb3_render_hook.cpp` — `RB3_NO_HUB_BAR_PLACEMENT_FIX` read at `:77`,
  `RB3_SCROLLBAR_THUMB_FIX_OFF` at `:101` — returned to the engine as
  `geomPolicy.hubBarPlacement` / `.scrollbarThumb` (`Rnd_Wgpu_RB3.cpp:2775-2778`).
  `RB3_NO_CROWD_REBIND` is rb3 `src/system/world/Crowd.cpp:932` (load-path bone rebind, not the
  draw path at all).
- The contract arm is **explicitly exclusive of the hacks**: `placementContractArm =
  sPlacementContract && skinned && !scrollbarThumb && !hubBarPlacement`
  (`Rnd_Wgpu_RB3.cpp:2878-2879`), and the `obj.world` selection is a single if/else chain —
  `scrollbarThumb` (`:2885`) → `hubBarPlacement` (`:2887`) → general skinned arm with the
  contract inside it (`:2892-2900`) → non-skinned (`:2902`). **Exactly one branch executes per
  draw. Under `RB3_PLACEMENT_CONTRACT=1` the hub-bar/scrollbar hacks do NOT double-apply — the
  kickoff's "the contract may double-apply with them" (`WAVE5_KICKOFF.md:23-24`) is mechanically
  wrong.**
- **Why the opt-out-no-op criterion was refuted** (W2.1 `STATUS.md` S3 + VERIFY, MEASURED): not
  double-apply, but the opposite — the contract is *provably vertex-invariant* (it reproduces
  flag-OFF vertex positions exactly, only recording `obj.world=meshWorld`), while the hub-bar/
  scrollbar meshes are *genuinely broken* flag-OFF (bone parent chains never receive the
  `SetWorldXfm`, per the in-file histories at `:2779-2806` and `:2807-2832`). A vertex-invariant
  change **cannot by construction** fix a vertex-position bug, so disabling a hack under flag-ON
  reverts that mesh to broken (S3 empirical: `RB3_SCROLLBAR_THUMB_FIX_OFF=1` under flag-ON →
  thumb at screen-center, 11.14% region diff vs 2.12% A/A noise). This is a **permanent
  property**, not missing work: no amount of Wave-5 effort makes those opt-outs no-ops without
  replacing the hacks with a *different, vertex-moving* fix — which is out of scope and exactly
  the "reconcile" trap the brief's step (1) invites an agent into.

**Ruling:** (a) The flip proceeds this wave. (b) The hacks are **retained co-active** — neither
removed nor gated off under the contract; the mutual exclusion at `:2878-2891` is the designed
composition and W2.1's verifier already certified it (this was the S3 "coordinator directive":
retain at flip). (c) Kill the subsumption task; replace it with a cheap **structural assertion**
(one-time re-read that the exclusion terms at `:2878-2879` and branch order at `:2885-2903` are
intact at the flip SHA) plus the UI-placement regression gate below. (d) REFACTOR_PLAN Phase-2's
"Flags/hacks DELETED this phase" list (`REFACTOR_PLAN.md:132-137`) still names these three —
the coordinator should annotate that their deletion premise is refuted (they are now permanent
companions of the contract, candidates for deletion only if a future vertex-moving UI-bone fix
lands). JUDGMENT on (c)/(d); (a)/(b) follow from MEASURED facts.

**The concrete UI-placement regression gate for the flip (replaces brief step 1):**

1. **Post-flip default-build capture, TWO screens:** (i) song_select — scrollbar thumb on the
   right-edge track + red-thumb region diff vs the S3 baseline
   (`W2.1/s3-artifacts/songselect_flagON_depth00.png`; broken signature = thumb at ~x=640,
   region diff ≳11% — S3 measured both sides); (ii) **main_hub** — the yellow highlight bar
   behind the focused item. **MEASURED gap:** S3 states `highlight_main`/`highlight_pattern`
   are *main_hub* meshes, "not observable in song_select" (`W2.1/STATUS.md:229-231`) — the
   kickoff's song_select-only A/B (and my brief's own framing) is blind to the hub bar. A
   main_hub capture (nav harness already reaches main hub; hub-bar look also documented in
   `project_visual_diff_2026_07_02` artifacts) is required.
2. Because the flip changes the *default*, the A/B is "pre-flip-SHA default build vs post-flip-SHA
   default build" — with an A/A pair on each side (S3's discipline: the char-preview panel is
   boot-phase-animated, so single-frame pixel diffs over-read).
3. Negative controls carried from S3: hands/skinning nets (`HandsBindOracle`, W0.5 lineup PASS
   all layers), crowd SKIN_CLAMP counts vs `W2.2/char/parsed-default.json`.

**The missing blocker the kickoff doesn't have (this is the real R-A finding):**

- **The flip breaks the committed splash golden.** MEASURED (W2.1 S3 + verifier, reproduced
  A/A 3×): flag-ON splash canonical draw count is **792**, flag-OFF **888** (the crowd mesh
  `0xc57f…` 211→115 draws accounts for the entire delta). Draw count is an exact-match axis of
  `drawlog-golden.py`; the committed `native/tests/goldens/drawlog/splash_screen.json` is an
  888-draw flag-OFF capture. A default-ON flip therefore turns `--fixed-clock --canonical-order`
  **RED on every subsequent run** (and W0.6/W2.6-style per-lane regression checks with it)
  unless the golden + residual sidecar are re-captured under the new default, or the harness
  pins the flag off. Pinning the flag off in the harness would make the gate permanently blind
  to the shipped configuration — re-golden is the right move.
- **That couples the flip to W0.3d-fix.** The staged fix (see R-B) changes exact draw order
  under `RB3_FIXED_CLOCK` and its own handoff says every exact-order golden must be re-captured
  after it lands (`W0.3d/STATUS.md:186-192`). Two golden-invalidating changes in one wave →
  **coordinator-sequenced: land W0.3d-fix FIRST, then the flip, then ONE re-capture** of
  `splash_screen.json` + a fresh N≥30 per-name-eps sidecar sweep (the W0.3d.S1 protocol), then
  the ≥15/15 green bar. Doing them in the other order forces a second re-golden.
- **"One-line flip" is not one line.** MEASURED: the read is presence-truthy —
  `sPlacementContract = getenv("RB3_PLACEMENT_CONTRACT") ? 1 : 0` (`Rnd_Wgpu_RB3.cpp:2874-2875`;
  classification `read:presence`). Flipping default-ON by changing `? 1 : 0` to `? 1 : 1`-style
  leaves **no opt-out** (`RB3_PLACEMENT_CONTRACT=0` would still enable it). The flip commit must
  invert the read to a registered opt-out — repo convention says `RB3_PLACEMENT_CONTRACT_OFF`
  (cf. `RB3_TRACK_LIGHT_OFF`/`RB3_VENUE_LIGHT_OFF`) — with the classification.json default
  updated and gen.inc regenerated (census stays exit 0). The opt-out also keeps the S1 oracle's
  fail-red demonstrable post-flip (opt-out ON → oracle RED), which should be run once as the
  flip's fail-red.

**Drum oracle (brief step 2): keep, and it gates the flip.** MEASURED: the W2.1 verifier's
deviation #2 — the oracle only checks `kind=="crowd"`; the `"drum"` kind is reserved but
unimplemented (`W2.1/STATUS.md:81-85, 324-327`). The general contract IS proven to place props
(verifier: 28/79 distinct skinned gameplay mesh-names carry non-origin `obj.world` flag-ON vs 0
flag-OFF, "incl. bone-attached instrument props"), so the risk is low and the work is small
(probe at the prop-attach site + one oracle kind, `placement_oracle.h` already carries the field
— rb3-only, no engine edit). JUDGMENT: cheap enough to keep as a hard gate; fallback if it stalls
= reviewer-judged Dolphin drum-position A/B as the drum evidence, with the oracle kind filed as
debt — do not let it silently drop like W2.1.S2 did.

## R-B — W0.3d-fix ⟂ W3.1: **disjoint at the file level — different repos. Parallelize. The real collision is the golden re-capture ordering with Lane A (above), not with W3.1.**

MEASURED:

| Item | Repo | Files / lines |
|---|---|---|
| W0.3d-fix | **rb3** | `src/system/rndobj/Utl.cpp` — `SortDraws` `:161-178` (patch hunk `@@ -172,6 +173,30 @@`, adds a `RB3FixedClockActive()`-gated name tiebreak around the `mat1 < mat2` pointer compare at `:174`; verified `W0.3d-fix.patch` still matches the live file, incl. the pre-existing HX_NATIVE null guard `:162-167`) |
| W3.1 | **engine** | `src/platform/Rnd_Wgpu_RB3.cpp` `WriteSceneUniforms` `:1160-1458` (`numLights` `:1420-1423`, `s.fogEnabled = 0` `:1429`, `s.numProjLights = 0` `:1431`, bind `sizeof(SceneUniforms)` `:1437`); `src/gfx/UniformStructs.h:18-56` (+static_assert `:56`); `src/gfx/standard_wgsl.inc` (struct `:58-70`, dir-light loop `:778-784`, point light `:523`, fog `:872`) |

Zero file overlap; not even the same repository. `SortDraws` is the rb3 scene-graph draw-vector
sort feeding traversal; it never touches the engine's uniform path. The kickoff's "possibly same
file (`Rnd_Wgpu_RB3.cpp`)" is refuted. **Amendment:** Lane B ∥ Lane C is fine; the sequencing
constraint to write into the kickoff instead is Lane B → Lane A: **W0.3d-fix lands before the
flip's re-golden** (R-A). Also note W0.3d's own recommendation (adopt it): give the fix a
registered opt-out (`RB3_DRAWSORT_DETERMINISTIC_OFF`) so the flake stays demonstrable as a
landed fail-red (`W0.3d/STATUS.md:193-198`).

One more Lane-A/Lane-C same-file note: the flip commit itself edits `Rnd_Wgpu_RB3.cpp`
(`:2874-2875`) — the same file W3.1 edits at `:1160-1458`. Functions are disjoint, but the
standing rule from the Wave-4 review ("even function-disjoint items should not run concurrently
in the same file" — one shared engine working tree) applies. The flip is tiny and gated only on
readiness evidence; **sequence: flip commit lands before W3.1 starts engine edits** (JUDGMENT;
trivially schedulable since W3.1 has its own S1 gate work to do first).

## R-C — W3.1 / DC3 blast radius: **split it. Fog + projLight are struct-neutral and ship this wave; 4→8 lights is the cross-backend contract change — defer to Wave 6 with a DC3-side owner.**

MEASURED:

- `SceneUniforms` (`UniformStructs.h:18-56`) **already contains** every field fog and projLight
  need: `fogColor/fogStart/fogEnd/fogEnabled` (`:23-27`) and
  `projLightDir/Color/ProjRow0/ProjRow1/numProjLights` (`:47-53`). The shared WGSL **already
  consumes them**: fog at `standard_wgsl.inc:872` (`isEnabled(scene.fogEnabled) &&
  isEnabled(material.materialFogEnabled)`), struct mirror `:58-70`. RB3's `WriteSceneUniforms`
  simply hard-zeros them (`:1429`, `:1431`). So **fog-from-RndEnviron + projLight-from-fakespots
  are RB3-TU-local fills — no struct edit, no WGSL edit, no static_assert change, zero DC3 blast
  radius.** DC3's behavior is unchanged by construction (its own `WriteSceneUniforms`,
  `Rnd_Wgpu.cpp:1234-`, keeps writing its own values).
- **4→8 lights is the opposite class:** `lightDirs/lightColors: array<vec4f,4>`
  (`UniformStructs.h:29-30`, WGSL `:61-62`), `pointLightPos/Colors/Ranges[4]` (`:35-37`),
  `static_assert(sizeof(SceneUniforms)==656)` (`:56`), bound by **both** backends via
  `sizeof(SceneUniforms)` (DC3 `Rnd_Wgpu.cpp:1644`, RB3 `Rnd_Wgpu_RB3.cpp:1437`) and mirrored in
  the shared WGSL struct + loops (`:778-784`, `:523`). Growing the arrays changes the byte
  layout for **every** DC3 draw. Mitigations exist (DC3 fills `SceneUniforms scene{}`
  value-init at `:1235`, so new fields zero; both bind sites use `sizeof` so sizes track), but
  two risks are unpriced in the kickoff: (i) the struct grows ~656→~930 B, which crosses the
  256-B uniform-offset-alignment boundary 768→1024 — per-scene ring stride +33% (DC3's scene
  ring is 16 KiB, `Rnd_Wgpu.cpp:287`; RB3's was already bumped to 64 KiB for venue lighting) —
  needs a measured scenes-per-frame check on DC3, and (ii) DC3 has **no visual gate in this
  campaign's harness at all**; `milo-engine-tests` + `WgslValidation` prove compile/layout, not
  lighting output. JUDGMENT: the Wave-4 review already flagged this exact class (E1); the right
  de-risk is the kickoff's own alternative — **fog only (plus projLight) first.**

**Amendment (Lane C rewrite):** W3.1a (Wave 5) = fog fill + projLight fill, engine RB3 TU only,
default-OFF behind ONE registered flag (`RB3_LIGHT_FILLS` or similar; census exit 0), gates =
`milo-engine-tests` 198/0/2 + canonical splash flag-OFF byte-identical + lineup PASS + a
venue A/B vs the Dolphin oracle (`c8-ground-truth-2026-07-01/t2-dolphin-oracle.md`, per
REFACTOR_PLAN Phase-3 exit) — matched frames per the A2/A3/A4 camera-desync lesson. W3.1b
(Wave 6) = 4→8 lights + WGSL lockstep + static_assert update + DC3-side visual smoke with a
named DC3 gate owner + the ring-stride measurement. Do not run W3.1b this wave.

## R-D — Over-width: **the shape is acceptable ONLY with W3.1 cut to W3.1a. Priority order: Lane A > Lane B > Lane C; Lane C is the first to shed.**

JUDGMENT on measured scope: W2.1-flip is mostly evidence + two small commits (drum oracle rb3-only;
flip = engine read-inversion + classification + re-golden with Lane B); W0.6b is classification
authorship with no behavior risk; W0.3d-fix is a staged, verified 25-line patch; W3.1a is the only
genuinely new code. That fits a wave — but the kickoff's W3.1 (with 4→8) does not, and starting
lighting is only correct in its struct-neutral half. The wave's verifier depth must go to the flip's
re-golden + UI gates (the only step that changes the shipped default this wave). If anything slips,
cut Lane C first, then W0.6b — never the Lane A re-golden discipline.

## R-E — Ground truth for the Dolphin A/B: **a subagent can produce the package; the coordinator's human-eyes sign-off remains the final flip gate — with a mandatory A/A protocol, because a known pre-existing confounder will otherwise produce a false FAIL (or worse, a false PASS on retry).**

MEASURED: the W2.1 verifier's gameplay A/B initially "looked alarmingly different" and was
dissolved only by 2×OFF + 2×ON A/A controls — a **pre-existing, A/A-variable pink bloom/exposure
wash** appears in both flag states with run-to-run intensity (`W2.1/STATUS.md:304-311`, filed as
deviation #3, W0.3d-family). And the render-regress memory records that a closing-gate "wash" FAIL
was once a camera-desync false positive — matched frames are required. **Amendment (brief step 3):**
the A/B package = camera-pinned (`rb3_director_disable` + `rb3_force_shot`, the S1 recipe) native
captures, **2 per flag state minimum**, compared against `dolphin-shots/gp_*.png` (gp_00 shows the
crowd spread house-left) + `images/retail-screenshots/` for drum-kit position; the agent's exit is
"package produced + numeric oracle green", never "flip done". The flip commit itself is the
coordinator's, after eyes on the package — the Wave-4 R-D split, unchanged. Additionally, since the
placement oracle (crowd) + the new drum oracle are numeric and agent-runnable, the human judgment
is scoped to exactly what only eyes can do: crowd *distribution looks right* and drum kit *sits at
the kit position* — not placement-vs-origin, which the oracles own.

## W0.6b — the brief's premise is stale and its exit gate is vacuous (MEASURED)

- "extend the census scanner … to include `rb3/src/system/`" — **already done**: W2.6's verifier
  landed the scan-root + `gameOnly` bucket (rb3 `a537c2a3`), and W2.1.S2's regen committed the
  resulting rows (`W2.1/STATUS.md:156-159`).
- "census check exit 0" — **already true today**: I ran `native_compat_census.py check` →
  `OK — 318 scanned flags all present in registry, regen clean.` An agent dispatched with this
  exit criterion can return instantly having done nothing.
- The actual remaining gap: the game-code flags are present only as **unclassified** gen.inc rows
  — MEASURED: `RB3_HANDS_BIND_FIX` (`NativeCompatFlags.gen.inc:143`) and `RB3_SKEL_REBIND_FULL`
  (`:259`) are `FlagClass::Unknown / "unclassified"`, absent from `classification.json`
  (`RB3_NO_CROWD_REBIND` likewise, `:186`).
- **Amendment (rewrite W0.6b exit):** author `classification.json` entries for
  `RB3_HANDS_BIND_FIX`, `RB3_SKEL_REBIND_FULL`, `RB3_NO_CROWD_REBIND`, and the ~86 game-root
  flags (do NOT add `RB3_FOOT_REST_CAPTURE` — never implemented, per W2.6.S1/S2); exit =
  **zero `FlagClass::Unknown` rows originating from the game scan-root** + ledger regenerated +
  census check still exit 0 + `--selftest` 14/14. Note the out-of-scope boundary W2.6's verifier
  measured: `src/band3` (~21 getenv files) is not a scan root — record it as accepted scope, or
  file it, but don't let the agent silently widen.
- **Collision (file:line basis):** W0.6b (bulk regen), the Lane-A flip (default change for
  `RB3_PLACEMENT_CONTRACT` + new `RB3_PLACEMENT_CONTRACT_OFF`), and W3.1a (new flag) **all write
  the same two generated/shared files** — engine `NativeCompatFlags.classification.json` +
  `NativeCompatFlags.gen.inc` (+ rb3 `NATIVE_COMPAT_LEDGER.md`). This is the exact "two lanes
  clobber gen.inc" failure mode W2.6's verifier documented live (Lane-A WIP sitting uncommitted
  in the shared file, `W2.6/STATUS.md:167-175`). **Amendment:** lanes author their
  classification entries as content handed to the coordinator (or commit classification.json
  additions under the git flock but do NOT run `gen`); **one regen, run by W0.6b LAST** (or by
  the coordinator at pin-bump), picks up all of them — W2.6's own prescription
  (`W2.6/STATUS.md:200-212`).

## Missing gates / SYS-1..7 audit (beyond the above)

1. **SYS-7 gate continuity across the flip (the golden break)** — covered in R-A; without the
   re-golden, every post-flip lane inherits a RED canonical gate and the wave's own hygiene
   checks go blind or get "expected-fail" waved through (the exact normalization SYS-7 exists to
   prevent).
2. **SYS-1 UI family regression from the flip** — covered in R-A: main_hub hub-bar capture added
   (song_select alone cannot see it, MEASURED); scrollbar region-diff thresholds from S3 reused.
3. **Post-flip fail-red** — the opt-out (`RB3_PLACEMENT_CONTRACT_OFF=1` → S1 oracle RED) run once
   at the flip SHA, so the shipped default keeps a demonstrable fail-red (same reasoning as
   W0.3d-fix's opt-out).
4. **W0.3d-fix flag-OFF byte-identity** re-checked at landing (`git diff --numstat` 25/0,
   guard-confined — already verified by W0.3d's verifier via `git apply --check`, cheap to
   re-run) + the four fail-red classes still RED post-land.
5. **Standing hygiene unchanged:** no `src/App.cpp` edits; engine tree's sibling uncommitted
   `FxSendNative.cpp` untouched; per-lane exact-file lists cross-diffed at dispatch; pin bumped
   once by the coordinator; hard rules 1–8 in force. The wave exit remains engine-tests 198/0/2 +
   lineup PASS — **plus, this wave only: the re-goldened splash canonical green ≥15/15 under the
   new default** (flip + W0.3d-fix both in).

## Summary of amendments (in kickoff order)

| # | Where | Change |
|---|---|---|
| A1 | Lane A / W2.1-flip step (1) | DELETE the subsumption/no-op proof (refuted permanently: vertex-invariant contract cannot fix vertex-broken UI meshes; and there is NO double-apply — branches mutually exclusive at `Rnd_Wgpu_RB3.cpp:2878-2891`, MEASURED). Replace with: hacks RETAINED co-active + structural assertion the exclusion terms are intact at the flip SHA + UI gate (A2). Coordinator annotates REFACTOR_PLAN:132-137 (deletion premise dead for these three). |
| A2 | Lane A / flip UI gate | UI-placement regression gate = default-build pre-flip vs post-flip captures with A/A pairs on BOTH screens: song_select (scrollbar thumb, region-diff thresholds from S3: broken ≈11%/center vs ≈2% noise) **and main_hub** (hub bar — NOT visible in song_select, MEASURED `W2.1/STATUS.md:229-231`) + HandsBindOracle/lineup/SKIN_CLAMP negative controls. |
| A3 | Lane A / flip mechanics | The flip is NOT one line: invert the presence-truthy read (`:2874-2875`) to a registered opt-out `RB3_PLACEMENT_CONTRACT_OFF`, update classification default, keep fail-red demonstrable (opt-out → oracle RED, run once post-flip). |
| A4 | Lane A ⟂ Lane B sequencing | The flip changes splash canonical count 888→792 (MEASURED) → committed golden breaks. Land **W0.3d-fix first, then the flip, then ONE re-golden** (`splash_screen.json` + fresh N≥30 per-name-eps sidecar), then ≥15/15 green under the new default. This is the wave's new hard exit. |
| A5 | Lane A / drum oracle | Keep as flip gate (cheap, rb3-only, `placement_oracle.h` `kind` field reserved); fallback if stalled = Dolphin drum-position A/B + oracle filed as explicit debt (not silently dropped). |
| B1 | Lane B / W0.3d-fix brief | Correct the premise: fix is rb3 `src/system/rndobj/Utl.cpp:161-178` (SortDraws), NOT the engine object-list/draw-submission path — disjoint from W3.1 (different repos), parallelize freely. Adopt the staged patch's own recommendation: registered opt-out `RB3_DRAWSORT_DETERMINISTIC_OFF` for a landed fail-red. |
| C1 | Lane C / W3.1 split | Wave 5 ships **W3.1a only**: fog + projLight fills — struct-neutral (fields + WGSL consumption already exist: `UniformStructs.h:23-27,47-53`, `standard_wgsl.inc:872`; RB3 zeros at `:1429,:1431`), RB3-TU-local, default-OFF, zero DC3 blast radius. **Defer 4→8 lights (W3.1b) to Wave 6** with WGSL/static_assert lockstep + DC3-side visual-smoke owner + the 768→1024 ring-stride measurement (DC3 scene ring 16 KiB, `Rnd_Wgpu.cpp:287`). |
| C2 | Lane A ⟂ Lane C | Flip commit (edits `Rnd_Wgpu_RB3.cpp:2874-2875`) lands before W3.1a begins engine edits in the same file (standing same-file rule). |
| D1 | Lane B / W0.6b | Rewrite: scanner extension ALREADY DONE (`a537c2a3` + W2.1.S2 regen) and `census check` ALREADY exit 0 (vacuous gate, MEASURED). Real exit = classify the ~89 game-root flags (incl. `RB3_HANDS_BIND_FIX`/`RB3_SKEL_REBIND_FULL`/`RB3_NO_CROWD_REBIND`; NOT `RB3_FOOT_REST_CAPTURE`), zero game-root `FlagClass::Unknown` rows, ledger regen, selftest 14/14. |
| D2 | All lanes | classification.json/gen.inc collision: three lanes write the same shared files → lanes author entries only; **ONE regen, sequenced last** (W0.6b or coordinator at pin-bump) — W2.6's documented failure mode and prescription. |
| E1 | Lane A / Dolphin A/B (R-E) | Agent produces the package (camera-pinned, ≥2 captures per flag state, vs `gp_*.png` + retail); coordinator human-eyes sign-off is the flip trigger. A/A protocol mandatory: the pre-existing A/A-variable pink bloom wash (`W2.1/STATUS.md:304-311`) WILL confound single-frame judgment. |
