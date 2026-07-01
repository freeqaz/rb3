# Verify: `crowd` — crowd merged/frozen → spread + animating (WAVE-3, adversarial)

Wave-3 verifier, 2026-06-11. Independent judgment of the **composed** master build,
not the isolated worktree. Read-only; no edits, no main-repo rebuild.

- Composed binary: `native/build-native/rb3-native`, built 21:00:38, AFTER the crowd
  cherry-pick `adb5240e` (20:59:44) and the pin bump `cca1869a` (engine `469c550`).
  Crowd code confirmed present on master (`RebindCrowdCharBonesToOwnSkeleton`,
  `RB3_NO_CROWD_REBIND`, `CROWD_REBIND_PROBE` all in `src/system/world/Crowd.cpp`
  and as strings in the binary).
- Method: same composed binary, A/B via the fix's own opt-out env
  (`RB3_NO_CROWD_REBIND=1` = BEFORE, default = AFTER), `SHARD_DBG=1
  SHARD_RATIO_DBG=1`. Plus live gameplay screenshots framing the real crowd.

## VERDICT: PASS (user-visible symptom fixed) — with one honest residual + one NEW shifted-load note

The crowd renders **full-bodied, densely spread across the small_club floor,
posed and animating**. The body-mesh shard-guard collapse that produced the
"merged into one spot, only floating heads" symptom is gone. The fix is real and
composes cleanly — no interaction regression with the sibling char-render /
mesh-cache / bloom landings was observed in the crowd path.

## EVIDENCE

### Visual (decisive) — real gameplay, crowd actually framed

The impl's `crowd-shot-capture.py` forced-cam approach does **NOT actually frame
the crowd in this build** (see "methodology gap" below), so I verified against the
director's natural crowd-cam cycling in live gameplay instead:

- `/tmp/rp3-crowd/manual/plain_game_cyc0.png`, `…_cyc3.png` — wide crowd-cam frames:
  dense crowd, full bodies, raised/cheering arms, spread left+right across the floor.
- `/tmp/rp3-crowd/manual/cyc3_rightcrowd.png`, `…_leftcrowd.png` — 3× crops:
  individual full-bodied figures (head/torso/arms/legs), distinct outfits, distinct
  poses, faces visible. This is the retail crowd look — NOT floating heads / green
  statues / skeletal remnants (the scout's `crowdshot_06/07/13` symptom baselines).

### Animation (objective, from engine probes over a full song)

- `male_crowd_body01.mesh`: **328 distinct worldExt values** over the song.
- `fist.mesh` (crowd_male01): **516 distinct bone0 world positions**.
  → bones re-pose per frame; the crowd idles/cheers (animating, not frozen).

### Shard census — same composed binary, A/B (8811 = fix ON, 8812 = opt-out)

| metric | BEFORE (opt-out) | AFTER (fix ON) | Δ |
|---|---|---|---|
| `crowd_body` SHARD_GUARD drops | 140457 | **6680** | −95.2% |
| `crowd_body` ratio | ~22–26× (worldExt ≈ 2240u) DROP | ~0.96–0.99 PASS | fixed |
| ALL crowd-dir drops | 143815 | 34623 | −75.9% |

The **6680 AFTER figure matches the impl doc's claim exactly.** (The impl's "63339"
BEFORE differs from my 140457 — that's playthrough-length / frame-count variance,
not a discrepancy in the fix; the AFTER number and the mechanism reproduce
identically.) Mechanism confirmed: BEFORE = body bind-offset mismatch flings
vertices ~2240u (ratio ~25×) → V24 guard drops every body draw; AFTER = inverse-bind
rebake collapses worldExt to ~bindExt (ratio ~1.0) → bodies pass.

## RESIDUAL #1 (impl disclosed, confirmed): body flicker on 2 archetypes

`crowd_body` AFTER still has a small high-ratio tail — `male_crowd_body03` and
`female_crowd_body02` reach ratio 7–9× (worldExt ~710–790u) at peak arm-swing
(rest-vs-mid-idle bake drift, exactly as the impl documented). ~110 ratio-path
drops / ~6680 guard-path body drops total. Not visible at crowd density in the
captures. Clean kill needs the engine change the impl flagged (separate the
offset-rebake-skip from the per-bone skin-clamp-skip — both currently gate on the
shared `mNativeBonesRebound` latch). Out of scope for the rb3-only Fix A.

