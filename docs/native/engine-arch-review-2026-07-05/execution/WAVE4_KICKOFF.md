# Wave 4 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE4_REVIEW.md`) — **all amendments adopted**; dispatched with the corrected shape below.

## COORDINATOR ACCEPTANCE (2026-07-06) — final dispatched shape

Fable review returned **dispatch-with-amendments**; all adopted. It caught a factual error I'd have shipped (R-D) and a scene-hollow gate (R-B). Net changes vs the draft:

- **W2.1 gate was scene-hollow (B1):** the only committed drawlog golden is `splash_screen` (no crowd/drum), so the canonical gate **cannot see the crowd co-location fix at all**. W2.1 **builds its placement gate first as S1**: (i) a **gameplay** drawlog golden (nav+camera-pinned, `RB3_FIXED_CLOCK`); (ii) the "right-not-just-different" oracle — assert each crowd instance's drawn `obj.world` translation matches the faithful `spXfm` it's posed with at `WorldCrowd::Draw3DChars` (`Crowd.cpp:404`), instances pairwise-distinct + spanning the bowl; drum prop-mesh world ≠ origin + consistent with the drummer's bone/waypoint. **Fail-red is free** — RED on today's all-identity build. (iii) reviewer-judged Dolphin `gp_*` wides + retail gameplay shots. The canonical splash gate is demoted to a *regression net* (count/bind-group/identity on flag-ON, byte-identical on flag-OFF).
- **W2.1 default-OFF staging (C1):** the change replaces the `else if (skinned) { identity }` arm (`Rnd_Wgpu_RB3.cpp:2848`) — **every skinned draw in the game** (band, hair, crowd, skinned UI). It's a coupled two-part change (obj.world = WorldXfm() **jointly with** the bind-relative palette basis; in-file comments at `:2797-2800` document the double-transform SKEW trap if only one half lands). Land behind **ONE** registered flag (`RB3_PLACEMENT_CONTRACT`, engine-side so the census enforces it) gating BOTH halves atomically — no intermediate commit with one half live. flag-OFF byte-identical; flag-ON exit adds a song_select hub-bar/scrollbar A/B (those injections live in the edited block, invisible to gameplay gates) + the name-scoped placement-hack opt-outs (`RB3_NO_HUB_BAR_PLACEMENT_FIX`, `RB3_SCROLLBAR_THUMB_FIX_OFF`, `RB3_NO_CROWD_REBIND`) proven no-ops + crowd SKIN_CLAMP negative control. Flip is a separate coordinator-gated commit.
- **W2.1 verify protocol (B2):** the W1.6-era "residual-name world failures = non-blocking" rule must **NOT** carry into W2.1 — W2.1 IS a world-alterer and the eye/face residual meshes are exactly the class it changes. Use A/A controls to re-separate genuine eye-flake from W2.1 effects.
- **W2.3 (A1):** `RebindCrowdCharBonesToOwnSkeleton` (`Crowd.cpp:409`) is **retained** this wave (default-replaces, not removes; delete only after its opt-out is proven a no-op — R5 pattern). Own default-OFF flag. Negative controls: 2D crowd imposter path byte-identical; crowd/extras SKIN_CLAMP counts unchanged.
- **Lane C was built on a refuted premise + wrong flag (D1/D2):** `RB3_SKEL_REBIND_FULL` is the *known-broken* control, NOT W2.2's flag; W2.2 landed `RB3_HANDS_BIND_FIX` (default-OFF) which **measured no benefit → no flip**, and the working `RebindHeadHandsAtRest` is **already default-ON** with the head graze already adjudicated structural. **There is no flip to ship.** Lane C rewritten: **(coordinator, done inline) W2.2-close** = README flag-name correction + no-flip decision recorded; **W2.6 (NEW fleet item)** = foot/shoe rest-capture coverage — the actual residual (`saddleshoe_skin.2` 4.73× guard-DROP via `RebindOutfitBonesToOwnSkeleton`), rb3-only, engine READ-ONLY, default-OFF, inheriting W2.2's four-layer gates + **flag-registry cleanup** (register `RB3_HANDS_BIND_FIX`/`RB3_SKEL_REBIND_FULL`; extend census to scan `rb3/src/system/` — currently uncovered).
- **W3.1 DEFERRED to Wave 5 (E1):** not RB3-local — `SceneUniforms` is the shared cross-backend WGSL contract (`UniformStructs.h`, `static_assert(...==656)`, bound by DC3 at `Rnd_Wgpu.cpp:1644`); 4→8 lights is a DC3-blast-radius change needing DC3 gates (engine-tests 198/0 incl. WgslValidation + DC3 visual smoke). Also Phase 3 is plan-blocked on the clean W0.3 golden that W0.3d only now produces.
- **W0.3d collision constraint (F1):** part (a) CharEyes/CharLookAt freeze under `RB3_FIXED_CLOCK` + per-name eps from an **N≥30-boot sample** (NO global-eps widening) — rb3 `src/system/char/` + sidecar, disjoint. Part (b) async-loader/worker completion-order is **diagnosis-only wrt Lane-A files** — if the fix wants `Rnd_Wgpu_RB3.cpp` or the object-list path Lane A edits, STOP and hand to coordinator for post-Lane-A sequencing.
- **Hygiene (F2):** engine-side flags registered at introduction (census exit 0); per-lane exact-file lists cross-diffed before dispatch; no `src/App.cpp` edits; `FxSendNative.cpp` untouched.

