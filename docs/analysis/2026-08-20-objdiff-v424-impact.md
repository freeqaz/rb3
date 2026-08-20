# objdiff v4.2.4 impact on rb3: a vetted wrong callee now reaches the canonical score

**Repo:** `rb3` (Rock Band 3, Wii / SZBE69_B8, MetroWerks CodeWarrior — *not* dc3-decomp, *not* rb3-xenon).
**Date:** 2026-08-20.
**Scope:** measurement and analysis only. No source files were changed.

## TL;DR

- rb3 **does** set `functionRelocDiffs=name_check`, so the change applies here.
- Controlled A/B over **identical object files**: **526 functions dropped**, **0 rose**,
  **87 functions left the matched set** (35,064 bytes).
- The **fuzzy control passes**: per-function `fuzzy_match_percent` is byte-identical on all
  41,254 functions, sizes identical, symbol set identical.
- Aggregate cost is tiny: **−0.003370pp** on the canonical size-weighted code score.
- Of the charged sites, the overwhelming majority are **CodeWarrior literal/ordinal naming
  artifacts** that the fix's three MSVC-shaped carve-outs do not recognise. But the charge
  did surface a **small, coherent set of genuine wrong-callee bugs** that were previously
  invisible to every metric — const-qualifier divergences and `MakeString<T>` type-parameter
  divergences.

## 1. Does rb3 use name_check?

Yes. `objdiff.json` has 1,876 units and a top-level `options` block:

```json
"options": { "functionRelocDiffs": "name_check" }
```

