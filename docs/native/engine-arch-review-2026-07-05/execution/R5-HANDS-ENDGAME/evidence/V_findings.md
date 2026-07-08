# Wave-18 Lane V (VISCAP) — visually-guided articulated Wii capture → **BOX EXHAUSTED (priced)**

Lane V (Wave 18, KEY=VISCAP). Executes the VERDICT §4 D5 follow-up #2 the kickoff
funded: the **visually-guided gameplay nav** path D5 priced FEASIBLE-but-unbuilt
(`D5_findings.md` "expensive_visual_gameplay_nav"). 

**Verdict: NO articulated Wii capture obtained. NO BRANCH LETTER ASSIGNABLE.** This is
the A1 exhaustion outcome (kickoff, BINDING) → coordinator executes **GT-D CLOSURE**
§3.4 (pre-authorized VERDICT §4 + kickoff A1). No re-pricing loop; no further
discriminator lanes.

But this is a **strong upgrade over D5**: the nav wall D5 hit is SOLVED, and the real
wall is now proven one level deeper and fully characterized.

## What was BUILT and WORKS (the capability D5 proved missing)

The visually-guided nav rig — all present on this box and verified:
`Xvfb :91` + `Dolphin -p x11 -v Vulkan` (dual RTX 3090) + a **guitar-extension
Wiimote** mapped to the FIFO + **F9 screenshots via xdotool** + a wait-stable
single-tap nav driver, on the **D2 Bank-8 patched-disc boot** (map-valid; the lane's
unique value over r1b's retail boot). Landed in
`milo-trace tools/wii_visgame_capture.py`.

**By SIGHT it clears the exact prompt flow D5 could NOT clear blind** (`video_backend=
Null`): `CHOOSE PROFILE → [GUEST]` · `Use Guest? YES` · Guitar nav-help `CONTINUE` ·
`Calibrate? → SKIP FOR NOW` · main hub `PLAY NOW → QUICKPLAY → CHOOSE SONGS → RANDOM
SONG → GUITAR → EASY → LOADING → live gameplay`. Reached **main_hub** and **live
gameplay on two songs** (The Beautiful People / Marilyn Manson; Bohemian Rhapsody /
Queen). Screenshots committed under `evidence/` (`V_nav_*.png`, `V_gameplay_*.png`).

## What blocked the capture (the real wall, now proven)

**The Wii Bank-8 debug patched-disc boot does not drive the band CharBone skeleton
animation in ANY reachable state — including confirmed-live gameplay.**

| state | finger relRot span / bone motion | evidence |
|---|---|---|
| overshell (D5) | 0° — bind pose | `D5_findings.md` |
| **main_hub vignette** | **0.00° over 10.5 s** — byte-identical to D2/D4 settled/bind (mid01→02 = 24.366°, …) | this lane; A7 route = idle body-sway only, no finger articulation |
| **live gameplay** | **0/992 CharBone world transforms move; span 0.0°** | `gpmotion.json`; scene PROVEN live (visual Δ = 25.97) yet 0 bone motion |

Decisive `gpmotion` probe (unlimited speed): scene **confirmed live** (note highway +
venue lights + title card advancing between consecutive frames, whole-frame luma
Δ 25.97; MEM2 heap chunk changes between reads → game loop live) while
**`charbone_movers_gt0p5u = 0 / 992`, `maxmove = 0.000`**. Reproduced at unlimited
speed with strumming input. The picked chain equals the bind pose identically across
every frame.

Corroboration (independent signals): `active_player_clip_drivers = 0` at EVERY state
(reproduces the D4/D5 red baseline at overshell, hub, AND gameplay); `exo_*`,
`target_*`, `bone_head/jaw/spine` all static too; `CharServoBone` VT has 0 resident
instances at gameplay. The read is not stale/dead — the SAME instrument returns the
D2/D4-validated bind pose and memory is proven live.

### Why this is decisive for the join (not an instrument gap)

The native port (`D3_delta_table_gameplay.json`) reads the **same CharBone-world
inter-bone representation** during gameplay and DOES get animated finger curl
(native mid01→02 = **54–99°** across members). So CharBone worlds ARE the
animation-bearing transform the entire D2/D3/D4/`interbone_framematch` machinery
operates on. The Wii side being static there means **the join's Wii half is
unobtainable in the representation the join requires** — not a naming/read bug.

Honest caveat: the visual scene advances (lighting sweeps, camera cuts, title fade),
and a whole-frame/region visual diff cannot cleanly isolate character-mesh
deformation from the dynamic venue. Whether the band *mesh* deforms via some
non-CharBone-readable path is unknowable from this test — and irrelevant to the join,
which needs CharBone world inter-bone tables (static in all reachable Wii states).

### Confound handled (speed / audio clock)

A normal-speed (1.0×) + Cubeb-audio reboot was attempted to exclude anim/song-clock
starvation; that capture landed on a **black LOADING** screen (8 s settle insufficient
at 1.0×) and was **inconclusive for gameplay**. The unconfounded decisive evidence is
the unlimited-speed `gpmotion` run where the scene was proven live yet bones static —
animation is not speed/audio-gated (gems scroll ⇒ clock runs; bones still frozen).

## Gate status

- **G-D5-1 capture validity — FAIL (correct red arm)**: no live band-driven CharBone
  motion; finger swing 0° (need ≥15°). Demonstrated now even at confirmed-live GAMEPLAY.
- **G-D5-2 / G-D5-3 — N/A**: no articulated captures.
- **G-D5-4 decision mechanicalness — GT-D by the fallback row**; no D5 JSON to classify.
  `scripts/analysis/v18_hand_classify.py` stands ready to classify mechanically per the
  §4 branch table IF an articulated capture is ever obtained.
- **G5′ — out of scope** (GT-A-only forensic; coordinator lane).

## Priced paths forward (for the coordinator)

1. **Visual gameplay nav — BUILT + EXHAUSTED for articulation.** Empirical correction
   to the VERDICT/D5 cost prior: gameplay is **not** a reachable articulation source on
   this boot; the Wii band CharBone skeleton is static there too.
2. **Root-cause of the static band (out of this box, a NEW item).** WHY the band
   CharBones stay at bind (`active CharClipDrivers = 0` everywhere) — candidates:
   (a) debug build stubs band performance anim; (b) Guest-profile / patched-disc boot
   never initialises the band director's performance-clip assignment; (c) anim gated on
   a subsystem not running headless. Resolving it could unlock the capture but is a
   distinct scoped investigation, not a V re-price.
3. **Alternative articulated ground truth** (e.g. a save-state from a real console
   session with the band performing, or a boot/mode that runs the director) would be
   required if the capture is ever needed; none is reachable within this lane.

## Reproduction

```bash
cd /home/free/code/milohax/milo-trace
V18_DISPLAY=:91 D2_USERDIR=/tmp/dolphin-v18 \
  python3 tools/wii_visgame_capture.py gpmotion   # boot→nav→confirm-live→measure
# => confirmed_live=True, scene visual Δ ~26, charbone_movers=0/992 (band skeleton static)
python3 tools/wii_visgame_capture.py session       # full nav→capture→pause-noise (span 0.0)
```

Instance-scoped `-u /tmp/dolphin-v18` + own Xvfb `:99`→`:91`; pgid-only teardown; other
lanes' Dolphin instances untouched. NO default flips, NO pin bumps, NO engine/game-source
edits. Disc dir `/home/free/tmp/wave17-d2/disc` (out of repo; provenance
`D2_boot_apploader_patch.md`).

## Process-lint compliance (OPTIONS §4)

- **Lint 3** — exhaustion called on the oracle-INDEPENDENT CharBone-world motion test
  (0/992 movers) + confirmed-live scene, NOT the pointer-chase driver census (flagged
  unvalidated on positives). Input liveness separately verified (taps change shell state).
- **Lint 7** — evidence committed (`V_exhaustion.json`, this doc, screenshots,
  `gpmotion.json`, capture JSONs).
- **Lint 10** — the visual nav rig + articulation detector + broad motion scan +
  `gpmotion` live-confirm probe were built and fired before any capture/branch claim. No
  fix attempted (A6: V stops at the branch letter).
