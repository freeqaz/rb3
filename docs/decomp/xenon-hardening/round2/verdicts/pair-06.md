# Pair 06 verdict — Wii `Poll__7TrackerFf` (0x801eeae0) ↔ Xenon 0x826b1f50

**Verdict: CORRECT — confidence HIGH.**

- Match type: BSIM, sim×conf 40.282 (BSim similarity 1.0 / confidence 40.282).
- Wii TU: `Tracker.o`; demangled `Tracker::Poll(float)`.
- Size ratio: Wii 0xA0 = 160 B vs Xenon 176 B → 1.1x (well inside expected 1.0-2.5x).

## Decisive evidence: callee-graph + control-flow skeleton are isomorphic

Both bodies are a single guarded block followed by one unconditional virtual call.
Step-by-step the two sides line up one-to-one, in the same order:

| step | Wii (asm @0x801eeae0) | Xenon (pseudo-C @0x826b1f50) | agree? |
|---|---|---|---|
| guard | `lbz r0,0x4(r3); cmpwi 0; beq` | `if (*(char*)(param_2+1) != 0)` (byte @ +0x4) | ✓ same field offset 0x4 |
| 1 | `li r4,-1; bl ReachedTargetLevel__7TrackerFi` | `Function_826B1B48(param_2, 0xffff…ffff)` | ✓ same `-1` arg; callee 0x826b1b48 is in the Tracker TU addr band (0x826b1xxx, same TU as SetupDisplays 0x826b1c78) |
| 2 | virtual call `lwz r12,0x0; lwz r12,0x38(r12); bctrl` passing f31 | `(**(code**)(*param_2 + 0x30))(param_1, param_2)` | ✓ virtual call; slot 0x38 vs 0x30 (uniform −8) |
| 3 | `stb 0,0x4(r31)` | `*(param_2+1) = 0` | ✓ same field offset 0x4 |
| 4 | `bl GetTrackPanel; lbz r4,0x29; bl SetSuppressTambourineDisplay` | `FUN_82b5e878(); Function_82B5FE00(_, byte@0x2d)` | ✓ both callees resolve (0x82b5e878=GetTrackPanel, 0x82b5fe00=SetSuppressTambourineDisplay) |
| 5 | `bl GetTrackPanel; lbz r4,0x2a; bl SetSuppressPlayerFeedback` | `FUN_82b5e878(); Function_82B5FE98(_, byte@0x2e)` | ✓ 0x82b5fe98=SetSuppressPlayerFeedback |
| 6 | `bl SetupDisplays__7TrackerFv` | `Function_826B1C78(param_2)` | ✓ 0x826b1c78=SetupDisplays |
| post | virtual call `lwz r12,0x3c(r12); bctrl` passing f31 | `(**(code**)(*param_2 + 0x34))(param_1, param_2)` | ✓ slot 0x3c vs 0x34 (uniform −8, consistent with step 2) |

Four of the five Xenon callees resolve through matches.json to the **exact named
Wii callees the Wii body calls, in the exact same positions**:
- `SetupDisplays__7TrackerFv` (0x826b1c78)
- `SetSuppressTambourineDisplay__10TrackPanelFb` (0x82b5fe00)
- `SetSuppressPlayerFeedback__10TrackPanelFb` (0x82b5fe98)
- `GetTrackPanel__Fv` (0x82b5e878), called **twice**, once before each suppress call.

The fifth (0x826b1b48 = ReachedTargetLevel) is name-unresolved but sits in the same
Tracker TU address band and is called with the same distinctive `-1` argument.

## Why the differences are expected (not disagreements)

- **Byte-field offsets** 0x29/0x2A (Wii) vs 0x2d/0x2e (Xenon): different MWCC vs MSVC
  struct layout — explicitly a non-signal per substrate caveat 1.
- **Vtable slots** 0x38/0x3C (Wii) vs 0x30/0x34 (Xenon): a *uniform* −8 (2-slot) shift
  across both virtual calls — the hallmark of a different toolchain vtable layout, not
  a different function.
- **`0xffffffffffffffff`** for the `-1` arg is Xenon 64-bit-register sign-extension
  noise (substrate caveat 2).

## Why this is not a coincidental BSim hit

The call signature — `GetTrackPanel()` twice bracketing a Tambourine-suppress and a
PlayerFeedback-suppress, plus `SetupDisplays`, plus a `ReachedTargetLevel(-1)`, plus
two adjacent virtual slots, all under one `byte@0x4` guard — is highly specific to
`Tracker::Poll`. No unrelated function plausibly reproduces this exact ordered
callee sequence. The structural shape and the resolved call graph corroborate the
BSim nomination independently.

## For the next agent

This is one of the cleanest packs in the band3 ACCEPT BSIM stratum: BSim similarity
1.0, full callee-graph corroboration, identical control flow. It is a positive data
point for "band3 ACCEPT BSIM precision is real, the dc3-BinDiff pessimism (0.193) is
an oracle artifact." Counts toward the CORRECT column with no caveats.
