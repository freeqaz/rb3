# Triage 2026-07-13 — LANE CROWD-BLOB

**User report (web):** "all of the crowd was still stuck together in a blob."
**Lane:** ground truth + attribution (READ-ONLY, no fixes).
**Binary:** `rb3-native` @ HEAD `deea5e95` (built 00:49, base `d699d837+`). Port 8901.
**Evidence dir:** `/tmp/triage-0713/crowd/`

---

## TL;DR

1. **The audience crowd is NOT a positional blob.** Live gameplay posdump: 268 crowd
   members, **0/268 at origin**, spread over x=[-161,161] y=[-297,-22] z=[68,74].
   Band roots also spread (**0/4 at origin**), distinct spots. A positional
   origin-collapse is **REFUTED** by engine-state data (GPU-independent).
2. **The most likely thing the user is seeing is the BAND, not the audience** —
   exactly as the 2026-07-02 "crowd at center" report turned out to be. On current
   HEAD all four band members **fail their stance body-clip lookup**
   (`could not find group stand/sit in body_clips`, `no clip w. flags IR`), so no
   valid standing/sitting pose is applied and they collapse into the grossly-wrong
   full-body poses documented as **W31 V1** (bassist folded over the bench, vocalist
   bent backward). This is a live regression owned by the concurrent **W33 Lane 2
   (V1-POSE)** — my finding hands them the concrete mechanism (see §Q4).
3. **Secondary contributor for the *audience* specifically:** the V24 `SHARD_GUARD`
   skin-ratio guard drops **6798** over-deformed crowd sub-meshes, so the rendered
   audience is sparse/thinned (survivors only). This is the known skinning residual
   **closed W23-29 as a measurement artifact — NOT reopened here** (per lane
   constraint). It is a *skinning* issue, not placement.
