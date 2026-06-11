# Pair 15 verdict — Unload__21CampaignSongInfoPanelFv ↔ Xenon 0x826035a8

**Verdict: CORRECT — confidence HIGH**

## Claim
Wii `Unload__21CampaignSongInfoPanelFv` (`CampaignSongInfoPanel::Unload`, Bank-8
`0x8028b9c0`) == Xenon `0x826035a8`. Nominator: BSIM, sim×conf 17.746
(sim 0.895 / conf 19.828), stratum BSIM 15-20.

## Decisive evidence
Identical control-flow skeleton and the same distinctive virtual-dispatch
fingerprint, with a byte-exact size match.

Wii (m2c / Bank-8 asm):
1. `Unload__7UIPanelFv(this)` — call super (parent `UIPanel::Unload`)
2. `temp = this->unk38` (offset **0x38**)
3. `if (temp != NULL) (*temp)->vt[0x8](1)` — virtual call slot 8, **literal arg 1**
4. `this->unk38 = NULL`
5. return

Xenon (Ghidra pseudo-C):
1. `Function_827EECF8()` — leading call (unresolved; positionally the super-call)
2. `puVar1 = *(param_1 + 0x44)` (offset **0x44**)
3. `if (puVar1 != 0) (**(code**)*puVar1)(puVar1, 1)` — vtable dispatch, **literal arg 1**
4. `*(param_1 + 0x44) = 0`
5. return

Every semantic element lines up: call-super-first, load one member pointer at a
fixed offset, null-check, conditional virtual call through the object's vtable
with the constant argument **1**, then store NULL/0 back into that same member,
then return. The literal `1` argument and the load-vtable-then-dispatch pattern
are a precise fingerprint, not generic boilerplate.

## Why the apparent differences are toolchain noise (not disagreement)
- **Field offset 0x38 (MWCC) vs 0x44 (MSVC)** — expected struct-layout divergence
  between Wii MWCC and Xbox360 MSVC (substrate caveat #1).
- **Super-call callee unresolved** (`Function_827EECF8`) — it's the leading call
  before any member work, exactly where `Unload__7UIPanelFv` sits on the Wii side;
  `UIPanel::Unload` exists in the map at `0x807ec380`. Absence of a resolved name
  is not evidence against (substrate caveat #4). No *contradicting* callee appears.
- **Size ratio ~1.0x** — CW map gives `Unload__21CampaignSongInfoPanelFv` size
  `0x54` = 84 bytes; Xenon body is 84 bytes. Within the expected 1.0-2.5x band,
  at the low end.

## Cross-checks performed
- `orig/SZBE69_B8/files/band_r_wii.map`: confirmed `Unload__21CampaignSongInfoPanelFv`
  @ `0x8028b9c0` size `0x54`, in `CampaignSongInfoPanel.o`; confirmed
  `Unload__7UIPanelFv` @ `0x807ec380` (the parent override target the Wii body calls).
- Body asm in evidence pack matches the map address/size.

## Caveat
No strings/asserts/floats on either side to cross-check (this is a small cleanup
override). The verdict rests on the structural fingerprint + byte-exact size +
call-super-first shape, all of which agree. This is NOT a thin/featureless stub:
the {super-call → member@offset → vtable-slot-8 dispatch with literal 1 → store 0}
sequence is specific enough to corroborate the BSim nomination.

## For the next agent
Pair 15 is a clean CORRECT/HIGH. It supports the hypothesis that band3 BSIM ACCEPT
(even the lower 15-20 sim×conf stratum) is genuinely correct and the dc3-BinDiff
band3 pessimism is an oracle artifact. No further digging needed on this pair.
