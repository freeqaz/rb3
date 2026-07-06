# Wave 7 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE7_REVIEW.md`) — **all amendments adopted**;
dispatched with the corrected shape below.

## COORDINATOR ACCEPTANCE (2026-07-06) — final dispatched shape

Fable review returned **dispatch-with-amendments** (10). Adopted:

- **A1 (blocker) — RESOLVED before dispatch:** the review ran in parallel with the coordinator's
  flip-cycle commits; pin bump + 792 re-golden + sidecar + harness inversions + docs are all
  committed (`402b8662`, `85013e52`). Verified: pin `1b045d9`, tracked tree clean.
- **A2 (stale premise):** the "unlanded W0.3d part-b patch" does not exist — the staged patch WAS
  the SortDraws tie-break, landed Wave 5 (`76f51077`), order-only and fixed-clock-gated (inert in
  real-time wash boots). WASH matrix has **no landing dependency**; prior #1 restated as "async
  residency, **no fix exists** — a residency fix would be new loader/ThreadCall work outside Lane
  A's declared files."
- **A3 — W3.3-fix is FLAG-FIRST:** the luma-keyed ceiling changes every hot-channel/sub-knee frame
  (SP overlay, big rock endings, hot menus) that the 0–25s sweep never samples. Land default-OFF
  behind a registered flag; the WASH matrix doubles as broad hot-frame coverage; coordinator flips.
  Note: the "staged patch" is a comment-only proposal (quoted block matches
  `rb3_postproc.wgsl.inc:186-191` byte-exactly) — implementation is mechanical, not `git apply`.
