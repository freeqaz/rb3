# Engine-Refactor Campaign — Cross-Wave Retrospective Synthesis

**Scope:** 15 completed waves (2026-07-05 → 2026-07-07), engine pin `a8089c3` → `84ccb9e`
(16 pin bumps). Wave 16 dispatched (kickoff + Fable review `WAVE16_REVIEW.md`) but not yet resulted.
**Sources:** `RETROSPECTIVE/wave01–15.md`, `/tmp/retro-checkpoints/wave*.json`, `execution/README.md`,
per-wave `WAVE*_REVIEW.md`. Written for the project owner + a follow-on strategy review.

**One-sentence read:** the campaign's *engineering discipline was excellent* (flag-first, opt-out
flips, pre-dispatch reviews, honest STOP-tripwires — no regression ever shipped default-ON, 10 real
fixes did) but its *diagnostic instruments were repeatedly confounded*, and the same three missing
measurement capabilities — a validated skinning oracle, a boot-determinism harness, and external
ground truth — cost roughly **9 of 15 waves** of re-derivation on two bug families.

---

## 1. SCOREBOARD

### Shipped default-ON (10 flips — "defaults now TEN", README Wave-15 close-out)

| # | Fix | Flag (opt-out) | Wave / commit | Held? |
|---|---|---|---|---|
| 1 | Crowd/drum placement contract | `RB3_PLACEMENT_CONTRACT` | W6 `fced18b` (mech W4 `6852caa`) | ✅ never refuted |
| 2 | Black singer head (nested-milo `Find` miss → null diffuse) | game-side, no flag | W6 `837808e1` | ✅ |
| 3 | Head/hands rest-capture rebind | `RebindHeadHandsAtRest` default-ON | W3 | ✅ *but does NOT fix the finger shard* (see §2) |
| 4 | UI text color floor | `RB3_UI_TEXT_FLOOR_STRICT` | W7 `a94762f` | ✅ |
| 5 | Hub grey menu-quad | `RB3_HUB_MENU_QUAD_OFF` | W7 `cda3b326` | ✅ |
| 6 | Venue chroma-preserve (grayscale song-start) | `RB3_PP_CHROMA_PRESERVE_OFF` | W8 `a320f9d` | ✅ |
| 7 | Hub ticker Y-stacking | `RB3_HUB_TICKER_YFIX_OFF` | W13 | ✅ |
| 8 | UI-after-grade composite (menu text wash) | `RB3_UI_POST_GRADE_OFF` | W14 (engine) | ✅ (3rd root-cause) |
| 9 | song_select album-art assembly move | `RB3_SS_ART_YFIX_OFF` | W14 (rb3) | ✅ |
| 10 | "(null)" gamertag → Player-N fallback | `RB3_PLAYER_NAME_FALLBACK_OFF` | W15 `3fdf482b` | ✅ |

Plus the **Phase-1 decomposition** (W2): `Rnd_Wgpu_RB3.cpp` 7,017 → 4,747 lines across 6 behavior-
preserving MOVE TUs (`RB3MeshCache`, `RB3MaterialBinder`, halo/postproc/quad, ring dedupe, render
hook), and the **W1.6 DrawContext / `RB3SceneBinding`** SYS-3 state-leak fix (W3 `9df8349`/`6221a56`)
— the load-bearing structural win that enabled the placement contract and every later composite fix.
None of these were ever refuted.

**Held / landed-OFF flags (earned but not flipped, or documented-dead):**
- `RB3_ROWFIX` (W15 `51ae685a`) — Part A fill-quad repaint works; held OFF because Part B (focused
  text stays white) is unfixed → bright fill + white text is *worse*.
- `RB3_VENUE_WHITE_GUARD` (W9/W10 `2998e78`) — HELD: primary `d_hi_frac` metric **sign-flips**
  (+17.96 vs −6.89), null control swings the same ±5–18 → indistinguishable from the BOOTRNG floor.
- `RB3_LOAD_DETERMINISM` (W12 `28c71424`) — opt-in partial reducer, ~62% spread cut but **PRIMARY-FAIL**
  (reseed fixes values not order); correctly not flipped.