## NEW ISSUE #1 (NOT in the impl doc): the fix SHIFTS load onto crowd HAND meshes

The impl reported only the `crowd_body` subset. Counting **all** crowd-dir drops,
the rebake moves the failure onto the crowd hand/gesture meshes:

| crowd-dir mesh | BEFORE (opt-out) | AFTER (fix ON) |
|---|---|---|
| `fist.mesh` | 2008 | **17505** (8.7× worse) |
| `clap.mesh` | 1301 | **10239** (7.9× worse) |

AFTER, `fist`/`clap` drop at ratio ~2.0–3.3× (bindExt ~35, worldExt ~75–93) — a
*different, milder* failure than the bodies' 25×. Net crowd-dir drops still improve
overall (143815→34623), and **the residual hand drops are NOT visibly perceptible**
at crowd density (cyc3 crops show arms terminating naturally; transient small-mesh
drops vanish in the throng). But this is a genuine regression on those two meshes
that the impl's body-only metric hides, and it likely shares the bake-drift root
cause (the hand mesh is bound to a hand bone whose rest-vs-live offset drifts even
more than the torso). Worth folding into the same engine-side clamp follow-up.
NOTE: `SHARD_RATIO` and `SHARD_GUARD` are two separate guard paths — `fist` passes
the RATIO path (~0.86) but is dropped 17.5k× by the GUARD path; I judged on the
GUARD path (the one that actually drops the draw) + the visual.

## Intro / tv3 vignette — distinguished, NOT conflated (the known triage trap)

Captured `tv3_a_screen` during the pre-gameplay transition
(`/tmp/rp3-crowd/tv3/tv3_tv3_a_screen_0[0-5].png`): it is a venue establishing
cinematic — "SHOW TONIGHT @9PM" poster, menu board, club door/signage. **No crowd
characters are framed in the vignette**, so there is no vignette-crowd-shard look
to mis-read as the gameplay bug here. (Aside, off-topic: the menu-board poster
frame `tv3_tv3_a_screen_00.png` shows dark square placeholders over the menu —
possibly the grey album-art-box residual noted as a separate plan follow-up, not a
crowd issue.) The gameplay crowd (cyc0/cyc3) is the real subject and is correct;
the two are clearly separable.

## METHODOLOGY GAP in the impl's verification (flag for the orchestrator)

`scripts/native/crowd-shot-capture.py` freezes the cam with
`{$band_director set disabled 1}` and forces `{band_director force_shot …}`. In the
composed build **neither resolves**: `$band_director` evaluates to int `0`,
`find_obj band_director` → null, `set disabled 1` returns 0 (no-op), and the forced
shots therefore do NOT stick — the director keeps auto-cycling and the captured
"crowd shots" actually frame the highway / band-char hair / venue detail, not the
crowd floor (see `/tmp/rp3-crowd/after/after_coop_dir_crowd*.png`). So the impl's
"before/after, same camera" visual rests on shots that didn't reliably frame the
crowd. The SHARD diagnostics (which the impl also used) ARE objective and do prove
the fix; the visual proof is better obtained from the director's natural crowd-cam
cycling in live gameplay (cyc0/cyc3 above). Recommend fixing the capture script's
director handle (or capturing across natural cam cycling) before reusing it.

## Band-char non-regression

Band characters render coherent in all gameplay frames (drummer kit + band figures
visible, no shards) — the rebake is crowd-Character-scoped and did not touch band
members. Consistent with the impl's claim and the disjoint file surface
(Crowd.cpp vs BandCharacter.cpp).

## Artifacts

- Logs: `/tmp/rb3-crowdshot-8811.log` (fix ON), `/tmp/rb3-crowdshot-8812.log` (opt-out).
- Live gameplay (decisive): `/tmp/rp3-crowd/manual/plain_game_cyc{0,3}.png`,
  `cyc3_{left,right}crowd.png`.
- Vignette: `/tmp/rp3-crowd/tv3/tv3_tv3_a_screen_0[0-5].png`.
- Forced-cam (showing the methodology gap): `/tmp/rp3-crowd/after/`, `/tmp/rp3-crowd/before/`.
