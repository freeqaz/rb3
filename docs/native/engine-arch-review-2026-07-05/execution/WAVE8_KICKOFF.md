# Wave 8 — Kickoff Design (coordinator draft, for Fable review before dispatch)

**Author:** coordinator. **Status:** REVIEWED (Fable, `WAVE8_REVIEW.md`) — **all amendments adopted**;
dispatched with the corrected shape below.

## COORDINATOR ACCEPTANCE (2026-07-06) — final dispatched shape

Fable review returned **dispatch-with-amendments** (8). Adopted:

- **A1 — the "one env-state bug" framing is half-wrong:** the grey-direction hypothesis ("fallback
  fires while env is lit") was **already refuted in Wave 6** (under `RB3_PP_OFF` the ms3000 render
  is pink, not grey — a grey-key firing would be composite-independent). The source has three env-
  state decision points (engagement `:1404` incl. a silent `mAmbientFogOwner`-null miss; grey-key
  no-lights fallback `:1523-1534` inside the engaged branch; DrawMesh env staleness `:2404-2409`),
  and the ms3000 grey lives in a FOURTH stage: composite mid/low-tone desaturation of hot venue
  input (interaction, not one machine; concrete probe candidate: the unorm intermediate at
  `RB3PostProc.cpp:155`). **S1 runs TWO hypotheses** — (H1) engagement-miss → pink @ms21000;
  (H2) composite desat of hot input → grey @ms3000 — both in Lane A's fence.
- **A2 — S2 splits into two flags/fixes, and the song-start gate targets `RB3_PP_OFF`'s look**
  (hue ~324.9, mid-tone sat 0.389) **with the venue path verified still engaged** — NOT the
  venue_light_off control, which is the flat unlit pink-base look; a fix could pass that gate by
  regressing to flat lighting (the exact W3.3-fix failure mode repeated).
- **A3 — the wash gate is mechanism-counter-based, not rate-based:** 0/8-vs-1/8 lets a no-op pass
  ~34% of the time. Primary gate = instrumented engagement-miss events **0/N, N≥16**, plus a
  deterministic forced-miss arm whose corrected fallback renders non-pink (venue_light_off's 8/8
  PINK is the deterministic fail-red).
- **A4 — game-side per-frame hands correction VERIFIED expressible:** the palette composes
  engine-side per draw but from game-writable state (`owner->BoneOffsetAt(b) * boneWorld`, mutable
  `Transform&`; the `NativeRepinHandsRigid` call site at `BandCharacter.cpp:581` already runs every
  Poll post-pose — an UNLATCHED variant drops into the same seam, zero engine edits). Two hazards
  the plan must handle: the palette reads the OWNER mesh's offsets, and the default-ON draw-time
  `WorldXfm_Force` pass (`:3421-3446`) recomputes bone worlds AFTER Poll — force the anchor chain
  before sampling or the correction composes against a stale wrist world.
