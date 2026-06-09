# Ghidra Extensions & Techniques Research — RB3 Decomp Context
*Research date: 2026-06-09*

## Setup Reminder

Our Ghidra project has **two programs**:
- **Bank-5 debug ELF** — DWARF-rich (~2009 build), good types/signatures, body may diverge ~20%
- **Target ELF (Bank-8)** — real bodies from the 2010 `.dol` + CodeWarrior `.map`, mangled names only, no types

We use pyghidra + a fork of pyghidra-mcp to drive Ghidra headlessly. Goal: find tools to make cross-program type/signature porting more powerful and cohesive.

---

## 1. Version Tracking Automation

### Ghidra Built-in Version Tracking (VT)
- **URL**: https://github.com/NationalSecurityAgency/ghidra/blob/master/Ghidra/Features/VersionTracking/src/main/help/help/topics/VersionTrackingPlugin/VT_Workflow.html
- **What it does**: Matches functions between two binaries via "correlators" (byte hash, instruction hash, structural graph, symbol name, BSim, etc.) and can port *markup* — names, comments, **function signatures** (return type + parameter types/names), and data-type annotations — from source to destination.
- **Relevance: HIGH** — This is exactly our core problem (Bank-5 DWARF types → Bank-8 target). The key insight: before running correlators, you must enable the option to **overwrite function signatures and parameters**, otherwise only the name transfers, not the types.
- **How we'd use it**: Run `SymbolNameProgramCorrelator` (exact mangled-name match) as the seed, then cascade with `ExactInstructionsFunctionHasher` / `StructuralGraphHash` for functions whose bodies diverge slightly between builds. Apply markup including full function signatures.
- **Caveat**: `AutoVersionTrackingScript.java` requires a GUI tool — it is **not headlessly runnable as-is**. See workaround below.

### Headless VT Workaround — `GhidraVersionTrackingScript` base class
- **URL**: https://github.com/dragonGR/Ghidra/blob/master/Ghidra/Features/VersionTracking/src/main/java/ghidra/feature/vt/GhidraVersionTrackingScript.java
- **What it does**: Abstract Java base class providing `createVersionTrackingSession()`, `runCorrelator()`, `getMatchesFromLastRunCorrelator()`. Does NOT apply markup yet (has TODO), but gives the session/correlator scaffolding.
- **Relevance: HIGH** — Starting point for writing our own headless VT script. The design gap (no GUI tool in headless) can be papered over with `ServiceProviderStub`. Confirmed by the Ghidra discussion at https://github.com/NationalSecurityAgency/ghidra/discussions/5362.
- **How we'd use it**: Subclass this, wire in ServiceProviderStub, run `SymbolNameProgramCorrelator` (our mangled names are a perfect seed), auto-accept exact matches, then call the markup-apply API to copy function signatures to Bank-8. This would replace our current manual DWARF-to-target porting.

### ghidra-patchdiff-correlator
- **URL**: https://github.com/threatrack/ghidra-patchdiff-correlator
- **What it does**: Extra VT correlators that can score matches *below 1.0* (partial similarity) — BulkInstructionsMnemonicsMatch, BulkBasicBlockMnemonicsMatch, etc. Standard Ghidra correlators only produce exact (1.0) matches; this fills the gap for functions whose bodies differ between Bank-5 and Bank-8.
- **Relevance: HIGH** — About 20% of Bank-5 bodies differ from Bank-8. These fuzzy correlators could still find structural matches for those functions where byte-exact matching fails, letting us port types even for divergent bodies.
- **How we'd use it**: Install as a VT plugin, use after the exact-match seed pass to catch structurally similar functions.

### CHDK Ghidra VT Workflow Guide
- **URL**: https://chdk.fandom.com/wiki/Ghidra_Version_Tracking_workflow_for_porting (HTTP 403 on direct fetch, but confirmed exists)
- **What it does**: Community guide describing a practical VT workflow for camera firmware versions — closely analogous to our two-build problem.
- **Relevance: MED** — Read for practical workflow tips; likely describes the exact correlator ordering we need.

