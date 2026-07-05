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

## W0.2.S2 — done

**Commit:** `f66ea359` — "W0.2: loud-by-default stub shim + boot census (band3 stubs)" (CHANGES)

Switched the generator to loud mode and stood up the census. Boots headless
(`/api/health` 200) with the loud shim; PIE relocations resolved with no
linker complaints (`call __hmx_stub_first_hit@PLT` + RIP-relative `leaq`).

**Files touched:**
- `scripts/native/gen_band3_link_stubs.py` — added `--mode loud` (now the
  DEFAULT; `legacy` kept for A/B). Loud mode emits, per `func` row, a
  `__hmx_tramp_<i>` trampoline (already-hit fast path `cmpb/jne/xor/ret`;
  first-hit `movb` latch + `leaq __hmx_name_<i>(%rip),%rdi` + `call
  __hmx_stub_first_hit@PLT`; `xorl %eax,%eax; ret`), a `.rodata`
  `__hmx_name_<i>` asciz, and a `.bss` `__hmx_latch_<i>` (1 byte), plus
  `.weak <S>`/`.set <S>,__hmx_tramp_<i>`. Labels use a numeric index `<i>`
  (not the mangled symbol) so they are always valid+unique regardless of
  symbol text; the real name lives only in the asciz + the `.weak/.set`.
  `data` rows keep their own writable `.bss .zero <N>`. Also emits the paired
  C++ table `band3_stub_table.inc`. Idempotent; `--check` gates both `.s` and
  `.inc` for drift (`OK: no drift`).
- `native/src/band3_link_stubs.s` — regenerated (loud trampolines).
- `native/src/band3_stub_table.inc` (NEW, generated) — `struct HmxStubInfo
  {const char* name; char kind; char cls;}`, `kHmxStubTable[]` (582 rows,
  `kind` 'F'/'D', `cls` 'A'/'N'/'D'), `kHmxStubTotal=582 / Func=521 / Data=61`.
- `native/src/rb3_stub_census.cpp` (NEW) — `extern "C" __hmx_stub_first_hit`
  (log-once-to-stderr `[STUB] first call to <sym>` + mutex-guarded hit list;
  asm latch already guarantees once), `extern "C" __hmx_stub_census_startup`
  (prints `[STUB CENSUS] linked=...` banner + arms `atexit` hit dump; safe to
  call twice), and `__hmx_stub_census_assert_unreachable_hits(std::vector<
  std::string>&)` (the gate query S3's gtest consumes — collects hits whose
  registry class is 'A'). All output behind `!getenv("RB3_STUB_QUIET")`.
- `native/CMakeLists.txt` — one line: `rb3_stub_census.cpp` into rb3-native
  SOURCES (staged via partial `git apply --cached` so the concurrent W1.1
  `test_wgsl_validation.cpp` edit in the same file was NOT folded into this
  commit; W1.1 later committed it itself as `908a5d1f`).
- `native/src/main_native.cpp` — file-scope `extern "C" void
  __hmx_stub_census_startup();` + one call at the top of `main()` (all entry
  modes get the census).

**Build/verify** (`native/build-agent-W0.2`, reused from S1):
- `cmake --build ... --target rb3-native -j8` → clean link (no PIE reloc
  errors).
- Headless boot (`RB3_GAME=1 RB3_HTTP=1 RB3_HTTP_PORT=18424 MILO_HEADLESS=1
  RB3_DATA=orig-assets/extracted`) → `GET /api/health` = `200`
  `{"ok":true,"data":{"status":"ok",...}}`.
- Startup banner: `[STUB CENSUS] linked=582 (func=521 data=61)`.
- **Once-only holds:** 16 `[STUB] first call to …` lines, **0 duplicates**
  (per-frame stubs like `Movie::Impl::Draw`, `WaitingUserGate::Poll` logged
  exactly once across ~9+ frames — the asm latch works).
- **atexit dump** captured via a clean exit (`MILO_MAX_FRAMES=30`):
  `[STUB CENSUS] exit: 16 of 582 weak stubs hit this run` + the 16-name list.
  (That run's process exit code 139 is the pre-existing Vulkan-teardown
  shutdown segfault documented in `RunGame`, which fires AFTER the census
  atexit already printed — unrelated to this change.)

**Boot hit-list (16 stubs — the authoritative clean-boot fire set for S3):**
```
_ZN14DataResultListC1Ev
_ZN5Movie4Impl12PlatformInitEv
_ZN15SaveLoadManager4InitEv
_ZN11PlatformMgr25RegisterSignInserCallbackEPFbP4UsermE
_ZN17NetMessageFactory18RegisterNetMessageE6StringPFP10NetMessagevE
_ZN10MemcardMgr4InitEv
_ZNK7Profile9GetPadNumEv
_ZN14ProfilePictureC1EiPN3Hmx6ObjectE
_ZN12WiiFriendMgr17UseConsoleFriendsEb
_ZN11UIListState8ProviderEv
_ZN15DiscErrorMgrWii16RegisterCallbackEPNS_8CallbackE
_ZN15WaitingUserGate4PollEv
_ZN5Movie4Impl4DrawEv
_ZN15DiscErrorMgrWii18UnregisterCallbackEPNS_8CallbackE
_ZN6WiiRnd20SetTriFrameRenderingEb
_ZN15VirtualKeyboard17IsKeyboardShowingEv
```
All 16 are registry `func` rows currently classified `assert-unreachable`
(none are Bink* ok-noop). **S3 must reclassify these** to `ok-noop` (with a
dated note) after confirming return-0 is genuinely acceptable — EXCEPT any
that look like a real swallowed call worth flagging. Two candidates to scrutinize
in S3 (do not blind-silence): `_ZN5Movie4Impl4DrawEv` (Movie draw — the intro-
movie path; returning 0 = no video, matches the native no-video-backend story,
likely ok-noop) and `_ZN17NetMessageFactory18RegisterNetMessageE...` /
`_ZN15SaveLoadManager4InitEv` / `_ZN10MemcardMgr4InitEv` (init/register calls
whose no-op could silently drop registration — verify they are genuinely inert
offline before promoting).

**Deviation from PLAN.md** (recorded): trampoline/name/latch labels use a
numeric index `__hmx_tramp_<i>` rather than a sanitized symbol suffix. This is
strictly safer than the sanitize-and-dedupe approach the Design block sketches
(no possibility of a sanitization collision, and no dependence on the mangled
name being a valid asm label), and the human-readable name is preserved in the
`.rodata` asciz that the census logs. No functional difference.

**Remaining for S3:** reclassify the 16 boot-hit rows, add the gtest
`test_stub_census.cpp` + `stub_census_smoke.py`, prove GREEN, demo FAIL-RED.