4. **VISUAL grading (pixels) is BLOCKED by a host environment fault**, not the game:
   the host NVIDIA driver/library is version-mismatched (`nvidia-smi`: "Driver/library
   version mismatch", NVML 610.43; Vulkan `VK_ERROR_INCOMPATIBLE_DRIVER`, no drivers).
   Dawn falls back to the **Null backend** → every `/api/screenshot` is a blank white
   frame (53+ captures byte-identical). The same tooling on this host previously got a
   real RTX 3090 (older log). **Host reboot required** to restore pixel capture.
   Therefore the 2026-07-02 visual checklist (shards / dark chars / grey skin) could
   **not** be re-graded this pass — see §Checklist.

---

## Method / boots used (4 of 6 budget; remaining 2 unspent — blank pixels have no value under Null GPU)

| Boot | Script | Purpose | Result |
|---|---|---|---|
| 1 | `crowd-origin-posdump.py --port 8901` | audience/band positional verdict (engine state) | **SPREAD**, verdict REFUTES H1/H2/H4 |
| 2 | `boot-to-song.py --port 8901 --hold 32 --interval 2` | wide gameplay timeline 2.5–36.6s | 16/16 shots — **all blank white (Null GPU)** |
| 3 | `crowd-shot-capture.py --port 8901` | forced crowd cameras (audience) | 6 shots — **all blank white** |
| 4 | `band-closeup-capture.py --port 8901 --member all` | per-member band pose closeups | gate PASS 34/34, `drops_band=0`; **all 34 frames blank** |

Pixel capture is impossible until the host GPU driver is fixed; boots 2–4 still yielded
useful *engine-state* signals (drop counts, body-clip misses) from their logs.

---

## Answers to the tasked questions

### Q1 — Are AUDIENCE crowd members positionally piled/blobbed?
**No.** `crowd-origin-posdump` on live gameplay (songMs≈2095, +3s settle):
```
crowd=268  at_origin=0/268  x=[-161.0,161.0] y=[-297.6,-22.1] z=[68.6,74.5]
```
Verdict: *"REFUTES H1/H2/H4 — skinning-guard drop, not placement-at-origin."* The
audience is spread across the venue. Forced-crowd-camera visual confirmation was
attempted (boot 3) but blocked by Null GPU. Evidence:
`/tmp/triage-0713/crowd/posdump_verdict.txt`, `posdump_band_roots.txt`, raw log
`/tmp/rb3-posdump-8901.log`.

### Q2 — Are BAND members piled/knotted? Transient or persistent?
**Not positionally piled** — band roots are spread & distinct at settle:
```
player0 root=(68.1,54.8,13.2)  player1 root=(-72.3,81.0,13.5)
player2 root=(-10.3,31.4,13.2) player3 root=(14.4,146.1,13.2)   (0/4 at origin)
```
inst-kit spheres ride near each root (no gross root/kit separation). **BUT the band
POSE is broken and persistent** (not just count-in): all four members fail stance
clip resolution the whole session (see Q4). So the band's problem is *pose*, not
*position* — and it is persistent, not a transient walk-on knot.

### Q3 — Is the 67e87ae1 walk-on snap still present and firing?
**Present, default-ON.** `src/system/bandobj/BandCharacter.cpp:4134` (guard at :58-64,
`RB3_WALKON_SNAP_OFF` opt-out) is intact; last touch of that file is unrelated
(`6ccc36e3`). Runtime is consistent with it working: band roots are spread and
distinct at ~5s (no member frozen at a shared/origin spot), so the walk-on knot the
snap targets is resolved. Direct "is any member frozen" visual check is blocked by
Null GPU, but the positional data shows no frozen-at-origin member. **Walk-on snap is
NOT the cause of the current band-pose breakage** (that is a clip-resolution failure,
below — a different layer).

### Q4 — Is the band breakage the SAME as W31 V1, or a distinct pile-up?
**Same phenomenon as W31 V1** (grossly wrong full-body poses), and this pass adds the
**mechanism**. On current HEAD every band player fails to resolve its stance body clip:
```
player0 could not find group stand in body_clips (char/main/main.milo)
player0 no clip w. flags IR|0x100000 in body_clips/stand
player1 could not find group stand in body_clips
player2 could not find group stand in body_clips ; no clip w. flags IR in body_clips/stand
player3 could not find group sit   in body_clips ; no clip w. flags IR in body_clips/sit
```
(guitar/bass/keys request `stand`, drummer requests `sit`.) The stance-group lookup
returns nothing, so members get no valid base stance and fall to a residual/default
pose → the non-anatomical collapse in the W31 evidence
(`.../W31-VISUAL-PASS/evidence/bath_bassist_collapsed_bench.png`, viewed this pass —
bassist folded over bench left, radiating limbs right). This is **distinct from a
positional pile-up** and distinct from the audience shard-cull. Evidence:
`/tmp/triage-0713/crowd/band_bodyclip_group_miss.txt`.

**Attribution vs concurrent lane:** this is the surface **W33 Lane 2 (W33-V1-POSE)**
already owns. Prime suspect commit **`a3916764` W31-SET-PLAY-DISPATCH** (band began
playing performance/body clips in-song for the first time — the exact regression
window W30→now). My contribution to that lane: the failing layer is **body-clip GROUP
resolution** (`stand`/`sit` groups + `IR`-flag clip filter returning empty in
`char/main/main.milo`'s `body_clips`), i.e. either the requested group names / flag
mask are wrong for the native asset, or `body_clips` is missing those groups on native.
`band-closeup` `drops_band=0` proves the meshes are present and NOT shard-culled — so
it is purely a pose/clip fault, not a mesh fault. (FAMILY-STOP note: nothing here
points at the SKEL rotation-basis family — signature is *missing clip*, not
axis-swapped limbs — so no basis-family reopen is implied.)

### Q5 — Suspect-commit window (since 2026-07-02, band/char/crowd anim surfaces)
Ranked by relevance to a band-pose regression "new since W30":

| Commit | What | A/B lever |
|---|---|---|
| **`a3916764`** W31-SET-PLAY-DISPATCH | band plays body/performance clips in-song (SyncProperty arg-order → 100%). **Prime V1 suspect.** | Decomp correctness fix — **no clean opt-out env** (W33 Lane 2 must find the committed gate; do not guess). |
| `3ed6118a` W32-PROP-FAN | starved instrument-MIDI prop drivers | `RB3_NO_MIDIDRV_ENTER_FIX=1` (proven NOT V1 — W31 run 4) |
| `1261bb78`/`b49edcbd` | `RB3_PROP_POSE_FULL` flipped default-ON | `RB3_PROP_POSE_FULL=0` |
| `67e87ae1` (pre-window, 07-02) | walk-on snap | `RB3_WALKON_SNAP_OFF=1` (present, not the cause) |
| engine pin bumps `6ccc36e3`→`24c4f95`, `69103c77`→`2ea8e34` | exit-trap + web policy family | n/a |

Flag A/Bs were **not run** this pass: they only change *pixels*, and pixels are blank
under Null GPU, so an A/B would be unobservable. Defer flag-bisect to W33 Lane 2 once
the host GPU is restored (or run it there, which already has the pixels).

---

## 2026-07-02 checklist re-grade — BLOCKED (Null GPU), best inference from engine state

Pixel grading is impossible this pass. Engine-state inferences only:

| Item | 2026-07-02 status | This pass |
|---|---|---|
| (a) BandPatchMesh pale needle/spike shards on hands/arms/faces | FIXED (`f0a95910` revert) | **NOT RE-GRADEABLE (no pixels).** `band-closeup drops_band=0` — no band meshes culled, but shard geometry is a *render* artifact invisible to the drop metric (documented gate blind spot). Needs pixels. |
| (b) dark/black characters | FIXED (compose math `153beaf`) | **NOT RE-GRADEABLE (no pixels).** |
| (c) grey untextured skin | FIXED web `266ffb1b` | **NOT RE-GRADEABLE (no pixels).** |

These must be re-verified once the host GPU driver is fixed (reboot).

---

## What blocks a complete answer / handoff

- **Host GPU driver mismatch (BLOCKER):** reboot the host to reload the NVIDIA kernel
  module (NVML 610.43 mismatch). Until then all native/web GPU renders are Null-backend
  blank. Evidence: `/tmp/triage-0713/crowd/gpu_null_backend_proof.txt`.
- After reboot, re-run boots 2–4 for the visual confirmation this pass could not get,
  and re-grade the (a)/(b)/(c) checklist.
- **Band-pose (V1) is owned by W33 Lane 2** — do not duplicate the fix. Hand them the
  `body_clips` group-miss mechanism (§Q4) as the concrete failing layer.

## Evidence index (`/tmp/triage-0713/crowd/`)
- `posdump_verdict.txt`, `posdump_band_roots.txt` — audience/band SPREAD verdict + roots
- `band_bodyclip_group_miss.txt` — the 4-player stance-clip resolution failures
- `gpu_null_backend_proof.txt` — host driver mismatch → Dawn Null backend
- `frames_all_blank.txt` — 53+ captures byte-identical blank white
- `gameplay/`, `crowdcam/`, `bandcloseup/` — blank PNGs (Null GPU) + engine logs
- Reference (real pixels, 1-day-old, same-ish code):
  `docs/native/engine-arch-review-2026-07-05/execution/W31-VISUAL-PASS/evidence/bath_bassist_collapsed_bench.png`
