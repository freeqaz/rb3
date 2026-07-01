# C8 / feet-in-floor — fresh measurement on current master (2026-06-30)

**Agent:** C8-measurement (Opus). **Engine pin:** `998b8734` (= engine HEAD; all
runs verified on current master). **Tool:** the NEW deterministic
`scripts/native/band-closeup-capture.py` (pins a `BandCamShot` via
`{rb3_force_shot}`/`{rb3_director_disable}`; this harness did NOT exist during the
prior C8 saga). All env-gated probes (`C8_PROBE`/`C8_SLOT`/`CHAIN_PROBE`/
`CHAIN_COMPOSE`/`SHARD_RATIO_DBG`); ZERO code changes, nothing committed.

---

## TL;DR (the surprising, decisive result)

**On current master, RB3 native's band-member feet MATCH the Xbox ground truth — the
DC3 feet-in-floor collapse does NOT reproduce in RB3.** And the "186u leg-fling" /
"15× boot ratio" from the prior wave-4 doc (`task-ik-mispose-impl.md`) does **not
reproduce at all** in the deterministic harness — for any member, seated or
choreographing. The leg pose is geometrically perfect (det=1, unit-length local
rows, hierarchy composes cleanly with dMag=0.00).

The hard C8 rotation-basis work (`491288ec`) plus the relaxed band-aware V24 guard
(engine pin `1010f5f`) and the four rest-rebinds (`BandCharacter` torso +
`Crowd::RebindCrowdCharBonesToOwnSkeleton` + `RebindInstStringsToRestBasis`) appear
to have **already absorbed the feet/rotation-basis divergence for RB3 band members.**
This is a markedly better state than the RB3 prior docs (and the still-open DC3 bug)
describe.

---

## 1. Feet-in-floor: does it reproduce on RB3 master? → NO

### The decisive number (RB3 player-LOCAL foot, floor ≈ 0)

Measured via `C8_SLOT` (bone world transform `.v`), filtered to the player-LOCAL
foot dumps (ankle world-z in [3,6] = character-local, not venue-placed). n=45 LIVE
samples across drummers + standing members, multiple frames:

| quantity | RB3 native LIVE (master) | RB3 REST (bind) |
|---|---|---|
| ankle_z | med **4.3** (min 3.5, max 5.9) | 4.2 |
| toe_z (floor ≈ 0) | med **−0.0** (min −0.2, p90 3.3) | 1.0 |
| toe − ankle | med −4.3 | −3.2 |

### Cross-reference to the DC3 / Xbox ground truth (now AVAILABLE)

The P0 blocker the RB3 prior docs flagged ("needs Xbox/Xenia ankle-Z ground truth")
**has been captured on the DC3 side** (`dc3-decomp/docs/sessions/2026-06-09-xenia-xbox-foot-truth.md`,
read-only GDB-RSP off a PLAYING Xenia song; VMX128 16-byte-aligned `mWorldXfm.v` at `+0x78`):

| platform | toe_z | ankle_z | pelvis_z | verdict |
|---|---|---|---|---|
| **Xbox 360 (ground truth)** | **0.006 … 0.53** | 4.1 … 5.4 | 36 … 39 | foot planted on floor |
| **DC3 native (BUG)** | **−4.3** | +1.6 … +2.0 | ~36 | ankle collapsed ~3u → toe punches −4 through floor |
| **RB3 native (THIS, master)** | **−0.0** (worst −0.2) | **4.3** | 37.6 | foot planted — matches Xbox |

**RB3's ankle sits ~4.3 above the floor (Xbox: 4–5) and the toe sits AT the floor
(Xbox: ~0).** RB3 native does NOT exhibit the DC3 signature (ankle collapsing to ~0.2
+ toe to −4.3). The whole `(A)` premise — "band-member feet sink ~4u below the floor
during gameplay" — is a DC3 gate (`FeetNotBelowFloorDuringGameplay -4.2`) that on RB3
**does not currently fire**: worst RB3 toe is −0.2, not −4.

### Visual confirmation

Drummer/guitar/bass closeups (`/tmp/c8meas/{drums,gtr,wide}/*.png`) all render clean
band geometry, no shards. As the prior docs noted, this club venue's band shots frame
**upper body / highway** — feet are off-frame in 100% of the deterministic shots
(`coop_d_n0*`, `coop_g_cg`, `coop_g_n0*`, `coop_b_b`). So even if a residual existed
it would be invisible in normal gameplay framing; but the bone telemetry shows there
is no sink to be invisible about.

---

## 2. Rotation-basis divergence (rest vs live) → REFUTED on the leg chain

The prior "NEW LEAD = leg/pelvis bone LOCAL lengths change rest→live" is **REFUTED**
by direct `CHAIN_PROBE`/`CHAIN_COMPOSE` measurement of the full ankle→root local
chain (player0 drummer, frame 16, representative of all members):

```
[CHAIN] 'bone_L-ankle' Lv=(18.49,0,0)|18.5| Ldet=1.000 Lrow=(1.00,1.00,1.00) Wv=(-3.6,1.0,4.1)
[CHAIN] 'bone_L-knee'  Lv=(15.00,0,0)|15.0| Ldet=1.000 Lrow=(1.00,1.00,1.00) Wv=(-3.6,2.1,22.6)
[CHAIN] 'bone_L-thigh' Lv=(0,0,3.61)|3.6|  Ldet=1.000 Lrow=(1.00,1.00,1.00) Wv=(-3.6,1.6,37.6)
[CHAIN] 'bone_pelvis'  Lv=(0,1.63,37.57)|37.6| Ldet=1.000 Lrow=(1.00,1.00,1.00) Wv=(0.0,1.6,37.6)
[CHAIN_COMPOSE] dMag=0.00 on EVERY link   (manualW == cacheW, no stale cache)
```

