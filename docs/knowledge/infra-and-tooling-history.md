# Infra, Tooling & Misc Fix History

Migrated from agent-memory: build system and objdiff gotchas, decomp-tooling setup notes, and
write-ups of assorted completed fixes/investigations that don't belong in the live decomp-pattern
docs. Read the relevant section before re-investigating something that's already been diagnosed
here — several of these cost multiple sessions to root-cause the first time.

## objdiff: missing `base_path` → false 0% unit

A unit suddenly reporting **0 matched / N functions at 0.00% fuzzy**, while the `.cpp` still
compiles fine, is a generated-config artifact, not a code regression. Seen on `main/BandOffline`
2026-05-26.

**Diagnostic:** compare the unit's `objdiff.json` entry against a healthy one — a healthy unit
(e.g. `main/App`) has `target_path=obj/App.o, base_path=src/App.o`; a broken one has
`base_path=None`. Missing `base_path` means objdiff has no base object to diff against the target,
so every function in the unit scores 0%, and the metadata is also impoverished (no `source_path`,
no `complete`).

**Root cause:** `objdiff.json`, `build.ninja`, and `config.json` are all *generated*, not
git-tracked. A concurrent/interrupted `configure.py` + SPLIT cycle (background permuters trigger
these) can write an incomplete entry for a unit, especially one recently in a WIP/compile-error
state.

**The real persistence mechanism is `report.cache`, not `objdiff.json`.** Sequence: a background
permuter build triggers SPLIT → "reconfigure on change" → `configure.py`. During that window
`objdiff.json` can momentarily lack `base_path`; a report run in that window caches 0% for the
unit into `build/SZBE69_B8/report.cache`. `configure.py` then self-heals `objdiff.json` (it DOES
generate `base_path` correctly with the standard args) and the dtk-split target `.o` is fine — but
the stale 0% sticks in `report.cache`. So you can see a correct `objdiff.json` + a present target
object + a report that still shows 0%. This recurs every time the background permuter reconfigures
— re-check the cache before trusting a sudden ~100-function category drop.

**Fix:**
```bash
python3 configure.py --version SZBE69_B8 --map        # regenerates objdiff.json base_path
# if target objects under build/SZBE69_B8/obj/*.o are also missing:
build/tools/dtk dol split config/SZBE69_B8/config.yml build/SZBE69_B8   # ~5s, 1876 objects
rm build/SZBE69_B8/report.cache && tools/ninja-locked build/SZBE69_B8/report.json   # durable fix
```

**Do NOT** `rm` a `build/SZBE69_B8/obj/<unit>.o` to "force a rebuild" — those are dtk-split
*target* (ground-truth) objects with no ninja compile rule; deleting one breaks the report/link
until you re-split. Build outputs live under `build/SZBE69_B8/src/*.o` and
`build/SZBE69_B8/obj/<module>/*.o`, which are the safe ones to touch.

Not a regression: BandOffline read 0% this way repeatedly on 2026-05-26 mid-wave while the actual
code was fine (92/103, 95.3% fuzzy) — don't burn agent time "fixing" a unit that dropped to
exactly 0/N at 0.00% fuzzy.

## objdiff "normalized" match metric masks real bugs (must-know)

objdiff (`../objdiff` fork) has **two different "normalized" definitions**, and the metric can
hide genuine source bugs. This is a durable gotcha — keep this explanation intact.

**Mechanism (`code.rs`):** `match_percent_normalized = raw match minus arg_diff_score`.
`arg_diff_score` accumulates *every* same-opcode argument diff: register swaps
(`PENALTY_REG_DIFF=5`), **immediate constants** (`PENALTY_IMM_DIFF=1`), **memory offsets**
(immediate → `PENALTY_IMM_DIFF`), and **failed-reloc targets** (Reloc arg → `PENALTY_REG_DIFF`).
So a wrong constant, a wrong struct-field offset, a wrong vtable slot
(`lwz r12,0x14(r12)` vs `0x1c`), or even a wrongly-called function are ALL normalized away and the
report reads `normalized=100`. Proven case: `CharLookAt::Multiply` counted "matched"
(`norm=100`) but instruction idx23 was `stfs f0,0x0(r5)` vs the target's `stfs f2,0x4(r5)`. The
TempoMap.h vtable bug (see feedback doc `feedback_tempomap_h_vtable_mismatch`) is exactly this
masked class — a wrong vcall offset is just another immediate diff.

**Dual-definition footgun:** the report/core "normalized" (above, arg-subtracted) is a *different
metric* from the CLI `diff` command's `normalized_match_percent` (`diff.rs:886`), which is raw
match under a RELAXED `function_reloc_diffs` and does NOT subtract args. Same word, different
number — a function can legitimately read 100 in `report.json` and 92.5 in the live `diff` output.

**Which `report.json` fields to trust:**
- **HONEST (raw, byte-exact):** per-function `fuzzy_match_percent` (= raw `match_percent`);
  top-level `matched_code_percent` (62.3% at time of writing, gated on raw==100). The orchestrator's
  `database.py` and the permuter's `scorer.py` read these, so grinding targets real bytes, not a
  gamed metric.
- **OPTIMISTIC (arg-subtracted):** per-function `match_percent_normalized`; top-level
  `fuzzy_match_percent` (81%) and `matched_functions_percent` (78.5%, gated on normalized==100).
  `scripts/analysis/compare_progress.py` prefers the optimistic field — be aware when reading its
  output.

**Scale measured 2026-05-26:** 1,352 functions were `normalized==100` but `raw<100`, inflating
`matched_functions` by +3.28pp (true raw-100 was ≈75.2%, not 78.5%). Distribution of the gap: 1,172
in raw 99-100%, 170 in 95-99%, 10 in 90-95%, 0 below 90%. Worst offenders were `Mtx.h`/`Vec.h`
float-math register cascades (benign). Zero units had intra-unit duplicate symbol names, so
objdiff's first-match `.find()` pairing was not mispairing functions.

**For the port / decomp policy:** register normalization is *correct* (a host compiler reallocates
registers freely) — the dangerous part is immediate/offset/reloc masking, since those map directly
to source semantics (constants, struct layout, call targets) that must actually be right. The
identified fix direction: split `arg_diff_score` so only register-permutation diffs normalize away,
and surface a per-function flag whenever the gap contains an immediate/reloc diff — this was not
confirmed as implemented, treat as a standing tool-improvement idea.

## Normalized-masking audit: confirmed real bugs it was hiding

A 2026-05-26 read-only audit (`scripts/analysis/audit_normalized_masking.py`, no rebuild) walked
the 1,310 functions objdiff counted as "matched" (`normalized==100`) but which were not byte-exact.
It diffs each function, drops register/reorder/frame/SDA/pool/branch-addr/`$NNN`-discriminator/
reloc-addend noise via a whole-function value-multiset, and surfaces genuine value differences.

**Result: 1,235/1,310 (94%) provably benign; 75 flagged REVIEW; ~49 judged in-scope after the noise
filter.** This confirms the normalized-masking mechanism above in practice.

**Real bug candidates the metric was masking (asm evidence at the time; needs re-verification
before acting, since 48+ days have passed):**
- `GemPlayer::Poll` — `lwz r12,0x14(r12)` (ours) vs `0x1c` (target): the known TempoMap.h vtable bug,
  rediscovered independently by the audit.