- `RB3_ENV_FOG` / `RB3_ENV_PROJLIGHT` (W5/W6) — default-OFF; **no boot-reachable venue authors fog**
  (34/34 `FogEnable()==false`), so nothing exercises the path.
- **7 dead hands cells**, all default-OFF with REFUTED headers: `RB3_HANDS_BIND_FIX`,
  `RB3_HANDS_POSEAWARE`, `RB3_HANDS_PERFRAME_CONJ`, `RB3_APPENDAGE_REST_ROT`,
  `RB3_APPENDAGE_ASSET_REBAKE`, `RB3_HANDS_SHELL_FIX`, `RB3_HANDS_RESKIN`.
- `RB3_PP_LUMA_CEILING` (W7 `7943bfa`) — documented **no-op on target** (built on a false Wave-6
  mechanism; flag-first kept it from shipping).
- `RB3_HUB_TEXT_CONTRAST` (W12) — faithful sub-fix, doesn't pass the gate alone.

**User-visible delta:** the menus, hub, song-select, and venue color grade are materially closer to
retail (10 shipped fixes span placement, lighting/grade, and UI layout/text/profile). The finger
shard, the stochastic venue wash, and the focused-row text polarity remain visible.

### Still broken after 15 waves (honest open list)

1. **HANDS / FINGERS — the flagship unfixed bug.** R·sin(θ) far-vertex smear (95–106u finger chain).
   After 7 measured-dead fix cells and 3 premise inversions, Wave 15 *adjudicated* it: numeric closure
   `angle(B·inv(R)) = 87.2°` (authored-bind-vs-`SetDeformation`-seed rotation), and named a
   **never-measured** fix cell — keep authored per-mesh offsets + repoint appendages to `own` via
   `SetBone(b, own, /*calcOffset*/false)` in `RebindHeadHandsAtRest`. **Dispatched Wave 16 Lane F; not
   yet confirmed.** Still open, still native-only-verified (no external ground truth).
2. **Focused-row text polarity (ROWFIX Part B).** White-on-navy vs retail black-on-white. Wave 15
   mislabeled it "engine glyph shader ignores font-material color"; Wave 16 review A1 **refuted that at
   source** (`standard_wgsl.inc:764`) and re-scoped it to an rb3 alt-font plumbing bug
   (`UILabel::DrawShowing` propagates `mColorOverride` only to the MAIN font mat, `:266-270`).
   Dispatched Wave 16 Lane T; not confirmed. Same family: partdiff GUITAR focused text.
3. **Boot / loader determinism (W0.3d part-b).** Root = global `gRand` **stream position**, mechanism
   = consumer-ORDER × variable-count rejection samplers (`CameraShot`/`Crowd`/`mAnims`) fed by a
   main↔worker glibc-arena race. `RB3_LOAD_DETERMINISM` gets ~62%, PRIMARY-FAIL. This is the **noise
   floor** that makes every per-boot arm-mean visual gate non-resolving. Open; large determinism seam.
4. **WHITE venue over-exposure real-lever.** Guard held OFF; blocked on #3.
5. **WASH residual (FX / swept-light phase axis).** After the chroma-preserve fix, the residual wash is
   per-FX phase fidelity, needs a co-sampling instrument (named Wave 11/12, never built). Open.
6. **4→8 scene lights** (`SceneUniforms` 656→752 growth, DC3 gates) — carried every wave since W6/W7,
   never done.
7. **W2.4 BandPatchMesh** — carried since W4, never addressed.
8. **song_select sidebar backing quad** — asset-authoring polish (W13 closed as asset-difference).

---

## 2. BUG-FAMILY TIMELINES

### A. Hands / finger shard — ~9 waves (W1, W3, W4, W6–W15), the campaign's dominant sink