**Final dispatched shape:** Lane A (engine, sequential, each default-OFF-staged) `W2.1 → W2.3`; Lane B (rb3 + diagnosis) `W0.3d`; Lane C (rb3-only) `W2.6` + flag-registry cleanup. W3.1 → Wave 5.

---

_(Original draft below, retained for provenance; superseded where the acceptance above differs.)_

**Draft status:** DRAFT — under Fable review, not yet dispatched.
Parent: `REFACTOR_PLAN.md` (Phase 2 W2.1/W2.3, Phase 3 W3.1), `execution/README.md` (Wave 1–3
results + hard rules 1–8 + the standing pre-dispatch review gate). Engine pin `6221a56`.

## Where we are (entering Wave 4)

- **SYS-1 skinning-bind half done** (W2.2, hands/fingers, landed default-OFF behind
  `RB3_SKEL_REBIND_FULL`, fail-red oracle in place). **SYS-2 done** (asset-name branches relocated).
  **SYS-3 done** (W1.6 DrawContext — `DrawMesh` now consumes an explicit `RB3SceneBinding`, no more
  mutable mid-frame scene bind group). Monolith 7,017 → 4,805. Engine-tests 198/0.
- **Open:** the SYS-1 *placement* half (crowd/drum-kit all at one point) is untouched; lighting
  (SYS-4) barely started; the draw-log gate is order-clean but eye-jitter-flaky (W0.3c partial);
  the hands/fingers fix is one adjudicated flag-flip from shipping.

## The user's headline asks, mapped to remaining work

"meshes deforming properly" → W2.2 (done, needs flip) + placement W2.1/W2.3. "fix lighting" →
W3.1 now, W3.2 BoxMap later. "UI parity" → Phase 4 (Wave 5). "graphics better / amazing" → all of
the above + Phase 5 polish. **Wave 4 = land crowd/drum placement, ship the hands/fingers flip,
start lighting, and make the draw-log gate clean.**

## Proposed Wave 4 lanes

**Lane A — placement + lighting on `DrawMesh`/`WriteSceneUniforms` (sequential, engine; these share
`Rnd_Wgpu_RB3.cpp`):**
- **W2.1** — the SYS-1 placement contract. Adopt Wii semantics: compose `obj.world =
  mesh->WorldXfm()` for skinned meshes too (bone palette using bind-relative matrices), so placement
  flows through the `RndTransformable` chain — hub bar / scrollbar / prop placement work with zero
  name-scoping. Now unblocked: `DrawMesh` carries `RB3DrawContext` (W1.6). Gated by W0.5 lineup +
  the residual-filtered canonical draw-log (co-location detection is exactly what it catches) + new
  per-draw-path placement goldens (crowd instances span the bowl; drum-kit prop bones ≠ origin).
