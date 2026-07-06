# W0.2 — Loud-by-default weak stubs + census + registry

**Item:** REFACTOR_PLAN W0.2 · lane doc `06-arch-crosscut.md` §2.3 (policy) / §2.1–2.2 (evidence).
**Planner:** Opus. **Status:** planned (implementation not started).
**Wave:** 1 (2026-07-05). Read `execution/README.md` HARD RULES before touching anything.

---

## Objective

Turn the silent, hand-asserted "none of these stubs is reached" belief into an **enforced,
loud-by-default invariant**. Concretely, for `native/src/band3_link_stubs.s`:

1. Replace the single shared silent `xorl %eax,%eax; ret` no-op (lines 17–21) with a per-symbol
   trampoline that **logs once to stderr on first call** and records the hit for a census, while
   keeping the already-hit path down to a few instructions.
2. Add a **startup + exit census dump** (linked stubs vs actually-hit stubs).
3. Add a **machine-readable registry** classifying every weak symbol as
   `assert-unreachable | ok-noop | data-blob`, and make the `.s` file **generated from that
   registry** (there is no generator today — the current regen procedure is a prose comment at
   `band3_link_stubs.s:14-16`; we introduce a real one so classification and emission cannot drift).
4. Add a **gate** (gtest in `rb3-tests` + a boot-smoke script) that fails red if a stub classified
   `assert-unreachable` is hit during a boot smoke, or if any weak symbol is unclassified / the `.s`
   has drifted from the registry.

This is the exact mechanism that would have surfaced the `DrawParticlesBillboard` (particles never
rendered) and `EndGame` (song never ended) invisible-failure bugs on frame 1.

### Faithful-reference / evidence citations

- Silent no-op body: `native/src/band3_link_stubs.s:17-21` (`__hmx_band3_noop_stub: xorl %eax,%eax; ret`).
- FUNC-stub aliasing pattern: `band3_link_stubs.s:23-…` (`.weak SYM` / `.set SYM, __hmx_band3_noop_stub`).
- DATA-stub reservations: `band3_link_stubs.s:1149-…` (`.bss`, per-symbol `.zero 256`/`4096`).
- Hand-asserted safety comment (the belief we are replacing): `band3_link_stubs.s:1-16, 24-37`.
- Scar-tissue precedent (NetSession promotion from weak stub → strong def after a zeroed
  `_ZTI10NetSession` crashed `dynamic_cast`): `band3_link_stubs.s:24-37, 1152-1157`.
- Prior-art loud path on the WEB build (native has the silent gap web already partly closed):
  `native/CMakeLists.txt:1035-1046` (`missing_stubs.js` + `rb3_pre.js` warn-then-return-0).
- Policy spec being implemented: `06-arch-crosscut.md` §2.3 items 1–3.
- Exit-gate wording: `REFACTOR_PLAN.md` Phase-0 exit gate item 3.

### MEASURED current counts (verify at implementation time; may drift)

`band3_link_stubs.s` = 1350 lines, **582 weak symbols**: **521 FUNC** (lines 23–1148, all aliased
to the one shared no-op) + **61 DATA** (lines 1149–end, each an own `.bss` `.zero` reservation).
The brief's "~832" is the total across *all three* GAS stub files
(`band3_link_stubs.s` 582 + `dta_link_stubs.s` + `rndobj_synth_link_stubs.s`); **W0.2's required
scope is `band3_link_stubs.s`** (the file the plan cites), with the other two folded in by S4 only
if the wave has time — the generator/registry are designed to extend to them.

---

## Design (binding for the subtasks)

**Source of truth = a registry TSV; the `.s` and a C++ table are GENERATED from it.**

- `native/src/band3_stub_registry.tsv` — one row per weak symbol, tab-separated:
  `symbol<TAB>kind<TAB>class<TAB>note`
  - `kind` ∈ `func | data`.
  - `class` ∈ `assert-unreachable | ok-noop | data-blob`.
    Invariants: `kind=data ⇒ class=data-blob`; `kind=func ⇒ class ∈ {assert-unreachable, ok-noop}`.
    Seeding rule (per §2.3): **unknown ⇒ assert-unreachable** (conservative). Seed `ok-noop` only
    where the current `.s` comment explicitly justifies "safe when called, returns 0" (e.g. the
    `Bink*` video no-ops, the NetSession base virtuals documented "never reached offline; noop").
  - `note` — free text carried into the generated `.s` as an inline comment (preserve the existing
    scar-tissue prose, incl. the "REMOVED" explanation blocks, so context is not lost).

