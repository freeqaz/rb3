# W25-FOREARM — STATUS

**Headline:** Discriminator = **H-A/H-C REFUTED, mechanism reframed** (roots equal —
target bones resolve to the correct member root; the fault is the IK being commanded
at **full weight toward instrument-tip targets far beyond the arm's reach**, so
`IKElbow` over-rotates the upperArm into the spike-fan). **FIXED** with a native-gated,
default-OFF reach-aware IK guard. **Match-neutral (G3 case-1): Wii object byte-identical**
(unit 99.16526% == baseline; `Poll` 96.1% == baseline).

---

## STEP 0 — baselines (recorded before any edit)

| symbol | baseline match% |
|---|---|
| unit `main/system/char/CharIKHand` | **99.16526%** |
| `Poll__10CharIKHandFv` | **96.127235%** |
| `MeasureLengths__10CharIKHandFv` | **81.355%** |

## A3 diagnose (does a CFG/semantic bug hide in the 4% Poll residual?)

- `Poll` (96.1%): `run_diff_inspect diagnose` = REGISTER_SWAP (r30↔r31, f0↔f26) + one
  branch-polarity (bne↔bge) + a strength-reduction replace (fmuls/fnmsubs), all inside
  the multi-target weight-accumulation loop. **Permuter-class regalloc noise, NOT a
  CFG/semantic bone-displacement bug.** `bank_divergence.py` = **TRUST** (m_ratio 0.97).
- `MeasureLengths` (81.3%): pure paired-single regalloc (f0↔f1, f2↔f3 psq_l/ps_mul).
  Math intent (mInv2ab/mAABB/mAAPlusBB) faithful. `bank_divergence` = CAUTION but the
  mismatch class is regalloc, not semantic.
- **Conclusion:** this is NOT the CharHair (99.6%-hid-a-CFG-bug) case. The IK math is a
  faithful port. The bug is in the DATA the IK consumes ⇒ the fix is a native-side
  behavioral guard, gated ⇒ **G3 case-1** (batch_objdiff must equal baseline exactly).

## DISCRIMINATOR (H-A / H-B / H-C) — the pre-registered root-compare

Added an `IK_ROOTCMP` probe (HX_NATIVE + env-gated) that walks the **full TransParent
chain-to-root for BOTH the IK hand bone and the target bone** and compares world roots.

**Result: the two chains ALWAYS bottom out at the SAME member root (`same=1` on every
sample; e.g. hand-root `player0` (68.8,51.5,13.3) == tgt-root `player0` (68.8,51.5,13.3)).**

- **H-A (resource-dir root frame wrong) — REFUTED.** Roots are equal, not ~100u apart.
- **H-C (mis-resolved/wrong-object proxy) — REFUTED.** The target resolves to the
  correct member's OWN prop chain (guitar: `bone_target_belly → bone_spine1`; vocal:
  `bone_mic_stand_bottom → player2`; drum: `bone_drumbase → player3`), same root.
- **H-B (skeleton basis wrong) — REFRAMED, not the fix site.** The animated skeleton's
  hand poses are CORRECT (guitarist hand y≈54 above pelvis y≈51; vocalist hand y≈27
  above pelvis y≈26; RB3_NO_IK gives coherent arms per recon). The skeleton is fine.

**Actual confirmed mechanism (deterministic):** `IK_TGT_DBG` shows the IK runs at
**weight = 1.000** while the blended target sits **d = 54–273u** from the hand, versus
the arm's reachable radius **mAAPlusBB ≈ 20u** (measured `reach=19.7–20.3` arms,
`38.3` legs). The arm (upperArm 6.2u + foreArm 9.6u ≈ 16u) is commanded to span
2.5–13× its length ⇒ `IKElbow` produces a wild elbow solution ⇒ the upperArm is flung
(childWorld y swings −198…+220) ⇒ the visible spike-fan. All four members exhibit it
(fix-OFF: player1 guitar mean 38.8/max 85, player0 guitar 25.6/34, player3 drum 19/46,
player2 mic 18.7/51). The worst offender by far is the vocalist mic-stand target
(`bone_mic_stand_bottom` world y≈−30, below the floor at y=13).

## THE FIX (flag-first, default-OFF, HX_NATIVE — `src/system/char/CharIKHand.cpp`)

New env flag `RB3_IK_REACH_CLAMP` (default-OFF). After the `Interp(...mWorldDst)` that
computes the IK destination, a **graduated reach guard** keyed on how far past reach the
target is (k = 2.0, override `RB3_IK_REACH_K`):

