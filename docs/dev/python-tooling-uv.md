# Python tooling env (uv)

The RB3 decomp Python tooling — orchestrator MCP server, objdiff/Ghidra analysis,
audio/visual verification, permuter helpers — runs in a **per-repo, uv-managed,
lockfile-frozen** virtualenv. Set up 2026-07-13; see the sibling repos
`rb3-xenon` and `dc3-decomp` for the identical pattern.

## Layout

- `pyproject.toml` — declared direct dependencies (this repo's tooling only).
- `uv.lock` — fully pinned, hash-locked resolution (**committed**; this is the freeze).
- `.python-version` — `3.10` (baseline the tooling was validated on).
- `.venv/` — the uv-managed env (gitignored).
- `venv` — a back-compat **symlink → `.venv`** (gitignored) so existing
  `venv/bin/python`, `source venv/bin/activate`, and `${PWD}/venv/bin/python`
  references (`.mcp.json`, docs, scripts) keep working unchanged.

`decomp-synth` (the shared classifier/diagnosis/permuter package) is consumed as
an **editable path source** from the sibling checkout `../decomp-synth`
(`[tool.uv.sources]`). It's pure-Python (tree-sitter only) — it does **not** pull
the ML stack. decomp-synth's own heavy ML/training deps live in *its* separate
`decomp-synth/.venv` (Python 3.13), not here.

## Usage

```bash
uv sync                 # create/refresh .venv from uv.lock (fast; cached wheels)
source venv/bin/activate # or venv/bin/python directly (symlink -> .venv)

# add / change a dependency
uv add <pkg>            # edits pyproject.toml + re-locks
uv lock                 # re-resolve after manual pyproject edits
```

## Background — why this exists

Before 2026-07-13 all three decomp repos shared **one** 8.6 GB venv
(`dc3-decomp/venv`, with `rb3`/`rb3-xenon` symlinked in) that had also
accumulated `decomp-synth`'s ML stack (CUDA/torch/triton ≈ 6.6 GB, plus
opencv/ncnn/pnnx/onnx/chromadb/playwright). The decomp tooling used almost none
of it. The migration split each repo into its own lean env:

| Repo | new `.venv` | was |
|---|---|---|
| `rb3` | ~250 MB | 8.6 GB shared |
| `rb3-xenon` | ~54 MB | 8.6 GB shared |
| `dc3-decomp` | ~163 MB | 8.6 GB (real dir) |

The old dc3 env is preserved as `dc3-decomp/venv-legacy-ml/` for rollback — delete
it to reclaim the 8.6 GB once the new envs are proven.

**Not managed by uv:** `lldb` (system LLVM Python binding; a few analysis scripts
import it, guarded). **Optional extras** live behind `--extra` (dc3-decomp only):
`pose` (ultralytics/opencv for `native/scripts/pose_server.py`), `ml` (broader
onnx/pnnx/ncnn/transformers research scripts), `gpu` (rdc-cli).
