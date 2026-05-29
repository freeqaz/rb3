# Investigation — permuter BUILD FAILED variants on HandleRGGemStart

**Created**: 2026-05-28
**Origin**: Wave E2c batch_auto sweep on `system/beatmatch/SongParser` reported that the `HandleRGGemStart__10SongParserFiRQ210SongParser14DifficultyInfoUcUcUci` function at 80% match had "most variants BUILD FAILED" during the sweep. This means some pattern in the registry is emitting source that doesn't compile.

This doc is the spec for Wave F3. Agent: Opus.

## Goal

1. **Reproduce** the BUILD FAILED behavior.
2. **Identify** which pattern(s) are emitting invalid C++.
3. **Fix** the pattern(s) — typically by tightening the AST match or by validating the synthesized code before yielding.
4. **Add a guard** to the framework so future patterns can't ship variants that don't even parse / compile.

## Reproduction

```bash
# Worktree to avoid touching main src/
cd /home/free/code/milohax/rb3 && tools/setup-worktree.sh wave-f3-investigate
cd ../wt-wave-f3-investigate

# Run the permuter on the target symbol and capture log output
python3 -m decomp_synth.hill_climber \
    --symbol "HandleRGGemStart__10SongParserFiRQ210SongParser14DifficultyInfoUcUcUci" \
    --rounds 6 \
    --verbose 2>&1 | tee /tmp/f3-permuter.log
```

(Check the actual CLI flags — `--verbose`, `--keep-builds`, or similar — by reading `decomp_synth/hill_climber.py:__main__` and `decomp_synth/__main__.py`.)

Look for:
- Lines containing "BUILD FAILED" / "compile error" / "syntax error".
- The associated `pattern_name` for each failure.
- The variant `description` text.

If the verbose flag doesn't surface failures, add temporary diagnostic prints to `hill_climber.py`'s scoring step (then revert before finishing). The score result struct has a `build_success` boolean.

## Likely suspects

The newest patterns are most likely:
- `switch_case_reorder` (Wave B5) — may produce malformed `switch` bodies if a case has multiple labels or complex statements.
- `inline_lerp_collapse` (Wave B4) — replaces 3-statement triples; if the parsing of the binary expression handles parens wrong, you'd get unbalanced expressions.
- `makestring_wrap_literal` (Wave B2) — fairly safe, but worth verifying.
- `pragma_pool_data_wrap` (Wave B3b) — inserts `#pragma` lines; if it inserts inside an unexpected scope, MWCC complains.
- `return_this_op_assign` (Wave B1) — inserts `return *this;`; should be safe.

But ANY pattern could be buggy. Don't anchor on suspects — let the log tell you.

## Hardening the framework

Once the buggy pattern is identified and fixed:

### Add a syntax-validation pre-filter
In `decomp_synth/generator.py` (or wherever variants get yielded), add a tree-sitter parse check on the synthesized source. If the parse fails or yields an ERROR node count above the baseline, drop the variant **before** dispatching it to the build queue.

Pseudocode:
```python
def validate_syntax(source_bytes: bytes, original_error_nodes: int) -> bool:
    tree = TS_PARSER.parse(source_bytes)
    new_error_nodes = count_error_nodes(tree.root_node)
    return new_error_nodes <= original_error_nodes
```

This isn't a full compile (too slow), but it catches the obvious cases (unbalanced braces, malformed statements). Cheap pre-filter.

### Add a regression test
Once a buggy pattern is fixed, add a test in `decomp_synth/tests/` that:
- Constructs a `FunctionContext` from a minimal source snippet that previously triggered the bug.
- Asserts the pattern's `generate()` either skips it or produces parseable output.

This prevents the same class of regression from sneaking back in.

## Acceptance