- `Game::AddPlayer` (`band3/game/Game`) — `lwz r12,0xd8(r12)` vs `0xc4`: a *new* wrong-vtable-slot
  candidate (live build raw 99.73% / normalized 99.89% — normalization was actively hiding it).
- `StoreRootPanel` (meta_band) — `new` size `li r3,0x58` vs target's `0x64`, plus a ctor/dtor member
  offset `addi r0,r3,0x38` vs `0x44` (both off by +0xc): suggests our class is 12 bytes too large.
- `SpotDrawParams::Load` (`world/SpotlightDrawer`) — member offsets `0x14/0x1c` vs `0x18/0x20`
  (+4): struct layout off by a word.
- `SongParser::TrackAllowsOverlappingNotes` — `li r3,0x1` vs `0x2`, `subi ...,0x4` vs `0x3`:
  logic/constant diff.
- `SaveLoadManager::HandleEventResponse` — `cmplwi r0,0x61` vs `0x5f`.
- `SongUpgradeMgr::AddUpgradeData` — `cmpwi r3,0x0` vs `-0x2`: both comparison-constant diffs.

**Benign classes the audit correctly filtered — do NOT chase these:** `_savegpr_N`/`_restgpr_N`
(frame size), `MILO_ASSERT` line-number `li r5,LINE` operands (especially in `NetCacheMgr`, which
is out of scope), `__FUNCTION__$NNN`/`__vt__` pool-addend offsets, `_f_bss`/`@LOCAL@` BSS naming,
`lis r0,0x80xx` literal-vs-`__vt__`-reloc, `Node__9DataArrayC` (const) vs non-const, `_outline_`
helper naming, `.text.NNNN` local-label vs named-callback naming.

**Takeaway for the metric itself:** all validated in-scope bugs were *immediate* diffs, not reloc
diffs — so the concrete fix for objdiff would be to stop folding immediate (Value Signed/Unsigned)
diffs into `arg_diff_score` while still folding registers + branch destinations. Re-running the
audit after such a fix should make `matched_functions` drop to the honest count.

## ninja concurrent-build corruption (fixed 2026-05-20)

Concurrent or killed `ninja` runs in this multi-agent repo used to cause three distinct failure
modes: an infinite `SPLIT->configure` loop, `premature end of file; recovering` warnings, and a
rebuild-everything storm (corrupt `.ninja_deps` → "deps are missing" for all ~1,150 objects).

**Fixes in place** (all via `tools/project.py` + `configure.py`, already regenerated — nothing
further to do here, this section is a durable explainer):
- **`deps="gcc"` removed from all 6 build rules.** Ninja now reads per-object `.d` files directly;
  there's no shared binary `.ninja_deps` cache left to corrupt. This is what permanently kills the
  rebuild-everything mode, and it's robust even against concurrent or killed builds.
- **`tools/ninja-locked`** — an flock wrapper that serializes builds. `objdiff.json`'s
  `custom_make` points at it, and a `ninja()` shell function in `~/.zshrc` routes bare `ninja`
  through it inside any repo that has the script.