| Wave | Hypothesis / claim | Fate |
|---|---|---|
| W1 | W0.1/W0.4 goldens are "the bind-pose-identity oracle whose absence caused the BandPatchMesh reverts" | **Blind** — golden feeds a correct-by-construction palette to both sides; can't see a wrong-basis palette (named W6) |
| W3 | W2.2: "fix exists, numerically gated, one flag-flip from shipping"; draw-time `SKINPOS≤92u` reads 68.2u clean | 92u tripwire is origin/near-bone anchored → **BLIND** to far-vertex shard; W4 headline amplified the claim |
| W4 | W2.6: foot/shoe "within structural envelope, no fix" | Verdict rests on the same blind oracle |
| W6 | Shard characterized correctly (R·sin θ, animated basis ≠ static magnet); prescribed **static** BL-A1 rebake | Prescription contradicts its own "animated basis" diagnosis; steered W7 into a dead class |
| W7 | Killed 2 static classes; concluded "needs a **per-frame** pose-aware correction" | **Wrong prescription** — sent Waves 8–14 chasing per-frame; the eventual fix is static |
| W8 | Per-frame conjugation | Refuted: 80u → 500–2600u (smoke tripwire) — 3rd dead class |
| W9 | "offset rotation basis conjugated 42–87° off, CONFIRMED 3 ways" | All 3 shared **one confounded reference** (char-space vs world-space, first-capture anchor); constancy = the confound's fingerprint, read as proof |
| W10 | Asset rebake = "the only unrefuted path"; S1 verdict MATCH | Rebake "fixes" the metric only by **freezing** the hands; `RB3_APD_DIAG` unmasks the **pre-repoint** reference (compares against a bone the draw doesn't use) — 5th dead class |
| W11 | Tier-2 rest-free joint-attachment → **palette/skeleton EXONERATED**; axis named = mesh SHELL; mechanism = "shared-magnet conjugation" | Instrument durable; **mechanism story wrong** (scalar angle-vs-identity hid a label swap) |
| W12 | `own`=shared magnet / `bound`=per-member → "87° irreducible with any single bone" → SKEL re-lane; 6th cell "class exhausted" | **own/bound labelled BACKWARDS**; "exhausted" certificate **confounded** (non-gender-split) |
| W13 | **Premise INVERTED** (runtime pointers: `own` IS per-member + animates); then "reskin is the faithful fix", "tears for everyone up to 35°" | Inversion = the wave's best moment (one cheap pointer dump); then manufactured 3 new false premises from aggregate metrics |
| W14 | Implemented RESKIN, refuted by own `wext` gate; "class CLOSED, fix out-of-scope for any bake" | Disposition right *by luck of a correct side-argument*; primary justification (`wext` + "exhausted") **wrong** |
| W15 | **Adjudication:** `angle(B·inv(R))=87.2°`; male authored offsets match `bound` to **0.1°**, female 28.9°; the confounded death certificate exposed by **gender-split**; named the never-measured repoint cell | The saga's numeric closure — computed **offline from already-committed matrices** |

**Churn:** ~7 fix-implementation cells built/measured/refuted (W3, W7, W8, W9, W10, W12, W14), 3
premise inversions (own/bound labels; per-frame-vs-static; exhausted-vs-confounded), 2 invalidated
oracles (W2.2 origin-skinpos; `wext>60` — Wave 15 declared it "NOT a hands-shard oracle", legit
two-hand extents reach 104u). **Every dead cell was on the wrong axis** — Wave 11's rest-free
instrument would have exonerated the skeleton axis in one run and redirected the whole saga.

### B. Venue wash / grayscale / WHITE / BOOTRNG — ~7 waves (W1–W2, W4–W12)

- **W1–W2:** draw-log jitter mis-scoped as "wall-clock splash animation, clock-fixable." Frozen clock
  (W2 `RB3_FIXED_CLOCK`, seed `0x5EED`) made the **count** deterministic (888×) but exposed a ~33%
  draw-**order** flake (336–354 mesh-identity swaps; two byte-identical binaries disagree with their
  own reruns). W1's `--determinism-check` reported an order-insensitive **multiset** → structurally
  couldn't see the order axis.
- **W4–W6 (wash):** W4 correctly measured the pink "wash" **flip-independent** (A/A controls,
  `W2.1/STATUS.md:304-328`) — but shipped it as prose+n=2, not a gate. **W5 un-got-it** (held the
  placement flip on an n=2 "wash only flag-ON" read, while an equally extreme flag-OFF outlier
  OFF_2=23.7 sat in the same montage). **W6 re-got-it** (`wash_score.py`, n=7, Mann-Whitney U=24.0
  p=1.0) → flipped. ~1.5 waves re-deriving W4's own answer.
- **W6–W8 (grayscale):** W6 diagnosed "Reinhard ceiling desaturates hot pink" → staged a ceiling
  patch. W7 built + landed it (`RB3_PP_LUMA_CEILING`), then its **own** tonal-band verify refuted it
  (grey is **sub-knee** desat, mid-sat 0.026 vs 0.389; ceiling is identity below the knee). W8 fixed it
  for real (`RB3_PP_CHROMA_PRESERVE`).
- **W7–W8 (wash mechanism):** W7 5-config matrix verdict = "P4 venue-light rewrite masks a pink base"
  (`venue_light_off` 8/8 PINK). W8 `RB3_WASH_PROBE` overturned it **in one run**: 0/8 boots miss
  engagement, PINK boots have **byte-identical lighting inputs** → downstream composite flood, not a
  revealed base. The discriminator (`RB3_PP_OFF` probe) was *named in WASH/STATUS.md* and never run.
- **W9–W11 (WHITE + root):** W9 staged a WHITE guard onto a gate it couldn't grade; W10 HELD it
  (sign-flipping metric); W11 finally named the root — global **`gRand` stream POSITION** (~11k
  draw-count spread, 12 distinct values / 12 boots).
- **W11–W12 (mechanism):** W11 handed it off as "async-loader **completion-order**, staged since W4."
  W12 refuted **both** halves: no staged patch exists (A1); H-TIMING refuted (511 completions
  byte-identical, land by frame 2, gdraw diverges frame 4 with **zero** completions) → **H-ORDER**
  (rejection samplers). H-ORDER held through Wave 15.

**Churn:** the boot-nondeterminism root was re-diagnosed from its own symptom in **≥5 separate lanes**
(eye jitter W1 → draw order W2/3 → preset pick W11 → gRand position W11 → consumer order W12) because
no artifact enumerated the orthogonal axes {count · submission-order · gRand-position · float-clock}.

### C. UI focused-text color — ~4 waves (W7, W12–W16)

- W7 shipped the hub text floor (`RB3_UI_TEXT_FLOOR_RELAXED`) — a durable win *and* later the proof
  that the glyph shader **does** honor material color.
- W12 filed hub + song-select-row + partdiff GUITAR as one "compositing" family. Hub half right
  (→ shipped W14). **Under-split:** the row/partdiff half has a *different* mechanism.
- W15 diagnosed the row half as "engine RndText shader ignores font-material color" → escalated to a
  full engine wave item.
- W16 review A1 (CRITICAL) **refuted at source**: shader multiplies `material.color.rgb` into every
  text draw; the campaign's **own** shipped W4.2 flip *proves* dark colors reach text pixels. Real
  cause = rb3 alt-font plumbing (`UILabel.cpp:266-292`). Grant widened to `src/system/ui/`.

### D. UI layout / composite (hub quad, ticker, album art, red band) — ~4 waves (W6, W13–W14)

Mostly clean wins (hub quad W7, ticker W13, album art W14). The one churn item: the song_select
**red band** was root-caused **three times** — W13 "ClearDepthForOverlay side-effect" → W14 "minimal
flush-only shim still shows it" → W14 correct: `FlushPostProcMidFrame`'s own depth-clear-on-resume
revealing a z-occluded SETLISTS quad, fixed with `LoadOp::Load` on menu re-open. Each wrong mechanism
was carried into the next wave's menu.

### E. Loader determinism (subset of B, but a distinct standing seam)

Named W12 (H-ORDER, held through W15), partially reduced (`RB3_LOAD_DETERMINISM` ~62%, PRIMARY-FAIL),
never closed. The sufficient fix (per-consumer isolated Rand streams or determinize every
rejection-sampler-feeding site) is staged design, correctly deferred as too large for an in-wave land.

---

## 3. PREMISE-FAILURE ANALYSIS

**Waves 12–16 each carried a false premise into dispatch** (all but one caught by the pre-dispatch
Fable review or mid-lane):

| Wave | False premise | Caught by | Root class |
|---|---|---|---|
| W12 | "W0.3d part-b staged patch exists" (3rd resurrection) | Fable A1, pre-dispatch | **Doc-drift** — a design note read as a landed patch |
| W12 | `own`=shared magnet / `bound`=per-member (labels) | W13 S.S1 runtime pointer dump | **Probe-label inversion** — inherited the probe's naming |
| W13 (G) | song_select red band = ClearDepthForOverlay; menus `mCanEndWorld=1` / `EndWorld()` no-op | W14 (minimal shim still shows band) | **Native-vs-Wii ambiguity** — native stubs behave unlike the Wii path |
| W13/W14 | "reskin is the faithful fix" / "offset-bake class exhausted" | W15 gender-split + offline algebra | **Confounded aggregate** metric (non-gender-split) |
| W15 | "engine RndText shader ignores font-material color" | W16 review A1, source re-derivation | **Native-vs-Wii ambiguity + missing self-consistency check** |
| (saga) | probe `own`/`bound` labels backwards | W13 | **Probe-label inversion**, survived W9→W12 (**4 waves**) |
| W9/W10 | dual-skin reference captured pre-rebind (measures a bone the draw doesn't use) | W10 `RB3_APD_DIAG` | **Confound in the reference frame** |

**Why this keeps happening — three shared roots:**

1. **Probe-label / scalar-shape mistakes that a matrix or pointer comparison would expose.** The
   own/bound inversion (W9→W13, 4 waves) and the "87° magnet conjugation" story (W11→W15) both trace
   to the *shape* of the instrument: it emitted **scalar angle-to-identity** values (87.3°, ±6–35°)
   that structurally **cannot reveal a label swap between two instances** or a relative rotation. The
   data to catch it existed at W9/W11 — Wave 15 recovered the truth (`B·inv(R)=87.2°`, `own≈B` at 3.1°)
   **offline from already-committed matrices**. Structural fix: **"bone-basis error claims must be
   matrix-relative, not scalar; instance identity must be pointer-verified"** — a review lint (W14
   gap #4) + a matrix-relational probe (W11 gap #2).

2. **Native-vs-Wii ambiguity with no external ground truth.** Every hands verdict through W15 is
   *internally consistent but externally unverified* — the campaign never had a Dolphin/milo-trace
   single-bone capture to answer "is the changed bind *right*?" (only "faithful-to-a-native-reference").
   The `mCanEndWorld`/`EndWorld()` no-op (W13) and the "shader ignores color" claim (W15) are the same
   shape: a native stub or native-only code path *looks* like a bug because there's no oracle for what
   the Wii actually does. Structural fix: **external ground-truth capture** (named W11 rank-3, W13/W15
   as fallback truth, still not built).

3. **Confounded aggregates read as verdicts.** The "class exhausted" certificate (W12→W14) was a
   **global** wext/Tier-2 regression that averaged a passing male (3.1°) under failing female/gloves/
   nails (28.9–170°). No gate split by gender/mesh until W15 — which the saga self-names as "the single
   biggest instrument lesson." Structural fix: **gender/mesh-split by default** in every skinning probe.

**The meta-pattern:** the campaign *created* new false premises fastest when it **reasoned forward from
a confounded metric instead of running one more direct probe**. W13 is the clean demonstration: it
inverted one bad premise with a cheap pointer dump (excellent), then immediately manufactured three
more by reasoning from aggregate own-vs-bound gaps and an inherited exhaustion claim. Direct
measurement beat elaborate symptom-metric reasoning **every** time it was tried.

---

## 4. TOOLING-GAP RANKING (by estimated wasted-agent-cost)

| Rank | Missing capability | Builds on (existing asset) | Waste it would have prevented |
|---|---|---|---|
| **1** | **External Wii bone-world ground truth** (Dolphin + milo-trace single-bone `WorldXfm` at matched clip time, diffed vs native `own`) | **`../milo-trace` exists with a full 45-task plan** (`docs/MASTER_PLAN.md`); the HTTP debug API exists for native capture | The **entire** hands saga's external-verification gap. Every one of 7 dead cells + the W15 adjudication rests on native-only surrogates. Named W11 rank-3, W13/W15 fallback truth — **never built**. Would have answered "is the runtime bone the authored basis, per-bone per-gender" in one run. **≈6–9 waves of exposure.** |
| **2** | **Validated, space-consistent, rest-capture-free skinning oracle** (Tier-2 joint-attachment on the *uploaded* palette; matrix-relative, gender/mesh-split, self-validated against known-good/known-bad frames) | Built **at Wave 11** (`4c93608`) — 4 waves too late; `rb3-viewer` exists for skinned draw inspection; the W0.1 golden pattern existed at W1 | W3 blind 92u oracle → W7 far-vertex → W9 confounded dual-skin → W10 pre-repoint confound. `wext>60` gated ~10 waves of pass/fail decisions and was **invalid the whole time** (W13/W15). **≈4–5 Opus fix-stages + 2 Fable reviews (W9–W10 alone).** |
| **3** | **Boot-determinism harness** (fixed `gRand` stream position + pinned director cut + per-call-site consumer-sequence differ, reported as a per-axis ledger) | Frozen clock (`RB3_FIXED_CLOCK`) existed W2; `RB3GRandDrawCount` (W11) is a global counter only; `RB3_LOADDET_PROBE` (W12) | Every per-boot arm-mean visual gate (WASH N=8 matrix W7, WHITE W9/W10) ran on a non-resolving floor. The wash/WHITE/BOOTRNG root was re-diagnosed in **≥5 lanes**. A sequence-differ would have printed "first divergence at `CameraShot.cpp:265`" and killed the completion-order hypothesis a full wave early. **≈2 diagnosis waves + ~80 boots of compute.** |
| **4** | **Time-controlled, N≥6-per-state numeric wash/exposure detector** as a standing gate | Built **at Wave 6** as `wash_score.py` (`--selftest` green) — 2 waves too late; the HTTP `/api/screenshot` API exists | W4 measured the wash flip-independent as prose+n=2; W5 re-measured it wrong (n=2, coordinator eye); W6 built the detector and proved it right (n=7). **≈1.5 waves** (the whole W2.1-flip-blocker lane + the W5 hold). |
| **5** | **Render-input state probe at capture** (per-boot lighting/composite digest — became `RB3_WASH_PROBE`) | Built **at Wave 8** (`71469af`) — the WASH matrix (~40 boots W7) chased a black-box answer it gave directly | W7's wrong mechanism verdict ("venue-light masks pink base") fully redone in W8. **≈1 verdict-stage + ~40 boots.** |
| **6** | **Per-tonal-band (knee-split) saturation decomposition** as the *diagnosis* gate (pre-impl) | Built as W7's *post-mortem* verify — should have been the W6 W3.3 gate | W6 staged + W7 landed `RB3_PP_LUMA_CEILING` (a documented no-op) against a wrong mechanism. **≈2–3 agent-stages.** |
| **7** | **Flavor-membership oracle** (`which-flavor <file|symbol>`: is X compiled into rb3-native vs DC3-only?) | CMake source lists exist; one query | W3's PRIMARY draw-order lead (`TransparentQueue.cpp`) was **DC3-only, never compiled** — discovered after a lane + fleet were scoped around it. Also the "goldens run in the DC3 suite" catch. **≈1 diagnosis stage.** |
| **8** | **Flag-execution hit-count probe** (`RB3_FLAG_HITCOUNT`, per-flag branch-entry count at exit) | The flag registry (W0.6) exists | W3's `RB3_HANDS_BIND_FIX` "measured no benefit" was a **false negative** (branch fires 0×). W7 step-0 re-derived the 0× fact by hand. |
| **9** | **"Does any shipped flag contradict this diagnosis?" cross-check** (one line per default-ON flag stating what it PROVES) | The 10 shipped flags + registry exist | W15's "shader ignores color" directly contradicts the shipped W4.2 flip; cost a full W16 Fable re-derivation stage. |
| **10** | **Bake-cell coverage tracker** (X-anchor × Y-bone table with measured/unmeasured status) | The dead-cell flags are already enumerated | The winning cell ("keep authored offsets + repoint") was **never measured** until W15; cells were tried serially W12–W15. |

Gaps 4, 5, 6, 8 were **all eventually built — 1–3 waves after they were needed.** The pattern is not
"nobody knew the tool was missing"; it's that tools were built as **post-mortems** instead of
**pre-dispatch diagnosis gates**.

---

## 5. PROCESS ASSESSMENT

### What worked (keep)

- **Flag-first + opt-out flips.** Every fix landed default-OFF, was numerically gated, then flipped by
  coordinator sign-off with an opt-out. This is why **no broken default ever shipped** despite ~4
  false-mechanism fixes reaching implementation (`RB3_PP_LUMA_CEILING` no-op, RESKIN, per-frame conj,
  asset rebake). W7 A3 (flag-first over "default-ON if the sweep is unambiguous") is the textbook case:
  *the sweep was unambiguous and wrong.*
- **Pre-dispatch Fable reviews.** Repeatedly paid for themselves before a single agent ran: W3 caught 4
  false premises; W4 D1 caught a would-ship known-broken path (`RB3_SKEL_REBIND_FULL` is the fail-red
  control); W6 A3 killed the bloom-halo suspect from source; W12 caught 3 phantoms at zero cost; W16 A1
  refuted the "shader ignores color" escalation and saved an engine wave.
- **Feasibility-gates-before-edit / honest STOP-tripwires.** W8 Lane B stopped at the smoke tripwire
  (80u→500u) rather than tune blindly; W13 S.S2 honored its STOP, changed no source, registered no fake
  flag on a degenerate seam. W8 Lane C's census-first (A6) killed a hypothesis with **zero code**.
- **Checkpoint/commit discipline.** Single-writer `classification.json`, STEP-0 serialization (W10),
  filtered `git apply --cached` to avoid clobbering a sibling lane's probes (W11), commit-first survival
  across quota blips. No lost-work incidents after W1's hard-rule-7 fix.
- **E1 human-eyes gate + honest partials.** "No flips, and that is the system working" (W10) is the
  right disposition for a diagnosis wave. Diagnosis-only waves (W9, W11) produced durable instruments.
- **The W15 dedicated adjudication lane** (synthesis-only, matrix math on committed data) is the model
  for *when* to run the full-saga review: **before** the 8th implement→measure→refute round-trip, not
  after the 7th dead artifact.

### What didn't (fix)

- **Evidence lived in `/tmp` and prose, not in durable gates.** W4 measured the wash flip-independent
  correctly, then lost it across a coordinator handoff because it shipped as prose+n=2 instead of a
  reusable detector. A correct finding that isn't a standing gate gets re-litigated.
- **Probe label inversions survived 4 waves** (own/bound, W9→W13) because instruments were **trusted as
  oracles when they weren't** — nobody pointer-verified the labels or matrix-verified the angles until
  forced. `wext>60` was used as a decisive quantitative gate across ~10 waves **without ever being
  validated** against known-good/known-bad frames.
- **`wext` (and the dual-skin/`SKINPOS` oracles) used as ground truth.** Each was later shown to measure
  the wrong thing (origin-anchor, pre-repoint reference, legit pose extent). A metric-validation harness
  ("fire each proposed oracle on known-GOOD and known-BAD frames, report separation") was never built.
- **Per-wave re-derivation of the same scene-graph / determinism facts.** No per-axis determinism ledger
  meant "part-b landed" silently meant "one of three axes landed" (W4→W11). The UI focused-text family
  was re-diagnosed hub-then-row-then-partdiff across W12→W16 without an upfront per-route split.
- **Tools built as post-mortems.** Gaps 4/5/6/8 were each built 1–3 waves after the wave that needed
  them, as the *verify* stage that refuted the wave's own fix — not as the *diagnosis* gate that would
  have prevented building the fix.

**Net for the follow-on strategy review:** the discipline layer is production-grade and should be
preserved verbatim. The leverage is entirely in **instrumentation**: build the three top-ranked tools
(external ground truth via the existing `../milo-trace` plan; a validated matrix-relative gender-split
skinning oracle; a boot-determinism harness with a per-axis ledger) **before** the next hands or wash
wave, and adopt two cheap review lints — "bone-basis claims must be matrix-relative + pointer-verified"
and "does any shipped default-ON flag already falsify this diagnosis?". Those would have collapsed an
estimated 9 of the 15 waves.