- `scripts/native/gen_band3_link_stubs.py` — reads the registry, emits **both**:
  1. `native/src/band3_link_stubs.s` (the linkable stubs), and
  2. `native/src/band3_stub_table.inc` (a C++ array the census/gtest consume).
  The generator **asserts 1:1**: every registry row → exactly one emitted symbol, and refuses to
  emit if a `func` row lacks a valid class. Running it with no registry change must be a no-op
  (idempotent) — this is the drift gate.

**FUNC trampoline (generated, per symbol S), the loud path:**
```asm
    .p2align 4
__hmx_tramp_<S>:
    cmpb  $0, __hmx_latch_<S>(%rip)   # already-hit fast path: cmpb+jne+xor+ret
    jne   1f
    movb  $1, __hmx_latch_<S>(%rip)   # latch in asm (no cross-ABI addressing)
    leaq  __hmx_name_<S>(%rip), %rdi  # RIP-relative → PIE-safe
    call  __hmx_stub_first_hit@PLT    # extern "C"; logs once + records hit
1:  xorl  %eax, %eax                  # preserve legacy "returns 0" behavior
    ret
    .weak <S>
    .set  <S>, __hmx_tramp_<S>
```
plus `.rodata` `__hmx_name_<S>: .asciz "<S>"` and `.bss` `__hmx_latch_<S>: .zero 1`.
(`@PLT` + RIP-relative `leaq` are required because rb3-native links PIE; confirm at build time.)
Returning 0 for all is behavior-identical to the old shared no-op, so a called stub cannot regress.

**DATA stubs:** unchanged in shape — keep each symbol's own writable `.bss` `.zero N` reservation
(they are read *and* written: singletons/globals/vtables). They are `data-blob` in the registry and
are **not** runtime hit-tracked (no code runs; a zeroed read cannot be cheaply trapped). Keep the
`_ZTI…` REMOVED comments.

**Census C (`native/src/rb3_stub_census.cpp`, hand-written, `#include "band3_stub_table.inc"`):**
- `extern "C" void __hmx_stub_first_hit(const char* name)` — the latch already guarantees once, so
  this just: (a) unless `RB3_STUB_QUIET`, `fprintf(stderr, "[STUB] first call to %s\n", name)`;
  (b) push `name` onto a mutex-guarded `std::vector<const char*>` hit list.
- `__hmx_stub_census_startup()` — prints `[STUB CENSUS] linked=<total> (func=<f> data=<d>)` at boot
  (loud-by-default; silence with `RB3_STUB_QUIET`). Registers `atexit` that prints the hit count +
  the hit list. Table totals come from `band3_stub_table.inc`.
- `int __hmx_stub_census_assert_unreachable_hits(std::vector<std::string>& out)` — for each recorded
  hit, `strcmp`-match it to a `kHmxStubTable` row; collect those whose class is `assert-unreachable`.
  Used by the gtest.
- Table `band3_stub_table.inc` shape: `struct HmxStubInfo { const char* name; char kind; char cls; };
  static const HmxStubInfo kHmxStubTable[]; static const int kHmxStubTotal/Func/Data;`
  (`cls`: `A`=assert-unreachable, `N`=ok-noop, `D`=data-blob).

**Where it plugs in:**
- `rb3_stub_census.cpp` added to the `rb3-native` SOURCES list (`native/CMakeLists.txt`, near the
  `.s` files at ~646); `rb3-tests` inherits it automatically (it reuses rb3-native SOURCES via
  `get_target_property`).
- `__hmx_stub_census_startup()` called once early in `main()` (`main_native.cpp:758`) — or at the
  top of `RunBoot` (`main_native.cpp:517`). Prefer `main()` so all entry modes get the census.
- gtest `native/tests/test_stub_census.cpp` added to the `add_executable(rb3-tests …)` list
  (`native/CMakeLists.txt:696-706`).

**Two verification surfaces (both required):**
1. **gtest** `test_stub_census.cpp` — `EngineTestFixture` boots the engine, then asserts
   `__hmx_stub_census_assert_unreachable_hits()` returns empty, and asserts registry completeness
   (`kHmxStubTotal` == weak-symbol count; no `A`-class hit). This is the CI gate. (Note: the engine
   is a process-global singleton and tests are serialized, so the hit set here is process-wide —
   *stronger* than a clean boot, which is fine for the "never hit" assertion.)
2. **boot-smoke script** `scripts/native/stub_census_smoke.py` — runs the standalone `rb3-native`
   binary headless (`RB3_HTTP=1`, GET `/api/health`, exit), parses the `[STUB CENSUS]` / `[STUB]`
   stderr → the authoritative **clean-boot hit-list**, fails on any `assert-unreachable` hit, and
   with `--check-sync` regenerates the `.s`+`.inc` to a temp dir and diffs against the committed
   files (registry↔output drift gate). This produces the "note which stubs fire during boot" list.