- **`restat=True`** on the `split` rule — this is what kills the `SPLIT->configure` loop.
- **`config.check_sha_in_default = False`** in `configure.py` — the SHA1 `ok` check no longer
  gates the default build (it can't pass until decomp is 100%). Run it on demand with
  `ninja build/SZBE69_B8/ok`.

**Rule going forward: build with `tools/ninja-locked`, never bare `ninja`.** A clean build ends
successfully and prints the progress report; `ninja: build stopped` should only ever appear if you
explicitly built the `ok` target.

**If incremental building ever breaks again:** `rm -f .ninja_deps .ninja_log`, then one full
`tools/ninja-locked` build regenerates the `.d` files from scratch.

## MessageTimer.h dtor fix (applied, commit `7cf12b75`)

**Status: done, do not re-attempt.** `src/system/obj/MessageTimer.h` (dtor around lines 89-92) was
fixed to:
```cpp
~MessageTimer() {
    mTimer.Split();
    AddTime(mObject, mMessage, Timer::CyclesToMs(mTimer.mCycles));
}
```
Landed in commit `7cf12b75` ("decomp: MessageTimer dtor Split+CyclesToMs - 5 Handle fns to 100%").

**Actual yield was smaller than predicted:** ~5 `Handle` functions reached 100%, well below an
earlier "50+ Handle/Poll" estimate. The dtor was a real blocker for a handful of functions, but the
bulk of functions still stuck at 98-99% trace to other causes in the same area: `AutoTimer` ctor
field order, vtable-thunk ordering, and format-string pool-offset churn within the same TU.

**If you hit a Handle/Poll function stuck at 98-99% with an `r5↔r6` or `±8` stack-shift signature,
don't re-edit this header** — look instead at `Timer.h`'s `AutoTimer` ctor (see feedback doc
`feedback_timer_header_lock`), or a sister function's format-string churn shifting the string pool.

## RndText::WrapText — partial decomp session log (~95% normalized)

`RndText::WrapText` in `src/system/rndobj/Text.cpp` reached **95.97% normalized (report.json) /
94.6% fuzzy (objdiff-cli)** on a 5,736-byte target via multiple manual + `beam_search` +
cluster-refinement sessions (2026-05-25/26). This is a decomp-pattern log, not an active TODO, but
the specific wins are reusable MWCC-pattern knowledge:

**Confirmed wins (in rough chronological/gain order):**
- Nested `while (curChar == '<' && mTextMarkup)` instead of `if` + `goto soft_loop_top` — removes
  a duplicate markup re-check the goto pattern produced.
- `charCount += parsed - cur;` after `ParseMarkup` — target counts markup-tag bytes as chars.
- Hoist `int numWp = 1;` **before** `wps[0]` init writes, so MWCC keeps `1` in a callee-saved
  register for the later `wps[0].isLineEnd/isHardBreak` writes instead of reloading `li 0x1` twice.
- Post-loop `emptyLine` field-write order **lineStart, lineStyle, lineWidth, lineEnd** (matches
  target; was previously lineStyle/lineStart/lineEnd/lineWidth).
- Strip-loop off-by-one: init `tmpLine.lineEnd = text + ne->byteIdx + 1;` and read `lineEnd[-1]`
  after `--lineEnd` — the target's asm only makes sense with the `+1` init.
- `unsigned char p = (unsigned char)lineEnd[-1];` with `(unsigned char)(p - '\t') > 1` range form —
  emits `clrlwi + cmplwi` matching the target instead of `extsb`.
- Renamed fields for readability: `Line::unk18→lineStart`, `unk1c→lineEnd`, `unk28→transform`,
  `unk58→lineWidth`; `RndText::unk12c→mHeightSpan`, `unk130→mMaxLineWidth`.
- `numChars > 256` (not `numChars + 1 > 256`) for the heap-vs-stack `WrapPoint` choice, with
  `wps = stackBuf` pre-assigned so the heap path is a plain overwrite.
- `memset(wps, 0, numChars * sizeof(WrapPoint))`, not `(numChars+1) * sizeof(WrapPoint)`.
- `Line tmpLine;` declared **inside** the line-build `while` loop body (per-iteration ctor,
  matches target), declared *after* strip-prep computations so MWCC sees the ctor immediately
  before the field assignments it feeds.
- `Style curStyle = style;` declared before `wps[0]` init/assignment (target reads style fields
  from a stack copy at frame offset 0x30).
- Decrement `tmpLine.lineEnd` (formerly `unk1c`) in place on the stack in the strip loop, not via a
  local + write-back.
- Range-check rewrite `if (p == ' ' || (p >= '\t' && p <= '\n'))` instead of chained `!=` checks —
  triggers MWCC's `subi/clrlwi/cmplwi/bgt` range-fold.
- Remove dead `tmpLine.startIdx = wp->charIdx; tmpLine.endIdx = ne->charIdx;` — target leaves these
  at ctor-zero.
- Strip-loop decrement-first: `while (ce > cs) { --ce; char p = *ce; if (!strip) { ++ce; break; } }`
  matches the target's decrement-then-reload body shape.

**Tried and reverted — do not retry these:**
- Swapping the `strlen`/`UTF8StrLen` declaration order: -0.1% (mangled symbol order was right but
  cascaded worse elsewhere).
- `for(;;) { if (curChar==0) break; if (curChar=='\n') break; ... }` instead of nested `while`:
  -0.8%.
- Using `continue` in the markup re-parse branch (removing `soft_loop_top`): -3.7%, cascades
  through register allocation.
- Hoisting `Line tmpLine` to function top (fixes a slot inversion but breaks the per-iteration ctor
  pattern): -3.3%.
- Hoisting `tmpLine` + `tmpLine = Line();` per-iteration reset: -4.9% (adds a stack slot for the
  temporary).
- Reordering `0.5f*ratio*mLeading*size` term operands: no change (commutative, as expected).

**Remaining ~5% gap at last measurement** (register-alloc/permuter-class, not worth manual chase):
a `Line` stack-slot inversion (tmpLine/emptyLine swapped vs target, ~139-instruction cascade); 3
control-flow polarity flips on `unsigned short` vs char-literal comparisons; ~287 register swaps
(dominated by r3↔r4); an r3↔r7 cascade from an inlined `lines.insert` call; and a post-loop
signed/unsigned int-to-float conversion operand-order diff.

**Cascade caveat:** changes to `WrapText` can shift `ComputeCharWidths` (sibling function in the
same TU) up or down — check it after any further edit here. At last check the other Text.cpp
partials were: Load 99.4%, AddLineUTF8 98.7%, NumCharsInBytes 97.1%, UpdateMesh 96.4%, ParseMarkup
94.4%, ComputeCharWidths 93.7%, UpdateLineColor 91.5%, `_Vector_impl::reserve` 89.1%, ReplaceLineText
87.4%.

## OutfitConfig.cpp — 14 missing string literals (cascade fix, mapped)

A 2026-05-19 pool-diff analysis (Wave 75H) found `OutfitConfig.cpp` was missing 14 string-pool
entries (vs only 1 extra), which was producing 26 partial-match functions in the 99.5-99.9% range.
**Root cause:** every `MILO_ASSERT`/`MILO_WARN`/`MILO_FAIL` with a fresh literal contributes to
`@stringBase0` at a fixed offset — a missing literal shifts every later function's offsets in the
same TU, so implementing the stub that emits it lifts the whole translation unit.

**Missing strings (verbatim from target asm) and the function that emits each, mapped in Wave 76A:**

| Pool offset | String | Emitting function | Status at time of writing |
|---|---|---|---|
| 0x01F | `"%s: Cannot render to texture (%s) while already rendering to texture (%s)."` | `MatSwap::Compose` (2564B) | stub |
| 0x074 | `"%s can't apply piercing deformation, before verts different than head..."` | `Piercing::Deform` (2052B) | stub |
| 0x0D6 | `"%s can't do piercing piece %d deform, head verts out of date, need to re-ao"` | `Piercing::Deform` | stub |
| 0x122 | `"%s mesh %s no longer matches piece %d, has fewer verts (%d v %d), must re-AO file"` | `Piercing::Deform` | stub |
| 0x174 | `"%s MeshAO has different vert count %d v %d from %s, can't apply"` | `MeshAO::Apply` (724B) | stub |
| 0x1B4 | `"%s MeshAO %s can't find matching mesh to apply"` | `MeshAO::Apply` | stub |
| 0x2A9 | `"norm_%s.texblendctl"` | `MatSwap::Compose` `MakeString` | stub |
| 0x2BD | `"%s_head_norm%02d.tex"` | `MatSwap::Compose` `MakeString` | stub |
| 0x2D2 | `"ObjPtr_p.h"` | template `MILO_ASSERT(f.Owner())` | **DECOMP_FORCEACTIVE** |
| 0x2DD | `"f.Owner()"` | same | **DECOMP_FORCEACTIVE** |
| 0x352 | `"milo"` | `InMilo` (332B) | stub |
| 0x357 | `"milo.dir"` | `InMilo` | stub |
| 0x360 | `"main"` | `InMilo` | stub |
| 0x365 | `"option > 0 && option < BandCharDesc::kNumPalettes"` | `MatSwap::Compose` inline assert | stub |

**Recommended fix order (cheapest first):**
1. Add `DECOMP_FORCEACTIVE(OutfitConfig, "ObjPtr_p.h", "f.Owner()", "")` near the top of the file —
   matches the existing pattern in `LayerDir.cpp:21-22`, `BandCamShot.cpp:76`, `BandList.cpp:35`.
2. `InMilo` — smallest, well-defined: `sMainDir->FindObject("milo", false)`, check
   `Symbol("milo.dir")`, compare `ClassName()` to `"main"`.
3. `MeshAO::Apply` (724B) — uses both `MILO_WARN` strings.
4. `Piercing::Deform` (2052B) — 3 of the missing strings.
5. `MatSwap::Compose` (2564B) — 4 of the missing strings.

**Ruled out:** `Tour.cpp` was investigated as another cascade candidate and disproven — all 86
strings are present there; its cascade comes from `MILO_ASSERT` variable-name differences (source
uses `performer`, target uses `pPerformer`) and function definition order, not missing strings. Not
a stub-add target.

## STLPORT network `#define` hypothesis — INVALIDATED

Wave 65 speculated that the `network` build group was missing `-d STLPORT`, causing Quazal's
`qVector<T>` to fall back to `std::vector<T, size_t>` instead of STLport's u16-sized vector, and
that this explained the `network` unit's 55-65% stagnation.

**Wave 66 tested this directly and disproved it:**
1. `STLPORT` is already defined globally by `src/system/stlport/stl/_config.h:760` whenever STLport
   headers are included — true for every TU, since `src/system/stlport` is first in the include
   path for all groups.
2. `_STLP_USE_SIZED_VECTOR` is defined by `stl_user_config.h` whenever `HX_WII` is set, which is
   part of the `base` flags inherited by every group.
3. So `#if defined(STLPORT) && defined(_STLP_USE_SIZED_VECTOR)` in
   `src/system/utl/VectorSizeDefs.h` is *always* active — sized vector is already on.
4. Adding `-d STLPORT` to the command line only triggers `(10108) macro 'STLPORT' redefined` in
   every TU that includes STLport headers.

**Actual cause of the network stagnation:** per-function analysis of `insert_unique` in
`network/Core/CallContextRegister` showed the mismatches are control-flow differences (4 condition
inversions), register allocation (`r28↔r31`), and stack-frame differences — not the vector's
size-type. The target uses fewer callee-saved registers than ours, pointing at source structure /
declaration order, not a preprocessor problem.

**Rule going forward: do NOT add `-d STLPORT` to any group's cflags.** For further `Rb_tree
insert_unique` improvements, analyze per-function (declaration order, helper inlining, branch
polarity) instead. See also the `_M_ptr._M_data` direct-access win noted in the STLport `_M_start`
feedback doc — that is the real stlport-class win, this define is not.

## Decomp tooling setup (Ghidra + m2c + analysis scripts)

Baseline tooling set up for decompilation work:

- **Ghidra MCP**, port 8001 (separate from DC3's port 8000), debug ELF at
  `milo-executable-library/rb3/Wii Proto (Bank 5) (Debug)/band_r_wii.elf`. The binary name inside
  Ghidra carries a hash suffix (e.g. `band_r_wii.elf-781439`). Start with
  `./tools/ghidra/pyghidra-service.sh start`. Not wired into the MCP config — accessible only via
  the Python client (later superseded by `bin/analyze-function`, which now also talks to a Bank 8
  target; see CLAUDE.md's Ghidra section for the current two-binary setup).
- **m2c** at `../m2c/m2c.py` (i.e. `/home/free/code/milohax/m2c/m2c.py`), target flag `--target
  ppc`. Reads asm files from `build/SZBE69_B8/asm/` in dtk format (`.fn SYMBOL, global`).
- **`bin/analyze-function`** (backed by `tools/analyze_function.py`) — combined objdiff + Ghidra +
  m2c view of a symbol.
- **`bin/decompile`** — m2c-only, quick decompilation straight from the asm files.

Use `bin/analyze-function SYMBOL` as the default entry point when investigating a function: Ghidra
supplies DWARF-enriched pseudo-C from the debug ELF, m2c supplies an independent second opinion
from raw disassembly.

## Native port kickoff (2026-05-25 planning)

The RB3 native port was formally kicked off 2026-05-25, at which point decomp stood at 59.58% code
/ 75.54% functions. DC3-decomp's native port (77+ sessions, full boot-to-gameplay with audio on
Linux x86_64 WebGPU) was adopted as the model.

**Locked strategy:**
- A new shared repo, `milo-native-engine` (at `/home/free/code/milohax/milo-native-engine/`, did
  not exist yet at kickoff), holds the runtime engine + native glue; both RB3 and DC3 decomps
  depend on it.
- Bootstrap is **copy-first, extract-later**: Phase 1 copies `dc3-decomp/native/` into `rb3/native/`
  to get RB3 booting; Phase 2+ hoists the shared subset into `milo-native-engine`.
- **Hybrid `src/system/` ownership**: each decomp keeps a matched fork for asm-verification; files
  migrate into `milo-native-engine` once asm-match work is done and native diverges. Both decomps
  stay matched against their original binaries; the *runtime* artifact is the shared engine.
- Shared code targets **clean LP64 modern C++17 / clang** — no `__declspec`, no MSVC-STL, no MWCC
  quirks. Per-decomp compat shims live outside the shared engine (`msvc_compat.h` for DC3,
  `mwcc_compat.h` for RB3).
- **v1 targets:** Linux x86_64 + macOS (arm64+x86_64) + Web (Emscripten). Windows skipped.
- **v1 scope:** single-player only, one song end-to-end (audio + venue + HUD + scoring). All of
  `src/network/`, `src/sdk/`, `src/system/rndwii/`, `src/system/os/` are skipped — replaced
  wholesale by the host platform.
- **Engine packaging:** sibling repo `../milo-native-engine`, consumed via CMake
  `add_subdirectory()` — no submodules. Engine commit pinned per-decomp via a soft
  `MILO_ENGINE_PIN` SHA variable in each decomp's `CMakeLists.txt` (`message(WARNING)` on mismatch,
  build proceeds anyway). One `milo-engine` static-lib target; tool targets (viewer, gltf exporter,
  render-test) opt in via `MILO_ENGINE_BUILD_TOOLS=ON`.
- **Debug tooling always on:** `HttpServer` + `DebugPanel` build into `milo-engine` unconditionally,
  no opt-out flag — deemed too painful to bring up without, and binary/dependency cost is
  negligible.
- **File split rule:** the dividing line is *game coupling*, not "native APIs vs engine". At
  kickoff, ~115 of `dc3-decomp/native/`'s files were slated to ship to `milo-native-engine` day 1
  (all of `gfx/`, `audio/`, `char/`, `stl/`, `export/`, `tools/`, `viewer/`, `render_test/`, plus
  ~40/80 `platform/` files); 5 needed cleanup hooks (`Rnd_Wgpu.cpp`, `MeshFilter.cpp`,
  `System_Native.cpp`, `ContentMgr_Stub.cpp`, `Achievements_Stub.cpp`); 16 stay DC3-specific
  (Kinect stack, Xbox SDK/Live/Memcard, DC3 link glue, telemetry, main entries); 3 were dropped
  entirely for RB3 (`xdk_shims.cpp`, `XmaSampleDecoder`, `pose/`). Full inventory:
  `docs/native/NATIVE_PORT_INVENTORY.md`.

**Why this shape:** match% in the decomp is verification that the C++ encodes the original
semantics; the *runtime artifact* is the shared engine, so decoupling them lets native code evolve
freely. DC3 is the newer Milo version and leads engine evolution, with RB3 mostly catching up
one-way. Cross-pollination only works if both decomps actually depend on the same engine repo —
copying without a shared repo would drift quickly.

**Operational notes (durable):**
- The roadmap doc is the canonical live artifact: `docs/native/NATIVE_PORT_ROADMAP.md` (per
  CLAUDE.md, Phase 0 is now complete and this has moved well past kickoff state — treat the above
  as historical framing, not current status).
- When fixing a native bug, check whether the file lives in the shared engine (post-extraction) or
  still in the decomp fork (pre-extraction); shared fixes land once, forked fixes need parallel
  application to `dc3-decomp` if the code is shared.
- Native build (CMake under `rb3/native/`) is a separate pipeline from the matched-decomp build
  (`tools/ninja-locked` building the matched DOL) — don't conflate them.

## Same-Instrument RB3Enhanced patch (Xbox 360, implemented, runtime pending)

This is an `rb3-xenon` / RB3Enhanced feature effort, not an RB3-Wii-decomp one, but it lives in the
adjacent repo family and the identifiers below are load-bearing if the work resumes.

**Feature:** allow multiple players on the same instrument in retail RB3 Xbox 360 (title
`45410914`). Investigated, implemented, and packaged via multi-agent workflows starting
2026-07-07.

**Why no XDK was needed:** the one translation unit (`source/SameInstrumentHooks.c`) calls only
game functions via fixed-address pokes, so it compiles freestanding with `cl.exe /c` (via wibo)
against `../rb3-xenon/src/xdk/LIBCMT` headers plus an 11-line `stdint.h` stub — no `xtl.h`, no
import libs, no `imagexex`. The Microsoft XDK itself (leaked/proprietary) was deliberately not
downloaded.

**Artifact:** `scripts/objcave_pack.py` relocates the compiled `.obj` into a game-XEX code cave at
**`0x82C25000`** (a BINK inter-section gap in `band.exe`) producing
`patches/45410914_same_instrument_full.patch.toml` — a 2,800-byte blob plus 4 static detours: Layer
A `0x8264B5F8`, Layer B `0x8259D948`, Layer C `0x8274ACF8`, `RecalcGemList 0x8276FBB0`. A
pure-poke fallback UI-unlock variant also exists:
`..._uispike_layerAC.patch.toml`. Requires Xenia Canary's `apply_patches` +
`writable_code_segments`.

**Design (3 enforcement layers + the actual blocker):**
- Layer A: `OvershellPartSelectProvider::IsActive` — un-grey the second controller slot.
- Layer B: `OvershellPanel::ResolvePartWaitStates` — advance `ChoosePartWait`→`ChooseDiff` instead
  of no-op.
- Layer C: `PlayerTrackConfigList::ProcessConfig` — remove the occupancy `MILO_FAIL`.
- **The real blocker:** a single shared `GameGem::mPlayed` bit lives on the *per-track*
  `GameGemList`, so two players on one instrument would share hit state. Fix: per-watcher
  `GameGemDB::Duplicate` clone inside `TrackWatcherImpl::RecalcGemList` (every other piece of hit
  state is already per-player).
- **Critical retail-vs-Wii ABI fact:** MSVC `std::vector` on this target is **12 bytes**
  (`{begin,end,capEnd}`), not the 8-byte layout assumed early on — all struct offsets had to be
  recomputed (`SongData::mGemDBs@0xb0`, `mTrackDifficulties@0x50`; `GameGemDB`/`GameGemList`
  `sizeof=0x10`; `TrackWatcherImpl::mSongData@0x50`, `mTrack@0x68`, `mGemList@0x1c`). Key addresses:
  `SetOvershellSlotState 0x8266DB58`, `UpdateAll 0x8259E5B0`, `BandUser::mOvershellState@0x20`,
  `GameGemDB::Duplicate 0x8276E590`, `CopyFrom 0x82769450`, `GetDiffList 0x8276E010`.

**Deliverable branch:** `feature/same-instrument` in freeqaz's RB3Enhanced fork at
`/home/free/code/milohax/RB3Enhanced` (upstream = `RBEnhanced/RB3Enhanced`), commits `1686219`
(boot-safe scaffold) → `397b2a3` (enabled + Xenia patch). Not pushed, not runtime-tested as of last
update.

**Version/targeting correction history (important if resuming):** the effort initially targeted
the wrong binary twice. `rb3-xenon`'s `orig/45410914/default.xex` is the **vanilla/base disc EXE,
version 0.0.0.1 (TU0)**, not TU5 — confirmed via XEX exec-info version and by the fact its `.text`
shares zero bytes with the real TU5 XEX at the 4 detour sites. RB3Enhanced targets TU5 (v0.0.5.1).
A later attempt to source "clean TU5" from `/srv/torrents/games/arbys/rb3/default.xex` turned out
to actually be **RB3 Deluxe (RB3DX)**, a community mod (proof: sha1-identical to the RB3DX repo's
`default.xex` and contains the `rbdxcache` string). This was ultimately resolved: **RB3DX = clean
TU5 plus a 170-byte patch, entirely in unnamed regions — zero named functions differ**, so a true
clean TU5 was reconstructed by applying the official TU5 XEXP
(`milo-executable-library/rb3/360 xexp/tu5/default.xexp`) to the *encrypted* retail base XEX (not
the decrypted one — XexPatcher validates the image key against the AES key) via
`rb3-xenon/tools/xexp-apply`, producing `rb3-xenon/_tu5probe/clean/clean_tu5.xex` (v0.0.5.1, no
`rbdxcache`, entry `0x8283CD20`). **One patch serves both RB3DX and clean TU5** — all 7 patch
functions, Layer B, and the cave are byte-identical at the same virtual address in both, and
12,817/12,817 mapped functions are identical between them.

**Xenia runtime bring-up (separate track, same effort):** several general (DC3-safe) Xenia fork
fixes were required and committed: `66d8d41c6` (`XFileFsDeviceInformation` impl + softened
read-only write-open assert), `757de34` (XamUser multi-user `--local_user_count` + extern stubs),
`ef5025af` (title-teardown object-table triple-assert). With these, the patched clean-TU5 XEX boots
to a live ~55fps menu render loop under NullGPU and also renders on a real RTX 3090 via
`--gpu=vulkan`; DC3 shows zero regression. The remaining live blocker was RB3's own
`XamShowDirtyDiscErrorUI` content-integrity self-exit during ARK init (game-side check, not an
emulator bug) — bypassed the standard RB3E way (`PlatformMgr::SetDiskError 0x82516320` → `BLR`,
flat offset `0x519320`, 4 bytes; note RB3DX's own dirty-disc-adjacent patch at `0x82575f9c` is a
*different* check). Past that bypass, RB3 runs 15 threads deep into init before hitting a systemic
Xenia JIT fault (`0x100000000` — guest 32-bit pointer arithmetic overflowing host 64-bit) inside
DTA/DataArray parsing of `config/band_keep.dta` at `0x8275026C`; this is the same fault family that
blocks retail (`0x8226045C`) and DC3 (`0x82311A94`) — a deep JIT/memory-model bug, not a
patch/content problem, and not expected to be fixable without Xenia Canary or real hardware.

**Status at last update:** patch confirmed non-destructive and correctly TU5/RB3DX-targeted at
runtime; ready deliverables live in `rb3-xenon/_tu5probe/clean/` (`clean_tu5.xex`,
`clean_tu5_nodd.xex` with dirty-disc bypass, `clean_tu5_nodd_siPATCH.xex` — the actual same-instrument
test build, sha `6ce44436`). Headless 2-guitar UI capture is blocked pending the JIT-fault fix.
Design docs live under `rb3-xenon/docs/plans/`: `rb3enhanced-same-instrument-patch.md`,
`same-instrument-derived-addresses.md`, `build-without-xdk-recommendation.md` (+ several sibling
docs on the XDK-free build path).

### TU5 migration of the patch (2026-07-07 — retargeted, load-bearing addresses changed)

Because RB3Enhanced targets TU5 (v0.0.5.1) while `rb3-xenon`'s decomp target was the base/TU0
disc EXE, the effort built and validated a **base→TU5 function map** and re-targeted the patch:
- **Map:** skeleton/content-hash matching gives **96.4% named functions identical (12,817/13,295)**;
  changed-set = **478**, of which only **81 are genuinely rewritten** — and analysis
  (`docs/plans/tu5-rewritten-functions-analysis.md`) showed the *entire* changed-set is ONE
  base-class member + vtable insertion rippling offsets/vtable-slots, so only ~25 are true rewrites
  (~2-3 eng-days). Work lives in worktree `rb3-xenon/.claude/worktrees/tu5-migrate` (branch
  `tu5-migrate`; TU0 frozen at tag `target/tu0-frozen`, main untouched). TU5 dtk-extracted at
  `band_tu5.exe`.
- **The patch survives TU5 with no logic rework** (6/7 patch fns are clean 1:1; `IsActive` is a
  whole-function override so its 56% divergence is irrelevant). CAUTION when resuming: the
  overshell/part-select area is the most-churned (`Reload`/`ResolveSlotStates`/
  `OvershellSlot::UpdateView` were rewritten and none are hooked) → smoke-test that path.
- **Addresses changed on retarget** — the original spike addresses were all off by a `−0x8000`
  flat-map error; corrected, and the **code cave moved `0x82C25000` → `0x82C8A000`** (the base cave
  is zeros in TU5). `default_tu5_patched.xex` byte-verified (675/675 writes, 4 detours = real `mflr`
  entries → `b cave`). The test build `clean_tu5_patched.xex` (sha `e411086b`,
  `gSameInstrumentEnabled@0x82C8AAA0=1`) is byte-verified. **Use the TU5 cave `0x82C8A000`, not the
  base `0x82C25000`, when resuming.**
- **`base_to_tu5_map.json` (built vs RB3DX) IS the base→clean-TU5 map** — rebuild delta ≈ 0, just
  relabel (per the RB3DX = clean-TU5 + 170-byte-patch finding above). Dirty-disc bail helper
  is `ShowDirtyDiscAndBail 0x8283D740` (called ~`0x8283D74C`).
- **Tooling:** `rb3-xenon/tools/va_disasm.py <VA> <n>` disassembles PE-section-correct (capstone
  PPC32 BE) — a flat `VA-0x82000000` read is WRONG for `band.exe`. Ghidra crossport
  (`docs/plans/ghidra-tu0-tu5-crossport.md`) names only 1,139/13,846 in Ghidra base; the VT (Exact
  Function Instructions) correlator is the structural cross-check for the 81, not a bulk namer.
  Divergence write-up: `docs/plans/clean-tu5-vs-rb3dx-divergence.md`.

## Decomp-synth "hop reward" / edit-contract research thread

An ML/data-pipeline research thread (distinct from asm-matching work) that mines this repo's git
history to build training data for an LLM decomp-fixing proposer. Captured here because the
identifiers (script names, commit hashes, dataset names) are the durable part if the thread
resumes; the ML conclusions themselves are research findings, not code to maintain.

**Join fix landed (`a14662e`):** `build_hop_reward_join.py` joins `escore_hops_v1` ×
`real_chain_rb3_clean` on `(symbol, ancestry_key, step_index_from)`. The naive `(repo, chain_id,
step)` key is a proven trap — `chain_id` namespaces are disjoint across source files, so naive
joins matched 1,306 rows with 0% symbol agreement. rb3-side join result: 1,483 ok / 56 multi-chain
/ 10 absent / 0 step-miss. The legacy `sha1(chain_id)%100<15` train/human split was retired because
it was training on a radioactive chain (724 = `Relativize__CharBonesSamples`).

**Findings (null results, do not re-litigate without new evidence):**
- **H1** (ΔE-weighted retrain gate): null / no-regression, pre-registered (CI [−0.120, +0.065], 13
  clusters). The identity (unweighted) ranker stays deployed.
- **Sub-hop dense labeling:** priced and intentionally not scaled further. Pilot (`7a14495`) was
  "GO-narrow" (100% fidelity, 42.4% intermediate build_ok, 29.1% full-path build_ok). Scale attempt
  (`18ca4eb`) transferred at 96.8% build_ok but only yielded ~126 routable hops / 731 built rows —
  **+47% labeled states, not the hoped-for 2-3×**. The binding constraint is coupled-edit classes
  (<15% buildable, several at 0%) — this needs a different synthesis strategy, not more budget.
  Dataset: `subhop_dense_labels_v1.jsonl` (gitignored).
- **D1 / graph-dynamics direction is closed** (`F-D1-NULL`): mined-chain edge signal is real but
  sub-threshold for planning use; "more data cannot lift this."
- **N1 verdict (`70904d1`):** the binding constraint is the *edit contract*, not proposer model
  quality. Only 28.8% (252/875) of real human fixes are expressible as one deployed single-line
  edit; the improver-arm corpus reading 99%-expressible is a survivorship artifact (those rows are
  *defined* as single-edit outputs). This is flat across match-% bands, not concentrated in
  low-match functions. Implication: the highest-value next lever is a multi-edit/whole-body edit
  contract redesign ("N5′"), not a bigger/better proposer.
- **N4 LoRA A/B:** executed on 2× RTX 3090, adapter engaged correctly (11× more buildable
  candidates than base in a 6-state smoke test), but the full run landed below its statistical
  floor on both arms (base 1/132, LoRA ~1/89 vs a floor of 13) → result is descriptive-only,
  consistent with the pre-committed "contract-bound, not proposer-bound" null.
- **Corpus mislabeling caught and fixed:** the improver corpus was initially mislabeled
  rb3-vs-dc3 (not contamination — DC3 shares the Milo engine with ~88% cross-compiler transfer, so
  its data is legitimately valuable). `label_improver_corpus.py` now tags repo/toolchain/engine
  zone; true counts were 241 rb3 (182 shared-engine) + 539 dc3 (431 shared-engine) = 780, correcting
  an earlier wrong "631 rb3-native" figure.

**Process lessons:**
- An Opus review pass caught the join-key defect *before* implementation — review → Q&A → plan →
  implement paid for itself.
- 4/10 "Answer" agents in one workflow died at the StructuredOutput retry cap; this is why RB3's
  CLAUDE.md now requires workflow subagents to checkpoint structured results to disk before
  returning (see the "Multi-Agent Workflows" section of CLAUDE.md).
- Watch for peer-agent collisions on parallel decomp-synth work — reconcile fix-forward and re-grep
  current `main` before staging, rather than assuming a stale view.

**Additional durable findings (identifiers/conclusions worth keeping):**
- **Counter-intuitive yield curve (banked):** single-move improver yield *rises* with fuzzy%
  (`[95,100)`≈0.22 > `[90,100)`≈0.13 > `[50,90)`≈0.04), the *opposite* of the headroom intuition —
  low-% functions need coupled multi-edit fixes the single-line edit contract can't express
  (mechanistically = the sub-hop token-class table: coupled edits <15% buildable). Tier-1/2
  band-chase was stopped at ~0.043 improvers/state as not worth it.
- **Proposer cost:** `deepseek-v4-pro` is the value proposer; `gpt-5.2` buys ~nothing over it at ~3×
  cost (tier-3 pilot). Meter cost via per-arm `transcripts.db SUM(cost)`, NOT `/credits` — shared-key
  pollution inflates the credits view up to 21×.
- **LoRA pipeline (`f43e65c`):** `train_proposer_lora.py` (QLoRA 4bit) + `serve_proposer.sh` (vLLM
  base+adapter at a local OpenAI endpoint) run GREEN on 2× RTX 3090; 7B/14B comfortable, 27B tight.
  GPU 0 had a peer process → use GPU 1. `llm_proposer` is OpenRouter-native when `LLM_BASE_URL` is
  unset, which sidesteps single-GPU contention for collection.
- **Next-lever direction if N4 stays null — `N5′`/`N5″` (multi-edit / parameterized edit-chain):**
  the frontier is a whole-body / multi-line edit contract for the ~71% structural fixes the deployed
  single-line contract can't express. A fill census over all 1,333 hops (3,942 content-edits) found
  only ~10.7% of fills are truly freeform (35.7% exact-copy, 17.4% lexicon, 22.4% gensym-locals,
  11.7% member-API, 2.2% target-literal); **parameterization + constrained-decode pointer-domains
  lifts content-closable chains 27.5% → ~64%**. Prototype arms to A/B: udiff baseline + anchored-op
  DSL (quoted anchors self-verify = pre-compile hallucination reject) vs whole-body text; hard
  copy-only pointer-heads are DEAD (≤53% coverage).

## `audio_verify.py` — reference-vs-output audio verification tool

`scripts/native/audio_verify.py` (+ the `/audio-verify` skill) is the tool built 2026-06-08 to
answer "is the game's captured audio the same song, played correctly?" — closing a gap left by
older reference-free tools (`audio_coherence.py` = clip/tone only; `audio_correlate.py`'s waveform
path was unreliable across captures).

**Pipeline:** ground truth comes from `decode_reference.py` (decrypt mogg → ffmpeg → apply the
game's actual pan/vol downmix, unclamped). It's compared against a headless capture (from
`capture_gameplay_audio.py` / `song-preview-audio-test.py`) across four axes:
- **Identity:** global centered-chroma cross-correlation (robust to mix/EQ/gain differences,
  since our downmix isn't necessarily identical to the game's mix) plus Chromaprint (`fpcalc`)
  raw-fingerprint bit-error-rate.
- **Rate:** onset-envelope resample-search → speed ratio (e.g. a 48000/44100 chipmunk shows a sharp
  1.088× peak; correct audio shows a flat speed curve).
- **Distortion:** reference-free clip-ratio / flat-top / wrap / crest-factor — the *trustworthy*
  signal, since spectral-divergence/HF/pitch-ratio versus a differently-mixed reference are
  info-only.
- **Noise/silence:** spectral flatness + zero-crossing rate (the real music-vs-noise discriminator:
  music <0.2, noise ~0.5) and loud-region RMS for silence detection.

Verdicts are `MATCH`/`DEGRADED`/`WRONG-SIGNAL`/`SILENT` (exit codes 0/1/2/3). `--selftest` runs
6/6 synthetic proofs — run it before trusting any verdict. `--rank s1,s2,s3` is the *robust*
identity test (right song should win by margin; the absolute chroma threshold of 0.45 is thin
because the clean-reference-vs-game-mix ceiling is only ~0.55).

**Design lessons baked into the tool:** center chroma frames (raw cosine has a non-negative floor
around 0.6); use NCC sliding-window correlation for a short-clip-into-long-reference match (global
xcorr dilutes the peak toward 0); require ≥85% overlap (max-over-offsets otherwise inflates short
slivers — white noise once scored 0.54 over 5s); the "null-lift" normalization idea was tried and
dropped (a consistent-key song self-correlates everywhere, producing a misleading tiny lift); the
rate-confidence gate must be high (0.40+) or a weak real-audio alignment can false-flag a chipmunk.

**Critical tool-trust lesson (Wave 09, 2026-06-09):** `audio_verify` once reported **MATCH / right
song / "not chipmunk"** on audio that was *actually severely corrupted* by a decimation-by-2 bug in
`StandardStream.cpp`'s `ConsumeData` (a float→int16 loop reading both `src[j]` and incrementing
`src++`, effectively reading every 2nd sample — 2×-pitch chipmunk plus broadband aliasing static).
The tool's chroma+fingerprint identity check is *too robust*: it matched the song through the
corruption, and the per-chunk 2× reads averaged out in the rate-search so that check missed it too.
The HF/THD and flatness sub-signals *were* flagging the corruption, but the aggregate verdict was
dominated by identity. **Lesson: add a hard spectral-rolloff/HF-energy quality gate (fail if
energy-above-11kHz is far above a clean-music baseline) so a "right song" identity match can't mask
"hosed playback" — and don't trust a MATCH verdict over an actual ear-test / spectrogram.** (The
underlying decimation bug itself — fixed by dropping the stray `src++`, gated `#ifndef HX_NATIVE`
so Wii stays byte-identical — is an audio-pipeline fix, not a tool fix; it's mentioned here only
because it's the case study that drove the tool-trust lesson.)

**Verified result (native build, 2026-06-08):** the default test song (`20thcenturyboy`) was
confirmed to be the right song, played correctly — not clipped, not noise, not chipmunked — via
both chroma and fingerprint identity. 83/84 songs had `.mogg` reference audio available (symlinked
from `orig-assets/extracted-xbox-full`) so `--rank` covers nearly the full roster. Reference docs:
`docs/native/audio-perf-loop/AUDIO_VERIFY_2026-06-08.md`, with baselines under
`docs/native/audio-perf-loop/baselines/w-verify-*`. The orchestrated, multi-signal *investigation*
mode (as opposed to a single check) is packaged as the `audio-perf-loop` skill.

Numerous follow-on audio waves (SFX latency, off-main-thread mixing, frame-stall load-in fixes, a
per-SFX-trigger PCM leak, an A/V calibration offset, missing venue-intro crowd audio) used this
tool and its harness as their verification backbone; those are audio/engine fixes rather than
tooling and are tracked in the audio/render project memory files, not duplicated here.

## Incremental load-perf (web) — diagnosis, infra & gotchas

A 2026-06-10 multi-agent investigation into web incremental-load stalls (screen transitions, preview
hover, in-game frame drops). Canonical plan/hub: `docs/native/incremental-load-perf/PLAN.md` (ranked
levers, invariants, wave-scheduled handoff) + `research/01..14`. The wave-by-wave engine/audio
fixes themselves are render/audio work tracked elsewhere; captured here is the durable **diagnosis,
infra, flags, and gotchas** that make load-perf debugging cheap next time.

**Diagnosis (measured, the key insight):** Milo's loader is **already async**; the native port
collapsed it at exactly **ONE seam** — `NativeStdioFile`'s ctor does a *blocking sync XHR* on a
MEMFS miss (`native_file.cpp` → `WebAssets.cpp` `xhr.open(...,false)` + per-byte `charCodeAt`), and
`ReadAsync` reads inline, leaving the `ReadDone`/`TempEof` machinery dormant. Native `lpu` (load
per update) is ~0ms everywhere ⇒ the **17 `PollUntilLoaded` drain sites are free once bytes are
resident — fix the File seam, NOT per-site conversion.** Preview hover froze because
`SongPreview::PrepareSong` → `TheSynth->NewStream` → `NewFile` sync-XHR'd the **entire 32-37MB mogg**
(NOT `MoggClip::EnsureLoaded` — an earlier claim that was refuted); warm re-hover was already fine
(MEMFS-resident). Boot/transition CPU cost was DXT→RGBA8 *software* decode, BE→LE milo byte-swap,
and first-draw upload bursts.

**What shipped (all web-only, `HX_NATIVE`/`__EMSCRIPTEN__`-gated → Wii byte-identical, all default-ON
and A/B'd behind opt-out flags):** async pending-File open (`WebPendingFile` + `/api/manifest`
size/404 oracle) and Range moggs (server 206) + preview prefetch; per-screen bundles
(`/api/bundle/screen/...`, hub→select fileReqs 20→1) + pipeline pre-warm (chunked Dawn flush,
550→80ms); loader read-ahead + a 2-slot Range read-ahead with a cross-open LRU chunk cache; wire-byte
cuts (SFX PCM→ogg sidecars, 59MB→8.5MB); **A4 mip-strip content downscale** (top-mip drop, venues =
75% of texture bytes) which turned a 1.5Mbps DNF into reaching game_screen; and **C progressive
sharpen** (A4-stripped venue loads fast, then a background sidecar fetch live-recreates each texture
at full res — byte-exact restore, keyed by a name-free `TexFingerprint`).

**Flags (opt-out unless noted):** `RB3_ASYNC_OPEN_OFF`, `RB3_MOGG_RANGE_OFF`, `RB3_BC_TEX_OFF`,
`RB3_PREVIEW_PREFETCH_OFF`, `RB3_LOADER_READAHEAD=N`, `RB3_MOGG_CACHE_MB=N`, `RB3_SFX_OGG_OFF`,
`RB3_WEB_DOWNSCALE`, `RB3_PROGRESSIVE_SHARPEN`, `RB3_SHARPEN_PER_FRAME=N`, `RB3_TEX_PREWARM_OFF`,
`RB3_GAMEWARM_OFF`, `RB3_UNPACK_CACHE_OFF`. (`RB3_METAMUSIC_SYNC`/`RB3_VENUE_SYNC` shipped default-SYNC,
i.e. those async levers are effectively no-ops as shipped.)

**Tools:** `analyze_net.py` (network census — use `peakConcurrent`/`overlappingMilos`/
`chunkReDownloads`, NOT `miloSerialΣ` which inflates under overlap), `scripts/web/_netmatrix.mjs`
(bandwidth/RTT matrix), `scripts/milo/mip_strip.py` (`strip`/`sharpen`), `firstframe-gate.mjs`,
`prewarm_encode_cache.py`, `_netbytes.py`.

**GOTCHAS (each cost real time the first time):**
- **"Stuck on song load (web)" is almost always a STALE DEPLOYED BUILD, not a code bug.** Rebuild
  `scripts/web/build.sh` on the deploy host. (Also: don't screenshot gameplay at +4s and call it
  broken — a song-intro cinematic runs up to ~25s at `songMs=0`; wait for `songMs>0`/track slide-in.)
- **Gate perf at 8Mbps/80ms + 4Mbps/150ms**, never only 20Mbps/40ms — the fast gate is provably
  blind to the serial-fetch wall-clock stall class (the canvas never freezes; ~85MB of milos
  download 100% serially, 45s for one venue milo @4Mbps).
- **A benchmark can lie — independently re-derive a load-bearing metric before building on it.** The
  "stem ring fills only ~743ms vs 9s" premise was a *bench bug*: `audio-stall-bench.mjs` seeded its
  low-water from the 32768-frame *output* ring (32768/44100 = 743ms), clamping every sample; the
  stem ring was always ~7-8s deep. A whole "realize the deep ring" workflow was spun on a bench bug.
- **At 4-8Mbps the pipe is throughput-bound (wall ≈ bytes/bandwidth).** Parallelism (read-ahead)
  kills RTT gaps but *cannot beat throughput* — the 40-50% wall cut did not materialize; the only
  remaining lever is **BYTES** (wire compression, content downscale). 1.5Mbps stays
  throughput-unplayable except via smaller assets.
- **`mip_strip.py` is Python byte-surgery because the engine has NO ObjectDir save path**
  (`DirLoader::SaveObjects` = `MILO_ASSERT(0)` stub). Downscaled assets + `.sharpen` sidecars are
  gitignored/machine-local → regenerate per-deploy (`gen_web_downscaled.py` +
  `prewarm_encode_cache.py --downscale`). The A4 visual gate must EXCLUDE BC5/DXN normals,
  BC3-alpha, and textures ≤256² (SSIM degraders); dc3's `validate_milo_entries.py` only checks the
  start entry-table, so the *visual* gate is the real texture-stream desync detector.
- **Engine web arms gate on `__EMSCRIPTEN__`, NEVER `HX_WEB`** (milo-engine is compiled `HX_NATIVE`-only).
  A native-only TU silently becomes a web no-op if it's missing from `RB3_WEB_NATIVE_GLUE`
  (`ERROR_ON_UNDEFINED_SYMBOLS=0` hides it) — check BOTH source lists.

## rb3-viewer asset renderer + the white-wig "deceptive 99.6% match" lesson

**`rb3-viewer` (2026-07-02, `5b8e0d05`, v2 `793e718d`)** — a standalone `.milo` asset renderer built
*into* `rb3-native` (no second link target): `rb3-native --viewer <milo-rel-path> [--out png]
[--sim N] [--list] [--azimuth/--elevation/--distance] [--test-bone] [--pose-dump] ...`; wrapper
`scripts/native/render-asset.py`; doc `docs/native/asset-viewer-2026-07-02/VIEWER.md`. It registers
char factories (`CharHair`/`OutfitConfig`/`RndAmbientOcclusion` Init) selectively — NOT wholesale
`CharInit`/`BandInit`, and NOT `Character` (a `CharacterTest`→overlay trap). GOTCHAS baked into it:
`OutfitConfig::Init` needs `BandCharDesc::Register()` first (else SIGABRT); `InitGpu` BEFORE `chdir`;
`_exit` after the PNG. `--sim` diverges for some strands standalone (no head frame / `CharCollide`
volumes) — `--test-bone` + an in-game gate are the reliable skinning checks. `RB3_HAIR_DBG=1` is the
hookup-coverage probe.

**White-wig bug FIXED (`81f38f3a`) — the durable lesson.** "Character renders with a white/collapsed
wig" was `CharHair::SimulateInternal` collapsing strand physics: the decomp **mis-scoped the closing
brace of `if (collides.size()!=0)`**, gating the per-point bone-update tail (SetWorldXfm / force /
friction / inertia / chain-advance) that the Wii target runs **unconditionally** (the empty-collides
`beq` lands *at* the tail, `.L_806D002C`; DC3 confirms). Collide-less free strands (crazyhawk,
ziggymullet, longmop, robertplant…) draped over faces; tight styles were fine. **objdiff read a
deceptive 99.6% "match" — the real bug hid as a single `diff_arg beq` branch-target mismatch.**
The fix was a one-brace move; 99.6% held (the CFG mismatch cleared, residual = regalloc noise) and
the in-game band-closeup gate passed. **LESSON: a `diff_arg` branch-target mismatch on a `beq` is a
possible real CFG bug, not noise** — the same "high match% masks a real bug" trap as the
normalized-masking sections above, but visible even at raw 99.6% fuzzy. Follow-ups all CLOSED
(`ede6911f`/`e023b8aa`): zero-coverage styles have authored `hookupFlags=0x0` (Wii-identical by
design, not a bug), and "white" was the collapsed-pose artifact (flat ribbon faces catching light),
not a texture problem. Evidence: `docs/native/asset-viewer-2026-07-02/` (scout-wig-bug.md,
land-report.md).
