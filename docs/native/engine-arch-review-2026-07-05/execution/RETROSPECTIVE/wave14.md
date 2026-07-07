# Wave 14 — Retrospective (hindsight review)

**Run:** `wf_bf82df0b-570`, 4 agents. Engine pin `3b5af48` → `fdf0ad9`.
**Lanes:** RESKIN (Opus×2, headline hands lane), UIGRADE U-CLEAN (Opus), W4.3-C2b-ASM (Sonnet).
**Net:** TWO flips shipped (defaults 7→9); one headline lane (RESKIN) implemented, measured, refuted.

Reviewer: retrospective, written after reading Wave 15 (which refuted this wave's central hands
conclusion). Read-only.

---

## What actually happened

| Lane | Verdict | Held up in Wave 15? |
|---|---|---|
| RESKIN R1 (feasibility) | FEASIBLE, +2 premise fixes | Data survived; conclusion mis-scoped |
| RESKIN R2 (fix) | REFUTED by own wext gate | **The refutation's gate was later invalidated; the "class CLOSED / fix out-of-scope for any bake" conclusion was WRONG** |
| UIGRADE U-CLEAN | FLIPPED default-ON | YES — SETLISTS red-band re-checked 0% both arms in W15 |
| W4.3-C2b-ASM | FLIPPED default-ON | YES — album_frame01 assembly held; revealed "(null)" gamertag → W15 fix |

Two of three lanes were clean, durable wins. The headline lane is where the wave went wrong.

## The two flips that held (credit where due)

- **UIGRADE U-CLEAN** corrected the red-band root cause for the *third* time across the campaign:
  it was not the `ClearDepthForOverlay` else-branch (the minimal flush-only shim still showed the
  band) but `FlushPostProcMidFrame`'s own depth-clear-on-resume revealing a z-occluded SETLISTS
  selection quad. Fix = menuBoundary-gated `LoadOp::Load` on the menu flush re-open; gameplay keeps
  `LoadOp::Clear` byte-identical. This is a genuinely good diagnosis-under-fire and it shipped.
- **W4.3-C2b-ASM** correctly identified `album_frame01.mesh` as a group *draw*-member but NOT a
  trans-child (refuting the earlier C2b4 claim it moved), and moved the whole assembly in each
  node's own frame. Held; surfaced the "(null)" gamertag for Wave 15.

## Where Wave 14 was wrong — the hands lane

Wave 14's RESKIN R2 concluded, verbatim: *"Vert/offset-bake class now CLOSED with 7 measured
artifacts… The genuine fix is asset/skeleton-side… out of scope for any bake."* Wave 15's
adjudication (Fable, synthesis-only) refuted this on three counts:

1. **The option set was NOT exhausted.** Wave 15 named a **never-measured cell**: keep each mesh's
   OWN authored offsets and *repoint* appendages to `own` via `SetBone(b, own, /*calcOffset*/false)`
   with **no rebake** — rb3-only, in `RebindHeadHandsAtRest`, the exact pattern the torso rebind
   already ships. Wave 14 declared the fix "asset/skeleton-side, out of scope for any bake"; the
   real candidate is a one-file rb3 repoint. Wave 14 mis-scoped both the *completeness* of the
   option table and the *location* of the fix.

2. **The gate that "decisively" refuted R2 was invalid.** R2 was refuted by the `wext` extent probe
   (flag-OFF 74.8u → flag-ON 87.7u, "decisive; three arms agree"). Wave 15: *"wext>60 is NOT a
   shard oracle for hands_naked — legitimate two-hands-apart extents at raised poses reach
   60–104u."* The decisive quantitative gate this wave (and the Wave 14 review) leaned on was never
   validated against ground truth and was measuring legitimate pose extent, not shard magnitude.

3. **A premise Wave 14 inherited was confounded.** The "6th dead cell" (W2.8g shared-B bake) death
   certificate, cited as part of "offset-bake class exhausted," was shown in Wave 15 to be
   **confounded**: the shared-B anchor is correct only for the *male* hands_naked and forced 28.9°
   (female) / 60–69° (gloves) / ~170° (nails) errors — an aggregate regression dominated by the
   meshes it wrongly re-anchored, not by the composition. The male palette alone reads Tier-1 3.1°.
   Wave 14 built its exhaustion argument partly on this confounded certificate.

R2's *mechanistic* claim — a vertex re-pose is provably invariant to the `inv(R)·L(t)`
animation-basis factor and merely amplifies far-vert radius — **is correct** and survives Wave 15's
option table (RESKIN stays a dead cell). So R2's disposition (do-not-flip) was right; but it was
right by luck of a correct side-argument, while the *primary* justification it published (the wext
regression + "class closed / fix out-of-scope for any bake") was wrong.

## The measurement that would have short-circuited the whole lane

Wave 15's numeric closure — **`angle(B·inv(R)) = 87.2°` for both L/R middlefinger03**, identifying
the default rebake's anchor (transient SetDeformation seed R) as 87.2° off both the authored bind B
and the pose the bones actually hold during play (≈B) — was computed **offline, from
already-committed evidence, with zero runtime** (`HANDS-ADJUDICATION/evidence/offset_basis_derivation.py`).

That data existed **before Wave 14 ran RESKIN.** The 87.2° per-bone relative rotation, the fact that
male authored offsets match `bound`'s basis exactly (0.1°), and that `own` sits at that same basis
during play (3.1°) — all derivable from the RESKIN R1 `[RESKIN_OFF]` captures + the SKEL APD_DIAG
log. A single run of a matrix-relative rotation analysis would have (a) named 87.2° as the operative
error, (b) pointed straight at "stop rebaking, keep authored offsets, repoint to own," and (c) shown
a vert re-pose cannot touch the `inv(R)·L(t)` factor — refuting RESKIN R2 *on paper, before writing
a line of `NativeReskinHandsAtRest`.*

