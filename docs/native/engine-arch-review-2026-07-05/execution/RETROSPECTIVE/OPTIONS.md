# Options Adjudication — Debugging Capability & What To Build Next

**Role:** options adjudicator, read-only pass over `RETROSPECTIVE/REPORT.md` + `wave01–15.md`
+ spot-checks of the underlying record (`execution/README.md`, `HANDS-ADJUDICATION/`,
`SKEL/evidence/`, `../milo-trace/docs/MASTER_PLAN.md`, `native/src` HTTP endpoints,
`images/retail-screenshots/`, repo filesystem). **Date:** 2026-07-07.
**Owner's question:** do we have sufficient debugging capabilities? What tooling should we
build? 15 waves in, some of the same bugs persist — what are the best options?

---

## 0. Direct answer to the owner

**No — but the gap is narrower and more specific than "we need more debugging."** The
campaign's *gate/discipline layer* (flag-first, opt-out flips, pre-dispatch reviews, STOP
tripwires, E1) is production-grade: ten defaults shipped, zero regressions shipped, every
false fix was caught before flipping. What is insufficient is the *diagnosis instrument
layer*, in exactly three places, and the record proves it quantitatively:

1. **No external ground truth.** Every hands verdict through Wave 15 is native-only-verified.
   Three of the campaign's premise inversions (own/bound labels W9→W13; per-frame-vs-static
   W7; exhausted-vs-confounded W12→W15) were artifacts of native-only probes. ≈6–9 waves of
   exposure (REPORT §4 rank 1) on one bug family.
2. **Unvalidated oracles used as gates.** `wext>60` gated ~10 waves of pass/fail decisions and
   was declared "NOT a hands-shard oracle" at Wave 15. The W2.2 origin-anchored `SKINPOS≤92u`
   was structurally blind to the far-vertex defect it certified as fixed (W3→W7). No proposed
   metric was ever fired at known-good/known-bad frames before being trusted.
3. **A known noise floor under every visual gate.** BOOTRNG (`gRand` stream position,
   rejection-sampler consumer order) makes per-boot arm-mean gates non-resolving; the WHITE
   guard is HELD on a sign-flipping metric because of it, and the root was re-diagnosed from
   its own symptom in ≥5 lanes.

The bugs that persist are not persisting because fixes are hard to *implement* — Wave 15
derived the hands fix **offline from matrices committed at Wave 9** — they persist because the
instruments kept returning confounded answers that the (excellent) discipline layer then
correctly refused to ship. Build the three instruments below and the discipline layer stops
being a brake on wrong answers and starts being a ratchet on right ones.

**One deliberate scope note:** Wave 16 (Lane F hands fix, Lane T ROWFIX text) is dispatched
but unresulted. Nothing below assumes its outcome; item #2 is how we *verify* it either way.

---

## 1. Spot-checks of the synthesis (what I verified before ranking)

The campaign's history is premise inversions, so I checked the REPORT's load-bearing claims
against the record before trusting its ranking:

- **Hands timeline (REPORT §2A) is faithful.** wave11/12/13/15.md and
  `HANDS-ADJUDICATION` (README Wave-15 table) agree in detail: own/bound label swap survived
  W9→W13 and was killed by one pointer dump; the W12 "6th dead cell" certificate was
  confounded (male 3.1° passing under female 28.9°/gloves/nails); the winning cell was never
  measured until named at W15; `offset_basis_derivation.py` exists in
  `HANDS-ADJUDICATION/evidence/` and computes `angle(B·inv(R))=87.2°` from committed data.
