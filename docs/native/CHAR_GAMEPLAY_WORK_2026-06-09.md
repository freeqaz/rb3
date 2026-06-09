# Gameplay Character Correctness — Working Doc (2026-06-09)

Goal (this arc): make the **band characters look correct in GAMEPLAY** (the venue band
during a song), then land everything on `master`. Companion to
[`CHAR_SKINNING_DEFORM_INVESTIGATION.md`](CHAR_SKINNING_DEFORM_INVESTIGATION.md) (the
authoritative skinning trail) and [`CUSTOMIZE_PREVIEW_FINDINGS_2026-06-09.md`](CUSTOMIZE_PREVIEW_FINDINGS_2026-06-09.md)
(the closet/preview arc, UPDATE 8 = final).

## Ground truth (verified 2026-06-09, headless screenshots)
Boot-to-gameplay via `scripts/native/keyboard-to-gameplay.py` (canonical, DTA-state-driven
nav) reaches `game_screen` reliably. A sustained game-screen burst (new `--game-burst N`
option) catches the venue cameras cutting to band-member closeups.

- **TORSO clothing: CORRECT.** Animates coherently (the wave-08 rebind), female no longer flings.
- **HEAD + HAIR (and hands/face — thin geometry): SHARDED.** On every camera CLOSEUP the
  member's head/hair explodes into white radiating spikes (`/tmp/rb3-k2g/burst_05.png`,
  `burst_06.png`). This is the **primary visible defect** = roadmap **C7/C8**.
- Baseline contact sheet: `/tmp/rb3-contact-DEFAULT.png` (montage of 24 burst frames).

## Why (documented, see wave-08 IMPLEMENT in the skinning doc)
The animated per-member bone's rotation BASIS differs from the static `char/main/skeleton.milo`
"magnet" basis the authored skin offsets were baked against (worldRot sign-flips). A vertex at
radius R with rotation error θ flings by ~R·sin(θ) → LONG-THIN geometry (hair/fingers) shards;
COMPACT torso survives. That's why the shipped rebind is **torso-only** (opt-out `RB3_NO_SKEL_REBIND=1`,
study-all-body `RB3_SKEL_REBIND_FULL=1`). Documented fix lead (OPEN): capture the per-member
skeleton's BIND/REST pose before any clip plays, rebake `offset' = meshBindWorld · inverse(perMemberBoneBindWorld)`.

## Harness
```bash
# canonical boot-to-gameplay + game-screen burst (new --game-burst)
python3 scripts/native/keyboard-to-gameplay.py --port <P> --diff hard \
    --out /tmp/rb3-x --game-burst 24 --burst-interval 0.7 --verbose
