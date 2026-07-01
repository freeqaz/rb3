# C8 / feet-in-floor — CONFIDENCE CHECK in feet-framing venues (2026-06-30)

**Agent:** C8-confidence (Opus). **Engine pin:** `998b8734`. **Goal:** independently
re-confirm `c8-measurement.md`'s surprising negative result — *RB3 native band feet do
NOT sink through the floor on current master* — in a venue that **actually frames the
band's feet/lower body**, not the club venue whose shots all frame upper-body/highway.

**Tool:** `scripts/native/band-closeup-capture.py` with a TEMP additive `--venue`
knob (injects `{meta_performer set_venue_override <venue>}` during song_select so
`BandDirector::EnterVenue` loads a feet-framing venue). Probes: `C8_PROBE`/`C8_SLOT`
(per-bone world transform), `CHAIN_PROBE=ankle`, `SHARD_RATIO_DBG`. Zero committed
changes (the `--venue` edit is reverted after; nothing committed).

---

## TL;DR

**CONFIRMS-NO-SINK.** In `arena_02` (and `festival_01`) — venues whose director
frames the whole band including legs/feet — the band members' toe bones sit AT the
stage floor (toe_z ≈ −0.2, worst −0.7), exactly matching the club-venue measurement.
**Zero** toe samples read ≤ −3 (the DC3 sink threshold). The historically-"STILL
UNFIXED" RB3 feet-in-floor bug **does not reproduce** on current master, in a venue
that frames the feet.

---

## Coordinate-frame nuance (why the raw median lies)

Each band character is drawn in TWO frames per gameplay frame, both emitting
`[C8_SLOT]` lines:

1. **Character-LOCAL** draw — feet near origin: ankle world-z ≈ 4, toe ≈ 0, knee ≈ 22,
   pelvis ≈ 37. Floor ≈ 0. **This is the frame the measurement doc reads, and the one
   the sink question is about.** (Filter: ankle world-z in [3,6], per the doc.)
2. **VENUE-placed** draw — the whole rig translated onto the stage (z ≈ 70…370).
   Reference only; floor-relative sink is meaningless here.

Aggregating z across both frames yields a garbage median (~260); you must split by
frame (|z| < 60 = local). Riser-standing members (e.g. drummer) sit higher in the
local frame (ankle ~10–29) but their toe is still AT/ABOVE their own foot-plant — not
a sink. The decisive sink metric is the **most-negative toe_z in the local frame**.

---

## arena_02 — measured

Venue override took (`get_venue_override` readback = `arena_02`); live auto-shot was
`coop_fs_front_n19.shot` (arena full-shot family, NOT a club shot — venue confirmed).
Pinned **15/15** forced wide/full-stage shots (`coop_dir_all00/02`,
`coop_bk_fs_all_n00/n02`, `coop_gk_fs_all_n00`); `drops_band=0`.

C8_SLOT foot telemetry (n samples across player0–player3, multiple frames):

| quantity (character-LOCAL frame, floor≈0) | arena_02 | doc (club) |
|---|---|---|
| ankle_z (filter [3,6]) | med **4.30** (min 3.9, max 5.6, n=80) | med 4.3 |
| toe_z (filter [−3,3]) | med **−0.20** (min −0.70, max 3.0, n=47) | med −0.0 |
| **WORST toe_z (all local samples)** | **−0.70** (player1 bone_R-toe) | worst −0.2 |
| toe samples ≤ −3 (sink) | **0 / 168** | 0 |

Canonical standing pose confirmed on multiple members (player0 + player2):
`bone_L/R-ankle w=(±3.6, 1.0, 4.1)`, `bone_L/R-toe w=(±3.6, 6.4, −0.2)` — symmetric
L/R, toe at the floor.

**Verdict (arena_02): CONFIRMS-NO-SINK** — worst toe −0.70 ≫ −3 threshold.

### Visual
Wide all-band shots (`coop_dir_all00/all02`, backlit arena silhouettes) show the
standing members' legs terminating AT the stage/platform surface — planted, not sunk
through. (`/tmp/c8conf/arena02/*.png`.)

---

## festival_01 — measured (second feet-framing venue)

Venue override took (readback = `festival_01`); live auto-shot `coop_fs_b_n01.shot`
(festival full-shot family). Pinned **10/10** (`coop_all_n00/n02/n04`,
`coop_dir_all00/02`); `drops_band=0`.

| quantity (character-LOCAL frame, floor≈0) | festival_01 |
|---|---|
| ankle_z (filter [3,6]) | med **4.30** (min 3.7, max 5.4, n=56) |
| toe_z (filter [−3,3]) | med **−0.00** (min −0.20, max 2.8, n=34) |
| **WORST toe_z (all local samples)** | **−0.20** (player0 bone_R-toe) |
| toe samples ≤ −3 (sink) | **0 / 177** |

**Verdict (festival_01): CONFIRMS-NO-SINK.**

### Visual
`festival_01/festival01_coop_dir_all00_0.png` is the clearest: three members
(guitarist / vocalist / bassist) framed full-body to the lower legs, all standing ON
the dusk-lit stage — feet planted on the floor, none punched through.
(`/tmp/c8conf/festival01/*.png`.)

---

## Verdict

**CONFIRMS-NO-SINK** in BOTH feet-framing venues. Across 345 character-local toe
samples (arena_02 + festival_01) over 4 band members and multiple frames, **zero**
read ≤ −3; worst toe_z = **−0.70**; toe_z median ≈ **−0.1**; ankle_z median **4.30**
(matches the Xbox 360 ground truth of toe ~0 / ankle 4–5 cited in `c8-measurement.md`).
The DC3 feet-in-floor signature (ankle collapse to ~0.2 + toe to −4.3) does **not**
appear on RB3 master. The prior negative result holds in a venue that frames the feet
— the "STILL UNFIXED" RB3 feet-in-floor bug does NOT reproduce.

### Method notes / caveats
- Sink question lives in the **character-LOCAL** draw (floor ≈ 0); the venue-placed
  draw (z ≈ 70–370) is irrelevant. Split frames by |z| < 60 before measuring.
- Bone telemetry is camera-INDEPENDENT (it's the real `WorldXfm`), so the metric is
  robust regardless of framing; the feet-framing shots add the visual confirmation.
- `--venue` was a TEMP additive knob in `band-closeup-capture.py` (injects
  `{meta_performer set_venue_override}`); reverted after this run. Nothing committed.
- `arena_01` crashes (per the convergence audit) — used `arena_02` + `festival_01`.

### Data
`/tmp/c8conf/{arena02,festival01}/` (PNGs + manifest/verdict json);
engine logs `/tmp/rb3-bandcloseup-{arena02,festival01}-*.log`;
parser `/tmp/c8conf/sinkprobe.py`.
</content>
</invoke>