- **The evidence-rescue incident is real.** README Wave-15: "APD_DIAG evidence rescued to
  `SKEL/evidence/` pre-dispatch (review A2)" — the adjudication's mixed-sign-gap input came
  "from the rescued log." Candidate (e) has one confirmed near-miss, not a pattern of losses
  (W1's lost commits were a git-discipline issue, fixed since).
- **`../milo-trace` is further along than the REPORT implies — and its named blocker is
  gone.** The repo is live (6 waves committed, first CONFIRMED dc3 decomp bug found via
  replay; wave 6 banked). Its Dolphin milestone (M3) is greenfield and MASTER_PLAN names
  "source a bootable disc" as the hard blocker — **but `Rock Band 3 (USA).wbfs` is sitting in
  the rb3 repo root** (and in 6 worktrees). Additionally, the native side of the capture seam
  (`/api/memory`, `/api/call`, `/api/replay/info`) is **already implemented** in
  `native/src`. Candidate (a) is materially cheaper than planned.
- **`images/retail-screenshots/` cannot back an SSIM harness.** It is ~10 mixed-provenance
  web images (a *360* title screen from TCRF, fandom gameplay shots, spriters-resource UI
  atlases) — reference material for human eyes, not pixel-aligned goldens. This kills
  candidate (b) *as specified*.
- **The UI-inspector rebuild claim checks out.** `scripts/native/_c34_flip_finder.py` /
  `_c34_holdlabel_probe.py` exist as one-off lane artifacts; ROWFIX built its own zmode/fill
  probes; W13's retro (gap #5) independently asks for exactly a "pixel-ROI → which draw +
  z-state" inspector after the red band was root-caused three times (W13→W14).
- **One correction to folklore:** two of the three "native-vs-Wii ambiguity" premise failures
  (W13 `EndWorld` no-op, W15 "shader ignores color") were caught by **source re-derivation**,
  not by emulator truth — so ground truth is not a cure-all for that failure class; the cheap
  cross-check lints (§4) cover those.

---

## 2. Ranked shortlist — build these

### #1 — Dolphin single-frame ground-truth probe (candidate **a**, scoped down)

**Build:** NOT the full milo-trace M3 record/replay pipeline. A thin probe: boot RB3 in
headless Dolphin from the on-disk wbfs, break at a pinned point (symbol addresses from
`orig/SZBE69_B8/files/band_r_wii.map` / Bank-8 map), dump a named struct region (all 38 hand
bones' `WorldXfm` + the authored bind for one member), and diff against the same dump from
native `/api/dta/eval` / `/api/memory` at matched clip time. One command:
`capture-wii-bones --shot <x> --frame <n> --symbol <bone>` → JSON both sides + per-bone
matrix-relative delta.

- **Cost:** 1–2 agent-waves. Wave A: Dolphin headless boot + GDB-stub/mem-read + map-driven
  address resolution + one successful bone dump (this is the whole risk). Wave B: matched-time
  native diff + report format. The milo-trace MASTER_PLAN's M3 "hard blocker" (bootable disc)
  is already satisfied; the native seam endpoints already exist.
- **Past waste it would have prevented:** the dominant line item of the campaign. W13's retro:
  "one run of 'dump each hand bone's authored bind and runtime pose from the real Wii binary'
  answers the entire hands question." Concretely: the shell-rebake detour (W12 SHELL_FIX, W13
  SKEL, W14 RESKIN — ~3 waves of refuted fix work), plus the own/bound inversion's 4-wave
  survival. REPORT prices the exposure at ≈6–9 waves.
- **Open bugs de-risked:** **hands fix verification** (it is the pre-registered Wave-16 Lane F
  fallback gate: "if the male visual gate misses: Dolphin+milo-trace single-bone capture");
  every future skinning/anim claim; and it is the only path to *true* retail-parity goldens
  (see §3, candidate b).
- **Known risks, stated honestly:** (i) the wbfs is retail USA; the decomp target is the
  Bank-8 *debug* DOL — first milestone must check whether Dolphin boots the debug `main.dol`
  against the retail filesystem, else capture from retail (still valid for authored-bind
  questions, but note it); (ii) the native port reads **360-ARK** assets — a Wii-vs-360
  authored-bind delta is conceivable and the probe should report it rather than assume it away
  (that ambiguity is itself worth settling — it silently underlies several "faithful-to-what?"
  arguments).
- **First milestone (go/no-go):** headless Dolphin boots the game to the hub and one scripted
  memory read returns a sane, map-named bone matrix. If the debug DOL won't boot in a day of
  effort, fall back to retail and re-price.

### #2 — Durable, validated skinning oracle in `rb3-tests` (candidate **f**, widened)

**Build:** promote the adjudication's arm-W/arm-S methodology into permanent gtest fixtures in
the existing `rb3-tests` target: real gender-split assets (male + female, naked hands + gloves
+ nails), **matrix-relative** per-bone comparisons (`angle(X·inv(Y))`, never
angle-to-identity), pointer-verified instance labels, per-mesh/per-gender reporting — plus the
**oracle-validation harness** W13 called "the single most expensive missing tool": any metric
proposed as a gate must first fire on a known-GOOD and a known-BAD frame and report its
separation before it may gate a wave.

- **Cost:** 0.5–1 agent-wave. The instruments exist as lane scripts
  (`offset_basis_derivation.py`, Tier-2 EXACT joint-attachment engine probe `4c93608`,
  arm-S/arm-W probe greps in `HANDS-ADJUDICATION/evidence/`); this is consolidation + CI
  wiring, not research.
- **Past waste it would have prevented:** `wext>60` gating ~10 waves while invalid; the W2.2
  blind oracle certifying an unfixed bug (W3→W7 walk-back); the confounded non-gender-split
  death certificate (W12→W15, the SKEL/RESKIN detour's second parent); scalar-shape probes
  hiding the label swap (W9→W13).
- **Open bugs de-risked:** **Wave-16 Lane F adjudication** (its §5 gates — gender-split Tier-1
  count>5°=0, Tier-2 ≤1u — become a one-command regression suite); prevents the *next*
  skinning bug (BandPatchMesh W2.4 is still queued, and memory records that family broke
  native twice before) from re-running this saga.
- **First milestone:** `rb3-tests --gtest_filter=Skinning.*` reproduces the Wave-15 arm-W
  verdict table (male 0.1°/3.1° PASS, female 28.9° FAIL pre-fix) from a clean build.

### #3 — UI render forensics: `/api/uidump` + per-draw provenance (candidates **c**+**d**, merged)

**Build:** one endpoint, two halves that only pay off together. (1) Scene-graph dump: for the
active panel, each UI object's authored xfm / show-state / draw-order / material color vs its
actual drawn rect (or "0 draws"). (2) Provenance in the existing `/api/drawlog`: which
Trans/group/anim/material drove each draw. Killer query, from W13's own retro (gap #5): *given
a pixel ROI, which draw last wrote it, from which authored object, with what z/blend state.*

- **Cost:** ~1 agent-wave. `/api/drawlog` exists; the binder/mesh-cache TUs from the Phase-1
  decomposition (`RB3MaterialBinder`, `RB3MeshCache`) are exactly the plumbing points; the
  ad-hoc versions have been written ≥5 times (C2a/C2b/C4, C34 side-scripts, ROWFIX zmode
  probes, `RB3_UI_FLOOR_DBG`).
- **Past waste it would have prevented:** the song_select red band root-caused **three times**
  (W13 ClearDepthForOverlay → W14 flush-shim → W14 LoadOp truth) — the ROI query names it in
  one run; the ROWFIX Part-B mislabel (W15→W16: `RB3_UI_FLOOR_DBG` on the *visible glyph
  submesh* would have shown main-vs-alt font material immediately — the lane had the probe but
  not the scene-graph context); "0 GPU draws for highlight_bar.grp" took a lane to establish
  and uidump prints it for free.
- **Open bugs de-risked:** **ROWFIX flip** (Part B alt-font plumbing verification +
  partdiff GUITAR, the named same-family sibling), sidebar backing quad, every future UI
  parity report (the UI family produced 6 of the 10 shipped flips — highest hit-rate area, so
  tooling it has the best fix-per-wave ROI).
- **First milestone:** `curl /api/uidump?panel=song_select` names, for the focused row, the
  fill quad + both font materials + drawn rects; the red-band ROI query reproduces the W14
  LoadOp diagnosis from the shipped build.

### #4 — Loader-determinism *scoped* sufficient fix + per-axis ledger (candidate **g**)

**Build:** not the "large determinism seam" the waves correctly deferred — the scoped version
W12 already priced: per-consumer isolated `Rand` streams (or call-site reseeding) for the
**three enumerated rejection-sampler families** (`CameraShot` shake, `Crowd` conditional
draws, unsorted `mAnims` walk), behind the existing `RB3_LOAD_DETERMINISM`, with the W11/W12
probes (`RB3GRandDrawCount`, `RB3_LOADDET_PROBE`) folded into a standing **per-axis ledger**
(count / submission-order / gRand-position / float-clock, each PASS/FAIL per boot) so "part-b
landed" can never again silently mean "one of three axes landed" (W4→W11).

- **Cost:** 1–2 agent-waves. The mechanism (H-ORDER) has held since W12; the consumers are
  named; A.S2's 62% partial shows the shape works and PRIMARY-FAIL shows exactly what's left
  (order, not values).
- **Past waste it would have prevented:** ~80 boots of non-resolving arm-mean gates (WASH
  matrix W7, WHITE W9/W10); the WHITE guard HELD on a sign-flipping metric; the root
  re-diagnosed in ≥5 lanes.
- **Open bugs de-risked:** **WHITE real-lever** (explicitly blocked on this), **wash residual**
  (the per-FX phase co-sampling instrument needs matched boots to compare), and *every* future
  per-boot visual gate — including the E1-adjacent numeric gates item #1 and #3 will want.
  This is gate infrastructure, not just a bug fix.
- **First milestone:** 10/10 boots identical gRand stream position at the pinned capture
  point with the flag ON (the exact PRIMARY that failed in W12), then re-grade the held
  `RB3_VENUE_WHITE_GUARD` on the now-resolving gate.

---

## 3. Candidates NOT worth building (as specified)

- **(b) Retail-parity SSIM harness vs `images/retail-screenshots/` — NO.** The golden set
  does not exist: ~10 web-sourced images, mixed platforms (the title screen is the *360*
  version), unmatched songs/venues/resolutions. SSIM against those measures provenance, not
  parity. Worse, until #4 lands, per-boot ROI metrics sit on the BOOTRNG noise floor — the
  campaign already proved arm-mean visual gates don't resolve there (WHITE, W9/W10). The
  human-ad-hoc pipeline (owner screenshots → U1–U4 filings) has been *effective* — 6 UI flips
  came from it. **Revisit only after #1 exists**, when Dolphin can capture true pixel goldens
  at scripted, matched scenes; then a small ROI-diff harness on deterministic menu screens
  (not gameplay) becomes real. Do not spend a wave on it now.
- **(e) Evidence auto-checkpointing — not a build item.** One confirmed near-miss (APD_DIAG
  rescue, README Wave-15). The fix is a 30-minute harness rule, adopted in §4: probes write
  under `execution/<KEY>/evidence/` directly (never bare `/tmp`), and the wave close-out
  checklist includes "evidence committed." No wave, no tool.
- **(d) standalone — merged into #3.** Provenance without the authored-side dump answers
  "what drew" but not "what *should* have drawn"; the lanes needed both every time.
- **Full milo-trace M3 record/replay for Wii — defer.** The record pipeline's value for *this*
  campaign is subsumed by the thin probe (#1). Extend toward full capture only if #1's
  single-frame dumps prove insufficient (e.g. the hands answer turns out to need a full anim
  timeline diff). milo-trace's Xenia/dc3 track continues on its own merits.

---

## 4. Strategy changes that need no tooling (adopt in the Wave-17 kickoff template)

1. **Matrix-relative + pointer-verified, or it isn't a bone claim.** Any basis/rotation claim
   must be a relative matrix product between named instances (`angle(X·inv(Y))`), with each
   instance's pointer identity dumped. Scalar angle-to-identity numbers are banned as
   evidence. (Would have killed the own/bound swap at W9 instead of W13; the 87° story at W11
   instead of W15.)
2. **Split by gender/mesh/route by default; aggregates cannot refute.** Any probe over
   heterogeneous populations reports per-population rows; an aggregate number may motivate but
   never close a cell. (The confounded death certificate, W12→W15; the U1 two-mechanism lump,
   W12→W16.)
3. **No unvalidated oracles as gates.** Before a metric gates a wave, fire it on a known-GOOD
   and known-BAD frame and record the separation in the lane STATUS. (wext, SKINPOS.)
4. **Shipped-flag contradiction check.** Before escalating a diagnosis to a new engine item,
   grep the flag registry's one-line "what this shipped flag PROVES" index; if a default-ON
   flag contradicts the claim, the claim is wrong. (W15 "shader ignores color" vs the shipped
   W4.2 flip — cost a full Fable re-derivation stage.)
5. **Diagnosis lanes get wide read grants.** Fix lanes stay narrow; a lane whose exit is a
   *mechanism* must be allowed to follow the mechanism across TU boundaries. (ROWFIX Part B
   stopped at its grant edge and mislabeled an rb3 plumbing bug as an engine gap.)
6. **Enumerate the option table before the second fix attempt in any family.** Maintain the
   (anchor × bone) — or family-equivalent — coverage table with measured/unmeasured status in
   the family STATUS. The hands winner sat unmeasured for 4 waves while measured-dead cells
   were re-argued. Run the adjudication lane **before** the 3rd dead cell, not after the 7th.
7. **Evidence lives in `execution/<KEY>/evidence/`, committed, or it doesn't exist.** Probe
   outputs feeding any conclusion get copied before the lane returns; `/tmp` is scratch.
8. **Flag hit-count on every negative result.** "Fix measured no benefit" requires the
   branch-entry count (`RB3_FLAG_HITCOUNT` pattern) proving the code ran. (W3's false-negative
   BIND_FIX fired 0×.)
9. **Flavor-membership check before scoping a lane on a file.** One `grep` of the CMake source
   lists: is this TU compiled into rb3-native? (W3's primary lead was DC3-only.)
10. **Tools ship as pre-dispatch diagnosis gates, not post-mortems.** If a wave's plan
    includes "build instrument X to verify the fix," build X *first* and let it grade the
    diagnosis before the fix is implemented. (Gaps 4/5/6/8 of REPORT §4 were each built 1–3
    waves late, as the refuting verify stage.)

---

## 5. Recommended Wave 17+ plan (if the owner adopts the top picks)

**Gate first:** adjudicate Wave 16 (Lane F hands cell, Lane T ROWFIX alt-font) before
dispatching. Its outcome moves emphasis but not the plan: if Lane F PASSES, #1 verifies the
flip and #2 locks it against regression; if Lane F FAILS, #1 is the pre-registered next step
by the adjudication's own §5.

**Wave 17 — instrument wave (3 lanes, no gameplay-code fixes):**
- **Lane D (Opus): Dolphin probe milestone 1** — headless boot from the wbfs (try debug DOL
  first, retail fallback), map-driven single-bone dump, diff vs native `/api` at matched clip
  time. Exit: the per-bone Wii-vs-native delta table for one member's hand chain, or a
  priced NO-GO.
- **Lane S (Sonnet): skinning fixtures** — arm-W/arm-S → `rb3-tests` gtest, gender/mesh-split,
  matrix-relative; plus the oracle-validation harness. Exit: Wave-15 verdict table reproduced
  in CI; Wave-16 §5 gates runnable as one command.
- **Lane U (Sonnet): `/api/uidump` + drawlog provenance.** Exit: red-band ROI query reproduces
  the W14 LoadOp diagnosis; focused-row dump names both font materials.
- Kickoff template gains §4 rules 1–10 verbatim.

**Wave 18 — cash the instruments:**
- **ROWFIX completion**: flip Part A+B together (and partdiff GUITAR) with Lane U's dump as
  the gate, per the Wave-16 review's rb3-side scoping.
- **Hands close-out**: whatever Wave 16 left open, decided against Lane D's ground truth —
  this family does not get another native-only-verified verdict.

**Wave 19 — determinism (#4):** per-consumer Rand streams for the three named samplers +
per-axis ledger; PRIMARY = 10/10 identical stream position; then immediately re-grade the held
WHITE guard and dispatch the wash per-FX co-sampling instrument on the now-quiet floor.

**Standing carried items** (4→8 lights, W2.4 BandPatchMesh, sidebar quad) slot in behind these
— #2's fixtures are the prerequisite that keeps W2.4 from becoming the next hands saga.

**What this buys, in the campaign's own accounting:** REPORT §4 prices the top three gaps at
≈9 of 15 waves of re-derivation. Waves 17–19 spend ~4–5 lane-waves buying the three
instruments that close them, against a backlog (hands verification, ROWFIX, WHITE, wash,
BandPatchMesh) where every remaining item is inside those instruments' coverage.
