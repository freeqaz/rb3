# Render regressions 2026-07-02 (user report: crowd pile / broken limbs / missing hands / dark chars)

User report (web, fresh rebuild): (1) all "crowd" members at one spot center-stage,
(2) limbs wrong — legs floating off the floor, arms jagged/overextended, (3) hands
not rendering, (4) characters dark / "textures not loading". All reproduced on
native (per CLAUDE.md debug-native-first rule). Triage + fixes below.

## Timeline anchor

Wig-gate run7 (this morning, /tmp/wig-gate/run7/) was geometry-clean and warm-lit.
Everything below regressed in the window after it: `43db1c59` (tool-only, cleared),
`30c51bad` (BandPatchMesh re-land), `ede6911f` (probe, cleared), `fadd179a` + engine
`04c8e1c` (compose dest-multiply), `7f603e17` + engine `5587ce0` (char real-light).

## 1. Exploded geometry (jagged arms, spike/missing hands, jaw slab) — FIXED

- Cause: `30c51bad` BandPatchMesh WorkVerts trio + FindXfm re-land. Second
  regression from this family (first: `4a49b1a4` → revert `82f390b1`).
- Proof: worktree bisect with positive control (revert → 6/6 crowd shots clean;
  restore → shards return). /tmp/regress-0702/crowd-nopatch/ vs crowd-master/.
- Fix: revert commit `f0a95910` (both files). Wii build green (280/280).
  Post-revert master verified clean: /tmp/regress-0702/crowd-fixed/.
- GATE LESSON (recorded in memory): band-closeup-capture drop/ratio metrics are
  BLIND to patch-shard corruption — PASS 34/34 pinned / 0 drops on visibly
  exploded frames. Any BandPatchMesh re-land needs a patch-bearing lineup +
  reviewer-judged wide frames. The "tentacle arm" red herring = the authored
  guitar_brain novelty guitar; the real signature is pale needle/spike shards
  attached to hands/arms/faces (venue flying-chair props are look-alikes).

## 2. Characters dark / "textures not loading" — ROOT-CAUSED, fix in flight

Outfit two-color composite (`OutfitConfig::MatSwap::Compose` → engine
`BandRnd::DrawRect` while `gRB3OutfitComposeActive`): the four layers
(base col1 fill / diffuse×col2 / interp / mask-alpha-gray) have TWO wrong
native mappings, and we've shipped both:
- REPLACE (pre-`04c8e1c`): RT collapses to last (near-white) layer → bright
  FLAT untextured characters + glowing white eyeballs.
- DEST-MULTIPLY (`04c8e1c`, current): RT collapses toward black product →
  black-silhouette characters with sparse glowing patches on clothing =
  the user's "dark chars / textures not loading".
Empirical A/B (same song moment, score 197): /tmp/regress-0702/band/ (multiply)
vs band-composeoff/ (replace) vs band-bothoff/. Correct math = per-layer
alpha-aware combine (authored two-color recolor), being derived + implemented
in the engine (Opus lane; layer-by-layer DrawRect instrumentation → real blend
equations → band-closeup gate vs Dolphin GT face oracle).

Related: engine `5587ce0` (char environs shade from mLightsReal, ambient cap
0.11/0.14) is a second darkness lever — re-judge AFTER the compose fix lands;
opt-outs RB3_CHAR_REAL_LIGHT_OFF / RB3_COMPOSE_MULT_OFF exist for A/B.

## 3. "Crowd at center" + "legs floating" — characterized: song-start WALK-ON

- NOT a placement bug: crowd-origin-posdump on live gameplay shows band roots,
  300+ crowd members, and props all SPREAD (verdict REFUTES origin-collapse).
- The pile is the 0–5s WALK-ON window: members knot near the drum kit, one body
  floats horizontally with stretched legs, guitarist's feet off the floor;
  resolves by ~6s. Reproduces on native (post-revert → NOT BandPatchMesh) and
  web identically. Evidence: /tmp/regress-0702/native-songstart/shot_05,07.png,
  /tmp/regress-0702/web-crowd/g_003s_1.png (pile) vs g_006s_2.png (resolved).
- Believed PRE-EXISTING (nothing anim-related in the regression window); on web
  the slow start makes it prominent. Scout lane running → walkon-2026-07-02/SCOUT.md.

## 4. Status / next

- [x] Revert landed `f0a95910`; native re-verified clean.
- [x] Compose math FIXED: engine `153beaf` (out = diff·lerp(col1,col2,interp.a),
      weight in the DXT5 alpha of `_interp_gw`/`_mask_gw`; mask = coverage
      dest-multiply), pin `e4b661ad`. 1-iteration gate pass, reviewer-judged
      (/tmp/compose-fix/attempt1-*). Char "darkness" resolved by this — the
      `5587ce0` real-light shading stays as shipped.
- [x] Walk-on: scout docs/native/walkon-2026-07-02/SCOUT.md; fix `67e87ae1`
      (HX_NATIVE snap out of held vignette pose at venue entry, Wii .o
      byte-identical, RB3_WALKON_SNAP_OFF opt-out). Measured correction to the
      scout: it was a cleanly-held stale pose (empty driver), not a frozen
      blend; the fix guarantees no member stays frozen if the intro group
      misses them. Count-in thin-geo shard slivers (strings/cymbals/hair) are
      the KNOWN pose-independent skinning residual — separate limitation.
- [x] Web rebuilt + deployed (release+debug) with all of the above; verified:
      walk-on knot gone at t=3s (members separated at their spots), patch
      shards gone, clothing print visible on some members.

## 5. Web-specific residuals (pre-existing — present in the PRE-fix run too)

- **Grey/flat char composites on web** while native (same code) is textured:
  the outfit composite bakes once at load, before web's async/mip-stripped/
  progressively-sharpened source textures are fully resident → RT bakes grey
  forever. Fix lane running (recompose-after-resident / strip-list exclusion).
- **One member floats horizontally through gameplay** (same white-boots member
  pre- and post-fix, ~9-20s+): never receives a live clip on web — likely
  clip streaming/bundle gap, NOT the walk-on snap (A/B: pre-fix run shows the
  identical floater). Deferred — next web lane after composites.
- Thin-geo shard clusters read worse on web (bigger/whiter) — same known
  skinning residual class as native.
