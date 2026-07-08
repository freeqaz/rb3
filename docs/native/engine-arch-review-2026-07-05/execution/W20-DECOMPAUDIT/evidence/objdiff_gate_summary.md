# Lane D — batch_objdiff gate + per-function diagnose extracts (Wave 20)

Build: build/SZBE69_B8 (report.json 2026-07-07). objdiff-cli via tools/ninja-locked.
All match% CONFIRMED via mcp orchestrator batch_objdiff + run_diff_inspect.

## batch_objdiff gate (6 sub-100 shortlist)

| symbol | fuzzy% | classification | rationale (from tool) |
|---|---|---|---|
| Filter__13BandCharacter... | 94.76 | REAL_DIFF | 4 branch/cmp opcode flip(s): [62] beq↔bne, [356] bne↔beq, [419] beq↔bne, [472] cmplwi↔cmpwi |
| OnInstallFilter__13BandCharacter... | 99.09 | REAL_DIFF | 1 branch/cmp opcode flip: [112] beq↔bne |
| ReplaceRefs__F... | 98.79 | REAL_DIFF | 1 insert/delete: [75] delete b |
| LoadSubDir__9ObjectDir... | 99.38 | REAL_DIFF | 3 insert/delete: [265-267] delete addi/li/bl |
| PostLoadInlined__9ObjectDir... | 99.80 | BORDERLINE | — |
| __as__21ObjDirPtr<9ObjectDir>FRC... | 95.17 | REAL_DIFF | 2 insert: [42][63] insert lwz |

NB: batch_objdiff "REAL_DIFF" is a heuristic (branch-opcode flip present); the deep
audit below shows every flip is bool-materialization / block-reorder driven by a
uniform register-allocation shift, NOT a changed predicate. run_diff_inspect
diagnose reports "Unexplained diff_args: 0" for every one.