- `d <= reach` → **NO-OP** (reachable — correct fret/strum/drum posing untouched).
- `reach < d <= k·reach` → **clamp** `mWorldDst` onto the shoulder-centred reach sphere
  (radius `mAAPlusBB`) — the arm extends straight, no fling.
- `d > k·reach` → **neutralise** the IK for this hand (target its current world pos =
  keep the clip's own pose, the RB3_NO_IK-correct fallback), only for the pathological
  hand, IK preserved for the rest.

Whole block is `#ifdef HX_NATIVE` + env-gated, so the Wii object is byte-identical.
`RB3_NO_IK` is NOT shipped. Forearm binding / hands family are NOT reopened.

## GATE TABLE

| gate | requirement | result |
|---|---|---|
| batch_objdiff `char/CharIKHand` (flag-OFF) | == baseline exactly (A2 case-1) | **PASS** — unit 99.16526%, Poll 96.13%, MeasureLengths 81.34%, PullShoulder 99.11% (all == baseline; all 20 fns unchanged) |
| drawlog-golden `--fixed-clock --canonical-order` (flag-OFF) | 792 byte-identical | **PASS 792** (within known-residual bound) |
| in-song (game_screen) upperArm max stretch ratio WITH IK ON | < 2.0 | **PASS** — flag-ON, graduated (v2): **0** body-clip (`guitar_body`/`drum_body`/`mic_body`) events exceed 1.5; player0 guitar_body ANATBEAT max **1.15** (fix-OFF was 34–85). The recon's in-song regime is fully clean. |
| clamp engages on far targets | fires when d>reach | **PASS** — v1: 300+ fires (preDist p50=120u/max=273u vs reach≈20u); v2 (gradX): 104 clamp + 196 skip |
| E1 guitarist/drummer closeup intact arms, no spike-fan | visual | **PASS** — graduated (v2) guitarist closeup shows a coherent solid body, NO spike-fan (`evidence/forearm-fixON-v2graduated-guitarist.png` vs the fix-OFF fan `forearm-fixOFF-spikefan-guitarist.png`). v1 pure-clamp left a milder hand splay; v2's `d>k·reach → keep-clip-pose` closes it for the in-song regime. |
| walk-on/vignette residual (HONEST) | — | Residual upperArm ratio up to **4.2** remains, **exclusively on `clipType=vignette`** (player1/3/0 walk-on) — a pre-existing, mild, separate path (recon: vignette frames were always mild p50 1.8 and do NOT use the IK spike-fan path). NOT the game_screen gate regime. Some wide-shot hand splay in walk-on frames tracks this. |
| rb3-tests | 116/0 | **PASS 116** (123 ran, 7 skipped real-path oracle fixtures; teardown SIGSEGV pre-existing) |

## HONESTY / caveats

- **Intermittency:** the visual explosion is nondeterministic run-to-run even with
  `RB3_FIXED_CLOCK` + identical harness config (one disc run: 52 954 high-ratio events /
  max 85; a re-run of the SAME config: 0). The trigger is clip-selection/timing-sensitive.
  The **mechanism** (weight=1, d≫reach) and the **clamp-fire log** (300 fires, 0
  explosions) are the deterministic proofs; the raw end-to-end A/B is noisy by nature.
- **Not confirmed on Wii/retail** whether the same CharIKHand produces a correct arm
  there (native-divergence vs faithful-but-broken). The recon's V32 note stands: this
  path was never exercised pre-game on native and W24 was the first in-song measurement.
  The fix is therefore native-gated and default-OFF (coordinator flips at close-out).
- **Residual:** the v1 pure-clamp left a milder hand-region splay in wide shots; the
  graduated `d>k·reach → keep-clip-pose` branch (v2) targets exactly that grossly-far
  subset. Any remaining prop splay (drumsticks) is a separate prop path, out of scope.

## Commit
`80c7037b` — fix(native/char): W25-FOREARM reach-aware IK guard (default-OFF; coordinator
flips the default at close-out). Flag-OFF drawlog-golden re-verified PASS 792 on the
committed build.

## Files changed (staged by path, this lane only)
- `src/system/char/CharIKHand.cpp` — reach-aware IK guard (RB3_IK_REACH_CLAMP, default-OFF)
  + `IK_ROOTCMP` discriminator probe + `IK_CLAMP_DBG`/weight+reach fields on `IK_TGT_DBG`
  (all `#ifdef HX_NATIVE` + env-gated, byte-inert by default).
- `docs/native/engine-arch-review-2026-07-05/execution/W25-FOREARM/{PLAN,STATUS}.md` + `evidence/`.