- **A5 — Lane B S3's hard exit = the in-engine `IK_SHARD_VERT` wext A/B** (<20u ON vs ~106u OFF,
  same-binary/same-member protocol) — the BL-A2 `RealPathFixture` gtest is a SKIP needing Lane-A-
  owned engine probes → optional/staged. Both arms run with `RB3_HANDS_POSEAWARE` **unset** (it
  overwrites the same meshes' binds at the same seam).
- **A6 — Lane C runs the `RB3_HEADMAT_DBG` probe census FIRST** (zero-new-code discriminator:
  null-diffuse vs authored-black vs alpha). Corrections: W2.7's fix lives in
  `OutfitConfig::SetSkinTextures` (character-skin path — venue quads never pass through it;
  "generalization" = a NEW call site); if the clean lever is `RB3MaterialBinder.cpp` (forbidden),
  the sanctioned outcome is a Wave-9 staged patch. Exit gates added: probe census + visual A/B +
  lineup + fail-red; Sonnet fixes only the confirmed-W2.7-family case, else escalate Opus.
- **A7 — baselines:** `RB3_PP_LUMA_CEILING` unset in EVERY Wave-8 arm; S1 first reproduces the
  matrix baseline rates on the current pin `a94762f` (the matrix ran on `af4a22a`); pre-Wave-7
  menu baselines are stale for captures (hub quad hidden, text darker).
- **A8 — standing gates restored in S2** (milo-engine-tests, DC3 zero-blast, classification regen
  by coordinator); Lane C fence explicitly forbids `BandCharacter.cpp` + `RB3MaterialBinder.cpp`;
  no missed collisions (Lane B is the sole BandCharacter.cpp writer).

**Final dispatched shape:** **Lane A** S1 two-hypothesis instrumentation → S2 two flags (H1
engagement fix, H2 composite desat fix), mechanism-counter gates + PP_OFF-look target → S3
independent verify; **Lane B** S1 plan (unlatched per-Poll seam, two A4 hazards) → S2 impl
default-OFF → S3 IK_SHARD_VERT A/B hard exit + saddleshoe bonus check; **Lane C** probe-census
first → fix only if W2.7-family (Sonnet) else diagnose+stage (Opus).

---

_(Original draft below, retained for provenance; superseded where the acceptance above differs.)_

**Draft status:** DRAFT — under Fable pre-dispatch review, not yet dispatched.
Parent: `REFACTOR_PLAN.md`, `execution/README.md` (Wave 1–7 results + hard rules + standing review
gate + Wave 8 menu), `execution/WASH/STATUS.md` (the matrix verdict). Engine pin `a94762f`.

## Where we are (entering Wave 8)

Five default-ON fixes shipped (placement contract, black head, hands rest-capture, text floor, hub
quad). The Wave-7 wash matrix **named the wash mechanism: the P4 per-environ venue-light rewrite**
(`RB3_VENUE_LIGHT_OFF` → PINK 8/8 vs default 1/8) — the pink cast is the broken/unlit-env base that
the rewrite stochastically masks. Crucially, the Wave-6 W3.3 diagnosis measured the OPPOSITE sign at
song start (`RB3_VENUE_LIGHT_OFF` = **color** at ms3000, default = grey): early in the song the
rewrite (or its grey fallback) CAUSES the wash; later it MASKS a pink base. **Both reopened visual
items (grayscale song-start W3.3b + stochastic wash) plausibly reduce to one env-state-handling bug
in the same code.** Separately: the finger shard has exactly one live fix class left (per-frame
pose-aware basis correction), with its far-vertex oracle already built and RED at 79-107u.

## Proposed Wave 8 lanes

**Lane A — the venue-light env-state bug (engine; owns `Rnd_Wgpu_RB3.cpp` env/scene area +
`RB3PostProc.*` + WGSL; sequential):**
- **S1 (root-cause, Opus):** instrument per boot, at BOTH timepoints (songMs ~3000 and the
  ms21000±250 pinned shot): which `RndEnviron` is `sCurrent`, whether the per-environ rewrite
  engaged, whether the grey fallback fired, and the frame's wash class. Correlate across ≥8 boots:
  the hypothesis to prove/refute is **one env-state machine misbehaving in both directions**
  (fallback fires while the env is actually lit → grey at song start; rewrite fails to engage on
  some boots → pink mid-song). Deliverable: the exact decision path (file:line) with per-boot
  evidence tables.
- **S2 (fix, Opus):** per S1 — deterministic engagement for the active shot's environ + a correct
  unlit/broken-env fallback (neither pink base nor grey-over-lit). Flag-first default-OFF,
  registered. Gates: (a) matrix protocol at the ms21000 shot → wash 0/8 flag-ON with the
  **pure-default arm as baseline** (WASH/STATUS.md disclosure: don't inherit the luma_on arm);
  (b) song-start sweep ms2000-6000 in color, matching the venue_light_off control's hue; (c) lineup
  PASS + venue A/B (no lighting regression on lit venues); (d) fail-red (disable the fix → both
  symptoms reproduce); (e) flag-OFF byte-identical (drawlog 792).
- **S3 (independent verify, Opus):** all S2 gates on a fresh build + beyond-scope spot-checks
  (2 other venues, menus). Recommend flip.

**Lane B — W2.8c per-frame pose-aware appendage correction (the hands fix; rb3 game-side
preferred):**
- **S1 (plan, Opus):** pick the seam for a PER-FRAME correction of the rotation-basis mismatch
  (animated bone basis ≠ static magnet basis baked into invBind). Preference order: (1) game-side
  per-frame update in `BandCharacter::Poll` lineage (collision-free, like the shipped rebinds);
  (2) engine palette-build hook — **only as a staged patch** (Lane A owns `Rnd_Wgpu_RB3.cpp` this
  wave; if the fix must live there it lands Wave 9 or at Lane A's tail by coordinator sequencing).
  The plan must state the math: the per-frame effective offset that cancels R·sin(θ) for per-bone-
  authored verts, and why it cannot regress the uniformly-authored meshes the rigid-anchor helped.
- **S2 (impl, Opus):** default-OFF registered flag, W2.2 four-layer gates.
- **S3 (verify, Opus):** BL-A2 oracle flag-ON **GREEN (<20u from the 79-107u RED baseline)** — the
  hard exit; FLING=0; no 200-460u band; crowd clamp byte-identical; lineup PASS; finger close-up
  before/after. **Bonus check:** the W2.6 saddleshoe guard-DROP under the fix (same rotation-basis
  family? — if yes, W2.6 closes for free; measure, don't assume).

**Lane C — venue black poster quads (SYS-5; game/asset-side; Opus diagnose → Sonnet fix):**
- From the Wave-6 W4.1 backlog handoff: solid-black poster/decal quads in the venue backdrop
  (visible in the part_difficulty backdrop and gameplay venue set-dressing). Diagnose: missing
  texture (null diffuse → flat black, the W2.7 black-head family — is it the SAME non-recursive
  `Find` bug on venue dirs?) vs authored shadow decals vs alpha issue. If it IS the W2.7 mechanism,
  the fix may be a generalization of the shipped head fix (recursive texture bind for null-diffuse
  materials, scoped + gated). Fence: game/asset code; engine files forbidden (Lane A owns the
  render backend this wave).

**Deferred:** 4→8 light arrays (DC3 gates), W2.4 BandPatchMesh, song_select minor residuals,
`wave6-boxmap-proto` (shelved by the Wave-7 DROP).

## Process rules (carried)

Commit-per-review-cycle (rb3 `/tmp/rb3-git.lock`, engine `/tmp/milo-engine-git.lock`);
checkpoints `/tmp/wave8-checkpoints/`; builds `flock /tmp/rb3-native-build.lock` or own build dir;
flags append-only under `/tmp/milo-engine-classjson.lock`, coordinator regens once; no pin bumps,
no default flips by lanes; placement contract is default-ON (OFF arm = `RB3_PLACEMENT_CONTRACT_OFF=1`);
text floor + hub quad now default-ON too (opt-outs `RB3_UI_TEXT_FLOOR_STRICT`,
`RB3_HUB_MENU_QUAD_OFF`) — capture baselines accordingly.

## Risks / open questions for the reviewer

- **R-A:** Is the "one env-state bug, two directions" framing sound, or does the Wave-6 W3.3
  composite-exposure diagnosis (pp_off restores color at ms3000 — a DIFFERENT control than
  venue_light_off) contradict it? Both controls restored color at ms3000 — which means the grey
  needs BOTH the venue-light rewrite AND the postproc composite active. Does that split S1's
  instrumentation across two subsystems, and is Lane A's file fence still right?
- **R-B:** For Lane B S1's math: is a game-side per-frame correction actually expressible through
  `SetBone`-style updates (the shipped rebinds are load-time/one-shot), or does per-frame
  necessarily mean the engine palette path (→ everything becomes a staged patch and the lane's
  value this wave is the design + oracle-verified prototype in a worktree)?
- **R-C:** Lane C's "same as W2.7" hypothesis — cheap to test first (the W2.7 STATUS documents the
  exact null-diffuse signature + probe). Should the lane be REQUIRED to run that probe before any
  new diagnosis?
- **R-D:** Anything in the Wave-7 flips (text floor default-ON in particular) that changes
  baselines for this wave's capture gates beyond what's noted?
