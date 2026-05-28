# Wave G — Tightened-scanner permuter target report

**Date**: 2026-05-28
**Origin**: After Wave F2 shipped `--require-asm-signal` gating in `pattern_scan.py`, the in-scope hit count dropped from 4,153 (Wave E) → 96 `asm_signal_match` (Wave G). This list is the trustworthy set: each hit has both an AST match AND a confirmed diagnosis signal for the pattern.

## Scan command

```bash
python3 -m scripts.permuter.pattern_scan \
  --patterns return_this_op_assign,makestring_wrap_literal,switch_case_reorder,member_readback,positive_branch_invert,demorgan_guard,abs_empty_else_negate,store_then_compound_add,cache_repeated_call,bitpack_or_reorder,symbol_str_compare,empty_size_swap \
  --incomplete-only --min-pct 80 --max-pct 99.9 \
  --require-asm-signal --json > /tmp/wave-g/tight_ast.json
```

(Note: `bool_materialize` excluded — known broad-fire pattern; would inflate counts.)

## High-confidence top units (asm-signal-matched only)

| Rank | Unit | Hits | Syms | Avg % | Pattern mix | Wave E/F status |
|---|---|---|---|---|---|---|
| 1 | `system/meta/StorePackedMetadata` | 6 | 5 | 93.2 | positive_branch_invert ×2, switch_case_reorder ×3, member_readback ×1 | **Wave F5 swept (0 wins)** — but F3 patches now in; retry worth it |
| 2 | `system/rndobj/PropAnim` | 6 | 4 | 97.6 | switch_case_reorder ×4, cache_repeated_call ×2 | **Untouched** |
| 3 | `system/rndobj/PropKeys` | 5 | 4 | 96.5 | switch_case_reorder ×4, demorgan_guard ×1 | **Untouched** |
| 4 | `band3/game/GemPlayer` | 4 | 3 | 96.5 | cache_repeated_call ×2, demorgan_guard ×1, store_then_compound_add ×1 | **Untouched** |
| 5 | `band3/meta_band/SaveLoadManager` | 4 | 4 | 97.9 | switch_case_reorder ×3, positive_branch_invert ×1 | **Untouched** |
| 6 | `system/beatmatch/SongParser` | 4 | 3 | 96.6 | switch_case_reorder ×1, member_readback ×1, demorgan_guard ×1, cache_repeated_call ×1 | Wave E2c swept (2 wins) |
| 7 | `system/bandobj/ChordShapeGenerator` | 3 | 3 | 94.1 | cache_repeated_call ×3 | **Untouched** |
| 8 | `system/world/CameraShot` | 3 | 2 | 92.5 | demorgan_guard ×1, cache_repeated_call ×2 | **Untouched** |
| 9 | `band3/bandtrack/VocalTrack` | 2 | 1 | 80.1 | store_then_compound_add ×1, cache_repeated_call ×1 | Untouched (large file, complex) |
| 10 | `band3/game/TrainerGemTab` | 2 | 1 | 99.2 | switch_case_reorder ×1, cache_repeated_call ×1 | **Untouched** |

## Wave G dispatch picks

3 parallel batch_auto sweeps on the highest-confidence untouched units:

- **G1**: `system/rndobj/PropAnim` — 4 switch_case_reorder hits at high %, ripe for the new B5 pattern.
- **G2**: `system/rndobj/PropKeys` — 4 switch_case_reorder hits at high %.
- **G3**: `band3/game/GemPlayer` — diverse pattern mix at 96.5% avg.

(Skipping high-% SaveLoadManager and TrainerGemTab — at 97-99% they're likely IPA-locked despite the AST signal; can revisit if G1-G3 yield clean wins.)

## What changed vs Wave E

| Metric | Wave E (loose) | Wave G (tight) | Δ |
|---|---|---|---|
| In-scope hits | 4,153 | 674 | -83% |
| `asm_signal_match` confidence | n/a | 96 | new |
| `excluded` (asm checked, no signal) | n/a | 53 | new |
| `unknown` (no cached diff) | n/a | 578 | new — needs cache warming |

The 578 `unknown` hits are the next analytic frontier — they need `/tmp/claude/diff_*.json` populated. Wave G defers that to a follow-up (use `--fresh-objdiff` flag) once initial sweeps confirm the methodology.

## Outcome log

(Sub-agents append below as they finish.)

| Wave | Unit | Agent | Status | Wins | Top win |
|---|---|---|---|---|---|
| G1 | `system/rndobj/PropAnim` | Sonnet 4.6 | DONE (31s) | 0 | n/a — all 5 candidates are volatile/callee-saved regswap (permuter-class) |
| G2 | `system/rndobj/PropKeys` | Sonnet 4.6 | DONE (109s) | 1 | `FloatKeys::FloatAt` 93.9%→96.4% (+2.5pp, declmove+stmt_reorder) |
| G3 | `band3/game/GemPlayer` | Sonnet 4.6 | DONE (321s) | 1 | `GemPlayer::AddHeadPoints` 91.2%→93.2% (+1.95pp, stmt_reorder+initlit) |