- **W2.3** — kill shared-`GeomOwner` skeleton aliasing for crowd + props (read the drawn mesh's own
  bones), so the latched `RebindCrowdCharBonesToOwnSkeleton` becomes unnecessary.
- **W3.1** — faithful lighting fills (additive): fog from `RndEnviron`, directional+point arrays
  4→8, populate `projLight` from environ fakespots. Rebase onto W1.6's `WriteSceneUniforms` /
  `RB3SceneBinding` (the light arrays live in `SceneUniforms`). No hack deleted; fills real gaps.

**Lane B — draw-log gate cleanup (parallel, mostly rb3):**
- **W0.3d** — make the canonical gate a clean non-probabilistic pass. (a) recalibrate the
  CharEyes/CharLookAt residual eps from a large sample OR freeze/zero the look-at state under
  `RB3_FIXED_CLOCK` so the residual sidecar shrinks; (b) root-cause the mechanism-2 async-loader/
  worker completion-order nondeterminism filed by W0.3c.S1 (the real draw-order flake). Exit:
  `--canonical-order` golden green ≥15/15 fresh boots with the residual sidecar empty or a
  defensibly-recalibrated eps, fail-red intact.

**Lane C — ship the hands/fingers flip (parallel, rb3-only):**
- **W2.2-flip** — produce the evidence package the flip needs: (1) adjudicate the ~69u head-region
  graze numerically against the torso-only baseline (is it structural/pre-existing or introduced?);
  (2) resolve or explicitly scope-out the foot/shoe path; (3) capture wide + hand-closeup A/B
  (flag-OFF vs flag-ON) and compare against fresh Dolphin t2 captures + `images/retail-screenshots/`.
  Then EITHER flip `RB3_SKEL_REBIND_FULL` default-ON in a one-line gated commit if the evidence is
  clean, OR present a go/no-go package for coordinator sign-off. Engine READ-ONLY (same seams as W2.2).

## Risks / open questions for the reviewer

- **R-A: Lane A concurrency.** W2.1/W2.3 edit `DrawMesh`; W3.1 edits `WriteSceneUniforms` — same
  file (`Rnd_Wgpu_RB3.cpp`). I've serialized them in one lane to avoid the W3.1⟂W1.6-class collision
  the last review caught. Is sequential-in-one-lane right, or are the functions disjoint enough post-
  W1.6 to parallelize safely? If serial, is the order W2.1→W2.3→W3.1 correct?
- **R-B: Does W2.1 need W0.3d first?** W2.1 is a placement change → world-xfm divergences on crowd
  meshes are the intended signal, and the canonical comparator already catches world-xfm-out-of-bound
  reliably (only the *eye* residual flakes, and eyes ≠ crowd). My read: W2.1 can use the residual-
  filtered gate now; W0.3d runs parallel to make it clean. Challenge this — is there a co-location
  case the residual-filtered gate would miss?
- **R-C: W2.1 is the biggest correctness change yet and touches the just-refactored `DrawMesh`.**
  What's the ground truth that crowd/drum placement is *right*, not just *different*? (Dolphin
  gameplay wides show crowd spread + drum-kit position.) Should W2.1 also stage default-OFF behind a
  flag like W2.2 did, given its blast radius?
- **R-D: W2.2-flip and "reviewer-judged."** The flip's gate includes reviewer-judged Dolphin A/B.
  A subagent can produce the A/B package but true visual judgment is the coordinator's/human's. Is
  "agent produces package → coordinator adjudicates the flip" the right split, or should the whole
  flip wait for explicit human sign-off?
- **R-E: Over-width.** Three lanes again. Is W3.1 pulling review budget from the two hard items
  (W2.1 placement, W0.3d determinism)? Should W3.1 defer to Wave 5 (lighting-focused) so Wave 4
  concentrates on placement + gate?

## What I want from the Fable review

Decisive rulings on R-A (concurrency/ordering — the collision class that's bitten us twice), R-C
(does W2.1 need default-OFF staging + what's its ground-truth gate), and R-B (is the residual-
filtered gate sufficient for placement). Concrete amendments to lane structure, item briefs, and
exit gates — rewrite the sentences you'd change. Flag any bug class from SYS-1..7 that a Wave-4 item
could regress without a gate to catch it.