---

## Subtasks

### W0.2.S1 — Registry + generator, behavior-preserving (MOVE)
- **model:** sonnet
- **goal:** Introduce `band3_stub_registry.tsv` (extracted from the current `.s`) and
  `gen_band3_link_stubs.py`, and regenerate `band3_link_stubs.s` in its **current silent-no-op
  form** so the file becomes generator-produced with **zero behavior change**. No trampolines, no
  census yet.
- **files to touch:**
  - NEW `native/src/band3_stub_registry.tsv`
  - NEW `scripts/native/gen_band3_link_stubs.py`
  - MODIFY `native/src/band3_link_stubs.s` (now generator output; still shared-no-op form)
- **approach:**
  1. Parse the current `native/src/band3_link_stubs.s`: walk `.weak <SYM>` lines; a symbol is
     `func` if it is before the `---- DATA stubs ----` marker (line ~1149) / aliased with `.set`,
     `data` if it is in the `.bss` section with a `.zero N` reservation (record N — 4096 for
     `TheRockCentral`, else 256). Capture any preceding `//` comment as the row's `note`.
  2. Emit `band3_stub_registry.tsv`: every func row `class=assert-unreachable` EXCEPT symbols whose
     current comment explicitly says safe/no-op-when-called (Bink*, the NetSession base virtuals) →
     `ok-noop`. Every data row `class=data-blob`, and store its reservation size in the note (e.g.
     `bss=4096`). Keep the header prose + the `_ZTI… REMOVED` blocks as generator template text.
  3. Write `gen_band3_link_stubs.py` with a `--mode legacy` (this subtask) that emits the file in the
     **current shape** (single `__hmx_band3_noop_stub`, `.set` aliases for func, `.bss .zero` for
     data) from the registry, preserving the header + REMOVED comments. Idempotent.
  4. Regenerate `band3_link_stubs.s`. Diff against the pre-change file: only ordering/comment
     cosmetics may differ; the set of `.weak` symbols and their func/data/no-op resolution MUST be
     identical.
- **verification:**
  - `python3 scripts/native/gen_band3_link_stubs.py --mode legacy` twice → second run leaves the tree
    clean (idempotent).
  - `grep -c '\.weak' native/src/band3_link_stubs.s` unchanged (582); func/data split unchanged
    (521/61).
  - `cmake -B native/build-agent-W0.2 -S native && cmake --build native/build-agent-W0.2 --target rb3-native -j8` succeeds.
  - `RB3_HTTP=1 <build>/rb3-native` boots; `/api/health` returns 200 (harness pattern:
    `scripts/native/song-select-capture.py`).
- **commit:** `W0.2: generate band3_link_stubs.s from registry (no behavior change)` (MOVE).

### W0.2.S2 — Loud per-symbol shim + census infra (CHANGES)
- **model:** opus
- **goal:** Switch the generator to emit per-symbol logging trampolines + name/latch tables + the
  C++ table `.inc`, add the census C file, wire it in, and confirm the game boots with the loud
  shim, capturing which stubs fire during boot.
- **files to touch:**
  - MODIFY `scripts/native/gen_band3_link_stubs.py` (add trampoline/`.inc` emission; make it the
    default mode)
  - MODIFY `native/src/band3_link_stubs.s` (regenerated: trampolines)
  - NEW `native/src/band3_stub_table.inc` (generated)
  - NEW `native/src/rb3_stub_census.cpp`
  - MODIFY `native/CMakeLists.txt` (add `rb3_stub_census.cpp` to the `rb3-native` SOURCES list near
    the `.s` files, ~line 646 — single added line)
  - MODIFY `native/src/main_native.cpp` (call `__hmx_stub_census_startup()` once early in `main()`)
- **approach:**
  1. Extend the generator: for each `func` row emit the `__hmx_tramp_<S>` trampoline + `.rodata`
     `__hmx_name_<S>` + `.bss` `__hmx_latch_<S>` per the Design block, and `.weak <S>` / `.set <S>,
     __hmx_tramp_<S>`. `data` rows keep the `.bss .zero N` shape. Also emit `band3_stub_table.inc`
     with one `HmxStubInfo` row per symbol + the three count constants. Sanitize `<S>` into a valid
     asm label suffix (map `.`, `$`, etc. to `_`, or index-suffix) — ensure uniqueness.
  2. Write `rb3_stub_census.cpp` per the Design block: `extern "C" __hmx_stub_first_hit`, startup +
     atexit dump, and `__hmx_stub_census_assert_unreachable_hits`. Guard the per-hit stderr line and
     the census dump behind `!getenv("RB3_STUB_QUIET")`.
  3. Add the source to CMake + the `__hmx_stub_census_startup()` call in `main()`.
  4. Build; resolve PIE relocation issues (`@PLT` on the `call`, RIP-relative `leaq`) if the link
     complains. Confirm `extern "C"` name matches the asm `call` target exactly.
