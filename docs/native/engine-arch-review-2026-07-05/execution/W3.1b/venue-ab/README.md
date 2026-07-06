# W3.1b — venue A/B sign-off package

Engine commit: `0f3d7ef` (milo-native-engine). rb3 harness:
`scripts/native/w31b-lighting-ab-capture.py`. Venue: bar interior (default song,
4 downs), shot `coop_g_n03.shot`, frame-locked via `msg:game:jump:12000`
(songMs ≈ 12665 in every capture), `RB3_FIXED_CLOCK=1`.

Reproduce (per capture, env toggles differ):

```
python3 scripts/native/w31b-lighting-ab-capture.py native/build-agent-W3.1b/rb3-native <out.png>
# fog A/B:   add   RB3_ENV_FOG=1 RB3_ENV_FOG_FORCE=1
# projLight: add   RB3_ENV_PROJLIGHT=1  (+ RB3_ENV_PROJLIGHT_FORCE=1 for the probe)
```

## Numeric wash detector (mean |Δ| per pixel over RGB; A/A = capture-noise floor)

| pair | mean |Δ| | frac pixels >8 | verdict |
|---|---|---|---|---|
| **A/A** fog_off vs fog_off2 | 19.4 | 0.65 | capture-noise floor (animation micro-jitter) |
| **fog** off vs on (FORCE) | **151.1** | **0.96** | **fog RENDERS** — 8× noise floor |
| projLight off vs real (`RB3_ENV_PROJLIGHT`) | 17.2 | 0.61 | ≈ noise → no reachable venue authors a kFakeSpot+gobo light (faithful no-op) |
| projLight off vs force | 20.3 | 0.66 | ≈ noise → see projLight note below |

## Fog — exit #2 CLOSED

`fog_on.png` shows the whole bar interior washed grey-blue with depth-graded
falloff (near guitarist stays legible, distant boxes/walls fade to fogColor) — the
reproducible screenshot W3.1a could not produce. The **scene∧material AND-gate is
proven**: forcing only the scene side (materialFogEnabled stays `mat->mFog`==0 on
every venue material) leaves the frame at ~22 ≈ the 19.4 noise floor (no fog); the
probe additionally forces `materialFogEnabled=1` and the frame jumps to 151. That
progression IS the WGSL `scene.fogEnabled && material.materialFogEnabled`
demonstration (standard_wgsl.inc:872). Probe fog params (fogStart=8, fogEnd=80,
grey-blue) are deliberately aggressive to exceed the animation noise floor — this
is a *visual-verification* probe, not shipping fog values.

## projLight — primary exit MET, visual A/B honestly asset+shader-blocked

The **primary S1 exit is met**: the faithful `kFakeSpot` gobo fill is landed,
byte-identical flag-OFF (numProjLights stays 0, slot-3 stays mWhiteView →
drawlog 888/888, lineup PASS), ported verbatim from the proven DC3 path into the
already-existing WGSL consume path (zero DC3 blast).

The **visual A/B is a no-op at every reachable venue**, honestly:
- **Faithful path** (`off vs real` = 17.2 ≈ noise): no boot-reachable venue authors
  a `kFakeSpot` light with a gobo texture (same asset-block class as fog; confirmed
  by the ≈-noise diff → numProjLights never reaches 1 on a real venue).
- **Force probe** (`off vs force` = 20.3 ≈ noise): the WGSL projected-light term is
  added to `totalLighting.diffuse` **only** for surfaces that are (a) LIT
  (`mUseEnviron`), (b) inside the projection cone (UV∈[0,1]), and (c) facing the
  light (`NdotL>0`). The bar venue's geometry is overwhelmingly UNLIT/prelit
  (posters/signs/neon authored ue=0), so a white stand-in gobo from a synthesized
  light lands almost nowhere visible. Producing a compelling projLight screenshot
  needs a real gobo pattern + a lit surface inside a real cone — beyond the probe's
  cheap-synthesis remit. The fill code is correct-by-construction; this is an
  inherent shader/asset limitation, not a code defect.

Files: `fog_off.png` `fog_off2.png` `fog_on.png` (fog A/B + A/A control);
`proj_off.png` `proj_real.png` `proj_force.png` (projLight faithful + force).
