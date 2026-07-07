# G1 — Golden compat (prov compiled-but-off)

## GREEN condition
`drawlog-golden.py --fixed-clock --canonical-order --scene splash_screen` PASS (792)
with prov code compiled in, RB3_DRAWLOG=1, RB3_DRAWLOG_PROV UNSET.

## Result: GREEN (prov-off byte-transparent)
- Golden PASSES on quiet runs (792, canonical): see g1_golden_prov_off.txt.
- Direct byte-diff of my build (prov compiled, OFF) vs pristine build-native at a
  matched 792-draw capture (g1_byte_diff_mine_vs_baseline.txt): the differing lines
  fall into EXACTLY the two PRE-EXISTING known-residual classes the canonical
  comparator already normalizes, both present baseline-to-baseline:
    1. opaque `"scene"` bind-group dense-id (e.g. 2 vs 0) — run-nondeterministic
       heap-handle first-seen ordering.
    2. `world[16]` of ~26 animated/character draws (translations ~-287/547/643 =
       venue+character positions) — timing-dependent float accumulation.
  idx/tris/verts, blend/zmode/layout/fmt/flags, name-hash, pipeline: ALL
  byte-identical. Static UI/menu draws' world[16]: byte-identical. Neither diff
  class is touched by prov (world comes from ctx.world; scene-token from the
  existing ring). => prov-off introduces ZERO new divergence class.

## Environmental flakiness (NOT caused by prov)
Under concurrent multi-agent load the golden harness is flaky for BOTH my build
AND the pristine build-native (baseline hit 200867 divergences on 2/3 runs; mine
1 PASS + smaller failures). This is the pre-existing ASLR/timing nondeterminism
the harness documents (W0.3.STATUS S1), shared by baseline, not a prov regression.
Structural proof: every prov code path is gated behind ProvOn()
(RB3_DRAWLOG_PROV), UNSET in the golden run.

## Fail-red (comparator is not vacuous)
The comparator FLAGGED real drift as "unexpected" (non-residual): a 789-draw
capture -> "1 unexpected divergence vs golden (0 known-residual tolerated):
count golden=792 candidate=789". So the comparator distinguishes the tolerated
opaque-token/animated-world reorder from a genuine count/content change.
