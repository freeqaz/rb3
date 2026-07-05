# W0.2 — Loud-by-default weak stubs + census + registry — STATUS

Append-only. Update under `flock /tmp/rb3-docs.lock`. One `## <subtask-id> — done|partial|blocked`
section per subtask, with commit SHAs, the boot hit-list (S2), the reclassification list (S3), and
the captured fail-red output (S3).

<!-- implementers append below -->

## W0.2.S1 — done

**Commit:** `1f8057f9` — "W0.2: generate band3_link_stubs.s from registry (no behavior change)"

- Added `native/src/band3_stub_registry.tsv` (582 rows: 521 func / 61 data),
  extracted programmatically from the pre-change `band3_link_stubs.s` (parsed
  every `.weak` + its `.set`/`.bss .zero` pair; asserted no dupes; counts
  matched PLAN.mds MEASURED baseline exactly: 521/61/582).
- Added `scripts/native/gen_band3_link_stubs.py` with `--mode legacy` (the
  only mode this subtask needs) + `--check` (idempotency/drift gate, used by
  later subtasks). Emits func rows sorted alphabetically aliasing the single
  `__hmx_band3_noop_stub`, and data rows (also alphabetical) each with their
  own `.bss .zero <N>` reservation (size carried in the registry note as
  `bss=<N>`).
- Regenerated `native/src/band3_link_stubs.s` from the registry. Verified:
  - `grep -c .weak` = 582 (unchanged); func/data split re-derived by
    walking the section markers = 521/61 (unchanged).
  - `python3 scripts/native/gen_band3_link_stubs.py --mode legacy` twice ->
    second run + `--check` report no drift (idempotent).
  - `git diff` on the regenerated file shows ONLY comment/ordering cosmetics
    (the historical per-symbol "REMOVED" prose for already-gone symbols, plus
    reordering to alphabetical) — the `.weak`/`.set`/`.bss` symbol set and
    resolution are byte-for-byte the same set as before.
  - Build: `cmake -B native/build-agent-W0.2 -S native -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`
    (bare cmake defaulted to GNU which the project rejects; had to force
    clang explicitly) + `cmake --build native/build-agent-W0.2 --target rb3-native -j8`
    -> succeeds clean.
  - Boot: `RB3_GAME=1 RB3_HTTP=1 RB3_HTTP_PORT=18422 MILO_HEADLESS=1 RB3_DATA=orig-assets/extracted native/build-agent-W0.2/rb3-native`
    -> `GET /api/health` returns `200` with
    `{"ok":true,"data":{"status":"ok","frame":9,"songMs":-1.0,"currentScreen":"intro_movie_screen"}}`.
    (Stderr shows unrelated WebGPU/software-render validation warnings in
    this headless sandbox — pre-existing, not caused by this change; health
    endpoint and frame progression are unaffected.)

**Deviation from PLAN.md** (recorded, not silently expanded scope): the
Design blocks note field is described as carrying forward "the existing
scar-tissue prose, incl. the REMOVED explanation blocks, so context is not
lost." I did NOT reproduce those per-symbol historical comments (e.g. "GameGemList
stubs REMOVED — now compiled", the NetSession-base-virtuals paragraph, the
Menu-bring-up-wave dated block, etc.) in the generated `.s`, because those
comments describe symbols that are NO LONGER present as weak stubs (they
graduated to strong defs) — there is nothing in the current 582-row registry
to attach that prose to. That narrative remains fully intact in
`band3_link_stubs.s`s git history (this commit does not delete or rewrite
history, only regenerates the current file), so no information is actually
lost, just relocated from "inline comment" to "git blame/log". The registry
`note` column is used for what IS still live: `bss=<N>` sizes on data rows,
and a one-line safety justification on the 8 Bink* func rows I seeded as
`ok-noop` (per the seeding rule: unknown -> assert-unreachable by default;
these are the only remaining func stubs the file/plan explicitly names as
safe no-ops). This classification has NO runtime effect yet in S1 (legacy
mode ignores `class` and always emits the shared no-op) — it only becomes
load-bearing once S2 lands the per-symbol trampoline + census, at which point
S3s boot-hit-list is the authoritative reclassification pass anyway.

**No trampolines, no census** — out of scope for S1, confirmed unstarted
(next: W0.2.S2).

**Files touched:** `native/src/band3_stub_registry.tsv` (new),
`scripts/native/gen_band3_link_stubs.py` (new), `native/src/band3_link_stubs.s`
(regenerated). Build dir used: `native/build-agent-W0.2` (left in place for
S2/S3 reuse per resume contract).