- **A4 — WASH matrix gets a 5th control arm:** W3.3-fix-OFF (else default) at the same N≥6, and
  **contract-default pinned in all configs** (do not inherit wash-measure's OFF/ON arms). "Did
  W3.3-fix collapse the wash rate" is answered within-protocol, not vs Wave-6 rates.
- **A5 — Lane B step 0 after the oracle exists:** A/B the EXISTING `RB3_HANDS_BIND_FIX` flag under
  the new BL-A2 oracle (one boot per arm) before writing any new rebake — it was judged "no
  benefit" under a rotation-blind gate. Outcome decides BL-A1's shape (extend/flip vs pose-varying
  basis error that no static rebake can fix, per the C8 doc's `calcOffset=true` drift note).
- **A6 — BL-A2 spec:** the oracle measures under an **animated pose** through the real rb3 band
  path (bind pose gives R·sin(0)=0 → structurally GREEN — the Wave-3 DC3-suite trap); fail-red =
  RED on today's build. `RefSkinVertex` is a `static` in engine `tests/test_skin_golden.cpp:164` —
  duplicate (~40 lines) into rb3-tests (chosen over an engine test-header export: no engine edit).
- **A7 — Lane C fence amendments:** `rb3_render_hook.cpp` added to Lane C's fence (focus-plumbing
  option lands there; unowned this wave otherwise); pin `RB3_HUB_MENU_QUAD_HIDE` **OFF** during
  W4.2 captures (S3's quad shares the main_hub screen). Disjointness from Lane A verified
  (header-only coupling, no shared statics).
- **A8 — two missed flip breaks FIXED PRE-DISPATCH by the coordinator:**
  `crowd-bone-gate-capture.py --no-placement-contract` now sets `RB3_PLACEMENT_CONTRACT_OFF=1`
  (was a silent no-op post-flip); `placement-gate-capture.py --expect-red` now runs with the
  opt-out env (its "unchanged build is RED" contract had inverted); `w21flip-ui-ab.py` docstring
  updated. `_w32-boxambient-ab.py` Wave-6 scores are NOT comparable post-flip → Lane D re-baselines
  if it captures.
- **A9 — Lane D scoped to decision-from-existing-evidence:** no Dolphin-harness building (none
  exists; ground truth is static shots). Decide from existing shots + W3.2's own venue-probe data
  (which already calls per-pixel Lambert "defensible"), or file an explicit human capture request.
- **A10 — model tiers tagged:** Opus for verify/verdict/oracle-design stages; Sonnet for the
  mechanical shader edit, W4.2 impl, and packaging.

**Final dispatched shape:** **Lane A** (sequential): W3.3-fix flag-first (Sonnet impl → Opus
verify) → WASH 5-config matrix (Opus); **Lane B** (sequential): BL-A2 animated-pose oracle (Opus)
→ step-0 `RB3_HANDS_BIND_FIX` A/B (Opus) → BL-A1 per step-0 verdict (Opus) → verify (Opus);
**Lane C** (sequential): W4.2 floor fix flag-first (Sonnet impl → Opus verify, quad-hide pinned
OFF) → hub-quad flip package (Sonnet); **Lane D**: W3.2b decision memo (Opus, plan-only).
Coordinator post-wave: flips (W3.3, W4.2, hub-quad, BL-A1 as earned), regen, pin bump, docs.

---

_(Original draft below, retained for provenance; superseded where the acceptance above differs.)_

**Draft status:** DRAFT — under Fable pre-dispatch review, not yet dispatched.
Parent: `REFACTOR_PLAN.md`, `execution/README.md` (Wave 1–6 results + hard rules 1–8 + standing
pre-dispatch review gate + Wave-6 backlog). Engine pin: post-flip HEAD (`1b045d9` lineage; exact pin
in `native/CMakeLists.txt` after the Wave-6 re-golden commit).

## Where we are (entering Wave 7)

Wave 6 shipped the two most user-visible fixes to default-ON: **crowd/drum placement**
(`RB3_PLACEMENT_CONTRACT`, flipped after the wash was proven flip-independent) and the **black
singer head** (W2.7). The user's four remaining visible complaints are now all root-caused with
fixes designed but not landed: **(1)** grayscale song-start = native postproc composite
over-exposing the authored stage-light reveal (staged luminance-preserving-ceiling patch, W3.3);
**(2)** washed-out selected menu text = unconditional UI-text color floor
(`RB3MaterialBinder.cpp:145-149`, W4.2); **(3)** missing hands / "transparent" torsos = finger
shard from rotation-basis invBind mismatch (W2.8 — and our W2.2 oracle is provably blind to it);
**(4)** the stochastic full-frame venue wash (PINK/NEARBLACK/WHITE, boot-nondeterministic, backlog
item with ranked mechanism prior). Wave 7 is a **landing wave**: convert diagnoses into shipped,
gated fixes.

## Proposed Wave 7 lanes

**Lane A — venue exposure family (engine; owns `Rnd_Wgpu_RB3.cpp`, `RB3PostProc.cpp`,
`rb3_postproc.wgsl.inc`; sequential):**
- **W3.3-fix (S1 impl → S2 verify):** land the staged luminance-preserving composite ceiling patch
  from `execution/W3.3/` (grayscale song-start). Gates: songMs-sweep A/B 0–25s (grey window gone,
  color matches the RB3_PP_OFF control's hue while keeping the reveal's exposure ramp), fail-red
  (revert the ceiling change → grey reproduces), lineup PASS, canonical drawlog green (postproc is
  post-scene; count must not move), default-ON only if the sweep is unambiguous else flag+flip.
- **WASH-matrix (S3):** on the W3.3-fixed build, run the 4-flag isolation matrix
  (`RB3_HIGHWAY_BLOOM_OFF`/`RB3_BLOOM_OFF`/`RB3_VENUE_LIGHT_OFF`/`RB3_TRACK_LIGHT_OFF`) with **N≥6
  boots per config**, scored by `wash_score.py`, songMs-pinned per the W2.1-flip-blocker protocol.
  First question: did W3.3-fix already collapse the wash rate (same mechanism space)? Then name the
  residual mechanism (ranked prior: async asset residency — the unlanded W0.3d part-b patch — >
  RB3PostProc grade > P4 venue-light). Fix if the mechanism is in-lane; else file with attribution.

**Lane B — the hands fix (rb3 game-side; `BandCharacter.cpp` + `native/tests/`; sequential,
oracle-first):**
- **W2.8.BL-A2 (S1):** far-vertex rotation-basis oracle in `rb3-tests` reusing the W0.1
  RefSkinVertex path (per the W2.8 diagnosis: whole-mesh ratio + origin skinpos are blind; the
  metric must track far vertices under bone rotation, `IK_SHARD_VERT`-style). Fail-red proven on
  today's build (the oracle must be RED on the current finger shard — that's the point).
- **W2.8.BL-A1 (S2 impl → S3 verify):** rotation-aware outfit invBind rebake
  (`RebindHeadHandsAtRest`/`RebindOutfitBonesToOwnSkeleton` extension), **default-OFF** behind a
  registered flag, inheriting W2.2's four-layer anti-revert gates + the new BL-A2 oracle GREEN
  flag-ON. Numeric gates: FLING=0, no 200-460u smear band, crowd clamp byte-identical, lineup PASS.
  Before/after finger close-up captures (the STOP-TRIPWIRE from W2.2 still applies: the earlier
  RB3_BOUND_REBAKE rest-rebake failed at 200-460u; bind-POSE-capture+rotation-aware is the new
  path). Flip is coordinator-gated next wave.