### ghidriff — Python Binary Diffing Engine
- **URL**: https://github.com/clearbluejar/ghidriff
- **What it does**: Headless Python tool (pyghidra-based) that diffs two binaries and identifies added/deleted/modified functions. Uses VT correlators under the hood (SymbolsHash, ExactBytes, ExactInstructions, StructuralGraphHash, BulkBasicBlockMnemonics). Outputs JSON + markdown. Fully scriptable and extensible via `GhidraDiffEngine` base class.
- **Relevance: MED** — Useful to audit how much Bank-5 vs Bank-8 actually diverges per-function. Does not port types itself, but exposes the same correlator pipeline we'd use. The extensible base class is a better starting point for a custom headless VT-with-markup tool than the raw Ghidra API.
- **How we'd use it**: Run it over our two programs to generate a per-function divergence map; use the match results as an oracle to decide which functions are safe for automatic type porting vs. need manual review.

---

## 2. Cross-Binary Diffing / Type Porting

### Ghidra BSim (Behavioral Similarity)
- **URL**: https://ghidra.re/ghidra_docs/GhidraClass/BSim/BSimTutorial_Intro.html
- **What it does**: Builds p-code feature vectors for every function; finds semantically similar functions across compiler variants, architectures, and minor code changes via cosine similarity. Works across PPC/x86/ARM/etc. Databases: H2 (local file), PostgreSQL, Elasticsearch.
- **Relevance: MED** — Useful for finding Bank-5 ↔ Bank-8 matches that are structurally similar but compile differently (different CW version → different register allocation). Does NOT transfer types — function matching only. Could seed VT sessions.
- **How we'd use it**: Build an H2 BSim DB from Bank-5 (which has DWARF-derived function names), query it against Bank-8. High-similarity results become trusted name/type porting candidates even when instruction hashes differ.
- **Caveat**: BSim is a VT correlator too (`BSim Program Correlator` in the VT plugin list), so it can be used directly in the VT markup-porting pipeline.

### Diaphora
- **URL**: http://diaphora.re/ / https://github.com/joxeankoret/diaphora
- **What it does**: The most capable binary differ, supporting name + comment porting, CFG diffing, similarity scoring. IDA-only; Ghidra port is "in development but will take very long."
- **Relevance: LOW** — IDA-only today. Not actionable for our headless pyghidra setup.

### ghidra2dwarf (cesena)
- **URL**: https://github.com/cesena/ghidra2dwarf
- **What it does**: Exports decompiled functions + types from a Ghidra program into DWARF sections embedded in a new ELF. Primarily for GDB source-level debugging. Supports headless mode.
- **Relevance: LOW** — Wrong direction for us (we want to *import* DWARF into Bank-8, not export from it). Might be useful if we ever want to expose Bank-8's recovered types to GDB/LLDB.

### ghidra-ExportDwarfELFSymbols (aldelaro5)
- **URL**: https://github.com/aldelaro5/ghidra-ExportDwarfELFSymbols
- **What it does**: Format-agnostic Ghidra script that creates an ELF with DWARF symbols (function names + entry/end addresses) from whatever Ghidra knows. Supports PPC32 big-endian. Input-format agnostic.
- **Relevance: LOW-MED** — Useful if we want to produce a symbol-annotated ELF from our Bank-8 program for use with external debuggers or analysis tools. Names only, no types.

### Ghidra DataType Archives (.gdt)
- **URL**: https://ghidra.re/ghidra_docs/api/ghidra/program/model/listing/DataTypeArchive.html
- **What it does**: Ghidra can export all data types from a program to a `.gdt` archive file, which can then be shared across programs and projects. Types in a .gdt file remain linked to their source archive.
- **Relevance: MED** — We can extract all struct/class/typedef/enum types recovered from Bank-5's DWARF into a `.gdt` file, then attach that archive to Bank-8. Types become available for manual or scripted application. This is complementary to VT (VT transfers per-function signatures; .gdt makes types available project-wide).
- **How we'd use it**: Script to export Bank-5's DataTypeManager to a `.gdt`, open it as a shared archive in Bank-8, then use VT or a custom script to apply those types to matched functions.

