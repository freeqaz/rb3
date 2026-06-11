# Pair 04 verdict — Wii `Poll__7NetSyncFv` (0x8030a270/0x80374950) ↔ Xenon 0x82586258

**VERDICT: correct — confidence HIGH**

Match type BSIM, sim×conf 40.168 (similarity 0.746 / confidence 53.844). TU `NetSync.o`.

## Decisive evidence
The control-flow skeleton is identical and the call graph independently corroborates,
including a **same-TU** callee.

| aspect | Wii (m2c + Bank-8 asm) | Xenon (Ghidra pseudo-C) | agree? |
|---|---|---|---|
| global gate | `TheUI->unk8 != 0` | `*(PTR_DAT_82c41b48 + 0x10) == 0` (same global, inverted branch) | yes |
| zero-branch action | `AttemptTransition(this, scr, idx)` | `Function_82585328(param_1, +0x2c, +0x30)` = `AttemptTransition__7NetSyncFP8UIScreeni` | yes |
| vcall A | slot `0x68` on `TheUI->unk24`, early-out | slot `0x60` on `+0x30`, early-out | yes (vcall + early-out) |
| vcall B | slot `0x84` on `TheUI->unk20`, early-out | slot `0x7c` on `+0x2c`, early-out | yes (vcall + early-out) |
| LockStepMgr field | `this->unk2C` | `param_1 + 0x38` | same role |
| InLock gate | `InLock__11LockStepMgrCFv(this->unk2C)` | `FUN_82592570(+0x38)` = `InLock__11LockStepMgrCFv` | yes |
| flag check | `unk28 == 0` | `*(*(+0x38) + 0x38) == '\0'` | same guard |
| RespondToLock | `RespondToLock__11LockStepMgrFb(mgr, 1)` | `Function_82593D30(*(+0x38), 1)` — arg=1, receiver=LockStepMgr field | yes (see note) |

Size ratio ~1.0x (Xenon 224 B vs Wii ~216 B). Two callees, two early-out vcalls,
one InLock gate, one flag check, one RespondToLock(_, 1), one AttemptTransition — all
present on both sides in the same order with the same control flow.

## The one apparent discrepancy (not a problem)
The third Xenon callee `Function_82593D30` resolves (via matches.json) to
`DispatchRMCCall__Q26Quazal17_DOC_VoiceChannel...` instead of `RespondToLock`. This is
a **secondary BSim mis-resolution of that callee**, not a property of our pair:
- The Wii body calls `RespondToLock__11LockStepMgrFb(mgr, 1)` with `li r4, 0x1`.
- The Xenon body calls `Function_82593D30(*(param_1+0x38), 1)` — same receiver (the
  LockStepMgr field at +0x38) and same literal arg `1`.
- Receiver + arity + arg value prove the Xenon function is semantically calling
  `RespondToLock(lockStepMgr, 1)`; BSim simply mis-identified *that callee* against a
  Quazal function (`0x82593d30 -> wii 0x800cc1b0`, an unrelated address). The mis-id
  is on the neighbor, not on this pair.

CW map confirms `InLock` + `RespondToLock` are both in `LockStepMgr.o` and
`AttemptTransition` is in `NetSync.o` (same TU as Poll) — consistent with the body.

## Commands run
- `grep -n "RespondToLock\|InLock\|AttemptTransition" orig/SZBE69_B8/files/band_r_wii.map`
- joined the three Xenon callee addrs through matches.json `function_matches[]`.

## For the next agent
This pair is a textbook correct BSIM ACCEPT: identical skeleton, same global gate,
same-TU callee (`AttemptTransition`) agreeing by name, plus `InLock` agreeing by name.
The only callee name disagreement is a downstream BSim error on the RespondToLock
neighbor — judge callee-name disagreements by checking the receiver+arg, not the
resolved label alone.