**Lane C — UI text + hub quad (engine `RB3MaterialBinder.cpp` — file-disjoint from Lane A — + rb3
captures; sequential):**
- **W4.2-fix (S1 impl → S2 verify):** relax/gate the UI-text color floor so authored focus colors
  survive. Gate (both directions): main_hub selection sweep shows focused = dark-on-gold /
  unselected = dimmed grey (vs retail), AND the three labels the floor originally rescued (news
  ticker, FRIEND RANKINGS, CHOOSE INSTRUMENT) remain readable — capture all screens A/B. Default-ON
  only if both directions pass; else flag+flip package.
- **Hub-quad flip package (S3):** visual A/B for `RB3_HUB_MENU_QUAD_HIDE` (ON vs OFF vs retail on
  main_hub, A/A pairs, no side effects on the PLAY NOW label itself since the quad is
  `playnow.lsw`) → coordinator flip decision.
- FENCE: Lane C does not touch `Rnd_Wgpu_RB3.cpp`/`RB3PostProc.cpp` (Lane A) or
  `BandCharacter.cpp` (Lane B).

**Lane D — SYS-4 point-light decision (plan-only → prototype; engine worktree):**
- **W3.2b (S1 plan, Opus):** escalate-or-drop per the Wave-6 refutation: the real fidelity gap is
  point-lights (Wii per-object light cube vs our per-pixel Lambert). Decide: (i) land the
  box-ambient prototype anyway (near-no-op today, harmless, completes the plumbing), (ii) redesign
  around a per-object point-light cube approximation, or (iii) drop until a venue shows a measured
  visual gap (Dolphin A/B on 2-3 venues quantifies whether per-pixel Lambert actually looks WORSE —
  it may be strictly better). Evidence-based recommendation; prototype in the existing
  `wave6-boxmap-proto` worktree only if (ii).

**Deferred:** 4→8 light arrays (DC3 gates still missing), W2.6 foot/shoe, W2.4 BandPatchMesh,
venue black poster quads (SYS-5, needs an owner + fence decision), song_select minor residuals.

## Process rules (carried from Wave 6)

Commit-per-review-cycle under `flock /tmp/rb3-git.lock` (engine: `/tmp/milo-engine-git.lock`);
checkpoint-resume at `/tmp/wave7-checkpoints/<stage>.json`; builds under
`flock /tmp/rb3-native-build.lock`; new flags registered append-only under
`flock /tmp/milo-engine-classjson.lock`, NO lane regen (coordinator regens once); no pin bumps, no
default flips by lanes (coordinator-only); NEVER git reset/rebase/stash/checkout-- on shared trees.
**Post-flip note:** the placement contract is now default-ON — any harness A/B whose OFF arm means
"contract off" must set `RB3_PLACEMENT_CONTRACT_OFF=1` (three harnesses already inverted; do not
copy stale no-env patterns from pre-Wave-6 STATUS files).

## Risks / open questions for the reviewer

- **R-A:** Is landing W3.3-fix default-ON in one step too aggressive? The patch changes the
  composite ceiling for EVERY venue/shot, not just song-start; the songMs-sweep gate may not cover
  hot-exposure moments elsewhere (SP overlay, big rock endings). Should it stage flag-OFF + a
  coordinator flip like every other behavior change?
- **R-B:** Does the WASH matrix need the W0.3d part-b async patch landed first (it's the top-ranked
  prior)? Landing a determinism patch mid-wave in Lane-A files is in-fence but adds risk — order?
- **R-C:** For BL-A1, is extending the EXISTING rebind functions the right shape vs a separate
  rotation-fix pass? The W2.2 tripwire history says rest-capture rebakes are dangerous; the reviewer
  should sanity-check the proposed mechanism against the C8 investigation doc (:104-156).
- **R-D:** W4.2-fix touching `RB3MaterialBinder.cpp` while Lane A owns the postproc/scene files —
  verify the TU split makes these genuinely file-disjoint (both were extracted from the monolith in
  Wave 2).
- **R-E:** Anything the placement flip breaks that the A5 sweep missed (goldens other than splash?
  scripts not matching the `RB3_PLACEMENT_CONTRACT` grep?)?
