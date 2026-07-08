# R5 D5 — articulated Wii capture attempt → **BOX EXHAUSTED (priced)**

Lane D5 (Wave 18, KEY=R5-HANDS-ENDGAME, task #102). Executes the named upgrade the
Wave-17 VERDICT §4 pre-registered: an **articulated Wii capture with a live clip+frame
join key** to discriminate GT-A (native animates Wii-faithfully) from GT-B/GT-C.

**Verdict: the discriminator-grade articulated Wii capture was NOT obtained within the
hard box.** This is the VERDICT §4 pre-registered box-exhaustion outcome → **GT-D
closure is pre-authorized; this lane REPORTS the priced exhaustion, the coordinator
executes closure** (per the charter, D5 does not declare closure itself).

## What was proven to work (the machinery is sound)

| Stage | Result | Evidence |
|---|---|---|
| D2 patched-disc boot (Bank-8 debug DOL / retail apploader) | **GO** ~36 s; Bank-8 map valid (992 `__vt__8CharBone`, `TheBandDirector` live) | `D5_clean_nav.log`, reproduces D2/D4 |
| **Pipe input injection** (emulated Wii Remote → FIFO `Pipes/rb3ctl`) | **WORKS** — taps demonstrably change shell state on 3 clean boots: `START`→CharDrivers 12→18; `A`→resident playerN clips 0→8 | `D5_nav_aggressive.{log,json}`, `D5_clean_nav.log` |
| Oracle-independent articulation detector (per-finger relRot span over a dwell) | Built + fires red on the frozen state | this doc |

The Dolphin "Pipe" ControllerInterface path (cribbed from the concurrent r1b lane's
`WiimoteNew.ini` + `docs/native/c8-ground-truth-2026-07-01/t2-dolphin-oracle.md`) drives
the map-valid Bank-8 boot. The reusable instrument landed in **`milo-trace
tools/wii_bone_dirboot.py`** (`setup_input` / `Pad` / `active_player_drivers` /
`wait_game_init` / `cmd_nav`; new `nav` subcommand).

## What blocked the capture (the box wall)

**No blind-navigable shell state animates the band hands.** Across ~60 taps over 3 clean
cold boots (one aggressive 50-tap schedule + two precise runs of the documented short
sequence `+` → `DOWN` → `A`·N), the hand chain stayed **byte-identical to the D2/D4
settled pose**:

| pair | Wii |relRot| this capture | D2/D4 stored | span over 11 s / 20 s dwell |
|---|---:|---:|---:|
| hand→middlefinger01 | 5.40° | 5.40° | 0.00° |
| middlefinger01→02 | 24.37° | 24.37° | 0.00° |
| middlefinger02→03 | 17.73° | 17.73° | 0.00° |
| hand→ringfinger01 | 7.43° | 7.43° | 0.00° |
| ringfinger01→02 | 26.13° | 26.13° | 0.00° |
| ringfinger02→03 | 5.74° | 5.74° | 0.00° |

The best state reached (18 CharDrivers / 8 resident playerN clips, **0 active**) is
**identical to D4's frozen `ui/overshell` census** — the band is *pre-loaded* behind an
un-cleared guest-profile / "Use Guest? YES" / nav-help-CONTINUE prompt flow, not a
playing `main_hub` vignette. Blind (Null video), those prompts could not be cleared
reliably, so `main_hub` was never cleanly entered. The `active_player_clip_drivers = 0`
census reproduces D4's red baseline at every reachable state — but the exhaustion does
**not** rest on that (possibly-blind) oracle: the **oracle-independent bone-motion test
(0° finger span)** is decisive that the hands genuinely do not move.

This is exactly G-D5-1's mandated **fail-red**: "the committed `D4_wii_drivers.json`
(0 active drivers) IS the red baseline — the harness must FAIL loudly on the D2 frozen
state and pass only with live drivers." The instrument behaves as designed; the required
world-state (a live hand-animating vignette) is simply not blind-reachable.

## Gate status

- **G-D5-1 capture validity — FAIL (correct red arm).** No live `playerN` driver;
  0 matched frames; mr-pair swing 0° (need ≥15°). Harness fails loud on the static state.
- **G-D5-2 / G-D5-3 — N/A.** No captures to derive ε_noise or re-run the join/red-team.
- **G-D5-4 decision mechanicalness — GT-D by the pre-registered fallback row**
  ("D5 box exhausted, no articulated capture → CLOSURE §3.4"). No D5 JSON to classify.
- **G5′ palette separation — out of scope** for this lane (a GT-A-only forensic stretch).

## Priced paths forward (for the coordinator)

1. **Cheap blind pipe-nav — EXHAUSTED.** This is the path the VERDICT §3 assumed cheap
   ("the ONLY missing piece is a Wii capture while a hand-animating clip driver is
   live"). The pipe-nav instrument is built and validated; it reaches only the overshell
   family, where the band is frozen in bind/settled pose. **Empirical correction to the
   VERDICT's cost prior: the missing capture is *not* cheap — the live vignette is behind
   a menu flow that is not blind-navigable.**
2. **Expensive visually-guided gameplay nav — FEASIBLE on this box, exceeds the D5 box.**
   `Xvfb` + `xdotool` + GPU render nodes are all present (per `t2-dolphin-oracle.md`,
   this is how the C8 oracle captured real gameplay). It needs: a GPU video backend under
   Xvfb, a Wii-Remote **guitar extension** mapping (to START a song), F9 screenshot
   feedback, and a deliberate ~8-screen gameplay sequence. **Gameplay is the reliable
   finger-articulation source.** This is a dedicated lane-wave build-out; it would still
   be the Bank-8 map-valid boot (this lane's unique value over r1b's retail boot).
   *Overlap:* the concurrent **r1b** lane runs retail blind (`video_backend=Null`) + GDB
   stub, bare Wiimote — it hits the SAME wall and has not produced an articulated capture.
3. **`main_hub` vignette — unconfirmed** it even finger-articulates on the debug boot
   (may be idle body sway only); gameplay remains the surest source.

## Reproduction

```bash
cd /home/free/code/milohax/milo-trace
tools/wii_bone_dirboot.py boot-check                    # GO: Bank-8 map live
tools/wii_bone_dirboot.py drivers  --out /tmp/drv.json  # 0 active = D4 red baseline
tools/wii_bone_dirboot.py nav      --out /tmp/nav.json  # pipe-input nav; exhausts w/o live drivers
# oracle-independent check: two `bones` dumps ~1.5 s apart → 0° finger relRot span (STATIC)
```

Instance-scoped by `-u /tmp/dolphin-d2`; pgid-only teardown; the concurrent r1b instance
(`/tmp/r1b-user`) was never touched. Disc dir `/home/free/tmp/wave17-d2/disc` (out of
repo; provenance in `D2_boot_apploader_patch.md`). No default flips, no pin bumps, no
engine edits.

## Process-lint compliance (OPTIONS §4)

- **Lint 3 (no unvalidated oracle as a gate):** the exhaustion is called on the
  oracle-INDEPENDENT bone-motion test (0° finger span), not on the pointer-chase driver
  census (which is explicitly flagged unvalidated on positives). Input-liveness was
  separately verified (a tap changes CharDriver count) so "no articulation" is not
  confounded with "input dead."
- **Lint 7 (evidence committed):** `D5_exhaustion.json` (machine-readable) + nav logs +
  this findings doc under `evidence/`.
- **Lint 10 (tools as pre-dispatch gates):** the articulation detector + fail-red nav
  harness were built and fired on the frozen state before any capture claim.