- The buggy pattern(s) identified with concrete file:line evidence.
- Fix applied; patterns/*.py file modified with a clear `# Fix:` comment explaining what changed and why.
- Regression test added to `decomp_synth/tests/`.
- Syntax pre-filter added (only if the bug class warrants it — if it's a one-off, skip the pre-filter).
- Update `docs/plans/permuter-mechanization-roadmap.md` outcome log: "Wave F3 — fixed <pattern>; <N> BUILD FAILED variants now caught upstream".
- Brief design note appended to this doc.

## Safety
- Work in worktree, not main repo.
- No `git stash`. No commits to main.
- Use `tools/ninja-locked` if a build is needed (the investigation should mostly be reading logs; only rebuild if you need to confirm a fix).
- Cap wall-clock at 45 minutes; if you can't ID the bug in that time, write up what you found and STOP.

## Out of scope
- Performance tuning of the pattern (only correctness).
- Adding new patterns.
- Touching the constraint_solver loop (that's Wave C1, separate).

## Findings (Wave F3, 2026-05-28)

### Root cause
`decomp_synth` is a symlink to the shared `dc3-decomp` permuter codebase, so all fixes apply globally. libclang's `is_available()` only checks that `clang.cindex` imports — it does NOT check that a compile_commands.json compdb exists. In the wave-f3 worktree (and presumably in the batch_auto runs that triggered this investigation) `_find_compdb_dir()` returns `None`, so `resolve_call_return_type()` returns `None` for every call. The existing pattern guards key off `return_type is not None`, so they never fired.

### Two buggy patterns identified

**1. `variable_extraction` — `int _tmp = X(...)` for record-returning calls**

`decomp_synth/patterns/variable_extraction.py:123-127` (pre-fix). The "emit untyped" decision treated `return_type is None` as "unknown — keep emitting", but in the no-compdb regime `None` is the only outcome. Result: for `info.mRGGemsInfo[uc - 24] = RGGemInfo(tick, info.mActivePlayers, GetFret(data), channel);` the pattern emitted

```cpp
int _tmp0 = RGGemInfo(tick, info.mActivePlayers, GetFret(data), channel);
info.mRGGemsInfo[uc - 24] = _tmp0;
```

— a hard MWCC compile error (no implicit `RGGemInfo`→`int`).

**2. `bool_cast` Pattern 3 — wrapping an assignment RHS in `bool(...)`**

`decomp_synth/patterns/bool_cast.py:149-172` (pre-fix). The pattern wrapped *any* call-shaped RHS in `bool(...)`. For the same SongParser line that produces:

```cpp
info.mRGGemsInfo[uc - 24] = bool(RGGemInfo(tick, info.mActivePlayers, GetFret(data), channel));
```

— another hard error (no `RGGemInfo(bool)` constructor).

### Fixes
Both fixes are pure syntactic gates that fire only in the no-compdb regime; the existing typed-resolution guards are untouched.

- **`variable_extraction.py`**: added `_is_assignment_rhs_to_complex_lvalue()` (call is the direct RHS of an assignment whose LHS is non-identifier — subscript / field / arrow access) AND `_syntactic_record_return()` (bare PascalCase callee without a known scalar-returning prefix like `Get`/`Find`/`Is`/`Has`/…). Both must hold AND `return_type is None` AND dialect is mwcc to drop the untyped form. Comment tag: `# Fix (Wave F3)`.
- **`bool_cast.py`**: added `_bool_assignable_lvalue()` — Pattern 3 only emits when the assignment LHS is a plain `identifier`. Subscript / field-expression / arrow LHS is rejected (no libclang required). Comment tag: `# Fix (Wave F3)`.

### Regression tests
- `decomp_synth/tests/test_variable_extraction_record_guard.py` — 8 tests: helper unit tests for PascalCase / scalar-prefix / lowercase / method-call detection, plus end-to-end emission tests for the SongParser bug shape AND counter-tests asserting that `GetFret(data)` and `obj.GetCount()` extractions are NOT over-blocked.
- `decomp_synth/tests/test_bool_cast_lvalue_guard.py` — 7 tests: helper unit tests for identifier / subscript / field-access / arrow LHS, plus emission tests asserting the SongParser shape no longer wraps, while `bool flag = IsActive()` shape still does.

All 14 new tests pass. The pre-existing failure in `test_header_variable_extraction_bridge` (asserts `auto _tmp0` but mwcc emits `int _tmp0`) is unchanged — that test was broken before this wave by dc3 commit `f985916b`.

### BUILD FAILED count, before / after
Single-shot run on the trigger symbol:

| State | Generated variants | BUILD FAILED |
| --- | --- | --- |
| before | 4 (`varext_0`, `stmt_reorder_0`, `boolcast_0`, `cmpflip_0`) | **2** (`varext_0`, `boolcast_0`) |
| after  | 2 (`stmt_reorder_0`, `cmpflip_0`) | **0** |

Hill_climber 2-round run (`--max-rounds 2 --max-variants 200 --workers 4`) after the fix reports `0 build failures` across 4 variants generated through beam depth 2.

### Syntax pre-filter — not added
Both bugs were *syntactically valid* C++ (tree-sitter parses `int x = RGGemInfo(...);` and `arr[i] = bool(RGGemInfo(...))` cleanly). The errors are semantic (MWCC's type system). A tree-sitter ERROR-node count check would not have caught either bug, so adding one wouldn't help here. The narrow syntactic gates inside each pattern are the right layer to fix the failure class. Left as future work: a libclang-backed validator that runs once per variant when `clang_types.is_available() AND _find_compdb_dir() is not None` — but that requires a working native build with `compile_commands.json`, which isn't a given in worktrees.

### Files touched (worktree → shared via symlink)
- `decomp_synth/patterns/variable_extraction.py`
- `decomp_synth/patterns/bool_cast.py`
- `decomp_synth/tests/test_variable_extraction_record_guard.py` (new)
- `decomp_synth/tests/test_bool_cast_lvalue_guard.py` (new)