- **Local bone lengths are CONSTANT and unit-scaled.** `Lrow=(1,1,1)`, `Ldet=1.000`
  on every bone — no scale corruption, no length change rest→live. Lengths: ankle
  18.5, knee 15.0, thigh 3.6, pelvis 37.6. The divergence is **rotation only**, never
  translation/length. (The lower leg over-extending — DC3's candidate — does NOT
  appear on RB3: knee/thigh/pelvis world-Z are byte-identical rest vs live.)
- **No stale-cache / severed-dirty / wrong-parent bug.** `CHAIN_COMPOSE` reports
  `dMag=0.00` (cached `WorldXfm` == `Multiply(local, parentWorld)`) on every link →
  the hierarchy composes correctly. (`CHAIN_PROPTEST` / `CHAIN_FORCE` levers exist for
  deeper probing but were unnecessary — the compose is already exact.)
- **Which bones diverge rest→live, and is it rotation or translation?** Only the
  **ankle and toe** move (pelvis/thigh/knee Δ=0 rest→live). The change is a real foot
  articulation: ankle Δy≈−3.5, toe Δy≈−4.2 in the rotated foot basis — a **rotation**
  (foot pointing), with Z (height) barely moving (ankle 4.2→4.1, toe 1.0→−0.2). This
  is the *normal* down-pointing foot, and crucially it lands the toe AT the floor
  (−0.2), not 4u below.

---

## 3. Cheapest testable hypotheses (env-gated, no commits)

| test | result |
|---|---|
| **IK on vs `RB3_NO_IK=1`** (guitarist, 8 frames @60s) | No material difference: max_band_ratio 2.47 (on) vs 3.31 (off), **zero band drops both**. IK is inert on the band leg pose — consistent with prior "IK-independent" finding and the DC3 "IK near-inert on both platforms" ground truth. |
| **leg-pose corruption with IK off** | None — leg coherent both ways (pelvis_z ~38–52 → toe_z ~0–33, normal ~33u leg in venue-world). |
| **scan all leg bones for world-Z fling >120 / <−60** | **ZERO** across drummer + guitarist runs. The prior 186u fling does not reproduce. |
| **forcing rest-basis on the leg chain** | Not needed — rest-basis ALREADY matches live on the upper leg (Δ=0); the only rest→live delta is the legitimate ankle/toe articulation that correctly plants the toe. No sink to reduce. |

---

## Concrete lead for the implementation agent

**There is no actionable RB3 feet-in-floor / rotation-basis bug to fix on current
master — the metric reads CORRECT (toe ~0, ankle ~4.3, matching Xbox).** The single
highest-value action is a **doc/status correction**: the RB3 char-rebake-scope and
ik-mispose docs (and the [[project_dc3_feet_in_floor_anim]] memory note) state RB3 is
"C8-blocked" / "feet sink ~4u" / "186u leg fling" — but the deterministic harness +
the now-available Xbox ground truth show RB3 band feet are **faithful** (the prior
flings were the transient venue-idle choreography burst + camera-framing artifacts,
not a steady-state pose bug; the thin-geo `_skin.2` drops that remain are the known
~1-in-4 wardrobe-roll *transient* small-bind ratio false-positives, off-frame in 100%
of closeups — already correctly handled by the V24 guard).

### If a residual is still wanted, here is exactly what to capture (it's cheap now)
The one frame/value that would *close* this for RB3 with certainty:
- a **feet-revealing band shot** in a venue whose director frames lower body (this
  club venue does not — try an arena/festival venue's `coop_*` shots, or add a
  temporary down-tilt to `force_shot`), then read the `C8_SLOT` toe_z in player-local
  space across the song. **Predicted result: toe_z ≥ −1 (planted), confirming no
  sink.** If it instead reads ≤ −3 on some venue/move, THAT venue+move is the real
  RB3 repro and the DC3 poll-order fix (`HamDriver.cpp:95-101` re-dirty-after-IK; see
  dc3 `19-feet-ik-wave3.md`) becomes the candidate — but no RB3 frame measured here
  shows it.

### Do NOT re-chase (refuted this session)
- "leg/pelvis local lengths change rest→live" — REFUTED (det=1, lengths constant).
- "186u leg fling / 15× boot ratio" — does not reproduce in the harness.
- stale-cache / severed-dirty-prop / wrong-parent — REFUTED (`CHAIN_COMPOSE` dMag=0).
- IK handedness / IK mispose — IK is inert (RB3_NO_IK A/B identical PASS).

---

## Repro

```bash
cmake --build native/build-native --target rb3-native -j16   # pin 998b8734
# feet geometry (player-local toe/ankle/knee/thigh/pelvis world.v):
C8_PROBE="skin.2,resource.mesh" C8_EVERY=25 CHAIN_PROBE="ankle" CHAIN_COMPOSE=1 \
  SHARD_RATIO_DBG=1 python3 scripts/native/band-closeup-capture.py \
  --member drums --frames 6 --frame-dt 500 --out /tmp/c8/drums --tag drums
# grep '[C8_SLOT].*ankle|toe' and '[CHAIN]'/'[CHAIN_COMPOSE]' in the engine log.
# IK A/B:  add RB3_NO_IK=1 to the second pass; compare max_band_ratio + drops_band.
```

Data this report is built on: `/tmp/rb3-bandcloseup-{probe,chain2,gtr,noik}-*.log`,
`/tmp/c8meas/*/`. DC3 ground truth:
`/home/free/code/milohax/dc3-decomp/docs/sessions/2026-06-09-xenia-xbox-foot-truth.md`
+ `docs/investigations/2026-06-10-roadmap-to-100/19-feet-ik-wave3.md`.