---

## 3. GameCube/Wii Gekko/Broadway Ghidra Support

### Ghidra-GameCube-Loader (Cuyler36) — PRIMARY LOADER
- **URL**: https://github.com/Cuyler36/Ghidra-GameCube-Loader
- **What it does**: Ghidra loader for `.dol`, `.rel`, Apploader, RAM dumps. Includes the Gekko/Broadway language definition (paired singles + dcbz_l). Optional CodeWarrior `.map` import with **automatic namespace creation and demangling**. Latest release: Ghidra 12.1 (May 2026). Actively maintained (22 releases).
- **Relevance: HIGH** — Already the right choice for importing our target DOL with the `.map` file. Key: it handles `Foo__5BarFv` demangling into `Bar::Foo` namespaces automatically, which matches how we already have things structured. Confirm we're on the latest release.
- **How we'd use it**: Already using it (or should be). Verify map import options are enabled and demangling is on. The `.map` import creates named symbols that our VT `SymbolNameProgramCorrelator` can then match against Bank-5.

### ghidra-gekko-broadway-lang (aldelaro5) — NOW MERGED INTO ABOVE
- **URL**: https://github.com/aldelaro5/ghidra-gekko-broadway-lang
- **What it does**: Sleigh language definition adding paired-single (`psq_*`, `ps_*`) instructions and GQR-based quantize/dequantize p-code ops. **Now merged into Cuyler36's GameCube Loader** — no separate install needed.
- **Relevance: HIGH** (already handled by the loader) — The paired-single p-code ops (`quantize`/`dequantize` with GQR type+scale) are critical for decompiler output on GQR-using code. Worth knowing they're there.

### GCAnalyzer — Automatic r2/r13 SDA Register Setup
- **URL**: https://github.com/Cuyler36/Ghidra-GameCube-Loader/blob/master/src/main/java/gamecubeloader/analyzer/GCAnalyzer.java
- **What it does**: Analyzes the `__init_registers` function at startup to automatically detect and set r2 (SDA2/_SDA2_BASE_) and r13 (SDA/_SDA_BASE_) register values across the full program address range. Also sets GQR0–GQR7. This is the fix for the longstanding Ghidra issue #325 where r13 SDA accesses decompile as `*(int*)(r13 + 0x1234)` garbage.
- **Relevance: HIGH** — This is baked into the GameCube Loader plugin and runs automatically. If we're loading our Bank-8 ELF with the loader, we get this for free. If we're loading a manually constructed ELF, we need to ensure this runs or replicate its logic.
- **How we'd use it**: Confirm the GCAnalyzer fires on our Bank-8 program. If not (since we synthetic-constructed it from a DOL), set r2/r13 manually via script or the Register Manager using values from the DOL's `__init_registers`.

### Ghidra Issue #325 — r13 SDA not understood by decompiler
- **URL**: https://github.com/NationalSecurityAgency/ghidra/issues/325
- **What it does**: Documents the root problem (Ghidra stock PPC only treats r2 as SDA, not r13). The GCAnalyzer above is the practical fix. Issue is closed; the loader is the resolution path.
- **Relevance: MED** (background context) — Explains why the loader's GCAnalyzer matters. If we ever load PPC binaries without the loader, we need the manual `Set Register Values` workaround.

### GhidraRPXLoader (Maschell) — Wii U RPX/RPL
- **URL**: https://github.com/Maschell/GhidraRPXLoader
- **What it does**: Loader for Wii U RPX/RPL format, also based on the Gekko/Broadway language.
- **Relevance: LOW** — Wii U (Espresso) not our target. Mentioned for completeness.

---

## 4. DWARF Import into Ghidra