- **verification:**
  - Build `rb3-native` in `native/build-agent-W0.2`.
  - `RB3_HTTP=1 <build>/rb3-native` headless boot → `/api/health` 200 (still boots).
  - stderr shows `[STUB CENSUS] linked=582 (func=521 data=61)` at startup and `[STUB] first call to
    <sym>` lines; at exit a hit-count + list. **Capture the full boot hit-list and paste it into
    STATUS.md** (satisfies the brief's "note which stubs actually fire during boot").
  - Sanity: re-hitting a stub does not re-log (once-only).
- **commit:** `W0.2: loud-by-default stub shim + boot census (band3 stubs)` (CHANGES).

### W0.2.S3 — Reclassify boot-hit stubs, gate gtest + sync script, fail-red demo (CHANGES)
- **model:** opus
- **goal:** Use S2's boot hit-list to promote genuinely-reached func stubs to `ok-noop`, add the
  gtest gate + the boot-smoke/sync script, prove GREEN, then demonstrate the required FAIL-RED.
- **files to touch:**
  - MODIFY `native/src/band3_stub_registry.tsv` (promote boot-hit func rows
    `assert-unreachable → ok-noop`, note: `hit during boot smoke <date>; returns 0 accepted`)
  - MODIFY `native/src/band3_link_stubs.s` + `native/src/band3_stub_table.inc` (regenerated)
  - NEW `native/tests/test_stub_census.cpp`
  - NEW `scripts/native/stub_census_smoke.py`
  - MODIFY `native/CMakeLists.txt` (add `test_stub_census.cpp` to the `add_executable(rb3-tests …)`
    list, ~lines 696–706 — single added line)
- **approach:**
  1. From S2's captured hit-list, for each `assert-unreachable` func stub that fired during a clean
     boot: confirm returning 0 is genuinely acceptable (it reached a no-op that the game tolerates),
     then set its class to `ok-noop` with a dated note. If any hit looks like a *real* swallowed
     call (an invisible-failure candidate, à la particles/NetSession), do NOT silence it — record it
     in STATUS.md as a finding and leave it `assert-unreachable` so the gate stays red on it (that is
     the tool working; flag for a follow-up item). Regenerate `.s`+`.inc`.
  2. `test_stub_census.cpp`: an `EngineTestFixture`-derived test that, after boot, calls
     `__hmx_stub_census_assert_unreachable_hits(out)` and `EXPECT_TRUE(out.empty())` (printing the
     offending names on failure), plus `EXPECT_EQ(kHmxStubTotal, <expected>)` and a check that no
     row is unclassified.
  3. `stub_census_smoke.py`: launch `rb3-native` headless (`RB3_HTTP=1`), poll `/api/health`, GET it,
     terminate, parse stderr census → exit non-zero on any `assert-unreachable` hit; `--check-sync`
     mode regenerates `.s`+`.inc` into a temp dir and `diff`s against the committed files (drift =
     non-zero). Mirror the launch/poll pattern in `scripts/native/song-end-test.py`.
  4. Build `rb3-tests`; run `test_stub_census` → GREEN.
  5. **FAIL-RED demo:** edit the registry to demote ONE boot-hit symbol back to `assert-unreachable`,
     regenerate, rebuild, run `test_stub_census` → it fails red listing that symbol. Paste the red
     output into STATUS.md, then revert to the GREEN registry (do NOT commit the demoted state).
- **verification:**
  - `cmake --build native/build-agent-W0.2 --target rb3-tests -j8 && ctest --test-dir native/build-agent-W0.2 -R StubCensus` GREEN.
  - `python3 scripts/native/stub_census_smoke.py` exit 0 on the committed registry; `--check-sync`
    exit 0 (no drift).
  - STATUS.md contains: the boot hit-list, the promotion list, and the captured RED output from the
    fail-red demo.
- **commit:** `W0.2: stub-census gate (gtest + smoke) + reclassify boot-hit stubs to ok-noop` (CHANGES).

### W0.2.S4 — Extend generator/registry to the other two GAS stub files (OPTIONAL, defer if wave is short)
- **model:** sonnet
- **goal:** Bring `dta_link_stubs.s` and `rndobj_synth_link_stubs.s` under the same generator +
  registry + census so the "linked vs hit" census is complete across all native weak stubs.
- **files to touch:** NEW per-file registries (or extend the one registry with a `file` column),
  MODIFY `scripts/native/gen_band3_link_stubs.py` (parameterize the input/output file), MODIFY
  `native/src/dta_link_stubs.s`, `native/src/rndobj_synth_link_stubs.s`.
- **approach:** same extract→generate→trampoline pipeline as S1/S2, one file at a time, each with
  its own MOVE (regenerate silent form) then CHANGES (trampolines) pair, reusing the S2 census C
  (the table `.inc` grows to include these symbols). Reclassify boot-hits as in S3.
- **verification:** same as S2/S3 for each file; boot still 200; gate GREEN; census `linked` count
  grows to the full ~832.
- **commit:** `W0.2: extend loud stubs + census to dta/rndobj_synth stub files` (MOVE then CHANGES).
- **note:** Not required for the W0.2 exit gate. Only start if S1–S3 are landed and green.

---

## Exit criteria (measurable)

1. `native/src/band3_link_stubs.s` is produced by `scripts/native/gen_band3_link_stubs.py` from
   `native/src/band3_stub_registry.tsv`; running the generator on the committed registry leaves the
   tree clean (idempotent) and `stub_census_smoke.py --check-sync` exits 0.
2. Every weak symbol in the `.s` has exactly one registry row (582 rows; 521 func + 61 data); no
   unclassified rows. The sync/completeness check **fails red** if a `.weak` lacks a row or the `.s`
   drifts from the registry.
3. `rb3-native` (built in `native/build-agent-W0.2`) boots headless (`RB3_HTTP=1`, `/api/health`
   200) with the loud shim; stderr shows `[STUB CENSUS] linked=582 …` + per-symbol `[STUB] first
   call to …` lines; a **non-empty boot hit-list is captured and recorded in STATUS.md** (REFACTOR_PLAN
   Phase-0 exit gate item 3: "census emits a non-empty hit-list on boot").
4. `test_stub_census` in `rb3-tests` is GREEN: no `assert-unreachable` stub was hit during boot.
5. **FAIL-RED demonstrated and recorded:** demoting one boot-hit symbol to `assert-unreachable`
   makes `test_stub_census` fail red (RED output pasted in STATUS.md); reverted to GREEN before the
   final commit.
6. Already-hit fast path is `cmpb/jne/xor/ret` (≤ ~4 instructions); no measurable boot-time
   regression vs the pre-change binary.

---

## Risks / conflicts

- **`native/CMakeLists.txt` `add_executable(rb3-tests …)` list (lines ~696–706) is a hot Wave-1
  collision point** — W0.1 (skin golden), W0.3 (draw-log golden), W0.4 (bone live-pose) all add a
  test file there too. Mitigation: add exactly ONE line (`tests/test_stub_census.cpp`) in S3; hold
  `flock /tmp/rb3-git.lock` around `git add`+`commit`; a conflict here is a trivial append-merge.
- **`native/CMakeLists.txt` `rb3-native` SOURCES list (~line 646)** — S2 adds one line
  (`rb3_stub_census.cpp`). Same single-line + flock mitigation.
- **W0.2 does NOT touch `Rnd_Wgpu_RB3.cpp`** → no collision with W0.3 / W0.6 / W1.1 (the render
  monolith / flag-registry / WGSL lanes). W0.2 is otherwise well-isolated: its owned files
  (`band3_link_stubs.s`, `band3_stub_registry.tsv`, `band3_stub_table.inc`, `rb3_stub_census.cpp`,
  `gen_band3_link_stubs.py`, `test_stub_census.cpp`, `stub_census_smoke.py`) are new or W0.2-only.
- **`main_native.cpp`** — S2 adds one call line in `main()`. Low collision risk (other items rarely
  edit the entry point); keep the edit to a single line, stage only this file, flock.
- **Do not leave `rb3-tests` red.** S3 must finish GREEN after the fail-red demo; the demoted-registry
  state is for demonstration only and must be reverted before committing.
- **PIE relocations:** rb3-native links PIE — the trampoline `call __hmx_stub_first_hit@PLT` +
  RIP-relative `leaq` are required. If the assembler/linker still complains, fall back to an
  indirect load of the symbol address; note the resolution in STATUS.md.
- **Stage only your files; never `git add -A`.** Concurrent agents have unrelated unstaged edits.
- **Never bump `MILO_ENGINE_PIN`** (coordinator does it once per wave). All W0.2 work is rb3-repo
  only — no `../milo-native-engine` changes.
