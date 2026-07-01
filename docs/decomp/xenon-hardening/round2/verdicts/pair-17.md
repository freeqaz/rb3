# Pair 17 verdict — CORRECT (high confidence)

**Claim:** Wii `Load__Q226@unnamed@MainHubPanel_cpp@17MainHubAdvanceMsgFR9BinStream`
(`MainHubAdvanceMsg::Load(BinStream&)`, Bank-8 `0x802e0320`) == Xenon `0x82603958`.

**Verdict: CORRECT. Confidence: high.**

## Decisive evidence
The Xenon callee `0x827a0358` carries a **real MSVC-mangled symbol** (not a guess):
`??5BinStream@@QAAAAV0@AAVString@@@Z` = `BinStream& BinStream::operator>>(String&)`
(`??5` is the MSVC mangling for `operator>>`). It was matched (BSIM) to the Wii
`__rs__9BinStreamFR6String` — the exact `>>` operator the Wii Load body calls.
That is name-level corroboration from the Xenon binary's own symbol, independent of
the BSim score that nominated the top-level pair.

## Structural identity (the same source function, two toolchains)
Both bodies are the identical straight-line deserializer skeleton, same arity
(`BinStream&`), same two ops in the same order, same field offsets:

| step | Wii (Bank-8 asm) | Xenon (pseudo-C) |
|---|---|---|
| read 1 byte from stream into a stack local | `Read__9BinStreamFPvi(this, r1+8, 1)` then `lbz 0,8(1)` | `Function_8279FF18(param_2, local_20, 1)` |
| store that byte (u32-widened) at object+4 | `stw 0, 4(r30)` | `*(uint*)(param_1+4) = local_20[0]` |
| read a String at object+8 | `__rs__9BinStreamFR6String(this, this+8)` | `operator>>(param_2, param_1+8)` |
| return | `blr` | `return` |

- **Field offsets agree across toolchains** (+4 for the byte field, +8 for the
  String) — usually they differ; here they match exactly.
- **Size ratio 1.0x:** Wii 0x58 = 88 bytes, Xenon = 88 bytes. Ideal (the brief's
  healthy range is 1.0–2.5x).
- **BSim similarity 1.0**, sim×conf 16.738.
- The demangled Wii name `MainHubAdvanceMsg::Load(BinStream&)` is a **deserializer**;
  the Xenon data flow (stream → local → object) is unambiguously a **read**, fully
  consistent with the name. No getter/parser contradiction.
- TU/family check: `band_r_wii.map` (line 14030) shows `Load` sits in a clean
  `MainHubAdvanceMsg` message-class family in `MainHubPanel.o`
  (NewNetMessage/Dispatch/**Load**/Save/Name/ByteCode — the standard Hmx message
  vtable interface). Real symbol, real TU.

## One apparent discrepancy, explained (not a problem)
The first Xenon callee `0x8279ff18` is *labeled* `Write__9BinStreamFPCvi` in the
callee table, while the Wii Load body calls `Read__9BinStreamFPvi`. This is a
**Read/Write VT-matcher confusion**, not a semantic disagreement:
- That callee's match is `VTCombinedReference` (sim 0.599), not a name/BSim match —
  `BinStream::Read` and `BinStream::Write` are near-identical bodies (both forward to
  a virtual byte-transfer), so a VT-reference correlator routinely swaps them.
- The **Xenon body semantics are unambiguously a read**: `Function_8279FF18` fills
  `local_20`, then `local_20[0]` is copied *into* the object — data flows
  stream→local→object = deserialize. A `Write` would copy object→stream; it does not.
- So the callee *label* is mis-resolved, but the *operation* is the read this Load
  must perform. Evidence-for stands; this is not evidence-against.

## For the next agent
- This pair is a textbook CORRECT band3 BSIM ACCEPT (sim×conf 16.7, the 15–20
  stratum). Despite being in the lowest BSIM stratum, the identity is rock-solid:
  perfect-1.0 BSim, 1.0x size, agreeing offsets, and a name-corroborated `operator>>`
  callee. Supports the hypothesis that the dc3-BinDiff band3 pessimism is an oracle
  artifact, not a real precision dip — at least for this pair.
- General lesson for these BinStream Load/Save deserializers: the Read/Write callee
  may be VT-swapped in the resolution table; judge the data-flow direction in the
  pseudo-C, not the callee label.