### Ghidra Built-in DWARF Analyzer
- **URL**: https://ghidra.re/ghidra_docs/api/ghidra/app/util/bin/format/dwarf/DWARFProgram.html
- **What it does**: Ghidra ships a DWARF2/3/4/5 analyzer that runs automatically on ELF imports with `.debug_info` sections. Can load external DWARF via dSYM-style `ExternalDebugInfo` pointers. Imports function names, parameter types, local variable names, structs/classes, enums, typedefs.
- **Relevance: HIGH** — This is already running on our Bank-5 debug ELF and providing the rich type information we have. The limitation: it does NOT support DWARF1. CodeWarrior's DWARF output is nominally DWARF2 (CW used a "DWARF1.1" transitional format for older targets, but the mwcceppc v4.3 era emits proper DWARF2). Confirm our debug ELF's DWARF version with `readelf -wi`.
- **How we'd use it**: Already exploiting this. Key issue: Ghidra does not support loading DWARF from a *separate* ELF onto an *existing* program (Issue #56 / #4458). This is the gap that the VT-based approach must fill.

### ghidra-dwarf1 (rafalh)
- **URL**: https://github.com/rafalh/ghidra-dwarf1
- **What it does**: Community extension adding DWARF1 analysis to Ghidra. Created for PS2 binaries (MIPS/DWARF1). Self-described as "very incomplete" and "may not work with other files." Last release v0.2 from March 2021.
- **Relevance: LOW-MED** — Only needed if our Bank-5 ELF uses actual DWARF1 (pre-DWARF2 CodeWarrior format). The mwcceppc v4.3.172 era should emit DWARF2. Check with `readelf -wi orig/...`. If it shows DWARF version 1 entries, this becomes more relevant. The PS2-focus means PowerPC behavior is untested and likely buggy.
- **How we'd use it**: Last resort if DWARF2 import is failing on some compilation units.

### Ghidra Issue #4458 — External DWARF File Loading
- **URL**: https://github.com/NationalSecurityAgency/ghidra/issues/4458
- **What it does**: Feature request to load DWARF from a separate file (the `.debug` ELF split from the stripped binary). Not yet implemented beyond macOS dSYM.
- **Relevance: MED** (explains gap) — Confirms that loading Bank-5's DWARF onto Bank-8 directly is not natively possible. The VT-based type-porting workflow is the correct architectural answer.

---

## 5. Matching-Decomp Ecosystem

### objdiff (encounter)
- **URL**: https://github.com/encounter/objdiff
- **What it does**: The core `.o` file differ used by all GC/Wii decomp projects. Supports ARM, ARM64, MIPS, PPC, SH, x86. Powers decomp.me's web frontend and VS Code extension. Milohax fork adds `--include-data` and instruction branch graphs.
- **Relevance: HIGH** (already using it) — No additional Ghidra integration needed; objdiff and Ghidra serve different roles (objdiff compares object files; Ghidra provides decompilation context).

### decomp-toolkit (encounter)
- **URL**: https://github.com/encounter/decomp-toolkit
- **What it does**: Splits `.dol`/`.rel` into per-TU `.o` files, processes CodeWarrior `.map` files, demangles MWCC symbols (`cwdemangle`), generates Ghidra-compatible ELFs. This is what produces our synthetic Bank-8 ELF.
- **Relevance: HIGH** (already using it) — The `.map`-derived symbol names it embeds are what the `SymbolNameProgramCorrelator` can match against Bank-5. No additional action needed.

### WiiBrew Ghidra Guide
- **URL**: https://wiibrew.org/wiki/Using_Ghidra_with_the_Wii
- **What it does**: Community wiki article covering GameCube Loader install, Gekko language selection, r2/r13 register configuration, IOS ARM analysis, FunctionID databases.
- **Relevance: MED** — Good checklist for setup hygiene. Confirms FunctionID databases exist for some Wii SDK components (though not publicly distributed for all SDK versions).

