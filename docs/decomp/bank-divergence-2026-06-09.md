# The "banks" and why Ghidra (Bank 5) sometimes lies — 2026-06-09

## TL;DR

- The decomp **target is Bank 8** (`orig/SZBE69_B8/sys/main.dol`, ~early-2010).
  It ships **no DWARF ELF** — only a DOL + CodeWarrior map.
- Ghidra (`tools/ghidra/pyghidra-service.sh`) decompiles the **Bank 5** ELF
  (~mid-2009) because it is the only DWARF-bearing build close to the target.
- Bank 5 is ~9–12 months older than Bank 8. **Only 54% of common functions have
  an identical body size; ~20% differ by >128 bytes (a real rewrite).** For those
  ~20%, Ghidra's pseudo-C is a *different function* than the one we're matching.
- m2c (`bin/decompile`) and objdiff both read the **Bank 8** dtk split
  (`build/SZBE69_B8/asm`), so they are always target-accurate. When Bank 5
  diverges: **trust m2c/objdiff, distrust Ghidra source intent.**
- Tooling: `scripts/analysis/bank_divergence.py <SYMBOL>` gives a per-symbol
  verdict; `bin/analyze-function` now prints the warning automatically.

## What are the "banks"?

`milo-executable-library/rb3/` collects Harmonix RB3 Wii **prototype debug
builds**, labelled "Bank N" — successive milestone snapshots during development
(the game shipped Oct 2010). Each carries `main.dol` (+ `band_r_wii.map`, and for
two of them a pre-strip `band_r_wii.elf` with full DWARF).

| Bank | DWARF ELF? | DOL date stamps | Identical-body % vs Bank 8 |
|---|---|---|---|
| Bank 1 | no | Dec 2009 | (no map shipped) |
| Bank 2 | no | Dec 2009 | **99.9%** (32 divergent fns) |
| **Bank 5** | **yes (40 MB)** | mid-2009 (Jan–Jun 2009) | **54.2%** (9,961 divergent) |
| Bank 6 | yes (60 MB) | 2008 | 27.3% (16,161 divergent) |
| **Bank 8** | **no** | early-2010 (Feb/Apr 2010) | **— (this IS the target)** |

`Wii Proto (Bank 8)` is **byte-identical** to `orig/SZBE69_B8` (sha1 of both
`main.dol` and `band_r_wii.map` match).

### The cruel irony

The bank that matches the target almost perfectly (**Bank 2, 99.9%**) shipped
**no ELF**. The only DWARF-bearing banks (5, 6) are from a different era. Using
Bank 5 over Bank 6 was the right call (Bank 6 is even more divergent), but Bank 5
still diverges from the target on ~46% of functions.

There is **no way to get DWARF that matches Bank 8** — DWARF lives only in the
pre-strip `.elf`, and Bank 8/Bank 2 didn't ship theirs. Bank 5 is the best DWARF
we have; the fix is to know *when* to distrust it, not to abandon it.

## Divergence, quantified (Bank 5 vs Bank 8, 21.8k common functions)

| magnitude | count | share | meaning |
|---|---|---|---|
| identical | 11,798 | 54.2% | Bank 5 DWARF matches target |
| ≤4 B | 619 | 2.8% | trivial codegen drift |
| 5–32 B | 2,578 | 11.8% | minor; source intent reliable |
| 33–128 B | 2,389 | 11.0% | moderate; cross-check |
| **>128 B** | **4,375** | **20.1%** | **major rewrite — Ghidra misleads** |

For the **~8,200 still-open targets** (functions <100% in `report.json` present
in both maps): **29.6% diverge, 841 (10.3%) major-diverge** — those 841 are the
BandHeadShaper-class traps where the default Ghidra leg actively misleads.

Plus **11,751 function symbols exist in Bank 8 but not Bank 5** (by exact mangled
name) — new/renamed functions for which Ghidra has no DWARF body at all.

## What to do

1. **Keep Bank 5 as the DWARF source.** It's right ~69% of the time (TRUST +
   trivial/minor) and gives types/locals nothing else does.
2. **Gate it per-symbol.** `scripts/analysis/bank_divergence.py <SYMBOL>` →
   `TRUST` / `CAUTION` / `MISLEADING` / `NO_DWARF`. `bin/analyze-function` prints
   a `⚠ [bank-divergence: …]` banner above the Ghidra section automatically.
3. **On MISLEADING, use the Bank-8 path:** `bin/decompile` (m2c on
   `build/SZBE69_B8/asm`) + objdiff. Both decode the actual target bytes.

### Bank 8 Ghidra decompilation — BUILT (2026-06-09)

Ghidra now decompiles the **real Bank 8 body**, not just m2c. How it works:

1. **DOL → symbolized Gekko ELF.** Ghidra has no DOL loader and a DOL has no
   symbols. `pyghidra_mcp.gamecube_dol` (in our pyghidra-mcp fork) transcodes the
   Bank 8 `main.dol` + CodeWarrior `band_r_wii.map` into a big-endian PPC ELF:
   each DOL segment becomes a section at its load address; every map symbol
   becomes a `.symtab` entry with address + size, typed `STT_FUNC` in executable
   segments. Ghidra's ELF loader then maps memory and auto-creates 41k functions
   with exact boundaries. The `0x8xxxxxxx` entry point makes the fork auto-select
   `PowerPC:BE:32:Gekko_Broadway`. Validated byte-for-byte against the DOL.
2. **One server, two programs.** pyghidra-mcp is multi-binary; the 8001 service
   loads Bank 5 (`band_r_wii`) AND the Bank 8 ELF (`bank8_target`). Every tool
   call selects one by `binary_name`. (Ghidra does NOT auto-correlate them; our
   tooling routes per call.) The fork also imports a raw `.dol` transparently
   (`build_import_plan` → `gamecube_dol.ensure_elf_for_dol`).