# contact sheet
montage /tmp/rb3-x/burst_*.png -tile 6x4 -geometry 240x135+2+2 /tmp/rb3-x-contact.png
```
Head A/B toggles: `RB3_NO_SKEL_REBIND=1` (no rebind, wave-06 static rebake) /
`RB3_SKEL_REBIND_FULL=1` (rebind head too) / `RB3_NO_DEFORM_LOAD=1` (no gDeforms morph).
Engine draw-time probes: `REBIND_DRAW_SKINPOS=1`, `REBIND_DRAW_FLING=1` (>120u shards).

## ⚠️ Concurrency hazard (coordination)
Multiple agents launch `rb3-native` from sibling worktrees. A cleanup `pkill -f rb3-native`
in ANY agent kills ALL of them by binary name → my boots intermittently die with SIGKILL
(`code -9`, "HTTP server never came up") even though nothing is wrong. **Mitigation: retry.**
Do NOT `pkill -f rb3-native` broadly; prefer `kill -9 <specific PID>` or
`pkill -f "[b]uild-native/rb3-native"` scoped to the main repo. (The `[b]` bracket trick also
stops `pkill -f` from matching its own command line — a self-kill that returns exit 144.)

## Lanes (who owns what, 2026-06-09)
- **THIS arc (orchestrator):** gameplay character VISUAL correctness (head/hair/hands shard, C7/C8).
- `.claude/worktrees/domino2-fix`: domino ② / default-on guest+preview (closet/web). Do not duplicate.
- `.claude/worktrees/exp-previewon`: preview-on experiment.
- Other worktrees: audio-perf, web GPU/IDB, decomp sweeps — unrelated files.

## ✅ FIX LANDED (`0de768a1`) — head/hair/hands coherent + animating in gameplay
`BandCharacter::RebindHeadHandsAtRest()` (called from `Poll()` BEFORE `Character::Poll()`):
on the first Poll the per-member skeleton still holds the SetDeformation gender-bind
**rest** pose; snapshot each per-member bone's rest WorldXfm, then for the
head/hair/hands/face (+ non-torso) skin meshes bake `mOffset = meshWorld · inverse(restWorld)`
and bind to the **live** per-member bone. At rest → identity (coherent); as the bone
animates (Character::Poll / CharHair / CharFaceServo drive the SAME per-member skeleton)
the verts follow → coherent AND animating. The rest snapshot covers late-streamed meshes.
Default-on, opt-out `RB3_NO_HEAD_REBIND=1`, diag `HEAD_REBIND_PROBE=1`. HX_NATIVE-only.

**Why it works where the prior attempts didn't:** the torso rebind (calcOffset=false at
Poll) can't fix thin head/hair geometry (residual basis error → R·sin θ shard); calcOffset
at the Poll site shards too (bone already mid-animation). The rest snapshot gives an EXACT
offset. Binding to the per-member skeleton (which CharHair/CharFaceServo drive) means the
"hair fights the rebake" concern of a static-magnet rebake does NOT apply.

**Verified:** REBIND_DRAW_FLING = **0** (no rebound mesh >120u); REBIND_DRAW_SKINPOS **37-65u**
(clean) incl. hair (hippyfringe 64u), fingernails (47u), legs/jacket; 17 meshes/~200 bones
rebound per member, ~108 rest poses captured. Closeups coherent (was: white-feather explosions
— `/tmp/rb3-contact-DEFAULT.png` vs `/tmp/rb3-contact-HEADFIX.png`, `/tmp/rb3-headfix-closeups.png`).
Torso untouched (complement scope); no crash full song.

## Status
- [x] Build green (`936bd8f4`).
- [x] Ground truth: head/hair shards on closeup.
- [x] Diagnosis+design 8-agent ultracode workflow (`gameplay-char-head-shard-diagnose`) — root cause = C8 skinning bind.
- [x] A/B: shard is independent of the rebind (torso-only) AND of gDeforms (NO_DEFORM_LOAD still shards) → it's the bind.
- [x] **Implement head-skinning fix (`0de768a1`).**
- [x] **Adversarial verify** — 5-agent review (verdict: ship-with-followups); must-fix + high-value should-fixes landed (`2580e128`): fallback latch, capture-on-first-resolve, finite guard, GeomOwner skip, StartLoad re-entry reset. Re-verified: all 4 members `latched=1`, REBIND_DRAW_FLING=0, coherent closeups, torso animates, full song no crash.
- [x] **Domino ② landed by concurrent domino2-fix agent (`65f7f0e6`)** — song_select UAF fixed at source, guest-profile + char-preview restored to default-on. My head-fix build is a child of it; default-on gameplay+closet verified safe.
- [x] Closet char fully coherent via real nav (default-on, no flags): `/tmp/rb3-realnav/closet_23.png`.

## Remaining follow-ups (noted, non-blocking)
- Partition torso/head by a structural property (bone count / bone-name set) instead of 4 hardcoded
  outfit names (trackjacket/vestdenim/plaidshirt/shred); verify a 2nd differently-customized band outfit.
- 1 residual `own==bound`-forever mesh per member stays on the engine clamp (latch now stops the rescan).
- `unordered_set` for the draw-tree collector (O(N²) now, harmless once latched).
- BandHeadShaper head-milo morph (cosmetic gender face shape) still deferred — separate from the shard.
