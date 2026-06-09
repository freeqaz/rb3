# Ghidra capabilities for the RB3 two-binary setup (2026-06-09)

How to make the **Bank 5 (full DWARF, wrong-era body) → Bank 8 (real target body,
names-only)** analysis maximally cohesive. Investigated against the local install
at `/opt/ghidra` (Ghidra **12.1**, build 20260518) by reading the shipped Java
source (`*-src.zip` inside each module's `lib/`) and the example scripts.

Our current bridge is `tools/ghidra/port_dwarf_types.py`: it map-matches mangled
names across the two CodeWarrior maps and applies Bank 5's **function signature**
onto each Bank 8 function via `ApplyFunctionSignatureCmd`. That gets typed params +
transitively-pulled struct types, and nothing else. Everything below is what Ghidra
offers *beyond* that.

---

## TL;DR — ranked

| # | Capability | Value for us | Effort | Verdict |
|---|---|---|---|---|
| 1 | **VT `SymbolNameProgramCorrelator` + `FunctionSignatureMarkupType` apply** (param **names** + param/return **types** + calling-convention + param comments, by exact mangled name) | **Very high** — strict superset of our current cmd; adds param *names* + comments | Low | **DO THIS** |
| 2 | **VT comment/label markup** (`PlateComment`/`Pre`/`EolComment`/`Post`/`Repeatable`/`Label` markup types) ported on byte-identical fns | **High** — Bank 5 DWARF gives plate sigs + any analyst comments/labels | Low–Med | **DO** (gate to exact-byte matches) |
| 3 | **Whole VT session via `AutoVersionTrackingTask`** (one call: symbol + exact-bytes + exact-instr + dupe-instr + implied-match correlators, auto-accept, auto-apply) | **High** — turnkey; also recovers the ~20% divergent fns via byte/instr correlators + implied matches | Low | **DO** (this is the cohesive end state) |
| 4 | **Decompiler Parameter ID analyzer** + `DecompInterface` `commitGlobalParams`/`commitLocalNames` | **High** for Bank 8 *before* porting (recovers `__thiscall`/this-ptr, param storage) | Low | **DO** (run once on Bank 8) |
| 5 | **DataType archive (`.gdt`) export of Bank 5's whole `DataTypeManager`** → resolve into Bank 8 | **Med-High** — brings globals-only / unreferenced types, makes manual struct work share a catalog | Med | **WORTH IT** |
| 6 | **Global VARIABLE typing** (`gHeadMale` etc.) via VT **Data correlator** + `DataTypeMarkupType`, or scripted `getDataAt().setDataType` by map-name | **Med** — types Bank 8 globals so decomp of accessors improves | Med | **DO** (VT data correlator does it for free in #3) |
| 7 | **`FillOutStructureCmd` / Auto Create Structure** (pcode-driven struct recovery) | **Med** — only for *new* structs not in DWARF (rare; DWARF is rich) | Med | Niche |
| 8 | **BSim** similarity DB | Low — we already have exact name matches | High | **SKIP** |
| 9 | **Function ID (`FidDb`)** library labeling | Low — map already names everything incl. libc/MSL | High | **SKIP** |

**Bottom line:** replace the bespoke `ApplyFunctionSignatureCmd` loop with a real
**VT session driven headlessly** (#1–#3). It is a strict superset (param names,
comments, labels, data types, divergent-fn recovery via byte/instr correlators) and
is the officially-supported path. Run the **Decompiler Parameter ID** analyzer on
Bank 8 first (#4), and export Bank 5's DataTypeManager to a `.gdt` once (#5) so the
catalog is shared.

---

## 1–3. Version Tracking (the big one)

VT is a first-class headless API. The whole thing is driveable from pyghidra/Jython
with no GUI. Key packages (source in
`/opt/ghidra/Ghidra/Features/VersionTracking/lib/VersionTracking-src.zip`):

- `ghidra.feature.vt.api.main` — `VTSession`, `VTAssociation`, `VTMatch`,
  `VTMatchSet`, `VTMarkupItem`, `VTAssociationManager`, `VTAssociationStatus`.
- `ghidra.feature.vt.api.db.VTSessionDB` — concrete session (constructable directly,
  no tool: `new VTSessionDB(name, sourceProgram, destinationProgram, consumer)`).
- `ghidra.feature.vt.api.correlator.program.*` — the correlator **factories**.
- `ghidra.feature.vt.api.markuptype.*` — the **markup types** (what gets ported).
- `ghidra.feature.vt.gui.task.ApplyMarkupItemTask` — applies markup with options.
- `ghidra.feature.vt.gui.actions.AutoVersionTrackingTask` — the turnkey one-shot.
- `ghidra.feature.vt.gui.util.VTOptionDefines` + `VTMatchApplyChoices` — every knob.

### Available correlators (factories) and which fit us

From `ghidra/feature/vt/api/correlator/program/`:

| Factory | What it matches | Fit for us |
|---|---|---|
| `SymbolNameProgramCorrelatorFactory` | exact symbol name (functions **and** data), one-to-one only | **PERFECT.** We have 41,680 exact mangled-name matches. This is the primary correlator. |
| `ExactMatchBytesProgramCorrelatorFactory` | byte-identical function bodies | Catches the ~80% non-divergent fns; corroborates symbol matches, feeds implied matches |
| `ExactMatchInstructionsProgramCorrelatorFactory` | identical instruction stream (mnemonic+operands, reloc-insensitive) | Same fns, more robust than raw bytes across reloc/addr deltas |
| `ExactMatchMnemonicsProgramCorrelatorFactory` | identical mnemonics only | Looser variant |
| `DuplicateFunctionMatchProgramCorrelatorFactory` | non-unique identical fns, disambiguated by operands | Recovers small/duplicate fns the unique correlators skip |
| `ExactDataMatchProgramCorrelatorFactory` | byte-identical defined data | Useful for vtables/static data |
| `DuplicateSymbolNameProgramCorrelatorFactory` | non-unique symbol names | N/A — our symbols are unique |
| `SimilarSymbolNameProgramCorrelatorFactory` | fuzzy symbol name | N/A — exact match available |
| `FunctionReferenceProgramCorrelatorFactory`, `DataReferenceProgramCorrelatorFactory`, `CombinedFunctionAndDataReferenceProgramCorrelatorFactory`, `SimilarDataProgramCorrelatorFactory` | call-graph / reference-based propagation from already-accepted matches | **The payoff for the divergent 20%.** Once name + byte matches anchor the program, these score additional matches by shared references. Thresholds tunable (`SIMILARITY_THRESHOLD`, `CONFIDENCE_THRESHOLD`). |

`SymbolNameProgramCorrelatorFactory` (verified in source): ignores default
`FUN_`/`DAT_`/`s_`/`u_` symbols, strips a trailing `_<addr>` suffix, and reports
only symbols with **exactly one** match in the destination — i.e. our every-mangled-
name case is its happy path. `MIN_SYMBOL_NAME_LENGTH` default 3 (our names are long;
irrelevant). `INCLUDE_EXTERNAL_SYMBOLS` default true.

### What markup ports (the win over our current script)

Each accepted `VTAssociation` exposes `getMarkupItems()` — instances of these
`VTMarkupType`s (`ghidra/feature/vt/api/markuptype/`):

- **`FunctionSignatureMarkupType`** — return type, **parameter types**,
  **parameter NAMES**, calling convention, parameter comments, inline/noreturn/
  varargs flags. *This is the upgrade:* `ApplyFunctionSignatureCmd` in our current
  script applies the prototype but loses the **parameter names** and comments that
  Bank 5's DWARF carries. The VT markup applies them (`PARAMETER_NAMES` →
  `SourcePriorityChoices.PRIORITY_REPLACE`, `PARAMETER_COMMENTS` → append/overwrite).
- **`FunctionNameMarkupType`** — the function name (we already have names from the
  map, so set this to `EXCLUDE` to avoid clobbering mangled names).
- **`DataTypeMarkupType`** — the data type at a **data** address (covers area #4:
  global variable typing like `gHeadMale`).
- **`LabelMarkupType`** — labels/symbols at an address.
- **`PlateCommentMarkupType`, `PreCommentMarkupType`, `EolCommentMarkupType`,
  `PostCommentMarkupType`, `RepeatableCommentMarkupType`** — all comment kinds.
  Bank 5's DWARF import generates plate comments (source file/line) and any analyst
  comments; these come across.

> **Caveat — comments/labels on divergent fns:** for the ~20% of functions whose
> Bank 5 *body* differs from Bank 8, EOL/pre/post comments are anchored to
> Bank 5 *instruction addresses* that don't line up with Bank 8. VT only transfers
> **function-entry-based** markup (signature, plate, name) safely for those; per-
> instruction comment markup items will mostly land at the entry or be excluded.
> **Recommendation:** for the byte/instruction-**exact** matches, enable all comment
> markup. For symbol-only matches (possibly divergent body), restrict to
> `FunctionSignatureMarkupType` + `PlateCommentMarkupType`. You can do this by
> running two passes with different apply options (see code sketch).

### The apply-options surface (`VTOptionDefines` + `VTMatchApplyChoices`)

Every knob is a `ToolOptions` entry. The ones that matter for us
(`ghidra/feature/vt/gui/util/VTOptionDefines.java`):

- `FUNCTION_SIGNATURE` → `FunctionSignatureChoices.REPLACE` (take Bank 5's sig wholesale)
- `PARAMETER_DATA_TYPES` → `ParameterDataTypeChoices.REPLACE`
- `PARAMETER_NAMES` → `SourcePriorityChoices.PRIORITY_REPLACE`  ← the names we're missing
- `FUNCTION_RETURN_TYPE` → `ParameterDataTypeChoices.REPLACE`
- `CALLING_CONVENTION` → `CallingConventionChoices.NAME_MATCH`
- `FUNCTION_NAME` → `FunctionNameChoices.EXCLUDE`  ← keep our mangled map names
- `DATA_MATCH_DATA_TYPE` → `ReplaceDataChoices.REPLACE_ALL_DATA` (for global typing)
- `LABELS` → `LabelChoices.ADD`
- `PLATE_COMMENT`/`PRE_COMMENT`/`END_OF_LINE_COMMENT`/`POST_COMMENT`/
  `REPEATABLE_COMMENT` → `CommentChoices.APPEND_TO_EXISTING`

### Headless driving — two routes

**Route A — turnkey `AutoVersionTrackingTask`** (least code; what `AutoVersionTracking
Script.java` wraps). It runs, in order: Exact Symbol → Exact Data → Exact Function
Bytes → Exact Function Instructions → Exact Mnemonics → Duplicate Function →
Reference correlators → Implied matches; auto-accepts unique matches and applies all
markup via `ApplyMarkupItemTask`. Driven from pyghidra:

```python
import pyghidra; pyghidra.start()
from ghidra.feature.vt.api.db import VTSessionDB
from ghidra.feature.vt.gui.actions import AutoVersionTrackingTask
from ghidra.feature.vt.api.util import VTOptions
from ghidra.feature.vt.gui.util import VTOptionDefines as D
from ghidra.util.task import TaskMonitor

# b5 = source (DWARF), b8 = destination (target body); both opened writable-on-b8
session = VTSessionDB("RB3 b5->b8", b5, b8, consumer)
folder.createFile("RB3 b5->b8", session, TaskMonitor.DUMMY)  # persist (optional)

opts = VTOptions("autoVT")
opts.setBoolean(D.RUN_EXACT_SYMBOL_OPTION, True)
opts.setBoolean(D.RUN_EXACT_FUNCTION_BYTES_OPTION, True)
opts.setBoolean(D.RUN_EXACT_FUNCTION_INST_OPTION, True)
opts.setBoolean(D.RUN_DUPE_FUNCTION_OPTION, True)
opts.setBoolean(D.RUN_REF_CORRELATORS_OPTION, True)
opts.setBoolean(D.CREATE_IMPLIED_MATCHES_OPTION, True)
opts.setBoolean(D.APPLY_IMPLIED_MATCHES_OPTION, True)
# tighten the speculative reference matches if false positives appear:
opts.setDouble(D.REF_CORRELATOR_MIN_SCORE_OPTION, 0.95)
opts.setDouble(D.REF_CORRELATOR_MIN_CONF_OPTION, 10.0)

task = AutoVersionTrackingTask(session, opts)
task.run(TaskMonitor.DUMMY)     # or ghidra.util.task.TaskLauncher.launch(task)
b8.save("auto VT", TaskMonitor.DUMMY); session.save()
```

> **Note:** `AutoVersionTrackingTask` reads only the *AutoVT* options (which
> correlators to run + score/conf thresholds). The **apply markup** options
> (`FUNCTION_NAME`=EXCLUDE, `PARAMETER_NAMES`=PRIORITY_REPLACE, comment choices,
> etc.) are pulled from the same `toolOptions` by `ApplyMarkupItemTask`, so set those
> on the *same* `VTOptions` object before constructing the task. By default it would
> apply the function **name** too — set `D.FUNCTION_NAME` → `EXCLUDE` to preserve our
> map-derived mangled names. This is the single most important override for us.

**Route B — explicit correlator loop** (full control over which markup applies for
which match class; recommended for us because of the divergent-body comment caveat).
Mirrors `CreateAppliedExactMatchingSessionScript.java`:

```python
from ghidra.feature.vt.api.correlator.program import (
    SymbolNameProgramCorrelatorFactory, ExactMatchBytesProgramCorrelatorFactory,
    ExactMatchInstructionsProgramCorrelatorFactory)
from ghidra.feature.vt.gui.task import ApplyMarkupItemTask
from ghidra.feature.vt.gui.util.MatchInfoFactory import ...  # MatchInfo for markup

def run_correlator(session, factory, apply_opts):
    f = factory()
    opts = f.createDefaultOptions()
    corr = f.createCorrelator(b5, srcSet, b8, dstSet, opts)
    results = corr.correlate(session, monitor)
    for m in results.getMatches():
        assoc = m.getAssociation()
        if not assoc.getStatus().canApply(): continue
        assoc.setAccepted()
        items = assoc.getMarkupItems(monitor)         # Collection<VTMarkupItem>
        ApplyMarkupItemTask(session, items, apply_opts).run(monitor)
```

Use **strict** apply_opts (signature + names + all comments + labels + data types)
for `ExactMatchInstructions`/`ExactMatchBytes` matches, and a **conservative**
apply_opts (signature + plate only, `FUNCTION_NAME`=EXCLUDE) for the
`SymbolName`-only matches that the byte/instruction correlators did **not** also
confirm (those are the possibly-divergent bodies). Run instruction-exact first so
its matches dominate, then symbol-name for the remainder.

`VTMarkupItem.apply(VTMarkupItemApplyActionType.REPLACE, toolOptions)` is the
per-item entry point (see `CreateAppliedExactMatchingSessionScript` lines 140–167)
if you want item-by-item control instead of `ApplyMarkupItemTask`.

### Example scripts shipped (read these)

`/opt/ghidra/Ghidra/Features/VersionTracking/ghidra_scripts/`:
- **`CreateAppliedExactMatchingSessionScript.java`** — the canonical headless
  session-create + run-3-exact-correlators + apply-markup recipe. Our Route B base.
- **`AutoVersionTrackingScript.java`** — wraps `AutoVersionTrackingTask`; the
  header comment documents the full `analyzeHeadless ... -postScript` invocation.
- **`SetAutoVersionTrackingOptionsScript.java`** — every AutoVT option with defaults;
  copy/edit for headless option setting (stashed in `state` env var `autoVTOptionsMap`).
- **`OverrideFunctionPrototypesOnAcceptedMatchesScript.java`** — iterate accepted
  associations and hand-transfer prototypes (a manual alternative to markup; shows
  the `Function.setReturnType/addParameter/setCallingConvention` path).
- `OpenVersionTrackingSessionScript.java`, `FindChangedFunctionsScript.java`
  (the latter is directly useful: it **identifies which functions changed** between
  the two builds — i.e. flags our divergent ~20% so we know not to trust Bank 5's
  body there).

**Feasibility: high and low-risk.** The session is a normal `DomainObject`; it can
be created transient (not saved) just to drive the apply. The destination program
(Bank 8) must be writable and analyzed; the source (Bank 5) read-only is fine. Run
with the pyghidra-mcp service **stopped** (project lock), same as `port_dwarf_types.py`.

---

## 4. Decompiler quality knobs

Run these on **Bank 8** to improve its names-only decompilation, ideally **before**
porting signatures (so the porter overwrites a good baseline, not a bad one).

### Decompiler Parameter ID analyzer

Registered name **`"Decompiler Parameter ID"`**
(`ghidra/app/plugin/core/analysis/DecompilerFunctionAnalyzer.java`, NAME constant
line 30; backed by `ghidra.app.cmd.function.DecompilerParameterIdCmd`). It runs the
decompiler over every function to recover **parameter storage, count, return
storage, and calling convention** (`__thiscall`/this-pointer recovery for our
member functions). On a names-only program this is the difference between
`undefined4 param_1` guesses and a correctly-sized parameter list. It is **off by
default**; enable it in the Bank 8 analysis options:

```python
from ghidra.app.cmd.function import DecompilerParameterIdCmd
from ghidra.program.model.lang import PrototypeModel
from ghidra.util.task import TaskMonitor
DecompilerParameterIdCmd(
    "Decompiler Parameter ID", b8.getMemory(),
    SourceType.ANALYSIS, decompileTimeoutSecs=60).applyTo(b8, TaskMonitor.DUMMY)
```
or via `setAnalysisOption(b8, "Decompiler Parameter ID", true)` + `analyzeAll`.

### `DecompInterface` options (`ghidra.app.decompiler.DecompInterface`)

The class our `port_dwarf_types.py` already uses. Verified public methods:
- `openProgram(Program)` (line 379)
- `setOptions(DecompileOptions)` (line 668) — feed a `DecompileOptions` configured
  for max recovery
- `toggleSyntaxTree(boolean)` (527), `toggleJumpLoads(boolean)` (632),
  `setSimplificationStyle(String)` (491; `"decompile"` is the full pipeline),
  `setSignatureSettings(int)` (995)
- `decompileFunction(Function, timeoutSecs, monitor)` (776)

`DecompileOptions` (`ghidra/app/decompiler/DecompileOptions.java`) knobs worth
setting: enable **"Respect read-only flags"**, **eliminate unreachable code**, and
crank the simplification. Defaults are fine; the big lever is the Param-ID analyzer
above, not DecompInterface flags.

### Commit recovered prototype to the DB ("Commit Params/Return")

After decompiling, `ghidra.program.model.pcode.HighFunctionDBUtil` is the API behind
the GUI's "Commit Params/Return" and "Commit Locals":
- `HighFunctionDBUtil.commitParamsToDatabase(HighFunction, useDataTypes,
  ReturnCommitOption, SourceType)` — write the decompiler-recovered parameter list
  back as the function's real signature.
- `HighFunctionDBUtil.commitLocalNamesToDatabase(HighFunction, SourceType)` — persist
  recovered local names.

For us this matters in the **reverse** direction too: after VT applies Bank 5's
signature, the decompiler re-derives **locals** in the *real* Bank 8 body. Those
locals are Bank-8-specific (Bank 5's locals don't map onto a divergent body), so
*recover them from Bank 8's own decomp* and commit, rather than porting from Bank 5.

---

## 5. Data Type Archives / DWARF (shared catalog)

Currently `ApplyFunctionSignatureCmd` pulls a struct into Bank 8 only when some
ported signature references it. **Globals-only types and unreferenced types never
come across.** Fix by exporting Bank 5's entire `DataTypeManager` to a `.gdt` once
and resolving it into Bank 8.

`ghidra.program.model.data.FileDataTypeManager` (verified API):
- `FileDataTypeManager.createFileArchive(File, LanguageID, CompilerSpecID)` — make a
  `.gdt` with our PPC `Gekko_Broadway` language so pointer sizes/alignment match.
- `dtm.addDataType(DataType, DataTypeConflictHandler)` / `dtm.resolve(...)`
- `dtm.save()` / `dtm.saveAs(File)` / `dtm.close()`

Conflict handlers (`ghidra.program.model.data.DataTypeConflictHandler`):
`REPLACE_HANDLER`, `KEEP_HANDLER` (USE_EXISTING), `DEFAULT_HANDLER` (RENAME_AND_ADD).

**Export sketch (Bank 5 → `.gdt`):**
```python
from ghidra.program.model.data import (FileDataTypeManager, DataTypeConflictHandler)
from java.io import File
src = b5.getDataTypeManager()
out = FileDataTypeManager.createFileArchive(File("/.../rb3_bank5.gdt"))
tx = out.startTransaction("import")
it = src.getAllDataTypes()
while it.hasNext():
    out.resolve(it.next(), DataTypeConflictHandler.REPLACE_HANDLER)
out.endTransaction(tx, True); out.save(); out.close()
```
**Apply to Bank 8:** open the archive (`FileDataTypeManager.openFileArchive(file,
False)`) and `b8.getDataTypeManager().resolve(dt, KEEP_HANDLER)` for each, or simpler:
`b8.getDataTypeManager().getAllDataTypes()` already shares with the program; just
add the archive as a source and resolve referenced types lazily. In practice the
one-shot full-resolve loop above (into Bank 8's DTM) is the robust path — it makes
Bank 8 carry Bank 5's **entire** type catalog so any later manual struct work and
global typing share one consistent set.

### DWARF analyzer / external debug

The DWARF analyzer is registered as **`"DWARF"`**
(`ghidra/app/plugin/core/analysis/DWARFAnalyzer.java`, `DWARF_ANALYZER_NAME`).
It already ran on Bank 5 (that's where its types/sigs/comments came from) — nothing
to do there.

There **is** an external-debug mechanism
(`ghidra/app/util/bin/format/dwarf/external/` — `ExternalDebugFilesService`,
`DebugInfoProviderRegistry`, `LocalDirDebugInfoDProvider`, build-id matching) that
lets a stripped program borrow DWARF from a sidecar file. **It does not help us**:
it keys on GNU build-id / debuglink, and Bank 8 is a *different build* of the binary,
so addresses don't line up. Name-based VT (#1) is the correct bridge, not external
DWARF. Noted so nobody burns time on it.

---

## 6. Global / data symbol typing (area 4)

Two ways to get Bank 5's global variable types (`gHeadMale : ObjectDir*` etc.) onto
Bank 8 globals:

1. **Free via VT (preferred):** the `SymbolNameProgramCorrelator` matches **data**
   symbols too (it iterates function *and* data symbols), and
   `DataTypeMarkupType` (`ghidra/feature/vt/api/markuptype/DataTypeMarkupType.java`)
   transfers the data type at the matched address. So if you set
   `DATA_MATCH_DATA_TYPE` → `ReplaceDataChoices.REPLACE_ALL_DATA` in the apply
   options, the AutoVT/Route-B pass already types every map-named global. (The
   `ExactDataMatchProgramCorrelator` + reference correlators reinforce these.)
2. **Scripted by map-name** (if you want it standalone): for each shared **data**
   symbol, `dt = b5.getListing().getDataAt(b5Addr).getDataType()`, resolve it into
   Bank 8's DTM, then `ghidra.app.cmd.data.CreateDataCmd(b8Addr, dt).applyTo(b8)` (or
   `b8.getListing().clearCodeUnits(...)` then `createData`). This mirrors what the
   existing `port_dwarf_types.py` does for functions, extended to data.

Route 1 is essentially zero extra code once VT is in place — just an apply option.

---

## 7. Structure recovery (pcode-driven)

`ghidra.app.decompiler.util.FillOutStructureCmd` /
`FillOutStructureHelper` (in `Decompiler.jar`) implement the GUI's **"Auto Create
Structure" / "Fill Out Structure"**: follow a pointer variable through the decompiler's
pcode and synthesize a struct from the offsets that get dereferenced. For us this is
**niche** — Bank 5's DWARF already has the real, named structs, so we rarely need to
*invent* one. Keep it in the back pocket for any Bank-8-only type that has no Bank 5
analogue (e.g. a struct that only exists in the newer build's rewritten code). Not a
priority.

---

## 8–9. BSim and Function ID — skip

- **BSim** (`Ghidra/Features/BSim`, `ghidra.features.bsim.query.*`) builds a feature-
  vector similarity database to find *similar* (not identical) functions across
  unrelated binaries. It's the right tool when you have **no symbols**. We have exact
  mangled-name matches for **all 41,680** shared symbols, so BSim adds nothing but
  setup cost (H2/Postgres DB, signature generation). **Skip.** (If we ever wanted to
  *quantify* how divergent the ~20% rewritten bodies are, `CompareExecutablesScript`/
  `CompareBSimSignaturesScript` could score self-similarity — a curiosity, not needed.)
- **Function ID** (`Ghidra/Features/FunctionID`, `FidDb`) labels **unnamed** library
  functions by hash against a curated DB (e.g. MSVC/MSL libs). Our CodeWarrior map
  already names everything including MSL/libc. **Skip.**

---

## Recommended end-state pipeline (Bank 8)

1. Enable + run the **"Decompiler Parameter ID"** analyzer on Bank 8 (recovers
   this-ptr/`__thiscall` + param storage on the real body).
2. Export Bank 5's `DataTypeManager` → `rb3_bank5.gdt`, resolve it into Bank 8 (shared
   type catalog incl. globals-only types).
3. Build a **VT session** (`VTSessionDB(b5, b8)`); run correlators **instruction-exact
   → bytes-exact → symbol-name → dupe → reference**, with apply options:
   `FUNCTION_NAME=EXCLUDE`, `FUNCTION_SIGNATURE=REPLACE`, `PARAMETER_NAMES=PRIORITY_
   REPLACE`, `PARAMETER_DATA_TYPES=REPLACE`, `DATA_MATCH_DATA_TYPE=REPLACE_ALL_DATA`,
   all `*_COMMENT=APPEND`, `LABELS=ADD`. Use **strict** markup for byte/instr-confirmed
   matches and **signature+plate only** for symbol-only (divergent-body) matches.
4. Optionally run `FindChangedFunctionsScript` first to label the ~20% divergent fns
   so callers know Bank 5's *body* (and per-instruction comments) are untrustworthy there.
5. Re-decompile Bank 8 and `HighFunctionDBUtil.commitLocalNamesToDatabase` to keep
   Bank-8-native local names (don't port locals from a divergent Bank 5 body).

This supersedes `port_dwarf_types.py` (which becomes the "signatures-only fallback").