## Foresight fairness — the review gate endorsed both false premises

The Wave 14 review (Fable, `WAVE14_REVIEW.md`) was the intended safety net and it *reinforced* the
two errors:
- It reasoned the vert re-pose "would make palette and geometry reference the SAME basis … what
  remains is ordinary LBS interpolation error" (WAVE14_REVIEW.md:60-68). That is exactly the
  mechanism Wave 15 disproved — the rebake anchors to seed R, not authored B, so the basis is *not*
  shared. The scalar-vs-matrix slip (treating ±6–35° scalar angle-to-identity gaps as the error,
  not the true 87.2° relative rotation) fooled the reviewer too.
- It mandated the wext gate as the arbiter (*"the gate must be the pre-registered quantitative
  target wext 95-106u → ≤60u"*, :68) without questioning whether wext is a valid shard oracle.

So the per-wave review process did not catch this; a **dedicated full-saga adjudication lane** (W15
Fable, synthesis-only, matrix math on committed data) did. The process lesson: run the adjudication
*before* the expensive implement→measure→refute round-trip, not after the 7th dead artifact.

## Recurring bug families

- **Hands (bind-basis / animation-basis shard):** the 7th measured dead artifact landed here
  (`RB3_HANDS_RESKIN`, default-OFF, do-not-flip). Wave 14 believed it had *closed* the class; Wave
  15 reopened it with a proof-level derivation and a named minimal fix → Wave 16. State after Wave
  14: still unfixed, and (transiently) mis-declared closed.
- **UI text / grade-exempt UI (red band):** UIGRADE U-CLEAN closed the hub/song_select red-band on
  the correct (3rd) root cause; held. The *focused-text* polarity residual (white-on-yellow) was
  deferred and turned out (W15 ROWFIX) to be a real engine gap (glyph shader ignores font-material
  color).
- **Layout (album art):** W4.3-C2b-ASM closed the album-art assembly move; held.

## Tooling gaps (what was missing, what it would have answered, what the campaign did instead)

1. **Matrix-relative per-bone rotation tool** — given two bone bases (authored bind B, seed rest R,
   live pose L), emit `angle(B·inv(R))` per bone from a log capture. Would have answered "what is
   the operative hands error and where does it live?" in ONE offline run — naming 87.2° and the
   repoint fix. Instead: an Opus×2 RESKIN lane implemented a full vertex re-pose
   (`NativeReskinHandsAtRest` + wiring + gender handling), measured it across 3 arms, and refuted it
   — an entire fix-implementation lane spent to add one cell to a table that a script completed
   analytically the next wave. The tool literally existed as `offset_basis_derivation.py` one wave
   later, run on data already committed before Wave 14.

2. **A validated shard oracle** — the campaign used `wext>60` as a decisive quantitative gate across
   multiple waves without ever validating it against ground truth. It was measuring legitimate
   two-hands-apart pose extent, not shard magnitude. The named ground-truth instrument
   (Dolphin + milo-trace single-bone `WorldXfm` capture, diffed against native `own`) never existed
   in the campaign. Cost: the wext gate produced a *confident wrong refutation* (R2) and, upstream,
   a *confounded death certificate* (W2.8g). One validated oracle would have prevented both.

3. **Gender-split-by-default instrumentation** — Wave 15 called gender-split "the single biggest
   instrument lesson of this saga" (male nb=38 vs female nb=40). The confounded W2.8g certificate
   Wave 14 inherited came from an *aggregate* measurement that a mandatory gender split would have
   decomposed on the spot (male 3.1° coherent vs female 28.9°). Instead the aggregate misread cost
   two extra waves of hands reasoning.

4. **A "scalars don't subtract" review lint** — several premise inversions across the saga trace to
   comparing angle-vs-identity scalars (±6–35°) instead of the true relative rotation matrix
   (87.2°). A cheap review rule ("bone-basis error claims must be matrix-relative, not scalar")
   would have flagged both the coordinator's and Fable's mechanism reasoning in Wave 14.

## Wasted-effort estimate: MODERATE-to-LARGE

The RESKIN lane was Opus×2. R1's data (female-authored offsets exist; hands meshes distinct +
self-owned; `ExportWorldXfm` is `exo_`-only) was genuinely useful and fed the eventual fix — not
wasted. R2's *conclusion* that a vert re-pose is dead is one real cell in the option table — mildly
useful. But the **implement→build→measure→refute round-trip on `NativeReskinHandsAtRest`** was
avoidable: its refutation was derivable offline from committed evidence with zero code (Wave 15
proved this). Charge one full 2-agent lane + the coordinator's pre-registered gate-building as
recoverable. Compounding it: the wave *closed the wrong door* ("fix out-of-scope for any bake"),
which — absent the Wave 15 adjudication — would have steered the campaign toward an asset/engine
rewrite instead of a one-file rb3 repoint.

## One-line verdict

Two durable flips and a mechanically-correct dead-end, wrapped in a hands conclusion that was wrong
in *scope* (class not exhausted), wrong in *location* (fix is rb3 repoint, not asset/engine), and
justified by an *invalid oracle* (wext) — all three of which a single offline matrix-relative
rotation run on already-committed data would have pre-empted before the headline lane was built.