### FunctionID Databases for Wii SDK
- **URL**: https://github.com/threatrack/ghidra-fidb-repo
- **What it does**: Community repo of `.fidb` Function ID databases for various targets. A `.fidb` contains function hashes; Ghidra matches them against unknown code to identify library functions.
- **Relevance: MED** — If someone has built a `.fidb` from the Wii SDK (RVL_SDK, NW4R, etc.), we could auto-identify SDK functions in Bank-8 without needing symbols. The GBAtemp community has discussed building Wii-SDK FIDBs; no canonical public release found but the technique is valid.
- **How we'd use it**: Build a `.fidb` from the Wii SDK headers + compiled objects (or from games with unstripped ELFs like 007: Quantum of Solace) and run FunctionID analysis on Bank-8 to auto-name SDK functions.

### Ghidra CodeWarrior Demangler (Cuyler36) — ARCHIVED
- **URL**: https://github.com/Cuyler36/Ghidra-CodeWarriorDemangler
- **What it does**: Standalone Ghidra extension to demangle CodeWarrior (`Foo__5BarFv` → `Bar::Foo`) symbol names. Archived Feb 2026.
- **Relevance: LOW** — Functionality is now **built into the GameCube Loader**. No separate install needed. Mentioned because some older guides still reference this standalone plugin.

### ret-sync
- **URL**: https://github.com/bootleg/ret-sync
- **What it does**: Synchronizes a live GDB/LLDB/WinDbg session with Ghidra (or IDA/BN) so the disassembler highlights the current instruction as you step in the debugger.
- **Relevance: LOW-MED** — Useful if debugging RB3 under Dolphin (which has a GDB stub) and want Ghidra to follow along. Dolphin exposes a GDB server on port 2345; ret-sync + GDB + Ghidra would give live symbol resolution during Dolphin sessions. Last major release 2022; GDB support is stable.
- **How we'd use it**: `target remote :2345` in GDB, start ret-sync GDB client, open Ghidra ret-sync plugin → decompiler follows execution live. Good for validating function boundary analysis.

---

## 6. Other Useful Tools

### pyghidra-mcp (clearbluejar) — pyghidra-based MCP Server
- **URL**: https://clearbluejar.github.io/posts/pyghidra-mcp-headless-ghidra-mcp-server-for-project-wide-multi-binary-analysis/
- **PyPI**: https://pypi.org/project/pyghidra-mcp/
- **What it does**: Headless Ghidra MCP server that exposes an *entire Ghidra project* (multiple programs) over MCP, enabling LLM-assisted cross-binary analysis in a single session. Exposes: function decompilation, cross-references, symbol lookup, import/export analysis, all across multiple binaries. v0.2.0 adds GUI-backed mode.
- **Relevance: HIGH** — This is the upstream of our pyghidra-mcp fork. Key feature for us: it already understands that a project can contain multiple programs (Bank-5 and Bank-8 both loaded) and can cross-reference between them. Our fork should be rebased to pick up the v0.2.0 multi-binary improvements.
- **How we'd use it**: An LLM agent (or our own MCP tools) could ask "show me the Bank-5 decompilation for `BandSong::Load` and the Bank-8 disassembly for the same address range" in one query, enabling semi-automated type-porting decisions.

### bethington/ghidra-mcp — 200+ MCP Tools
- **URL**: https://github.com/bethington/ghidra-mcp
- **What it does**: Heavier Ghidra MCP server with 200+ tools, Docker support, batch operations, Ghidra Server integration. GUI plugin + headless. Covers data-type management, function analysis, cross-references.
- **Relevance: MED** — Feature-rich alternative to pyghidra-mcp. The data-type management MCP tools (per DeepWiki docs) could expose the `.gdt` export/import workflow over MCP, enabling automated type porting pipelines.

