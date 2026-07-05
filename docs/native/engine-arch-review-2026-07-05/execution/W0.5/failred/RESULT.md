# W0.5.S4 — Fail-red proof (REFACTOR_PLAN exit-gate #2)

Broken-skin env used to induce shards on the current build:

```
RB3_NO_SKEL_REBIND=1 SHARD_GUARD_OFF=1 SHARD_RATIO_DBG=1 RB3_NO_SKIN_CLAMP=1
```

Exploded WIDE frame (committed evidence): `exploded_cand_coop_g_b_0.png`

Compared against golden: `coop_g_b_0.png` (`scripts/native/goldens/w0.5-lineup/`)

## Verdict summary

| Layer | Tool | Verdict | Evidence |
|---|---|---|---|
| OLD gate metric | `band-closeup-capture.py` | **PASS** | `drops_band=0` `drops_total=0` `max_band_ratio=4.96` (its own cap; committed evidence: `old-gate-verdict.json`) |
| Perceptual image layer | `visual_diff.py --perceptual` | **PASS** | score=81.24 vs min_score=35.0 (`image-layer-verdict.json`) |
| NEW gate (numeric, composite) | `lineup-gate.py` | **FAIL** | layers={'image_advisory': 'PASS', 'segA': 'PASS', 'ratioB': 'FAIL', 'countC': 'PASS', 'pin': 'PASS'} (`new-gate-verdict.json`) |

## What failed in the NEW gate

`ratioB` (per-mesh world-extent-ratio layer, layer B) FAILed: `n_offenders=5` mesh(es) exceeded `per_mesh_ratio_cap=8.0` (golden-derived bound):

| mesh | ratio | class | cap |
|---|---|---|---|
| `male_extra_head01.mesh` | 9.43 | other | 8.0 |
| `male_extra_head03.mesh` | 11.42 | other | 8.0 |
| `male_extras_head11.mesh` | 11.22 | other | 8.0 |
| `male_extras_eyebrows11.mesh` | 41.87 | other | 8.0 |
| `goatee_resource.mesh` | 17.04 | other | 8.0 |

Meanwhile `max_band_ratio=4.39` stayed under its own separate bound (8.0) — the offending meshes are classified `other` (crowd/extras patch geometry, not the `band`-tagged skinned meshes the OLD gate's `drops_band` counter tracks), which is exactly the blindness class W0.5 targets: a shard explosion the OLD gate's `drops_band==0` metric structurally cannot see, but the NEW per-mesh ratio layer catches because it checks every mesh, not just band-classified drops.

## Conclusion

The NEW composite gate (`lineup-gate.py`) FAILs a numeric layer (`ratioB`) on a build with induced skin-shard corruption, while the OLD band-closeup gate metric PASSes (`drops_band=0`) and the perceptual image-compare layer PASSes (score 81.24 >= min 35.0). This is the literal REFACTOR_PLAN exit-gate #2 evidence: the new gate is non-blind to a shard class the old gate and the image layer both miss.

## Reproduce

```
scripts/native/w0.5-failred.sh --bin native/build-agent-W0.5/rb3-native
```
