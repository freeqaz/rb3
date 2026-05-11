# Fixable: Comparison Patterns

Patterns around comparison operators and the branches CW generates from them.

## != vs < for Loop Comparison

`i != count` generates `beq` (via `add.` setting CR0), while `i < count` generates `cmplwi/ble`. For deque/vector iteration, `!=` often matches the target better.

**Example:** In `VocalTrack::HitTambourineGem`, `i != count` generated the correct empty-loop `beq` check, while `i < count` produced extra `cmplwi r0, 0x0; ble`.

## See Also

- [fixable-casting.md](fixable-casting.md#truthiness-test-vs-explicit-comparison-flips-fcmpu-operand-order) — float truthiness vs explicit `!= 0.0f`
- [fixable-bool-mask.md](fixable-bool-mask.md) — bool materialization and condition inversion
