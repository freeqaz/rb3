# Session 2026-06-02 — Track A (gameplay FX/HUD parity)

Goal: work the "remaining gaps" Track A (gameplay FX + HUD visual parity) for the RB3
native/web port, starting with A1 hit-flame FX. Driven Opus-led with adversarial
multi-agent verification workflows; image verdicts adjudicated by the main loop.

## Headline outcomes

| Item | Outcome |
|---|---|
| **A1 — hit/flame FX** | ✅ **DONE + committed.** Flames render at the strike line on hits (were absent). |
| **A2 — gem/fret glow** | 🔶 Diagnosed → shared emissive root → **BLOCKED on venue-env bring-up.** |
| **A4 — highway/lane glow + lighting** | 🔶 Diagnosed → **BLOCKED on venue-env bring-up** (deep V2). |
| **A3 — SP/multiplier HUD** | glow part BLOCKED (same root); safe non-glow parts: _see A3 section_. |
| Emissive material fix | Validated by probe, implemented, **reverted** (net-negative until scene is dark). |

## Commits made
- **engine** `milo-native-engine` `3b18fed` — `gfx(rb3): real RndParticleSys billboard
  renderer in BandRnd (A1 fix-B)`.
- **rb3** `99ec8434` — `native: gameplay hit-flame FX (A1) + glow/lighting diagnosis`
  (fix-A in `GemTrackDir.cpp`, `Part.h` `RelativeXfm()` accessor, `MILO_ENGINE_PIN`
  bump to `3b18fed`, + the diagnosis docs).

---

## A1 — hit/flame FX (DONE)

**Diagnosis corrected the roadmap premise.** Full FX *simulation* works (autohit →
`GemPlayer::Hit` → `GemSmasher::Hit` → `hit.trig` → 570 `LaunchParticles`/song; particles
spawn, show, positioned, up to 24 active). Two real root causes:

