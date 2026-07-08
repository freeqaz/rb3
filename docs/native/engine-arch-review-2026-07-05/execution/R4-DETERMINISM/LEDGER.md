# R4 — Loader-determinism per-axis PASS/FAIL ledger

The standing artifact that makes a "partial landing" impossible to misread as "landed" (the
W4→W11 failure the campaign is built to prevent). Four gating axes + one informational axis,
each boot graded against a reference boot. Produced by
`scripts/native/loaddet_gate.py --ledger`; machine copy: `evidence/M3-ledger.json`.

> **Axes v2 (close-out review F5):** the axis semantics below were revised from PLAN-R4 §3.4
> mid-run (disclosed in STATUS; PRIMARY definition untouched, fail-red reproduced under v2).
> Cite THIS file, not the PLAN, for axis definitions.

## Run: M3 gate at scale — `--arm both --n 10 --k 300 --jitter 200 --ledger`
Binary `native/build-agent-R4/rb3-native`. Input-free post-anchor drive under
`RB3_LOADDET_JITTER=200`. Seam ON = `RB3_FIXED_CLOCK=1 RB3_LOAD_DETERMINISM=1`.

| Axis | Definition (PASS = equal to reference boot) | ON-arm result |
|---|---|---|
| **stream** (PRIMARY) | `postAnchorDelta` (gdraw@anchor+k − gdraw@anchor) identical | **10/10 — PASS** (all 0) |
| count | post-anchor per-frame gRand draw-delta sequence md5 + cumulative post-anchor delta (anchor-relative; pre-anchor absolute gdraw is non-gating by design) | 10/10 |
| order | DirLoader/DataLoader completion SEQUENCE (kind:name) md5 identical | 10/10 |
| clock | fixed clock active + post-anchor span == k frames (constant dt; absolute anchor frame varies and is reported, non-gating) | 10/10 |
| callerOrder *(info, non-gating)* | post-anchor attrib caller (frame,offset) sequence md5 | 1/10 |

**PRIMARY: PASS.** OFF-arm fail-red under identical jitter: `postAnchorDelta` spread **7179**
(distinct 10/10: 32679…39858) — the divergence the seam removes. absSpread ON 3 vs OFF 10104.

### Reading the axes
- The four **gating** axes all pass 10/10: the shared gRand stream position, its per-frame draw
  count, the loader completion order, and the fixed-clock capture span are all boot-invariant
  under the seam.
- **`callerOrder` 1/10 is expected and non-gating.** After isolation the particle/eye consumers
  still draw — on their *private* streams — and *when* each particle spawns within a frame is
  timing-dependent, so the per-frame caller distribution varies boot-to-boot. That variance no
  longer reaches gRand (stream = 10/10), which is the whole point of isolation rather than
  order-forcing. The ledger surfaces this cleanly: had the fix instead tried to force caller
  order, this axis would gate; it does not, because the design decoupled count from order.
- Absolute anchor frames vary 2586–2863 (~4.6 s of pre-anchor load-timing jitter). The seam
  re-bases AT the anchor, so this is exactly the axis it is not meant to fix; the PRIMARY metric
  is measured anchor-relative and is invariant regardless.

## Inertness (gates G2/G3)
- **G2** flag-OFF runtime inertness: normal boot (no `RB3_LOADDET_*`) emits **0 `[LOADDET]`**
  lines (`evidence/G2a-normal-boot.txt`); flag-OFF splash_screen drawlog matches the committed
  golden, **792 draws** (`evidence/G2b-drawlog.txt`). PASS.
- **G3** Wii-match inertness: `batch_objdiff` on every touched unit == report.json baseline;
  `RandomInt`/`RandomFloat`/`CreateParticles` stay exactly 100% (`evidence/G3-match-inertness.md`).
  PASS.

## Disposition
`RB3_LOAD_DETERMINISM` remains opt-in (a fixed-clock determinism *harness* seam, per the A3
precedent). PRIMARY now resolves — the classjson PARTIAL/DO-NOT-flip → PASS-PRIMARY flip is
**coordinator close-out work**, not this lane's, and is gated on the flag-ON gameplay-scene
drawlog re-golden noted in plan §M3 (the seam changes the gRand stream by construction; splash
goldens are unaffected — reseed never fires pre-gameplay).
