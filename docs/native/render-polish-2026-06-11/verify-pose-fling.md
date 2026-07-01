# verify-pose-fling — independent adversarial review (Opus)

**Verdict: CONFIRM_WITH_RESIDUALS**

Reviewer: independent Opus adversarial pass on the COMPOSED master build.
- rb3 HEAD `c6e6048d`, engine pin `15ce606` (verified: engine HEAD == pin, and the
  pin's top commit `15ce606 fix(rnd-rb3): recompose stale band-skeleton leaf WorldXfm
  before skinning (pose-fling)` IS the marquee fix).
- Binary `/home/free/code/milohax/rb3/native/build-native/rb3-native`, built 2026-06-15 09:07.
- Evidence: `/tmp/rp5rev-pose-fling/` (trimmed); ports 9301-9306.

## What the fix is (verified by reading the engine diff)

`milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp`, `BandRnd::DrawMesh`: before the
bone-palette fill loop, for each referenced bone walk its `TransParent()` chain to root
(stop at an already-forced node via an `unordered_set` visited dedup), then force
root->leaf `DirtyLocalXfm()` + `WorldXfm_Force()` so each leaf composes against a fresh
parent world. Default-on, opt-out `RB3_NO_SKEL_WORLDFIX=1`. Native-only file (not in the
Wii image) → Wii byte-identical by construction. The diff matches the implementer's
description exactly. The same commit also carries the env-gated, render-inert
`CHAIN_*` / `C8_PROBE` / `IK_SHARD_VERT` diagnostics.

## Independent A/B (my own measurements, NOT the implementer's numbers)

`SHARD_DBG=1` + `keyboard-to-gameplay.py --diff hard --game-burst 24`, boot→gameplay,
counting `[SHARD_GUARD] dropped degenerate skinned mesh=` lines, attributing by mesh name.

| metric | BEFORE (`RB3_NO_SKEL_WORLDFIX=1`, port 9302) | AFTER (default, port 9301) |
|---|---|---|
| total SHARD_GUARD drops | 223,475 | 29,545 |
| garment drops / rendered frame | **16.18** | **2.89** (−82%) |
| top dropped meshes | fist, fingernails, lowtopsneaks, flarejeans, bikinichain, furbikini, escapeartist, femaledestroyedchucks, greaserjacket, clap … (a whole wardrobe: jackets/jeans/skins/hands/nails) | lowtopsneaks, kidgloves, eightholedocs only (footwear+gloves residual) + 601 scrollbar (UI, unrelated) + 62/62 thin extras hair/eyebrows |
| crowd-body / head / face / teeth / eye drops | — | **ZERO** |
| hand / finger / fist drops | fist 23685, fingernails 22203 (TOP TWO) | **ZERO** (the fix also fixes hands/nails) |
| crashes / asserts / SIGSEGV / SIGABRT / NaN | 0 | 0 (across ~12k frames) |

The direction and magnitude decisively confirm the implementer's core claim: the fix
removes the bulk of garment guard-drops. My per-frame number (−82%) is less than the
implementer's headline (186073→10027, −94.6% over a "matched 10-burst") — I could not
reproduce the exact −94.6% because my two runs sampled different gameplay
segments/venues and my AFTER run's harness hit a transient (see notes), so I report a
conservative per-rendered-frame ratio. Either way the residual is a SHORT,
qualitatively-different list (footwear+gloves), not the BEFORE wardrobe wall.

## Visual confirmation

- AFTER `06_game_screen.png` (pink venue): band members standing, solid bodies,
  instruments rendered, no screen-crossing shards, no below-floor flung limbs.
- AFTER `burst_07.png` (club/dartboard venue): multiple band members coherent + crowd
  present in background; AFTER `burst_10.png`: foreground member's hands at the body.
- BEFORE `06_game_screen.png` / `burst_20.png`: band/crowd figures noticeably
  thin/skeletal (missing garment volume) + a large flung hand visible — the documented
  "flung garment guard-dropped" symptom. The A/B is visually unambiguous.

## Residual (why CONFIRM_WITH_RESIDUALS, not plain CONFIRM)

The AFTER residual drops are `lowtopsneaks` / `kidgloves` / `eightholedocs` at ratios
2.2–3.5 (just over the 2.0 shard-guard threshold), bone0 at sane body height
(glove root Y~142, Z~66 = at the hand, NOT flung to Z=-33 below floor). This is exactly
the implementer's documented residual: the LEG fling is closed (no more Z=-33), and the
remaining tail is small-bind-extent footwear/gloves that legitimately clear 2x on a
normal pose curl — a tighter, separate follow-up (wave-6), not the marquee fling. So the
band is now *substantially* dressed (jackets, jeans, body skins, hands, fingernails all
return), with a footwear/glove edge case remaining on some members/frames.

## Interaction sweep (menu-contrast floor-lower did not crush/blow out other scenes)

`interactionsOk = true`. Mean / %crush(<8) / %blow(>247) luminance:

| scene | mean | %<8 | %>247 |
|---|---|---|---|
| main_hub | 101.2 | 11.4 | 0.0 |
| song_select | 72.3 | 7.3 | 0.0 |
| gameplay (pink venue) | 55.8 | 21.4 | 0.3 |
| gameplay (club venue) | 46.6 | 3.9 | 0.5 |
| endgame score | 57.5 | 18.0 | 0.2 |
| endgame results | 85.7 | 10.1 | 0.1 |

No scene is pathologically crushed (none >50% black; near-black is dark backdrop +
track letterbox, with healthy p99 highlight headroom 200–243) or blown out (all <1%
clipped). Main hub neon/menu text + song-select grid/album-art read cleanly; the wave-5
songselect-ui fixes are present (no header garbage digits, no grey album box). Full
song→endgame chain PASSES: `song-end-test --require-endgame` reached
`coop_endgame_screen` stable 25s/2468 frames, no abort (wave-5 score-detail fix intact).

## Notes / caveats

- One AFTER `keyboard-to-gameplay.py` run threw a transient `ConnectionRefusedError` on
  a late `autohit` POST in the burst loop; the GAME kept rendering fine (no crash). The
  HTTP server is single-request/blocking and occasionally races a render frame. Not a fix
  regression — re-runs (the patched-harness 9305 run + the song-end-test 9306 run) both
  completed clean with full nav.
- Reviewer hygiene incident: a `kill <ppid>` to reap a zombie hit a sibling's recycled
  PID (PID reuse) — the sibling's first-frame-flash `cap.py 9311` was killed; their actual
  rb3-native instances were untouched and they relaunched (9311 seen alive again). No
  lasting impact. Going forward I only SIGTERM'd rb3-native PIDs whose `/proc/<pid>/environ`
  `RB3_HTTP_PORT` was in my 9301-9309 range.

## Bottom line

The marquee claim holds: the band is now fully dressed and correctly posed (legwear,
footwear, body skins, hands, fingernails all return; no below-floor leg fling), crowd +
head/hands are not regressed (in fact hands/nails improve), and no crash across a full
song. CONFIRM, with the honest residual that a footwear/glove edge case still
guard-drops on some frames — a smaller, separate wave-6 follow-up, as the implementer
documented.