This was opted into on 2026-08-12 by `ca01cdbfd` ("Ship functionRelocDiffs=name_check: start
checking what we call"), merged as `63021eec8`. The generated report confirms it independently —
v4.2.4 writes a `provenance` block naming the ruler actually used:

```json
"provenance": { "tool_version": "4.2.4", "tool_commit": "39144b470916",
                "diff_config": ["functionRelocDiffs=name_check", ...] }
```

## 2. Why the on-disk report.json was NOT a valid "before"

The task assumed the existing `build/SZBE69_B8/report.json` could serve as the baseline. It
could not, and the reason is worth recording.

That file was written at **2026-08-12 00:44:00**. The commit that flipped the config to
`name_check` (`ca01cdbfd`) landed at **00:44:30** — thirty seconds later. The on-disk report
therefore predates the flip and was generated under a **different relocation ruler entirely**.

The tell was arithmetic, not archaeology: comparing it to the fresh v4.2.4 report showed
`matched_code_percent` going **up**, 62.1722 → 63.1412. A charge-only change can only lower
scores. Anything that raises one proves the two sides were not measured with the same
instrument.

**Method actually used.** A control binary was built at `6d50dae` — the exact parent of the
namecheck commit `b14ba45` — into a separate `CARGO_TARGET_DIR` so the shared
`bin/objdiff-cli` symlink (shared with `dc3-decomp` and `rb3-xenon`) was never disturbed. Both
reports were then generated from **the same prebuilt object files**, with the same
`objdiff.json`:

| | binary | commit |
|---|---|---|
| A (before) | `objdiff-cli 4.2.3` | `6d50daef2857` |
| B (after) | `objdiff-cli 4.2.4` | `39144b470916` |

`ninja -n` confirmed zero object edges were stale, and `git log` confirms zero rb3 commits
since Aug 12 — so A and B differ **only** by the objdiff version. The three commits the merge
brought in are `6d50dae` (in A), `b14ba45` (the namecheck fix), and `ae19080` (touches only
`objdiff-cli/src/cmd/diff.rs`, so it cannot affect `report generate`). The isolation is exact.

## 3. Headline numbers

The rb3 report carries **two different rulers in one file**, and conflating them is easy.
Reading `objdiff-cli/src/cmd/report.rs`:

- L1098 `if match_percent == 100.0 { measures.matched_code += symbol.size }` — `matched_code`
  is gated on per-symbol **fuzzy**.
- L1136 `if match_percent_normalized == 100.0 { measures.matched_functions += 1 }` —
  `matched_functions` is gated on the **canonical** score.

So one headline moves and one cannot.

| measure | ruler | before (4.2.3) | after (4.2.4) | delta |
|---|---|---|---|---|
| `matched_functions` | canonical | **32,019** / 41,254 | **31,932** / 41,254 | **−87** |
| `matched_functions_percent` | canonical | 77.614290% | 77.403404% | **−0.210886pp** |
| `measures.fuzzy_match_percent` (see §4 — misnomer, holds canonical) | canonical | 81.894600 | 81.891230 | **−0.003370pp** |
| `matched_code` | fuzzy | 7,217,828 / 11,431,244 B | 7,217,828 / 11,431,244 B | **0** |
| `matched_code_percent` | fuzzy | 63.141228% | 63.141228% | **0** |
| `complete_units` | — | 742 / 1,876 | 742 / 1,876 | **0** |
| `matched_data_percent` | — | 31.459267% | 31.459267% | **0** |

**Denominators:** 41,254 functions; 11,431,244 code bytes; 1,876 units.

Per-function deltas:

- **526** functions dropped on `match_percent_normalized`.
- **0** functions rose. (This is the structural sanity check — a charge-only change that
  raised any score would indicate something else moved.)
- **87** functions left the matched set (were exactly 100.0, now below), totalling
  **35,064 bytes**.
- Size-weighted bytes charged: **384.8 of 11,431,244 = 0.003366pp**, which reconciles with the
  headline −0.003370pp to float precision.

Largest functions leaving the matched set:

| bytes | new canonical | unit | symbol |
|---|---|---|---|
| 3,220 | 99.98758 | `main/system/char/Char` | `CharInit__Fv` |
| 2,132 | 99.96248 | `main/band3/game/Player` | `LocalSetEnabledState__6PlayerF12EnabledStateiP8BandUserb` |
| 2,072 | 99.98070 | `main/system/bandobj/TrackPanelDir` | `SetupApplauseMeter__13TrackPanelDirF...` |
| 1,660 | 99.93976 | `main/band3/tour/TourDesc` | `Configure__8TourDescFP9DataArray` |
| 1,560 | 99.97436 | `main/system/bandobj/BandCamShot` | `Copy__11BandCamShotF...` |
| 1,376 | 99.97093 | `main/band3/game/ChordbookPanel` | `StrumString__14ChordbookPanelFi` |
| 1,060 | 99.92453 | `main/band3/meta_band/Matchmaker` | `OnSearchFinished__14BandMatchmakerFv` |
| 952 | 99.53782 | `main/band3/game/GamePanel` | `UpdateLatency__9GamePanelFv` |

Drops concentrate in `main/sdk/ec/src/ec_asyncOp` (37), `main/sdk/RVL_SDK/src/usbmic/usbmic`
(13), `main/sdk/ec/src/ec_string` (13), `main/system/bandobj/OutfitConfig` (12),
`main/system/rndobj/MeshAnim` (10).

## 4. Fuzzy control: PASS (with a misnomer to be aware of)

**Per-function `fuzzy_match_percent` is identical on all 41,254 functions.** Symbol sets are
identical (0 only-in-A, 0 only-in-B); the `size` field changed on 0 functions. The control
passes cleanly and nothing else moved.

One thing looks alarming and is not. The **aggregate** `measures.fuzzy_match_percent` *did*
move, 81.894600 → 81.891230. That field is misnamed. `objdiff-cli/src/cmd/report.rs:1096`:

```rust
measures.fuzzy_match_percent += match_percent_normalized * symbol.size as f32;
```

The top-level and per-unit field named `fuzzy_match_percent` is a size-weighted average of the
**canonical** score, not of fuzzy. It moving is therefore expected and is in fact the truest
single headline for this change. This is the same misnomer family that `ae19080` fixed for
`objdiff-cli diff` (which added `canonical_match_percent`); **`report generate` still has it.**
240 units show a moved unit-level "fuzzy" for this reason while every function inside them has
an unchanged per-function fuzzy.

## 5. Do the MSVC carve-outs fire on rb3? No.

Three noise classes remain folded, all named for MSVC constructs. rb3 is CodeWarrior, and none
of the three can match:

- **Register save/restore helpers.** `config/SZBE69_B8/symbols.txt` contains **0**
  `__savegprlr`/`__restgprlr` symbols. CodeWarrior emits **72** `_savegpr_N` / `_restgpr_N`
  helpers instead. The carve-out's name pattern does not match them.
- **Placeholder-named enclosing symbols (`fn_<addr>` EH funclets).** Only 4 `fn_`-prefixed
  symbols exist repo-wide, and rb3 builds with `-Cpp_exceptions off`, so there are no EH
  funclets.
- **MSVC function-local-static scope ordinals (`?BD@` / `?BH@`).** CodeWarrior spells the same
  concept `@LOCAL@<func>@<name>@<ordinal>`, which the carve-out does not recognise.

**Consequence, and this is the actionable tooling finding:** the *equivalent* noise is charged
on rb3 that would be forgiven on an MSVC target. The sweep found **188 charged sites in the
`_savegpr_N` / `_restgpr_N` class** — e.g. `Mat__19QuestFilterProviderCFiiP10UIListMesh` is
charged for `_savegpr_27` vs `_savegpr_26`, which is pure register allocation, precisely the
thing the MSVC carve-out exists to forgive. If the carve-outs are ever generalised, matching
CodeWarrior's `_savegpr_`/`_restgpr_` and `@LOCAL@...@N` spellings would remove a large slice
of rb3's 526 drops without weakening the check.

## 6. Classification of charged sites

Method: for each of the 526 dropped functions, `bin/objdiff-cli diff -p . -u <unit> <symbol>
-c functionRelocDiffs=name_check --include-instructions --full-listing -o - -f json`, then
bucket every row whose target-side and base-side relocation names disagree.

Roughly **7,600 charged rows** across the 526 functions. By class:

| class | rows | verdict |
|---|---|---|
| one side has no relocation at all (`named_symbol` vs absent, etc.) | ~2,300 | artifact |
| `__FUNCTION__$<ordinal>` (CodeWarrior per-TU counter) | 806 | artifact |
| `@<ordinal>` anonymous literal pool entries | 557 | artifact |
| `@F_<hex>` / `@D_<hex>` content-addressed FP literals vs `@floatBase0` | ~900 | artifact |
| `@LOCAL@...` function-local statics | ~510 | naming / config (see below) |
| `_savegpr_N` / `_restgpr_N` | 188 | artifact (regalloc; §5) |
| `@STRING@...@<ordinal>` string-pool ordinals | ~250 | artifact |
| `@GUARD@...` static-init guards | ~90 | artifact |
| **both sides a real named symbol, and they disagree** | **310** | **triaged below** |

By opcode, charged rows are dominated by `addi` (2,076) and `lis` (1,887) — address-formation
pairs against literal pools — with **847 on `bl`**.

### The 310 both-sides-named rows

- **157 "different function"** — but most are *symmetric*: the same two names appear crossed in
  both directions (`strlen`↔`UTF8StrLen`, `GetDiffGemList`↔`GetMixList`,
  `OptionStr`↔`__ct__6Symbol`, `StartGetMinMaxReq`↔`StartSetInterface`). A symmetric pair is
  the signature of two adjacent rows being **positionally paired after a scheduling swap** —
  a real code-ordering difference, but not a wrong callee. Others sit inside low-fuzzy
  ctor/dtor chains (`Message`/`DataNode`/`String`) where objdiff is lining up rows it merely
  aligned; per the standing caution, these are **no evidence about any individual row**.
- **143 "same basename, different signature"** — mostly STL/`ECVector` template instantiation
  churn in `main/sdk/ec/*`, in functions with fuzzy well below 95. Not adjudicable per-row.
- **10 "const-qualifier only"** — same class, same method, same arguments, differing by exactly
  one character of CodeWarrior mangling (`...C F...` vs `...F...`). These **cannot** be pairing
  artifacts at the fuzzy levels involved (99.8+), and they are real.

### Are the `@LOCAL@` rows a config defect?

Worth checking explicitly, since in dc3 three of the loudest findings in this class turned out
to be config defects rather than source bugs. Here they are **neither** — they are a source
*naming* divergence.

The target-side names come from `config/SZBE69_B8/symbols.txt`, e.g. lines 71049-71054:

```
@LOCAL@UpdateLatency__9GamePanelFv@count@1  = .bss:0x80C8EBEC; // size:0x4 scope:local
@LOCAL@UpdateLatency__9GamePanelFv@beep@3   = .bss:0x80C8EBF0;
@LOCAL@UpdateLatency__9GamePanelFv@frame@4  = .bss:0x80C8EBF4;
@LOCAL@UpdateLatency__9GamePanelFv@frames@5 = .bss:0x80C8EBF8;
```

Against our build these charge as `count@1`↔`sFlashCnt@1`, `beep@3`↔`sBeep@3`,
`frame@4`↔`sToggle@4`, `frames@5`↔`sMs@5`, `wasPressed@2`↔`sLastBtn@2`. **The ordinals match
perfectly on every one** — the slots, order, sizes and addresses all agree; only the identifier
spelling differs. The config is correct and the code is correct. What this actually says is
that `symbols.txt` preserves the *original* local-static names and our decomp source renamed
them. Renaming the source to match would close the charge and improve fidelity, but it is
cosmetic and carries zero behavioural risk. (Note two names in the same function —
`latency_test`, `pad_button` — already agree, which is why they are not charged.)

## 7. Real leads (verified pairing)

Each of these was re-read with surrounding instruction context and sits between runs of
`equal` rows on both sides, at fuzzy ≥ 99.3 — so the pairing is independently confirmed, not
assumed.

**1. `main/band3/meta_band/StoreMenuProvider :: GetFileName__17StoreMenuProviderFi`** (fuzzy 99.912)

```
  46 equal      T: addi  r5, r5, @stringBase0@l        | B: addi  r5, r5, @stringBase0@l
  47 equal      T: addi  r3, r5, 0x79                  | B: addi  r3, r5, 0x79
  48 diff_arg   T: bl    MakeString<i>__FPCci_PCc      | B: bl    MakeString<Us>__FPCcUs_PCc   <==
  49 equal      T: b     0x794                         | B: b     0x8a4
```

Row 45 is `lhz r4, 0x0(r3)` on **both** sides — a 16-bit load either way. The original promoted
that `u16` to `int` at the call and instantiated `MakeString<int>`; we pass the `u16` directly
and instantiate `MakeString<unsigned short>`. A cast at the call site is the fix. This is the
same class as dc3's `MakeString<unsigned char>` vs `MakeString<char>`.

**2. `main/band3/meta_band/SavedSetlist :: GetIdentifyingToken__15NetSavedSetlistCFv`** (fuzzy 99.948)

`MakeString<i>__FPCci_PCc` vs `MakeString<Q212SavedSetlist11SetlistType>__FPCc...`. Same class:
the original passed the enum as `int`, we pass the enum type.

**3. `main/system/synthwii/.../TDStretch :: __dt__Q210soundtouch9TDStretchFv`** (fuzzy 99.849)

```
  24 equal      T: mr    r3, r30       | B: mr    r3, r30
  25 diff_arg   T: bl    _MemFree__FPv | B: bl    __dl__FPv   <==
  26 equal      T: mr    r3, r30       | B: mr    r3, r30
```

The original destructor calls `_MemFree` directly; ours routes through `operator delete`. This
is a genuine behavioural divergence, not just a name.

**4. `main/system/meta/StorePanel :: OnMsg__10StorePanelFRC24CommerceMgrOpCompleteMsg`** and
**`main/system/movie/Movie :: OnMovieSetTrack__FP9DataArray`** (fuzzy 99.861 / 99.375)

`Node__9DataArrayCFi` (const) vs `Node__9DataArrayFi` (non-const), **6 charged sites** across
the two. The most systemic of the const findings — `DataArray::Node` is being called on a
non-const path where the original used the const overload.

**5. `main/band3/bandtrack/TrackPanel :: Reset__10TrackPanelFv`** (fuzzy 99.956)

```
  83 equal      T: mr  r3, r31                                 | B: mr  r3, r31
  84 diff_arg   T: bl  TrackerDisplayReset__10TrackPanelCFv     | B: bl  TrackerDisplayReset__10TrackPanelFv   <==
  85 equal      T: mr  r3, r31                                 | B: mr  r3, r31
```

`TrackPanel::TrackerDisplayReset` should be `const`.

Also in this class, same shape, lower priority: `GetConfigByUserGuid__21PlayerTrackConfigList`
(const, in `main/band3/game/GameConfig :: GetTrackNum`, fuzzy 99.844), `rindex__6StringCFi`
(const, 2 sites), and `hex_decode` in `main/sdk/ec/src/ec_string` calling `begin()` where the
original called `get_pointer()` (fuzzy 98.09 — verify before acting).

**None of these were visible to any metric before v4.2.4.** They cost zero canonical points and
zero fuzzy points under the old fold.

## 8. Caveats

- **`objdiff-cli diff` and `report generate` disagree on scores for the same symbol.** On
  `main/App :: AppDebugModal__FRbPcb` the CLI reports fuzzy 99.44954 / canonical 99.49541 while
  the report says fuzzy 99.86239 / canonical 99.90826 (score 120/21800 vs an implied 30/21800).
  This reproduces under both 4.2.3 and 4.2.4, so it is **pre-existing and orthogonal** to this
  change — but it means the per-site classification in §6 is derived from the CLI's diff, which
  is not row-for-row the same diff the report charged. The *class distribution* is sound
  evidence; individual counts should not be quoted as the report's charged set. Worth a
  separate investigation.
- Below roughly 95 fuzzy, objdiff pairs rows it merely lined up. The "different function" rows
  in §6 are evidence a class exists and nothing more. Only the §7 leads were verified.
- 526 drops is a **class census, not a bug list**. On the evidence here, well over 95% of the
  charged rows are CodeWarrior naming artifacts.

## 9. Reproduction

```bash
# control binary, isolated target dir so the shared bin/objdiff-cli symlink is untouched
git -C ../objdiff worktree add /tmp/objdiff-ctl-6d50dae 6d50dae --detach
CARGO_TARGET_DIR=/tmp/objdiff-ctl-target cargo build --release -p objdiff-cli \
  --manifest-path /tmp/objdiff-ctl-6d50dae/Cargo.toml

# two reports over the SAME objects
/tmp/objdiff-ctl-target/release/objdiff-cli report generate -o /tmp/report_4.2.3.json
ninja build/SZBE69_B8/report.json     # 4.2.4, via the repo's own report rule
```

`build/SZBE69_B8/report.json` is **not tracked** (`.gitignore:7` ignores `build/`); it has been
left as the build produced it. The `progress.json` and `.decomp_db_synced` edges downstream of
it were deliberately not run, to avoid writing a shared database during a read-only
investigation; a subsequent plain `ninja` will pick them up.
