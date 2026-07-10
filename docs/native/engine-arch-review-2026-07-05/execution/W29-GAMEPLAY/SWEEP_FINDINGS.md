# W29-GAMEPLAY — Part D: user-directed gameplay bug sweep (findings only)

**Bounds (CA7):** seeds triaged FIRST; **0 new boot-to-song runs** (reused the
coordinator's `/tmp/rb3-boot2song-verify/` seed captures + Part B's 2 vocalist
closeup runs as second angles); NO fix code. Anomaly-class `grep -c` across every
lane run log = **0** for `FAIL-MSG|abort|MILO_NOTIFY|OOB|MILO_WARN` (table in
STATUS.md §Probe/anomaly counts). Findings feed the Wave-30 menu.

Retail pairing note: the available `images/retail-screenshots/` gameplay shots
(`yt_qRagnZCIMzk_gameplay_guitar.png`, `fandom_gameplay_guitar.png`) are all
**highway-cam** — none is a band-member closeup — so neither seed has an exact
retail pair. Where a pair is absent it is stated explicitly (CA7).

---

## SEED 1 — guitarist: no hands posed on the instrument

**Symptom.** `gameplay_003.png` (coordinator seed) shows the guitarist with no hand
on the fretboard/strings. **Second camera angle** (`gameplay_002.png`, same club/pub
venue, same run) confirms it: **both arms hang in a neutral/dormant pose** — neither
hand grips the neck or sits at the strum position.

**Evidence.**
`evidence/sweep/seed1_guitarist_dormant_hands_angle1.png` (=gameplay_003),
`evidence/sweep/seed1_guitarist_dormant_hands_angle2.png` (=gameplay_002).

**Retail comparison.** No exact retail band-closeup pair (highway-cam only). Retail
RB3 guitarists are hand-posed on the instrument via the performance system, so
dormant hands is a native divergence, not faithful framing.

**Suspected layer / disposition.** This is **NOT a new bug** — it is the
**W28-documented default-OFF PROP-hand state**. The seed run had
`RB3_PROP_POSE_FULL` default-OFF, so the `RB3_IK_REACH_CLAMP` neutralizes the
grossly-unreachable static prop-tip IK targets (`mode=skip` → keep the idle clip
pose) and the arms stay at rest. With the flag ON, W28 pieces (1)+(2) already
produce a coherent hand-on-guitar for **strum + fret** (W28 STATUS §Visible
hand-on-instrument; reconfirmed this wave — Part A strum/fret PASS the analyzer).
The root reason the hands need IK at all: **the on-stage band plays only idle +
expression clips, never an instrument-performance clip** (Part A finding — CHARDRV
evidence), so hand placement falls entirely to IK against static prop targets.

**Proposed Wave-30 lane.** Coordinator default-ON decision for `RB3_PROP_POSE_FULL`
(pieces 1+2), gated on E6(b) global-`mFinger` scoping — Part B retires the
vocalist-mic half of that blocker. (Not this lane: flipping a default is
coordinator-only.)

---

## SEED 2 — green/olive faces on band members under venue lighting

**Symptom.** `gameplay_006.png` (coordinator seed): two band members — a bespectacled
member and a long-haired member — have strongly **green/olive faces** under a purple
hexagon venue backdrop. Green is not the purple ambient colour.

**Evidence.** `evidence/sweep/seed2_green_faces_purple_venue.png` (=gameplay_006);
second-angle differential from `gameplay_002.png` and the Part B vocalist closeup
(`evidence/vocalist_closeups/micAB_ON_coop_v_n01.png`).

**Differential observation (second angle).** In the **same run**, the club-venue
guitarist (`gameplay_002`) has a **normal tan face** — so the green is
**member/venue-position-specific, not a global wash**. The club-venue **vocalist**
(Part B closeup) is *also* green-faced. So green faces recur across members but not
uniformly.

**Retail comparison.** **No retail pair** (retail shots are highway-cam; no
band-closeup lighting reference exists in `images/retail-screenshots/`). Cannot
confirm vs faithful RB3 green stage key-lighting without ground truth.

**Suspected layer.** Two live hypotheses, undecided: (a) a **char-env lighting**
mismatch on specific member head models — the C8-faces fixed family
(memory: 4 causes, one = char-env lighting) — where the face light rig diverges from
the body; or (b) **faithful green stage key-light** hitting certain stage positions.
The face-vs-body/face-vs-guitarist differential leans toward (a) but is not decisive.

**Proposed Wave-30 lane.** Obtain a retail band-member closeup lighting reference,
then A/B char-env face lighting vs the venue stage key-light on the green-faced
member models (C8-faces family). Label deferred until a retail pair exists (CA7
discipline — do not call it a bug without ground truth).

---

## Other observations (one-line Wave-30 candidates)

- Log hygiene is clean: **zero** `FAIL-MSG/abort/MILO_NOTIFY/OOB/MILO_WARN` across all
  8+ lane run logs (boot-to-song seed run, 5 prop-probe runs, 4 band-closeup runs).
  No new engine-notify/OOB anomaly to chase this wave.
- `rb3-tests` and the bounded-boot drawlog capture both exit with a **pre-existing
  teardown SIGSEGV** (rc=-11 / core on exit) AFTER all work completes — the known
  exit-trap (W0.3 STATUS S1; memory: web-release exit-trap OPEN). Unrelated to any
  W29 change; Wave-30 candidate to close the native exit-time teardown crash.
