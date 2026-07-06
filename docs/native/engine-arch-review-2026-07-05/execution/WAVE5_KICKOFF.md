# Wave 5 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE5_REVIEW.md`) — **all amendments adopted**; dispatched with the corrected shape below.

## COORDINATOR ACCEPTANCE (2026-07-06) — final dispatched shape

Fable review returned **dispatch-with-amendments**; all adopted. It caught (again) a backwards causal premise (A1) and a missed gate that the flip itself breaks (A4). Net changes:

- **No subsumption step (A1) — VERIFIED in source:** the contract arm explicitly excludes the UI hacks (`Rnd_Wgpu_RB3.cpp:2878-2879`, `placementContractArm = … && !scrollbarThumb && !hubBarPlacement`; mutually-exclusive if/else). There is **no double-apply**. The opt-out-no-op criterion was refuted for the *opposite* reason — the contract is vertex-invariant, so it can never fix the vertex-broken hub-bar/scrollbar meshes (a permanent property, not open work). **Delete the subsumption step; flip proceeds with the name-scoped hacks retained co-active.**
- **THE flip breaks the committed golden (A4) — the big miss:** flag-ON canonical count is **792 vs the committed 888** (crowd mesh `0xc57f` draws 211→115). A naive default-ON turns `drawlog-golden.py` RED wave-wide. **Required order (forces Lane A sequential):** `W0.3d-fix` (deterministic draw order) **→** flip **→ ONE re-golden** (`splash_screen.json` + a fresh N≥30 per-name-eps sidecar) **→ ≥15/15 green under the new default** = the new hard wave exit.
- **Flip mechanism (A3):** the read is presence-truthy (`getenv("RB3_PLACEMENT_CONTRACT") ? 1 : 0`, `:2874-2875`), so a naive "one-line flip" ships with **no opt-out**. Invert to a registered **`RB3_PLACEMENT_CONTRACT_OFF`** opt-out; keep fail-red demonstrable (opt-out set → placement oracle RED).
- **UI-placement regression gate (A2):** `highlight_main`/`highlight_pattern` are **main_hub** meshes, not song_select. The flip's regression gate = pre/post-flip default-build A/B **with A/A pairs** on **both** song_select (scrollbar; S3 thresholds: broken ≈11%/screen-center vs ≈2% noise) **and main_hub** (hub bar).
- **Drum oracle stays a flip gate (A5):** W2.1's oracle only checked `kind=="crowd"`; add the reserved `kind=="drum"` assertion (drummer prop-mesh bone/waypoint world ≠ origin, consistent with the drummer). Cheap, rb3-only. Dolphin drum-position A/B is the stall fallback.
- **W3.1 splits (C1):** ship **fog + projLight only** this wave — **struct-neutral, zero DC3 blast radius** (the `SceneUniforms` fields + WGSL consumption already exist; RB3 just hard-zeros them at `:1429/:1431`). **Defer 4→8 lights to Wave 6** (a 656→~1024-byte cross-backend contract change crossing the 256-B alignment boundary, DC3 scene ring, no DC3 visual gate exists yet). This wave's lighting item is **W3.1a**.
- **W3.1a serializes after the flip (C2, same-file):** W3.1a edits engine `WriteSceneUniforms` (`:1160-1458`) and the flip edits `:2874` — **same file** `Rnd_Wgpu_RB3.cpp`, so no concurrent editing (hard rule). W3.1a runs at Lane A's tail, after the flip commit.
- **W0.6b is classification, not scanner work (D1) — corrected:** the scanner extension already landed (`a537c2a3`); census exits 0 at 318. The ~89 game-root flags sit as `FlagClass::Unknown` in `gen.inc`. W0.6b **classifies** them (`probe|workaround|feature|perf`); exit = zero game-root Unknown rows.
- **classification.json single-writer (D2):** the flip, W3.1a, and W0.6b all touch `NativeCompatFlags.classification.json` — W2.6's live-clobber failure mode. **All edits under `flock /tmp/milo-engine-classjson.lock`, append-only rows, NO `gen.inc` regen by lanes; the coordinator does ONE regen + reconciliation at wave end.**
- **Flip sign-off split (E1):** a subagent produces the camera-pinned Dolphin A/B package (≥2 captures per flag state, A/A protocol mandatory — the pre-existing A/A-variable pink bloom wash confounds single-frame judgment); the **coordinator human-eyes sign-off triggers the flip commit.**

**Final dispatched shape:** **Lane A** (sequential, engine+rb3): `W0.3d-fix → W2.1-flip (+re-golden, coordinator sign-off) → W3.1a`; **Lane B** (parallel, rb3+engine-json): `W0.6b` (classify Unknown flags). 4→8 lights + BoxMap → Wave 6. classification.json edits flock'd + append-only; coordinator regens once.

---

_(Original draft below, retained for provenance; superseded where the acceptance above differs.)_

**Draft status:** DRAFT — under Fable review, not yet dispatched.
Parent: `REFACTOR_PLAN.md` (Phase 2 flip, Phase 3 lighting), `execution/README.md` (Wave 1–4 results
+ hard rules 1–8 + standing pre-dispatch review gate). Engine pin `609efb7`.

## Where we are (entering Wave 5)

Three of the worst mesh bug families from the original review now have landed fixes:
**hands/fingers** (W2.2 — `RebindHeadHandsAtRest` default-ON), **crowd/drum-kit placement** (W2.1 —
`RB3_PLACEMENT_CONTRACT`, vertex-invariant, oracle-proven, **default-OFF**), and the **SYS-3
state-leak** (W1.6 DrawContext, shipped). The monolith is decomposed (7,017→4,805). Engine-tests
198/0. **Lighting (SYS-4) is now the biggest untouched frontier**, and the crowd/drum fix is one
deliberate flag-flip from shipping.