Use it:
- `bin/analyze-function --bank8 SYMBOL` — Bank 8 program on the live server.
- `tools/ghidra/bank8_decompile.py SYMBOL` — standalone (no service; own project).
- Service: `tools/ghidra/pyghidra-service.sh start` loads both; opt out of Bank 8
  with `RB3_GHIDRA_NO_BANK8=1`.

Proven on the motivating case: `Init__14BandHeadShaperFv` decompiles to the full
3084-byte Bank 8 body — including the `MemFindHeap`/`MemPushHeap`/`MemPopHeap`
calls and the final `gVisemes` null-guard loop that the Bank 5 view had elided.

### Mapping Bank 5 DWARF types onto Bank 8 — the cohesive view (2026-06-09)

The Bank 8 program has the real body but no types; Bank 5 has full DWARF types
but a wrong-era body for ~20% of functions. `tools/ghidra/port_dwarf_types.py`
bridges them: it matches functions by **mangled name** across the two CodeWarrior
maps (the builds have different addresses but the same symbols — 41,680 shared)
and applies Bank 5's typed **function signature** onto each Bank 8 function with
`ApplyFunctionSignatureCmd`, which resolves the referenced struct/class types into
the Bank 8 program transitively. Ghidra's decompiler then propagates those types
through the **real Bank 8 body**.

Result (proven): `GetNum__FPCciP9ObjectDiri` goes from
`void GetNum(undefined4, undefined4, undefined4, int)` to
`int GetNum(int fpn, ObjectDir *dir, int test)` — return type, struct-pointer
param types, and param names, all on the target body. Porting *signatures* is safe
even for divergent-body functions because a function's interface is stable even
when its body changed.

All project-mutating enrichment runs with the service **stopped** (it holds the
project lock); restart after. The `--all` port saves periodically (a long run
can't lose everything to a timeout).
```
tools/ghidra/pyghidra-service.sh stop
cd ../pyghidra-mcp && uv run --python 3.10 --project . \
    python ../rb3/tools/ghidra/port_dwarf_types.py --all   # ports + saves all 41,680 (~100 min)
tools/ghidra/pyghidra-service.sh start
```
After this, `bin/analyze-function SYMBOL` (default Bank 8) shows the target body
**with types**.

### Small-data-area register fix — LANDED + persists (verified)

Our synthetic Bank 8 ELF never ran a GameCube loader, so Ghidra didn't know the
Gekko SDA bases; CodeWarrior addresses small-data globals as `r13 + off` / `r2 + off`,
which decompiled as noise (`*(char *)(unaff_r13 + -0x6400)`). `tools/ghidra/enrich_bank8.py`
reads `_SDA_BASE_`/`_SDA2_BASE_` from the map, sets them as program-wide register
context, **then runs the PowerPC Constant Reference Analyzer** (`PowerPCAddressAnalyzer.added()`)
over the executable blocks so the `r13/r2+off → global` *references* are materialized
to the program DB — that is what makes the fix survive save+reload (setting the
context value alone is transient). **Verified: r13-noise in `BandHeadShaper::Init`
is 0 in a fresh `--verify-only` reload.** (Absolute-addressed globals like `gHeadMale`,
`sChinNum` are named regardless; only `.sdata`/`.sbss` need this.)

GOTCHA — decompile cache staleness: pyghidra-mcp's decompile cache
(`rb3/cache.db`) keys by `(address, ELF-file-hash)`. In-project enrichment
(SDA/signatures/VT) modifies the PROGRAM, not the ELF file, so the hash is
unchanged and `invalidate_on_binary_change` never fires — the live server keeps
serving pre-enrichment decompilations. **Clear `cache.db` (service stopped) after
any enrichment** for the changes to show via `analyze-function`. (Fork TODO:
invalidate the cache on program-modification, or add a clear-cache MCP tool.)

### Version Tracking — global var TYPES + comments (supported headless path)

VT ports markup from Bank 5 → Bank 8 — a superset of the signature port: also
global-variable *types*, labels, and comments — with `FunctionNameMarkupType`
excluded to keep our mangled names.

Hand-rolling the session in pyghidra does **not** work headless: `VTSessionDB`
construction throws `LockException` at `addSynchronizedDomainObject` (it wants
exclusive control of the dest program, which the `GhidraProject.openProgram`
consumer model doesn't grant — cf. GH discussion #5362). The **supported** path is
`analyzeHeadless`, which runs `AutoVersionTrackingTask` inside a headless tool that
supplies the `ServiceProvider` the task expects. Wrapped up as:
```
tools/ghidra/pyghidra-service.sh stop
JAVA_HOME=/usr/lib/jvm/java-21-openjdk tools/ghidra/run_version_tracking.sh
# auto-detects program names; SetRB3VTOptionsScript sets FUNCTION_NAME=EXCLUDE +
# the correlators; RB3AutoVersionTrackingScript runs the task. Then:
rm -f rb3/cache.db   # bust the stale decompile cache (see SDA gotcha above)
tools/ghidra/pyghidra-service.sh start
```
GOTCHA: the Ghidra domain-file session name can't contain `>` (use `_`). Run with
JDK 21 (Ghidra 12.1). The signature port + SDA fix already deliver typed params +
named globals; VT layers global-var *types* + comments on top.

## Method (reproducible)

`scripts/analysis/bank_divergence.py --report` rebuilds the index by parsing the
`size` column (col 2) of both CodeWarrior maps, keyed by mangled symbol. Identical
size is a strong proxy for identical body; size delta is a proxy for divergence
magnitude. The cache lives at `build/SZBE69_B8/bank_divergence.json` and
auto-refreshes when either map is newer.
