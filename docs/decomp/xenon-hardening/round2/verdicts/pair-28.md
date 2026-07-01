# Pair 28 verdict — CORRECT (high confidence)

**Claim:** Wii `TrackTypeToScoreType__F9TrackTypebb` (Bank-8 `0x80171cd0`, TU `Defines.o`)
== Xenon `0x82670e70` (match: `SwitchSigHasher`, simconf n/a).

**Verdict: CORRECT. Confidence: HIGH.**

## Decisive evidence
Exact case-by-case semantic equality of a 13-case switch (TrackType enum →
ScoreType), including the two *distinctive conditional* cases each keyed to the
*correct* bool argument and returning the *same* value pair.

### Side-by-side (Wii asm/jump-table `@15533[0xD]` vs Xenon pseudo-C)
| case | Wii (asm) | Xenon (pseudo-C) | agree |
|---|---|---|---|
| 0 | `cmpwi r5,0` (arg2) → 0 if 0 else 6 | `-(param_3!=0) & 6` → 0/6 | ✅ same arg, same pair |
| 1 | li 2 | return 2 | ✅ |
| 2 | li 1 | return 1 | ✅ |
| 3 | `cmpwi r4,0` (arg1) → 3 if 0 else 4 | `(param_2!=0)+3` → 3/4 | ✅ same arg, same pair |
| 4 | li 5 | return 5 | ✅ |
| 5 | li 9 | return 9 | ✅ |
| 6 | li 7 | return 7 | ✅ |
| 7 | →default (Fail) → 0xB | →default → 0xB | ✅ |
| 8 | li 8 | return 8 | ✅ |
| 9 | →default (Fail) → 0xB | →default → 0xB | ✅ |
| 10/11/12 | li 0xA | return 10 | ✅ |
| >12 / default | Fail + li 0xB | return 0xb | ✅ |

- **Arity match:** 3 params (`TrackType` enum + 2 bools) both sides. CW demangle
  `__F9TrackTypebb` = `(TrackType, bool, bool)`.
- **Argument fingerprint:** case 0 ↔ 3rd param (Wii r5 / Xenon param_3), case 3 ↔
  2nd param (Wii r4 / Xenon param_2). Both sides pin the *same bool* to the *same*
  conditional return. A SwitchSig collision would not preserve which bool drives
  which case.
- **Default fingerprint:** cases 7, 9, and >12 all collapse to 0xB on both sides
  (Wii via the `bgt`/missing-table-entry → Fail+`li 0xB`; Xenon `default: return 0xb`).
- **Size ratio sane:** Xenon 160 B vs Wii ~204 B (65 asm lines). Wii body is
  inflated by the inlined `FormatString`/`Debug::Fail` assert path on the unreachable
  default; MSVC outlined/elided it. ~0.8x — well within the 1.0–2.5x norm.

## Caveats / non-issues
- Brief header lists Wii addr `0x8018e1c0`, but the evidence pack's own asm body
  AND the CW map agree on `0x80171cd0` (`Defines.o`). The asm body is ground truth;
  the header addr typo does not affect the judgment.
- Leaf on the Xenon side (0 callees) so there is no call-graph corroboration —
  but the switch-value fingerprint is self-sufficient and far more specific than a
  callee list would be here.
- m2c is read-from-the-same-Bank-8-asm (a reading aid). I judged against the raw
  asm + jump table directly; they agree.

## Reproduction
- Wii asm: `build/SZBE69_B8/asm/band3/game/Defines.s` (`.fn TrackTypeToScoreType__F9TrackTypebb`).
- CW map: `orig/SZBE69_B8/files/band_r_wii.map:7853`.
- Xenon pseudo-C: evidence pack `docs/decomp/xenon-hardening/round2/evidence/pair-28.md` §"Xenon pseudo-C".

## For the next agent
This is a textbook TRUE SwitchSig identity — keep it in the ACCEPT tier. SwitchSig
matches that survive an argument-role + return-value case-by-case check (not just
case *count*) are very high precision; pair-28 is the cleanest in the SwitchSig
stratum (cf. pair-29 ActiveScoreType, also SwitchSig, also corroborated). The
SwitchSigHasher signal is trustworthy when the case→value mapping (and conditional
arg binding) matches, not merely the case shape.