## Proposed Wave 5 lanes

**Lane A — SHIP the crowd/drum placement fix (rb3 + coordinator flip):**
- **W2.1-flip** — the readiness work the W2.1 verifier flagged, then the flip. (1) Prove the
  name-scoped placement hacks are **subsumed** by the contract: `RB3_NO_HUB_BAR_PLACEMENT_FIX`,
  `RB3_SCROLLBAR_THUMB_FIX_OFF`, `RB3_NO_CROWD_REBIND` — under `RB3_PLACEMENT_CONTRACT=1` each opt-out
  must be a **no-op** (the contract does the placement, the hack becomes redundant). If not no-ops,
  reconcile (the hacks live in `rb3_render_hook.cpp` post-W1.7; the contract may double-apply with
  them). (2) Add the **drum-specific** oracle assertion (drummer prop-mesh bone/waypoint world ≠
  origin, consistent with the drummer) — W2.1's oracle only checked `kind=="crowd"`. (3) Capture
  reviewer-judged Dolphin gameplay A/B (crowd spread + drum position) vs `dolphin-shots/gp_*.png` +
  retail. (4) **Flip `RB3_PLACEMENT_CONTRACT` default-ON** in a one-line commit once 1–3 are green.

**Lane B — flag-registry + determinism cleanup (parallel, mostly rb3):**
- **W0.6b** — finish what W2.6 PART 2 dropped: extend the census scanner (`native_compat_census.py`)
  to include `rb3/src/system/`, then register `RB3_HANDS_BIND_FIX`, `RB3_SKEL_REBIND_FULL`, and the
  ~86 game-code flags it surfaces in `NativeCompatFlags.classification.json` (census check exit 0),
  regen the ledger. Closes the coverage gap that let `RB3_HANDS_BIND_FIX` go untracked.
- **W0.3d-fix** — apply the staged async-loader/worker completion-order determinism patch W0.3d
  part (b) diagnosed (it touches the object-list/draw-submission path; DrawMesh work has now settled
  post-W2.1/W2.3, so the coordinator-sequencing hold is released). Exit: the `--canonical-order`
  golden green with the residual sidecar shrunk toward empty.

**Lane C — start real lighting (engine + shared, DC3-blast-radius):**
- **W3.1** — faithful lighting fills (SYS-4, staged additive): fog from `RndEnviron`
  (`FogEnable/GetFogStart/End/FogColor`), directional+point light arrays **4→8** (GX cap), populate
  `projLight` from environ fakespots. **DC3-aware:** `SceneUniforms` lives in the shared
  `src/gfx/UniformStructs.h` (`static_assert(...==656)`, bound by DC3 at `Rnd_Wgpu.cpp:1644`, consumed
  by shared `standard_wgsl.inc`) — extend struct + WGSL **in lockstep**, and gate on
  `milo-engine-tests` 198/0 (incl. `WgslValidation`) as a hard cross-backend gate + a DC3-side visual
  smoke. Default-OFF behind a registered flag; no hack deleted (fills real gaps only).

## Risks / open questions for the reviewer

- **R-A: Is W2.1-flip's subsumption analysis a blocker or can the flip proceed with the hacks
  co-active?** If the name-scoped hacks are NOT no-ops under the contract, is the right move to
  (i) reconcile/remove them before flipping, or (ii) flip and leave them (risking double-apply on
  hub-bar/scrollbar)? What's the concrete test that the flip doesn't regress the UI placement family?
- **R-B: W3.1 concurrency with W0.3d-fix.** W0.3d-fix touches the object-list/draw-submission path;
  W3.1 touches `WriteSceneUniforms`/`SceneUniforms`. Both in the engine, possibly same file
  (`Rnd_Wgpu_RB3.cpp`) or shared headers. Are they disjoint enough to parallelize (Lane B vs Lane C),
  or does one need to serialize? Measure the file overlap.
- **R-C: W3.1 DC3 blast radius.** Changing `SceneUniforms` (656-byte cross-backend contract) is the
  exact class the Wave-4 review flagged. Is the DC3 gate I've specified (engine-tests + WgslValidation
  + DC3 visual smoke) sufficient, or does W3.1 need a DC3-side owner/gate I'm not pricing? Is 4→8
  lights even the right first lighting step, or should Wave 5 lighting be smaller (fog only) to
  de-risk the shared-struct change?
- **R-D: Over-width again.** Four items across three lanes. W2.1-flip is high-value (ships a fix);
  W0.6b is hygiene; W0.3d-fix unblocks the clean gate; W3.1 is a new frontier. Should W3.1 defer to a
  lighting-dedicated Wave 6 (with W3.2 BoxMap) so Wave 5 concentrates on shipping + cleanup, or is
  starting lighting now correct?
- **R-E: Is there a ground-truth gate for W2.1-flip's Dolphin A/B that a subagent can actually
  produce**, or does the flip need my (coordinator) visual sign-off as the final step (agent produces
  package, I flip)?

## What I want from the Fable review

Decisive rulings on R-A (the subsumption question — this gates shipping the crowd/drum fix, the
wave's highest-value outcome), R-B/R-C (W3.1's collision + DC3 blast radius), and R-D (is lighting-now
right or should Wave 5 be ship+cleanup only). Concrete amendments to lanes/briefs/gates; flag any
SYS-1..7 regression a Wave-5 item could cause without a gate to catch it — especially a UI-placement
regression from the W2.1 flip.