1. **fix-A (visibility):** the per-lane smasher hit-FX particle systems are collected by
   `setup_draworder` (smasher_plate.dta) into `after_gems.grp`, wrapped by
   `after_hide.grp`, which `GemTrackDir.cpp:497-498` explicitly **hides** (retail
   composites the FX via the track's `smasher_fx.grp` path, inert on the BandRnd backend).
   Fix = additive `#ifdef HX_NATIVE` keeping `after_hide.grp` shown. Verified the FX layer
   then draws (`DrawShowing` fires for all flame systems; was 0).
2. **fix-B (the real renderer gap):** `DrawParticlesBillboard` was a **weak no-op stub**
   on RB3's BandRnd backend (`rndobj_synth_link_stubs.s:67`); the DC3 `Part_Wgpu.cpp`
   renderer is **not compiled for RB3**. Implemented `BandRnd::DrawParticles` in
   `Rnd_Wgpu_RB3.cpp` (camera-facing billboard quads, `Multiply(p->Pos3(),
   sys->RelativeXfm(), worldPos)` so relative-frame particles land at the strike line,
   full-tex UVs (RB3 has no UV tiling), blend via `MapBlend`, diffuse via `GetRB3TexView`,
   group0 = scene viewProj, depth test/no-write, pipeline cached per (fmt,blend,depth));
   free strong `DrawParticlesBillboard` displaces the stub.

**Verified:** pink radial-flares + white bursts render at the strike line on hits; correct
z-order; no regression; clean link; venue `.part` systems un-stubbed as a benign bonus.
*(A review agent false-FAILED it by mistaking band-character costume geometry — the white
feathery glam-rock outfit on "20th Century Boy" — for blown-out particles, and traced the
uncompiled dc3 `Part_Wgpu.cpp`; adjudicated by direct image review.)*
Minor follow-up: flare reads pink vs retail's blue/white (per-asset tint, non-blocking).

Build engine: `cmake --build native/build-native` (the `milo-engine` lib is a **separate**
target; `--target rb3-native` does **not** rebuild it).

---

## A2/A3/A4 glow — shared root, BLOCKED on venue-env

**Shared root cause (validated by runtime probe):** `BandRnd::DrawMesh` drops the material
EMISSIVE feature — never reads `mEmissiveMultiplier`/`mEmissiveMap` (stays 0), and
`MakeMaterialBindGroup` hardcodes the emissive slot (binding 5) to `mBlackView`, while the
WGSL shader already implements emissive. Gems carry `prism_mat.mat` mult=1.0
map=`prism_gem_emissive.tex`; surface ×0.4; gem_smasher_glow ×0.9. The map-presence guard
(`mEmissiveMap ? mult : 0`) is essential (many HUD/rails/plate mats have mult=1.0 + null map).

**Implemented + 4-agent verified + REVERTED.** Standalone the emissive fix is net-negative:
gem-core lift marginal (gems already glossy), no bloom halo, **and a regression** — prelit
sustain note **trails white-clip/blow out** (finalColor = baseColor + baseColor·mult·emissive
≈ 2× on prelit). HUD/highway glow not delivered.

**KEY: the glow payoff is GATED ON SCENE-LIGHTING.** The flat 0.45 gray ambient flood washes
out emissive contrast. So A2/A3/A4 glow is a **coupled, scene-lighting-led unit** (scene
darkened → re-add validated emissive + a hue-preserving blowout clamp + bloom-halo), verified
FRAME-LOCKED (the songMs captures drift 44–58%).

### Scene-lighting (A4) — BLOCKED at the source
Probe-first found: `RndEnviron::sCurrent` at gameplay is a **degenerate default** — ambient
`(1,1,1)` white, **zero lights** in both lists — because the **venue `.milo` is deferred on
native** (`WorldInstance::SyncDir`, `Instance.cpp:304-374`: defers `world/vignette/` +
`world/shared/` proxies). Porting `WriteSceneUniforms` to read it would make the scene
*whiter/flatter*, not dark+lit — so the implementer correctly made **no change**.

The deferral is a **known-hard, documented, prior-session-stuck V2 task** (the
inlined-cached-shared proxy instancing gap — a null-`Dir` assert; the 2026-05-28 audit tried
several fixes and concluded it needs "a many-to-one parent-chain abstraction or per-proxy
shadow dirs"). **⇒ the entire glow/lighting track is blocked on venue-environ bring-up**, a
deep world-subsystem effort.

RB3 accessor contract for when env is usable: `RndEnviron::sCurrent` (static),
`mLightsApprox`/`mLightsReal` public members, `RndLight` `GetColor()`/`GetType()`/`Range()`/
`WorldXfm()` (dir = m.y). Use `mLightsApprox` ONLY (no `ObjDirItr<RndLight>` — WASM hang).

Full per-item plans: `docs/sessions/native/roadmap-2026-06-02/A2_A3_A4_glow_diagnosis.md`.
Memory: `project-a234-emissive-glow-shared-rootcause`, `project-a4-scene-lighting-env-empty`,
`project-a1-hit-flame-fx-diagnosis`.

---

## A3 — HUD safe (non-glow) parts

**Both safe non-glow parts ALREADY WORK on native — no code change needed.** Probed in an
isolated worktree; the diagnosis's "A3 missing" premise was a **capture artifact** (a short,
low-score native capture compared against high-score retail screenshots):
- **Multiplier number** (`multiplier.lbl`): the feed fires (`StreakMeter::SetMultiplier`
  reaches 2/3/4), `UpdateMultiplierText` shows the label, and the strike-plate disc renders
  `3x`/`4x` in **both** the unmodified A1 baseline and the after captures — via the normal
  `BandLabel` text path, **not** the emissive/glow path. The `Reset` force-hide
  (`StreakMeter.cpp:161-177`) is correctly overridden by the live note-hit path once mult>1.
- **5-star scoreboard** (`BandStarDisplay`, `star0..star4`): all 5 stars instantiated, fed
  via `SetNumStars`, progressive reveal works (1 disc at score 1,870 → 3 filled + next-outline
  at 48,278, matching retail's progressive 4-filled+1-outline at 78,250). The "only 1 disc"
  was a <1-star low-score state, not a bug.

Both reviewers PASS; implementer left an **empty worktree diff** (no fake fix). The only A3
gap is the **cyan streak-meter / SP-overdrive glow ring** — same emissive/glow family as
A2/A4, **blocked on venue-env / scene-lighting** (parked).

**Learning:** verify progressive HUD elements at a HIGH game state (≥60s song-time, multiplier
>1, ≥1 star) — a short low-score capture makes working elements look "missing."

---

## Methodology learnings (carry forward)
- **Probe-first is non-negotiable.** Static analysis gave the wrong A1 answer once and would
  have shipped a regressive A4 white-flood; runtime `fprintf` probes (MILO_LOG is **swallowed**
  on native; `grep -a` the logs — a stray byte makes grep treat them as binary → false zeros)
  caught both.
- **Adversarial multi-agent verification + main-loop adjudication.** Implementer self-
  assessments were over-optimistic every time; independent reviewers caught real regressions —
  but a reviewer also once false-failed (mistook character geometry for a defect, traced the
  wrong/uncompiled source). **Always adjudicate verdicts by reading the actual frames + the
  actual compiled source.**
- **Captures aren't frame-locked** (songMs targets drift 44–58%); pixel A/B is unreliable.
  Add an on/off env toggle and capture the SAME frame both ways for real A/B.
- **Engine is a separate `milo-engine` cmake target.** `--target rb3-native` does NOT rebuild
  it; use `cmake --build native/build-native`. Worktree native builds need explicit
  `-DMILO_ENGINE_PATH=/home/free/code/milohax/milo-native-engine`.
- **Concurrency:** the rb3 + engine repos have multiple active agents; stage only your own
  files (`git add <paths>`, never `-A`); the engine pin (`native/CMakeLists.txt`) + hot files
  are contended — use `tools/setup-worktree.sh` to isolate.

## Open / next
1. **A3 HUD safe parts** — see A3 section (in flight / outcome above).
2. **Glow/lighting track (A2 + A4 + A3-glow)** — all gated on **venue-environ bring-up**, a
   deep V2 world-subsystem task (`Instance.cpp` inlined-proxy instancing). Either take it on
   as a dedicated effort, or do an interim synthesized track-light hack. Once the scene is
   dark: re-add the validated emissive + blowout clamp + bloom; then revisit A3 glow.
3. Minor: A1 flare color (pink vs retail blue/white) — per-asset tint check.
