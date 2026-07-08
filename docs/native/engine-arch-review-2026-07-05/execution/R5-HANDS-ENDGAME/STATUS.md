# R5-HANDS-ENDGAME — STATUS

Wave-17 delivered the adjudication **VERDICT.md** (decision **C**): neither the true
reskin (A) nor closure (B) is choosable from D4's table; the discriminator is the named
upgrade **D5 — articulated Wii capture with a live clip+frame join key**, hard-boxed at
~1 lane-wave, with a mechanical branch table and GT-D closure pre-authorized on box
exhaustion. See `VERDICT.md` §4.

---

## D5 / Wave-18 — articulated Wii capture attempt → **BOX EXHAUSTED (priced)**

Executed the pre-registered D5 lane (task #102). **Result: the discriminator-grade
articulated Wii capture was NOT obtained within the hard box → GT-D per the VERDICT §4
branch-table fallback row.** This lane REPORTS the priced exhaustion; per the charter it
does **not** declare closure — the coordinator executes the pre-authorized GT-D.

Full detail + reproduction: `evidence/D5_findings.md`; machine-readable
`evidence/D5_exhaustion.json`; nav logs `evidence/D5_nav_aggressive.{log,json}`,
`evidence/D5_clean_nav.log`.

**What works (machinery sound).** The D2 patched-disc Bank-8 boot is GO with a valid map
(992 `__vt__8CharBone`, `TheBandDirector` live). **Pipe input injection works** — an
emulated Wii Remote mapped to a FIFO (`Pipes/rb3ctl`) demonstrably changes shell state on
3 clean boots (`START`→CharDrivers 12→18; `A`→resident playerN clips 0→8). The reusable
instrument landed in **`milo-trace tools/wii_bone_dirboot.py`** (`setup_input`/`Pad`/
`active_player_drivers`/`wait_game_init`/`cmd_nav`; new `nav` subcommand).

**What blocked it (the wall).** No blind-navigable shell state animates the band hands.
Across ~60 taps over 3 cold boots, the hand chain stayed **byte-identical to the D2/D4
settled pose** (mid01→02 pinned at 24.37°, ring01→02 at 26.13°, …; **0° finger relRot
span** over 11 s and 20 s dwells, measured by an oracle-INDEPENDENT bone-motion test).
The best state reached (18 CharDrivers / 8 resident playerN clips, **0 active**) is
identical to D4's frozen `ui/overshell` census — the band is pre-loaded behind an
un-cleared guest-profile / "Use Guest? YES" / nav-help prompt flow, not a playing
`main_hub` vignette. Blind (Null video) those prompts can't be cleared reliably, so
`main_hub` was never cleanly entered. This is G-D5-1's mandated fail-red behavior working
as designed; the required live-vignette world-state is not blind-reachable.

**Empirical correction to the VERDICT's cost prior.** VERDICT §3 assumed the missing
capture was cheap ("the ONLY missing piece is a Wii capture while a hand-animating clip
driver is live"). Built + validated the pipe-nav instrument the assumption implied; the
live vignette is **behind a menu flow that is not blind-navigable**. The capture is
obtainable only via a **visually-guided gameplay nav** (Xvfb + GPU video backend +
xdotool screenshots + a guitar-extension Wiimote + ~8-screen gameplay sequence per
`docs/native/c8-ground-truth-2026-07-01/t2-dolphin-oracle.md`) — feasible on this box but
a **dedicated lane-wave build-out that exceeds the D5 hard box**. (The concurrent r1b lane
runs retail blind + GDB, bare Wiimote — same wall, no articulated capture either.)

**Gates.** G-D5-1 FAIL (correct red arm: 0 live drivers, 0° mr swing). G-D5-2/3 N/A (no
captures). G-D5-4 → GT-D by the pre-registered fallback. G5′ out of scope (GT-A-only).

**Charter.** No default flips, no pin bumps, no engine edits; only files this lane owns
(milo-trace tool in its repo; this hub + `evidence/`). Instance-scoped `-u /tmp/dolphin-d2`;
pgid-only teardown; r1b untouched.

**Handoff to coordinator.** Execute the pre-authorized GT-D closure (VERDICT §4 / PLAN
§3.4) with the §3 clamp-shorthand correction — OR, if the campaign wants to still fund the
discriminator, dispatch a **visually-guided gameplay-capture lane** (Xvfb+GPU+screenshots
+guitar-ext) reusing this lane's pipe-nav instrument; the Bank-8 map-valid boot remains
this lane's unique value over r1b's retail boot.
