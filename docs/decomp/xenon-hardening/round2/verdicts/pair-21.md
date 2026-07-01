# Pair 21 verdict — CreateController__14ChordbookPanelFv ↔ Xenon 0x826966f0

**Verdict: CORRECT. Confidence: HIGH.**

## Claimed identity
- Wii `CreateController__14ChordbookPanelFv` (Bank 8 `0x8016c4c0`, TU `ChordbookPanel.o`)
- Xenon `Function_826966F0` (`0x826966f0`, 232 B)
- Nominated by BSIM, sim×conf 17.353 (similarity 0.504 / confidence 34.431), stratum BSIM 15-20.

## Decisive evidence
Three distinctly-named, semantically-loaded callees resolve (via matches.json)
to the EXACT callees the Wii body calls, in the same order, with matching literal
args — plus both sides reference the same two distinctive Symbol strings.

## Step-for-step control-flow alignment (Wii m2c/asm vs Xenon pseudo-C)

| step | Wii | Xenon | agree |
|---|---|---|---|
| 1 | `unk68` non-null → vcall `(*p)->unk8(p,1)` | `*(p+0x6c)` non-null → `(**p)(p,1)` | YES (vcall w/ arg 1, offset 0x68≈0x6c) |
| 2 | `this->unk68 = NULL` | `*(iVar4+0x6c) = 0` | YES |
| 3 | load `this->unk40->unk230` (0x40/0x230) | `*(*(iVar4+0x44)+0x260)` | YES (toolchain offset shift) |
| 4 | `GetController(TheGameConfig, ...)` → local | `Function_8266ADB0(&local_40, DAT, iVar1)` → local | YES (2-arg, result-to-local; Wii callee unmatched in this run) |
| 5 | two `__ct__6SymbolFPCc` (string offsets 0x2D6, 0x2CA) | two `??0Symbol@@QAA@PBD@Z` (Symbol::Symbol(char const*)) | YES |
| 6 | `SystemConfig__F6Symbol6Symbol6Symbol(...)` | `Function_824FCE58(...)` = **SystemConfig** (resolved) | YES |
| 7 | conditional ptr adjust (`this`→`&unk38`; `r28`→`r28->unk0`) | `if (iVar1!=0) iVar9 = ...` | YES (null-guarded ptr fixup) |
| 8 | `GetGameplayOptions__8BandUserFv(r28)` then vcall (vt 0x1C) | `Function_8266D140` = **GetGameplayOptions** (resolved) then `(**(p+0x10))()` | YES |
| 9 | `NewController__F...(.., 0, .., (TrackType)0xA)` | `Function_8276AEF0(.., 0, .., 10)` = **NewController** (resolved) | YES — literal `0` and `0xA`/`10` match exactly |
| 10 | store result → `this->unk68` | `*(iVar4+0x6c) = uVar8` | YES |

## Resolved-callee agreement (strongest signal)
- `SystemConfig__F6Symbol6Symbol6Symbol` — resolved, called by both.
- `NewController__FP4UserPC9DataArrayP23BeatMatchControllerSinkbb9TrackType` —
  resolved, called by both; the 6-arg call carries the SAME literal args `0`
  and `0xA`/`10` (TrackType) on both sides.
- `GetGameplayOptions__8BandUserFv` — resolved, called by both, each immediately
  followed by a virtual call on the returned object.
- `Symbol::Symbol(char const*)` — two ctors on each side.

5 of 6 Xenon callees resolve to matched Wii symbols; the 6th (`Function_8266ADB0`)
is the only un-resolved one and sits exactly where Wii calls `GetController`
(2-arg, result-to-local) — consistent, not contradictory.

## String agreement
Xenon references `'controller'` and `'beatmatcher'`. Both appear in this exact
function's Wii string pool (`...mGemPlayer\0beatmatcher\0controller\0...`); they
are the literals the two Symbol ctors construct. Distinctive, non-generic strings
present on both sides → corroborates the identity independent of BSim.

## Size / shape sanity
232 B (Xenon) vs ~103 asm lines ≈ 412 B (Wii). Ratio ≈ 0.56 — Xenon is slightly
smaller (MSVC handled the two assert/`MakeString`+`Fail` blocks more compactly),
well within a same-source-function expectation. No leaf-vs-call-heavy mismatch:
both are call-heavy with identical callee identity.

## No wrong-match signals
- Callee identity AGREES (not disjoint).
- Strings AGREE (not disjoint).
- Same arity at the load-bearing `NewController` call (6 args, matching literals).
- Control flow is isomorphic (null-guarded vcall → clear → load member → 2 Symbol
  ctors → SystemConfig → 2 null-guarded ptr fixups → GetGameplayOptions+vcall →
  NewController → store), no switch-count or early-out discrepancy.
- Semantics match the demangled Wii name (`ChordbookPanel::CreateController` —
  it constructs a beatmatch controller via `NewController`).

## For the next agent
This is among the strongest packs in the sample: the BSim score (17.4, low-mid
band) UNDERSTATES the true confidence because the call-graph + string + literal-arg
corroboration is overwhelming. Datapoint for the round-2 thesis that band3 BSim
"low" simconf does NOT imply low precision — the pessimistic dc3-BinDiff oracle's
band3 dip (0.193) looks like an oracle artifact here, not a real error.
