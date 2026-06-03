# Session 2026-06-03 — P4 venue lighting: per-environ rewrite, shipped DEFAULT-ON

Follows `SESSION_2026-06-02_track-A-polish.md`, which shipped P4 (venue point
lights) **opt-in / default-off** because it "read washed/desaturated-grey." This
session took P4 to **default-on** by finding the real architecture bug behind the
wash, fixing it, and verifying broadly. Engine `8528923`, rb3 pin bumped to match.

## TL;DR

| Item | Before | After |
|---|---|---|
| **P4 venue lighting** | opt-in `RB3_VENUE_LIGHT=1`, washed grey | ✅ **default-on**, moody coloured stage-lit venue; opt out `RB3_VENUE_LIGHT_OFF=1` |

The "wash" was **not** white stage lights (the prior doc's guess). It was two
bugs: (1) the scene uniforms were only re-written on **camera** change, so the
whole venue was lit by ONE environ (whichever was current at the last camera
write) — the venue actually scopes **~20 RndEnvirons to mesh-groups per frame**;
(2) my grey fallback key directional fired whenever an environ had no
*directional*, washing the coloured-point-only environs (the theater stage spots)
grey.

## What the venue really contains (RB3_VENUE_PROBE — ground truth)

The default quickplay venue is a rich multi-area urban scene. Per-environ lights
(type 0 = point, 1 = directional), e.g.:
- `theater.env` — **red** (1.19,0,0) r700, **cyan** (0,.62,.88) r1500, **purple**
  (.40,0,.67) r1000, **amber** (.90,.57,0) r800 — the coloured stage spots
- `chars.env` (the band) — grey `rim.lit` directional + 4 grey `*_silhouette.lit`
  points (1.58, r~38) → intentional silhouette/backlight
- `geom.env` (interior) — one local warm-yellow point (1,1,.5, **r30**) + ~black ambient
- `crowd.env` — pink point; `street_slomo_geom.env` — 5 coloured street points;
  `cityscape`/`rooftop_foreground` — warm orange spots; `sky.env` — bluish ambient, 0 lights
- buildings (`back_left`/`buildings_dim`) — ~black ambient, no lights → dark

So the colour is all there; it just wasn't reaching the right meshes.

## The fix (engine `8528923`, `src/platform/Rnd_Wgpu_RB3.{cpp,h}`)

1. **Per-environ scene-uniform rewrite.** In `DrawMesh`, after the existing
   "camera changed → re-write" block, an `else if`: when venue lighting is on and
   the cam is `world.cam` and `RndEnviron::sCurrent != mLastSceneEnv`, re-write the
   scene uniforms + `SetBindGroup` + update `mLastSceneEnv` (new `void*` member).
   Each mesh-group now gets its OWN environ's lights. Mirrors the existing
   per-camera rewrite. Gated to `world.cam` → game.cam + menu cams byte-identical.
2. **Fallback only when fully unlit.** `if (dl == 0)` → `if (dl == 0 && pl == 0)`,
   so coloured-point-only environs keep their colour instead of a grey key.
3. **Default-on gate.** New `sVenueLightEnabled()` reads `RB3_VENUE_LIGHT_OFF`
   (default-on), used by both the read (WriteSceneUniforms) and the rewrite (DrawMesh).
4. **Ring buffer 16KB → 64KB** (`InitGpuResources`). See the bug below.

## The latent bug the adversarial review caught (probe-CONFIRMED)

A code-review agent flagged: the scene-uniform ring is 16KB = ~21 768B-aligned
slots; it wraps to offset 0 at capacity; each bind group pins a fixed offset; all
`queue.WriteBuffer`s land before submit. So if a frame does **>21** scene writes,
a wrapped write clobbers an earlier draw's still-referenced offset → silently
wrong lighting (not a crash). The per-environ rewrite adds writes.

Instead of trusting the static estimate (my own "~20 envs" comment came from a
**global-dedup probe across all boot scenes**, not one frame), I instrumented
`RB3_RING_PROBE` and measured: typical frames do **3–15** scene writes (safe), but
busy establishing-shot frames hit **24** → 3 intra-frame collisions each. **Real
bug.** My captures looked fine because the corruption is subtle in a moody scene.
Fix: 64KB = ~85 slots; 24 ≪ 85 ⇒ every frame's offsets are distinct with margin.
(Probe-first here *confirmed* static analysis and sized the fix — the inverse of
the usual "probe refutes the reviewer" outcome.)

## Verification (main-loop image adjudication + 2 adversarial reviewers)

- **Gameplay**: clear win. Same-angle A/B (2600ms establishing): OFF = flat evenly-
  lit showroom; ON = dark venue with magenta/purple/warm stage wash, band
  silhouetted, **highway/gems/HUD pixel-identical** (game.cam untouched). Matches
  retail's dark-backdrop-highway-pops aesthetic (`yt_qRagnZCIMzk_gameplay_guitar.png`).
- **Song-select**: unchanged (UI over an already-dark world.cam bg).
- **Menu hub**: moodier/less-washed character — closer to retail's dark-neon hub.
- Across 5+ gameplay angles + SP shots: nothing pure-black/white, band always
  visible, gems always crisp.

## Open / follow-ups (unchanged or new)

- 🟡 **Uneven across environs** (reviewer note, non-blocking): some envs (a
  wood-panel interior in late shots) still read bright/flat. Per-environ exposure
  tuning is a future polish, not a regression.
- ⛔ **P1 gem bloom-halo** — still deferred (engine branch `trackA-bloom`
  `332dfba5`); needs the additive-halo-only redesign. Unchanged this session.

## Method (subagent / workflow strategy)

Isolated **engine worktree** under `~/tmp` (RAM-safe; NOT /tmp) on branch
`p4-venue-tune` + an isolated native build dir pointing `MILO_ENGINE_PATH` at it;
main-loop owned all builds + runtime probes + image judgment (avoids build-dir
contention, keeps image-adjudication central). Two parallel read-only Opus review
agents before landing — **code-correctness/safety/perf** (caught the ring bug) and
**visual-regression/parity** (confirmed ship). Land = commit on the worktree
branch → ff engine main → bump rb3 `MILO_ENGINE_PIN`. Probe-first beat/confirmed
static analysis throughout (the "white lights" wash theory was wrong; the ring
overflow was right — measured, not assumed).