### Ghidrathon (Mandiant/FLARE)
- **URL**: https://github.com/mandiant/Ghidrathon
- **What it does**: Embeds CPython3 inside Ghidra via Jep, giving full Python3 access to the Ghidra API (including pip-installed packages like numpy, capstone, etc.). Works headlessly.
- **Relevance: MED** — The built-in PyGhidra (now merged into Ghidra 11+) does the same thing. Only needed if our Ghidra version predates PyGhidra integration (< 11.1) or if we need specific Jep behavior. Our `pyghidra` library already provides this bridge.
- **How we'd use it**: Redundant with our current pyghidra setup. Not needed unless we hit a pyghidra limitation.

### Ghidra 11.x / 12.x Built-in PyGhidra
- **URL**: https://github.com/NationalSecurityAgency/ghidra/blob/master/Ghidra/Features/PyGhidra/README.md
- **What it does**: Upstream NSA absorption of the pyghidra project. Ships CPython3 bridge, virtual environment support, headless + GUI script execution. Current in Ghidra 11+ / 12.x.
- **Relevance: HIGH** — The foundation everything else runs on. Ensure our Ghidra version (currently driving at ~12.0–12.1 per GameCube Loader) has this built in.

### BSim Program Correlator (inside VT)
- **URL**: https://ghidra.re/ghidra_docs/GhidraClass/BSim/BSimTutorial_Intro.html
- **What it does**: BSim is also exposed as a VT correlator, meaning it can participate in the Version Tracking pipeline directly — fuzzy-matching Bank-5 vs Bank-8 even when instructions differ. H2 local DB requires no server.
- **Relevance: MED** — Add this to the VT correlator cascade after the exact-hash correlators for the ~20% divergent body functions.

---

## Summary: Top 5 Most Actionable Things to Try

### 1. Write a headless VT script with SymbolNameProgramCorrelator + markup apply (HIGHEST VALUE)
Our mangled names are already a perfect seed for `SymbolNameProgramCorrelator`. The missing piece is wiring this into a headless script that also applies markup (function signatures with types). Starting point: `GhidraVersionTrackingScript` base class + `ServiceProviderStub` workaround documented in https://github.com/NationalSecurityAgency/ghidra/discussions/5362. Expected outcome: automatic propagation of Bank-5's DWARF-recovered parameter types and return types onto every exactly-named function in Bank-8. This replaces the current manual DWARF-porting step at scale.

### 2. Install ghidra-patchdiff-correlator for fuzzy-body matching
For the ~20% of functions whose bodies differ between Bank-5 and Bank-8, the stock exact-hash correlators produce no match. Install https://github.com/threatrack/ghidra-patchdiff-correlator and run `BulkMnemonicsMatchProgramCorrelator` + `BulkBasicBlockMnemonicsMatch` as a second pass after the exact-name seed. These score partial structural similarity and can propose matches for manually-accepted type porting.

### 3. Verify GameCube Loader's GCAnalyzer is running on Bank-8 + confirm r2/r13
The `GCAnalyzer` in https://github.com/Cuyler36/Ghidra-GameCube-Loader auto-sets r2 and r13 from `__init_registers`, fixing the decompiler's SDA dereference garbage. Since our Bank-8 ELF is a synthetic re-assembled DOL, confirm this analyzer fired and r2/r13 are set. If not, set them via script (`setRegisterValue(r13, _SDA_BASE_)` over the full address range). This single fix can dramatically improve decompiler output for global-variable-heavy functions.

### 4. Build a .gdt DataType Archive from Bank-5 for project-wide type availability
Export Bank-5's full DataTypeManager (all structs, classes, enums, typedefs recovered from DWARF) to a `.gdt` file, then attach it as a shared archive to Bank-8. Types then appear in Bank-8's data type manager and can be applied to variables, parameters, and return values manually or via script — even for functions the VT correlator could not match. Use `DataTypeManager.saveToPackedFile()` from a pyghidra script. Reference: https://ghidra.re/ghidra_docs/api/ghidra/program/model/listing/DataTypeArchive.html

