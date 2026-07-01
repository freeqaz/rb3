# Pair 19 verdict — SaveGlobalOptions__10ProfileMgrFR23FixedSizeSaveableStream ↔ Xenon 0x82532b48

**Verdict: CORRECT (confidence: high)**

## Claim
Wii `SaveGlobalOptions__10ProfileMgrFR23FixedSizeSaveableStream` (Bank 8 `0x8034aa60`,
TU `ProfileMgr.o`) == Xenon `Function_82532B48` (`0x82532b48`).
Match type BSIM, sim×conf 17.482 (similarity 0.739 / confidence 23.656).

## Decisive evidence: all three Xenon callees are the exact three Wii callees

The Wii body's prologue + Save portion calls exactly three distinct functions:
`WriteEndian__9BinStreamFPCvi`, `Write__9BinStreamFPCvi`, and
`Save__11ModifierMgrFR23FixedSizeSaveableStream`. The Xenon body's three callees
are the same three:

| xenon callee | xenon symbol (preserved) | Wii callee it equals |
|---|---|---|
| `0x827a0108` | `Function_827A0108` (resolved via matches.json) | `WriteEndian__9BinStreamFPCvi` ✓ |
| `0x8279ffc8` | **`?Write@BinStream@@QAAXPBXH@Z`** (MSVC-mangled, preserved on this symbol) | `Write__9BinStreamFPCvi` ✓ |
| `0x82571cc8` | `Function_82571CC8` (resolved via matches.json) | `Save__11ModifierMgrFR23FixedSizeSaveableStream` ✓ |

Note: the evidence pack's resolution table mislabeled `0x8279ffc8` as
`Read__9BinStreamFPvi`, but the Xenon symbol itself was preserved as
`?Write@BinStream@@QAAXPBXH@Z`, which MSVC-demangles to
`public: void __cdecl BinStream::Write(void const *, int)` — i.e. **exactly**
`Write__9BinStreamFPCvi`. So the call-graph corroboration is even stronger than the
pack's table suggests: the byte-write callee is provably `BinStream::Write`.

`Save__11ModifierMgrF...` is called with a global singleton (`DAT_82dcd660`), exactly
as the Wii body calls it with the `TheModifierMgr` global. This is a distinctive,
non-generic callee — `ModifierMgr::Save` is unlikely to be co-called with
`BinStream::WriteEndian`+`BinStream::Write` by any function other than the
ProfileMgr global-options serializer.

## Corroborating signals

1. **Same function role:** both are stream serializers — read a long sequence of
   struct fields, write each to the `FixedSizeSaveableStream` arg via 4-byte
   (`WriteEndian`) or 1-byte (`Write`) writes. This matches the demangled Wii name
   `ProfileMgr::SaveGlobalOptions(FixedSizeSaveableStream&)`.
2. **Version-constant-first idiom:** the very first write on both sides is a 4-byte
   write of a small integer constant (Wii `0x20007` = `lis 0x2; addi 0x7`; Xenon a
   small const Ghidra rendered as `7`). Classic serialization-format-version header.
3. **Matching arity / call shape:** Xenon `(param_1, param_2)` — `param_2` is the
   stream passed to every write call, `param_1` is the object whose fields are read.
   Mirrors Wii `(this, FixedSizeSaveableStream& arg0)` exactly.
4. **Write skeleton aligns through the prologue:** version(4); a group of six 4-byte
   writes; then the interleaved byte/int pattern (1,4,1,4,1,1,...) is congruent for
   the first ~14 writes; then ends with the `ModifierMgr::Save` call. Field offsets
   differ between toolchains (expected per substrate caveat #1).
5. **Size ratio sane:** Wii ~836 B (209 asm lines), Xenon 532 B → 0.64x. Within the
   expected band; NOT a 10x impostor blowup. Xenon being shorter is consistent with
   the tail divergence below.

## The one divergence — explained, not disqualifying

The Wii body, AFTER `ModifierMgr::Save`, has an extra tail: construct a `String`
from `@stringBase0+0x80`, `BinStream << String`, destroy the String, then write a
literal 0 byte, an 8-byte zero block, two more bytes (0x5b3/0x5b4), a 4-byte field
(0x5b8), and finally `this->0x558 = 0`. The Xenon decompiled body shows only
`*(param_1+0x38) = 0` after the Save call (the `mGlobalOptionsSaveState`-style
"unchanged" flag clear — Wii does the equivalent at field 0x558) and returns.

This is a **cross-platform serialization-format-version difference**: RB3 on
Xbox 360 wrote a different (here, smaller / older-revision) GlobalOptions blob than
the Wii build. The latter half of the field list and the embedded-String write
simply aren't present in the X360 revision. This is exactly the expected class of
benign divergence for a same-source-function pair across the two platform builds —
not a wrong-match signal. (A Ghidra early-stop on the Xenon decomp would produce the
same partial view, but the field-write congruence through the prologue plus the
flag-clear-then-return tail already establish identity regardless.)

## Why not "wrong"
No disjoint-string conflict (neither side's string set contradicts the other; Xenon
shows 0 strings, the Wii String write is the only literal and it's in the diverged
tail). No leaf-vs-call-heavy mismatch (both are call-heavy serializers). Callee
identity is perfectly consistent and distinctive. The semantics match the Wii name.

## For the next agent
This pair is a textbook "correct, with a cross-platform format-version tail
divergence." Use it as a positive exemplar of: (a) trusting preserved MSVC-mangled
callee symbols over the resolution-table join (the `?Write@BinStream@@...` case shows
the join can mislabel a correctly-preserved symbol), and (b) not penalizing a shorter
Xenon tail when the prologue write-skeleton + all distinctive callees agree.
