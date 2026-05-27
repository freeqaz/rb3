# objdiff-cli JSON extensions (milohax fork)

`bin/objdiff-cli` is the milohax fork of objdiff (built from
`/home/free/code/milohax/objdiff`, symlinked into `bin/`). Beyond the stock
JSON diff output it adds two structured capabilities useful for AI-assisted
decomp. Verify you have the fork build with:

```bash
bin/objdiff-cli diff --help | grep -q -- --include-data && echo "fork build OK"
```

## Data-symbol diffs (`--include-data`)

Stock objdiff diffs *code*. The fork also emits a structured byte/relocation
diff for **data** symbols — vtables, pointer/jump tables, string pools, static
initializers. See the `/data-diff` skill for the workflow; this is the schema.

```bash
bin/objdiff-cli diff -p . -u <unit> <data_symbol> -f json --include-data
```

A `data_diff` object is added to the JSON for data symbols (omitted for code
symbols, so the flag is a safe no-op on functions):

```jsonc
"data_diff": {
  "match_percent": 100.0,
  "mismatch_byte_count": 0,
  "total_byte_count": 16,
  "segments": [
    { "offset": 0, "size": 16, "kind": "equal" }
  ],
  "relocations": [
    { "offset": 0,  "size": 4, "kind": "equal", "target_symbol": "__RTTI__8FilePath" },
    { "offset": 8,  "size": 4, "kind": "equal", "target_symbol": "__dt__8FilePathFv" },
    { "offset": 12, "size": 4, "kind": "equal", "target_symbol": "Print__6StringFPCc" }
  ]
}
```

- **`segments[]`** — contiguous byte runs, in order. `kind` is one of `equal`,
  `replace`, `insert`, `delete`. `bytes` (hex) is the **target** side; `base_bytes`
  (hex) is the **base** (decompiled) side, present only when it differs. Equal runs
  carry no bytes. This makes string/init-value typos directly comparable.
- **`relocations[]`** — the actionable signal for vtables/pointer tables.
  `target_symbol` is where the **target** reloc points; `base_target_symbol` names
  where the **base** build points when it differs (a vtable slot resolving to the
  wrong function); `addend`/`base_addend` likewise. A relocation present only on the
  base side surfaces as `kind: "insert"` with an empty `target_symbol`.

This is the *diff* counterpart to `/vtable` and `/resolve-vcall` (which read the
target `.o` only): `--include-data` tells you **where the decompiled build diverges**
from the target, slot by slot.

## Control-flow (branch) graph

With `--include-instructions`, each instruction row carries the per-side branch
graph objdiff already computes (the GUI's branch arrows):

```jsonc
{ "index": 13, "match_type": "equal",
  "target_branch_to":   { "target_index": 20, "branch_idx": 0 },
  "base_branch_to":     { "target_index": 20, "branch_idx": 0 } }
{ "index": 20, "match_type": "equal",
  "target_branch_from": { "source_indices": [13], "branch_idx": 0 },
  "base_branch_from":   { "source_indices": [13], "branch_idx": 0 } }
```

- `*_branch_to` `{ target_index, branch_idx }` — the row this row branches to.
- `*_branch_from` `{ source_indices: [...], branch_idx }` — rows that branch here.

All indices reference the `index` field of rows in the same `instructions`
array. `branch_idx` is objdiff's per-branch color/group id. Use this for
control-flow-aware analysis (loop bodies, branch reordering, fall-through
changes) without re-deriving the CFG from raw asm.

## Reference

Source of truth: the fork at `/home/free/code/milohax/objdiff`
(`objdiff-cli/src/cmd/diff.rs`, `docs/research/next-work.md`).