### 5. Use ghidriff to audit inter-build divergence before writing VT automations
Run https://github.com/clearbluejar/ghidriff over Bank-5 vs Bank-8 to produce a JSON function-by-function divergence map. This tells us: (a) which functions have identical instruction streams (safe for automatic type porting), (b) which are structurally similar but not byte-identical (candidates for fuzzy VT), and (c) which are wholesale different (need manual review). Feed category (a) directly to the auto-apply markup pipeline; triage (b) and (c) separately. This avoids accidentally porting types from a Bank-5 function whose body is too different from Bank-8's to trust the match.

---

## URL Reference Index

| Tool | URL |
|---|---|
| Ghidra VT workflow docs | https://github.com/NationalSecurityAgency/ghidra/blob/master/Ghidra/Features/VersionTracking/src/main/help/help/topics/VersionTrackingPlugin/VT_Workflow.html |
| Headless VT discussion | https://github.com/NationalSecurityAgency/ghidra/discussions/5362 |
| GhidraVersionTrackingScript | https://github.com/dragonGR/Ghidra/blob/master/Ghidra/Features/VersionTracking/src/main/java/ghidra/feature/vt/GhidraVersionTrackingScript.java |
| ghidra-patchdiff-correlator | https://github.com/threatrack/ghidra-patchdiff-correlator |
| ghidriff | https://github.com/clearbluejar/ghidriff |
| Ghidra BSim intro | https://ghidra.re/ghidra_docs/GhidraClass/BSim/BSimTutorial_Intro.html |
| Ghidra DataTypeArchive API | https://ghidra.re/ghidra_docs/api/ghidra/program/model/listing/DataTypeArchive.html |
| Ghidra-GameCube-Loader (Cuyler36) | https://github.com/Cuyler36/Ghidra-GameCube-Loader |
| ghidra-gekko-broadway-lang | https://github.com/aldelaro5/ghidra-gekko-broadway-lang |
| GCAnalyzer (r2/r13 auto-set) | https://github.com/Cuyler36/Ghidra-GameCube-Loader/blob/master/src/main/java/gamecubeloader/analyzer/GCAnalyzer.java |
| Ghidra issue #325 (r13 SDA) | https://github.com/NationalSecurityAgency/ghidra/issues/325 |
| ghidra-dwarf1 | https://github.com/rafalh/ghidra-dwarf1 |
| Ghidra issue #56 (external DWARF) | https://github.com/NationalSecurityAgency/ghidra/issues/56 |
| objdiff | https://github.com/encounter/objdiff |
| decomp-toolkit | https://github.com/encounter/decomp-toolkit |
| WiiBrew Ghidra guide | https://wiibrew.org/wiki/Using_Ghidra_with_the_Wii |
| ghidra-fidb-repo | https://github.com/threatrack/ghidra-fidb-repo |
| CW Demangler (archived) | https://github.com/Cuyler36/Ghidra-CodeWarriorDemangler |
| ret-sync | https://github.com/bootleg/ret-sync |
| pyghidra-mcp | https://clearbluejar.github.io/posts/pyghidra-mcp-headless-ghidra-mcp-server-for-project-wide-multi-binary-analysis/ |
| pyghidra-mcp PyPI | https://pypi.org/project/pyghidra-mcp/ |
| bethington/ghidra-mcp | https://github.com/bethington/ghidra-mcp |
| Ghidrathon | https://github.com/mandiant/Ghidrathon |
| ghidra2dwarf | https://github.com/cesena/ghidra2dwarf |
| ghidra-ExportDwarfELFSymbols | https://github.com/aldelaro5/ghidra-ExportDwarfELFSymbols |
| LRQA VT guide | https://www.lrqa.com/en/cyber-labs/version-tracking-in-ghidra/ |
| awesome-ghidra | https://github.com/AllsafeCyberSecurity/awesome-ghidra |
| Diaphora | http://diaphora.re/ |
| Decompedia GC/Wii | https://decomp.wiki/platforms/gamecube-wii |
