# Lane RESKIN — R2 — STATUS — VERDICT: **REFUTED** (fix implemented, gate FAILED)

R2 implemented the R1-headline per-member hands reskin exactly per the R1 recipe
(flag-first, default-OFF `RB3_HANDS_RESKIN`). It builds clean, fires correctly, and
is Wii/flag-OFF byte-identical. **But the pre-registered quantitative `wext` gate
FAILS: flag-ON is a REGRESSION, not a fix.** R1's feasibility thesis (the hands
shard is a rest-shape / bind-basis problem fixable by a vertex re-pose) is REFUTED
by measurement. The per-member-reskin lever is now closed with evidence.

Kept in-tree default-OFF as the definitive measured dead-end (campaign precedent:
`RB3_APPENDAGE_REST_ROT` / `RB3_APPENDAGE_ASSET_REBAKE`), so the class is not
re-attempted. NO clamp / no workaround (STOP-TRIPWIRE respected).

Engine pin `3b5af48` (untouched — ZERO engine TU edits, as R1 predicted). Built +
run in the isolated `.claude/worktrees/RESKIN` worktree (main `build-native` was
blocked by Lane A's uncommitted `SongSelectPanel.cpp`, not my file). Source authored
in the main tree and synced verbatim to the worktree to build/measure.

---

## What was built (the implementation is correct; the thesis is what failed)

`BandCharacter::NativeReskinHandsAtRest()` (rb3 `BandCharacter.cpp`, HX_NATIVE),
wired from `SyncObjects()` right after `NativeCaptureRestPoseAfterDeform()` (skeleton
posed at the gender-bind rest, hand meshes' authored `mOffset`/bones still intact,
skinned-mesh cache freshly rebuilt, before the Poll-time rebake consumes the
offsets). Per the R1 KEY FINDING it does NOT route through `RndMeshDeform::Reskin`
(ExportWorldXfm returns the live pose only for `exo_` bones); it computes the LBS
blend directly, mirroring `MeshDeform.cpp:329-395`:

```
per bone b (hands_naked): A_b = mesh->BoneOffsetAt(b) (authored invBind, intact)
                          own_b = Find(BoneTransAt(b)->Name())  (gender-posed bone)
                          T_b = A_b . NativeCharSpaceRestXfm(own_b)  (CHAR space, RISK-1)
per vert v:  weighted = (Σ_i w_i . T_{v.bone_i}) / Σ_i w_i   (v's own 4-bone weights)
             v.pos = v.pos . weighted ; rotate+renorm v.norm
mesh->Sync(0x1f) ; latch by std::set<RndMesh*> (RISK-2)
```

Fires exactly as R1 predicted (`RB3_RESKIN_PROBE` → `[RESKIN_APPLY]`):
`player0` male 1876v/38 bones (all 38 resolved, 1876 mutated), `player1` **female**
1256v/40 bones (all 40 resolved, 1256 mutated), `player2/3` male 1876v. Per-member
meshes distinct + self-owned (mutate in place, zero extra memory). The gender-distinct
mechanism gate (A8-i) PASSES; the offset-provenance (A8-ii) and no-matching-TU-edit
(A8-iii) constraints hold. **But the fix does not work.**

## The gate that fails — `wext` (IK_SHARD_VERT, UNEDITED engine probe)

A/B on the unedited `IK_SHARD_VERT` engine probe (`Rnd_Wgpu_RB3.cpp:4394-4403`,
logs the drawn skinned world-extent when `wext > 60u`), gameplay burst, ~1000
hands_naked draws/arm, `RB3_FIXED_CLOCK=1`:

| arm | hands_naked wext | mean | verdict |
|---|---|---|---|
| **flag-OFF** (default rebake) | 60–106u, 39 distinct (animating) | **74.8** | baseline shard |
| **flag-ON** (reskin + rebake) | 60–105u | **87.7** | **REGRESSION** |
| flag-ON, `RB3_NO_HEAD_REBIND` | 75–187u | 136 | much worse (isolation) |

Gate target was 95-106u → **≤60u**. Flag-ON does not approach ≤60; it **rises**.
Probe verified live (crowd/clap meshes still fire ~7000 lines/run), so the silence
is not a broken measurement — hands_naked genuinely stays >60u flag-ON.

E1 band screenshots corroborate (both genders on screen, same fixed-clock frame):
`evidence/e1/band_flagOFF_burst08.png` vs `band_flagON_burst08.png` (+`_burst10`) —
flag-ON shows **larger** dark hand shards on the female singer, the guitarist, and
the drummer.

## Root cause — why a vertex re-pose CANNOT fix this (refutes R1)

The drawn shard is an **animation-basis** problem, not a rest-shape one. With the
default-ON rebake, `BoneOffset(b) = meshWorld·inv(own_rest_b)`, so at rest the drawn
shape is exactly the (re-posed) verts — coherent, no explosion. But during
animation, `skinPos(t) = v'·meshWorld·inv(own_rest_b)·own_live_b(t)`: the delta
`own_live` vs `own_rest` (a rotation-basis divergence) is **IDENTICAL** flag-ON vs
flag-OFF by construction. The reskin only changes `v` → `v'` (the rest shape). Since
the re-posed `v'` sits at a **larger radius** from its bones than the authored `v`,
the `R·sin(θ)` far-vert smear is **amplified** — hence 74.8 → 87.7u.

This is the same dead-end class as the already-refuted `RB3_APPENDAGE_REST_ROT`
(world-rest lever) and `RB3_APPENDAGE_ASSET_REBAKE` (freeze), and confirms the
standing finding at `BandCharacter.cpp:~1273`: "the char-space rebake ALREADY
applies to hands_naked yet leaves a real shard … requiring the authored per-member
bind pose from `skeleton_unshared.milo` (asset data)." **The genuine fix is
asset/skeleton-basis (correct the animation basis), not any vert/offset bake.**

## Gates (R2)

- **wext gate: FAIL** (74.8 → 87.7u; target ≤60u). Decisive; three arms agree.
- flag-OFF byte-identical / Wii byte-identical: **PASS** (single early-return
  `if(!sReskin)return;` at the top; whole function + call under `#ifdef HX_NATIVE`).
- No edits to the wext/instrument TUs (A8-iii): **PASS** (only `BandCharacter.cpp/.h`
  edited + the engine `classification.json` flag registration; the `IK_SHARD_VERT`
  probe TU `Rnd_Wgpu_RB3.cpp` untouched).
- gender-distinct output (A8-i): **PASS** (female 40 bones vs male 38, distinct
  meshes/vert counts, both fully reskinned). Moot given the overall FAIL.
- STOP-TRIPWIRE: **respected** — no offset-bake variant, no clamp/freeze.

## Disposition

Kept in-tree, **default-OFF, do-NOT-flip**, with the measured refutation in the
function header (`NativeReskinHandsAtRest`), the header decl, the call-site comment,
and the engine `classification.json` entry (`RB3_HANDS_RESKIN`, class workaround,
not-live REFUTED). This closes the "per-member reskin" lever the offset-bake class
exhaustion had pointed at — the next lever is the asset/skeleton animation-basis
(skeleton_unshared.milo per-member bind), out of scope for a vert/offset fix.

## Evidence
- `evidence/reskin-probe-gameplay.log` — R1 `RB3_RESKIN_PROBE` authored-offset dump.
- `evidence/e1/band_flag{OFF,ON}_burst{08,10}.png` — before/after band (both genders).
- wext A/B logs regenerable:
  `RB3_HANDS_RESKIN=1 IK_SHARD_VERT='*' RB3_FIXED_CLOCK=1 python3
  scripts/native/keyboard-to-gameplay.py --bin
  .claude/worktrees/RESKIN/native/build-native/rb3-native --song-downs 3
  --game-burst 15 --burst-interval 0.3` (drop `RB3_HANDS_RESKIN=1` for the OFF arm);
  grep `IK_SHARD_VERT.*hands_naked` in the harness's engine log for `wext=`.
- Source: `BandCharacter.cpp` `NativeReskinHandsAtRest` + `SyncObjects` call site;
  `MeshDeform.cpp:298-399` (mirrored blend); `Rnd_Wgpu_RB3.cpp:4394-4403` (wext probe,
  UNEDITED); rebake distinct-branch `BandCharacter.cpp:1750-1757`.
- Checkpoint: `/tmp/wave14-checkpoints/R2.json`.
