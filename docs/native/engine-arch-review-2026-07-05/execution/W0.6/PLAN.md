# W0.6 — NativeCompat flag registry SKELETON + generated tracking doc

**Item:** REFACTOR_PLAN §W0.6 · lane `06-arch-crosscut.md` §3.1–3.3 · Wave 1.
**Planner:** Opus (this doc). **Status:** ready for implementation.

## Objective

Collapse the **229 scattered `getenv` reads** (140 distinct engine flags + 89 distinct
glue flags, MEASURED 2026-07-05) into **one typed registry** that is the single source of
truth, and generate a **burn-down tracking ledger** from it. This wave delivers the
**SKELETON only** — the registry module, the census/gen/check tool, the generated ledger,
and a **5-call-site demonstration** of the new read pattern. It does **NOT** rewire the other
~224 sites; that churn collides with Phase-1 monolith extraction (W1.2–W1.7) and is a later
wave (§W5.3 burns the ledger down to zero).

**Why this is week-1 quick-win scope (REFACTOR_PLAN:44):** the registry + generated doc are
additive infra with a tiny, uncontended behaviour-preserving footprint. It unblocks Phase 3
lighting (REFACTOR_PLAN:112 — "delete the ~10 default-ON lighting workaround flags … Blocked
on W0.6 flag registry") and W2.x (REFACTOR_PLAN:121).

### Faithful-reference citations

There is no Wii/decomp faithful reference for this item — every flag in scope is a
**native-port-only** construct (the Xbox/Wii build has no `getenv` compat layer; see
`milo-native-engine/src/platform/NativeSettings.h:5-7` "don't exist on Xbox 360"). The
"faithful alternative" column in the ledger tracks, per workaround flag, whether the
**matched-source** behaviour it stands in for is live yet — that status is authored by hand
from lane 06 §3.1–3.2 and existing memory entries, not derived from a decomp file:line.

### Authoritative flag classification (source of truth for the curated sidecar)

Lane `06-arch-crosscut.md` §3.1 already enumerates and classifies the load-bearing set. The
implementer MUST seed the curated classification from it verbatim:

- **§3.1(a) probes** (`class=probe`, default-OFF, archaeological): `BONE_PROBE`,
  `BONE_PROBE_NAME`, `SHARD_DBG`, `SHARD_BONE_DBG`, `CHAIN_PROBE/MTX/FORCE/COMPOSE`,
  `XBONE`, `XBONE_TRACK`, `C8_PROBE`, `C8_EVERY`, `SKIN_PROBE`, `SKEW_PROBE`, `VERT_PROBE`,
  `SLOT_PROBE`, `GEM_VTX`, `SMASH_DBG`, `HUB_BAR_PROBE`, `IK_SHARD_VERT`, … (dozens more —
  the `*_PROBE`/`*_DBG`/`*_TRACE` naming families).
- **§3.1(b) shipped default-ON workarounds** (`class=workaround`, opt-out): **Engine (21)**
  `RB3_TRACK_LIGHT_OFF`, `RB3_VENUE_LIGHT_OFF`, `RB3_BLOOM_OFF`, `RB3_HIGHWAY_BLOOM_OFF`,
  `RB3_HIGHWAY_WATERMARK_OFF`, `RB3_CHAR_REAL_LIGHT_OFF`, `RB3_CROWD_DIM_OFF`,
  `RB3_COMPOSE_MULT_OFF`, `RB3_FRET_GLOW_OFF`, `RB3_PART_HAZE_OFF`, `RB3_PART_MATCOLOR_OFF`,
  `RB3_PART_NEARFADE_OFF`, `RB3_NOISE_OFF`, `RB3_PP_OFF`, `RB3_RTT_OFF`, `RB3_BC_TEX_OFF`,
  `RB3_SCREENMASK_FALLBACK_OFF`, `RB3_SCROLLBAR_THUMB_FIX_OFF`, `RB3_UNPACK_CACHE_OFF`,
  `RB3_PIPELINE_PREWARM_OFF`, `SHARD_GUARD_OFF`; plus skinning family `RB3_NO_SKEL_REBAKE`,
  `RB3_NO_SKEL_WORLDFIX`, `RB3_NO_SKIN_CLAMP`, `RB3_NO_STEM_ANCHOR`, `RB3_NO_MESH_CACHE`,
  `RB3_NO_PRECLEAR`, hub `RB3_NO_HUB_BAR_PLACEMENT_FIX`, `RB3_NO_HUB_HIGHLIGHT_FIX`,
  `RB3_NO_HUB_BAR_SHARD_EXEMPT`. **Glue (17)** `RB3_GAMEWARM_OFF`, `RB3_HEAP_TRIM_OFF`,
  `RB3_MOGG_RANGE_OFF`, `RB3_MOGG_READAHEAD_OFF`, `RB3_SCREEN_BUNDLES_OFF`,
  `RB3_SFX_CACHE_OFF`, `RB3_SFX_OGG_OFF`, `RB3_TEX_PREWARM_OFF`, `RB3_XMA_PREFETCH_OFF`,
  `RB3_PREVIEW_PREFETCH_OFF`, `RB3_ASYNC_OPEN_OFF`, `RB3_BOOT_BUNDLE_OFF`,
  `RB3_CROWD_IMPOSTER_OFF`, `RB3_NO_SFX`, `RB3_NO_SETLIST_FIX`, `RB3_NO_GUEST_PROFILE`,
  `RB3_NO_AV_CALIBRATION`.
- Everything else (value knobs `MILO_CAM_*`, `RB3_*_MS`, `*_SCALE`, `*_THRESH`, HTTP/port,
  `RB3_DATA`/`RB3_GAME` paths) → `class=perf` or `class=feature` per the sidecar author's
  read; unclassified new finds → `class=unknown` (surfaces as a NEEDS-CLASSIFICATION row).

## Key facts established by investigation (2026-07-05)

1. **Engine consumption is a live working-tree `add_subdirectory`, NOT a fetched pin.**
   `native/CMakeLists.txt:72-86, 224` — `MILO_ENGINE_PATH=../../milo-native-engine` is added
   directly; the `MILO_ENGINE_PIN` check is a **WARNING only** (never a hard error). ⇒ A new
   engine header lands in the glue build **immediately** after the engine commit, with **no
   pin bump** (which is forbidden by Wave hard-rule 3 anyway). The pin-mismatch warning during
   the wave is EXPECTED; do not silence it.
2. **`src/` is `PUBLIC` on target `milo-engine`** (`milo-native-engine/CMakeLists.txt:456-458`).
   ⇒ glue includes the new module as `#include "platform/NativeCompatFlags.h"` exactly like
   the existing `#include "platform/GameRenderHook.h"` (`rb3/native/src/rb3_render_hook.cpp:27`).
3. **Where the module belongs:** `milo-native-engine/src/platform/`, added to
   `MILO_ENGINE_PLATFORM_SOURCES` (`CMakeLists.txt` — the non-GPU always-built partition,
   list starts at the `set(MILO_ENGINE_PLATFORM_SOURCES` line; alpha-insert after
   `MapFile_Stub.cpp`). This partition is compiled for **both** dc3 and rb3 flavors and web,
   so the registry is universally available. Do NOT put it in the `_RB3`/GPU partitions.
4. **Existing read idioms are NOT uniform — this is the central correctness hazard.** Two
   distinct semantics ship today (both MEASURED):
   - **presence-based** `getenv("X") != nullptr` — ANY value incl. `""`/`"0"` triggers.
     e.g. `rb3_gamewarm_native.cpp:85` (`RB3_GAMEWARM_OFF`), `:132` (`RB3_TEX_PREWARM_OFF`).
   - **truthy-based** `e && e[0] && e[0] != '0'` — only non-empty, non-`"0"` triggers.
     e.g. `rb3_platform_native.cpp:77` (`RB3_NO_SFX`), `rb3_prefetch_native.cpp:32`
     (`RB3_PREVIEW_PREFETCH_OFF`), `rb3_heap_maint_native.cpp:45` (`RB3_HEAP_TRIM_OFF`),
     and the engine gate `Rnd_Wgpu_RB3.cpp:6157/1196/2278`.
   A registry that resolves every flag with one rule would **silently change behaviour** at
   the mismatched sites. The registry MUST expose both resolution modes and each rewire MUST
   preserve its site's exact original semantics (verified). This is why S2 is `opus`.
5. **Read-once precedent:** `NativeSettings::Init()` + `NativeSettings::Get()`
   (`NativeSettings.h:49-89`) is the house pattern (function-local `static` singleton, env
   parsed once, `fprintf(stderr,"[NativeSettings] …")` on active overrides). Mirror it.
6. **The 5 chosen rewire sites are all native-target glue** (confirmed present in
   `native/CMakeLists.txt` for `rb3-native`): `rb3_gamewarm_native.cpp`,
   `rb3_heap_maint_native.cpp`, `rb3_platform_native.cpp`, `rb3_prefetch_native.cpp`. None is
   `Rnd_Wgpu_RB3.cpp` (owned by W1.1/W0.3) or any other Wave-1 file (see §Risks).

## Design (authoritative for S1–S3)

**Data model (typed):**
```
enum class FlagClass { Probe, Workaround, Feature, Perf, Unknown };
enum class FlagRead  { Presence, Truthy, Value };   // legacy env semantics preserved
struct NativeCompatFlag {
    const char* name;              // "RB3_GAMEWARM_OFF"
    const char* def;               // human default string: "on" (opt-out) / "off" / "240"
    FlagClass   cls;
    FlagRead    read;              // how the raw env string is interpreted
    const char* owner;             // subsystem: "render/lighting", "load/perf", "skinning", …
    const char* faithfulStatus;    // "n/a" | "not-live: <reason>" | "live" | "probe"
    const char* docAnchor;         // ledger row anchor, e.g. "RB3_GAMEWARM_OFF"
};
```
- **The table is GENERATED** into `NativeCompatFlags.gen.inc` (committed) by the census tool,
  by joining the fresh scan (names + guessed read-mode + call-site count) with the **curated
  classification sidecar** (`NativeCompatFlags.classification.json`, hand-authored `class`/
  `owner`/`faithfulStatus`). New scanned flags absent from the sidecar emit a row with
  `cls=Unknown` (a NEEDS-CLASSIFICATION marker). Regen is **idempotent**: curated fields live
  only in the sidecar, never in generated output.
- **No runtime JSON.** `.gen.inc` is pre-baked C brace-init included by `NativeCompatFlags.cpp`.
- **Read-once accessor** (mirrors `NativeSettings::Get()`):
  - `NativeCompat& NativeCompat::Get();` — function-local static; on first call resolves every
    table row's runtime value once (env read once) and caches. Logs active non-default
    workarounds to stderr `[NativeCompat] …` like NativeSettings does.
  - Semantics-preserving query for the demo:
    `bool NativeCompat::OptOutActive(const char* name);` → returns the feature-ENABLED state
    for an opt-out flag, resolving the raw env by the flag's declared `FlagRead` mode
    (`Presence` for `RB3_GAMEWARM_OFF`/`RB3_TEX_PREWARM_OFF`; `Truthy` for the rest). i.e. it
    reproduces `static int s=-1; s=(getenv…?0:1); return s;` **exactly** per mode.
  - Table exposure for the doc generator/self-check: `span<const NativeCompatFlag> Table();`
    and `const NativeCompatFlag* Find(const char* name);`.
- **Probe compile-out MECHANISM (skeleton only, do not mass-apply):** define
  `#ifndef MILO_COMPAT_PROBES\n  #define MILO_COMPAT_PROBES (defined(DEBUG)||!defined(NDEBUG))\n#endif`
  and `bool NativeCompat::ProbeActive(const char* name)` that returns `false` unconditionally
  when `!MILO_COMPAT_PROBES` (so release strips probe reads). Demonstrate on ≤1 probe or just
  ship the helper unused — do NOT rewire the dozens of probe sites this wave.

**Tool (`scripts/analysis/native_compat_census.py`) subcommands:**
- `scan`   → walk `milo-native-engine/src/**` + `rb3/native/src/**` for `getenv("…")`; emit
  deterministic JSON inventory `{name, files:[…], sites:N, readModeGuess, defaultGuess}` to
  a fixed path. Deterministic ordering (sorted by name).
- `gen`    → join scan ∪ sidecar → write `NativeCompatFlags.gen.inc` (engine) + the ledger
  markdown (S3 path below). Idempotent.
- `check`  → fresh `scan` vs the committed registry's known-flag set; **exit nonzero** if any
  scanned `getenv` flag is absent from the registry (the "flags any getenv NOT in the
  registry" gate), OR if `gen` output would differ from committed (regen-clean gate).
- `--selftest` → built-in fixtures proving scan finds both idioms and `check` goes red on an
  injected unregistered flag. Mirror the house style of
  `scripts/analysis/flat_member_retype_scan.py` (`--selftest` convention).

**Generated ledger:** `docs/native/engine-arch-review-2026-07-05/NATIVE_COMPAT_LEDGER.md` —
one table row per flag: `name | class | default | owner | faithful-status | sites`. Header
banner says "GENERATED by scripts/analysis/native_compat_census.py — do not edit by hand".
Includes a summary line: total flags, count by class, and the **default-ON-workaround count**
(the number §W5.3 must drive to 0).

## Subtasks

### W0.6.S1 — Census/gen/check tool + sidecar schema  ·  model: sonnet
**Goal:** the mechanical scanning tool + the curated-sidecar format, standalone and self-tested.
**Files to create/touch:**
- `rb3/scripts/analysis/native_compat_census.py` (new)
- `milo-native-engine/src/platform/NativeCompatFlags.classification.json` (new — seed the
  §3.1 classifications above; leave `faithfulStatus` best-effort, `owner` by subsystem)
**Approach:**
1. Implement `scan`: regex `getenv\("([A-Za-z0-9_]+)"\)` over `*.cpp`/`*.h`/`*.mm` under the
   two roots (absolute paths from a repo-root constant; the engine root is
   `../../milo-native-engine` relative to rb3). Record per-flag file:line list, site count,
   and `readModeGuess` by inspecting the surrounding ~2 lines: `!= nullptr`/`!= NULL` ⇒
   `presence`; `[0] && … != '0'` ⇒ `truthy`; `atoi`/`atof` ⇒ `value`; else `unknown`. Emit
   sorted JSON to `rb3/docs/native/engine-arch-review-2026-07-05/execution/W0.6/census.json`.
2. Define the sidecar JSON schema (`{ "FLAG": {class, owner, faithfulStatus}, … }`) and seed
   it from the §3.1 lists in this PLAN (verbatim class assignment). Do NOT invent flags — only
   classify what exists.
3. Implement `gen`: pure function of (scan ∪ sidecar). Emits nothing here yet writes to the
   engine `.gen.inc` path + ledger path but those are consumed in S2/S3; for S1, `gen` must
   run and produce syntactically-valid C rows (each row a `{ "NAME","on",FlagClass::…,
   FlagRead::…,"owner","status","NAME" },` line). Unknown-sidecar flags ⇒ `FlagClass::Unknown`.
4. Implement `check`: re-scan, compare set against the flag names embedded in the committed
   `.gen.inc` (grep the names out — do NOT compile). Nonzero + a clear list on any missing
   flag or on `gen`-would-differ.
5. `--selftest`: temp-dir fixtures with a `presence` and a `truthy` `getenv`, assert scan
   classifies both; inject an unregistered flag into a fake `.gen.inc` set and assert `check`
   exits nonzero. Target 6/6-style pass banner.
**Verification:**
- `python3 rb3/scripts/analysis/native_compat_census.py --selftest` → all pass.
- `python3 …/native_compat_census.py scan` → `census.json` lists ≥229 distinct flags
  (sanity: `grep -c '"name"' census.json` ≳ 229) and correctly tags `RB3_GAMEWARM_OFF` as
  `presence`, `RB3_NO_SFX` as `truthy`.
**Commit:** rb3 repo, `W0.6:` prefix, files above only (flock `/tmp/rb3-git.lock`). Append
STATUS.

### W0.6.S2 — Registry module (engine) + 5 uncontended rewires  ·  model: opus
**Goal:** the typed read-once registry, seeded from S1's `gen`, with 5 behaviour-preserving
call-site rewires spanning BOTH read modes.
**Files to create/touch:**
- `milo-native-engine/src/platform/NativeCompatFlags.h` (new)
- `milo-native-engine/src/platform/NativeCompatFlags.cpp` (new)
- `milo-native-engine/src/platform/NativeCompatFlags.gen.inc` (generated by `gen`, committed)
- `milo-native-engine/CMakeLists.txt` (add `.cpp` to `MILO_ENGINE_PLATFORM_SOURCES`, alpha
  order after `MapFile_Stub.cpp`)
- rb3 rewires (5 sites): `rb3/native/src/rb3_gamewarm_native.cpp` (`RB3_GAMEWARM_OFF` @~85,
  `RB3_TEX_PREWARM_OFF` @~132 — **presence** mode), `rb3/native/src/rb3_heap_maint_native.cpp`
  (`RB3_HEAP_TRIM_OFF` @~45 — **truthy**; rewire ONLY the OFF gate, leave `RB3_HEAP_TRIM_FRAMES`
  value-read as-is), `rb3/native/src/rb3_platform_native.cpp` (`RB3_NO_SFX` @~77 — **truthy**),
  `rb3/native/src/rb3_prefetch_native.cpp` (`RB3_PREVIEW_PREFETCH_OFF` @~32 — **truthy**).
**Approach:**
1. Author `NativeCompatFlags.h` per the Design §: `FlagClass`/`FlagRead` enums,
   `NativeCompatFlag` struct, `NativeCompat` class with `Get()`, `OptOutActive(name)`,
   `Find(name)`, `Table()`, `ProbeActive(name)`, and the `MILO_COMPAT_PROBES` macro.
2. `NativeCompatFlags.cpp`: `static const NativeCompatFlag kFlags[] = {
   #include "NativeCompatFlags.gen.inc" };`, resolution logic per `FlagRead` (Presence:
   `getenv(name)!=nullptr`; Truthy: `e&&e[0]&&e[0]!='0'`; Value: raw string), read-once
   caching, `[NativeCompat]` stderr log of active non-default workarounds.
3. Run `python3 scripts/analysis/native_compat_census.py gen` to produce `.gen.inc` + refresh
   the sidecar if new flags surfaced. Commit the generated `.inc`.
4. Rewire the 5 sites: replace each `static int s=-1; s=(getenv…?0:1)` with
   `NativeCompat::Get().OptOutActive("RB3_…")`, **matching the flag's `FlagRead` mode to the
   original idiom** (presence for gamewarm/tex-prewarm, truthy for the other three). The
   observable return value MUST be identical for every input (`unset`, `""`, `"0"`, `"1"`,
   `"x"`). Keep the enclosing helper function signatures unchanged. Add
   `#include "platform/NativeCompatFlags.h"`.
5. Build both targets in your own dir.
**Verification:**
- `cmake -B rb3/native/build-agent-W0.6 -S rb3/native && cmake --build rb3/native/build-agent-W0.6 --target rb3-native -j8` → clean (pin-mismatch WARNING expected, not an error).
- `cmake --build rb3/native/build-agent-W0.6 --target rb3-tests -j8` → builds; run it, green.
- Behaviour parity spot-check (headless): boot `RB3_HTTP=1 …/rb3-native` once with no env and
  once with `RB3_GAMEWARM_OFF=0` and confirm the `[NativeCompat]`/gamewarm log matches the
  pre-rewire behaviour (presence mode ⇒ `RB3_GAMEWARM_OFF=0` DISABLES gamewarm, same as before).
- `NativeCompat::Get().Find("RB3_NO_SFX")->read == FlagRead::Truthy` (assert in a tiny
  rb3-tests case OR a one-off `/api/dta/eval`-adjacent check; a gtest is preferred).
**Commits (separate, per repo, flock the matching lock):**
- engine: `W0.6: add NativeCompat flag registry (additive infra, no behaviour change)` —
  `.h/.cpp/.gen.inc` + CMakeLists (milo-native-engine repo, `/tmp/milo-engine-git.lock`).
- engine: sidecar refresh if changed (may fold into the above).
- rb3: `W0.6: route 5 opt-out flags through NativeCompat (behaviour-preserving)` — MOVES
  commit, 4 glue files (`/tmp/rb3-git.lock`). Do NOT bump `MILO_ENGINE_PIN`.
Append STATUS with all SHAs.

### W0.6.S3 — Generate ledger + wire clean-regen check + fail-red demo  ·  model: sonnet
**Goal:** the generated burn-down ledger, a green regen/coverage check, and a recorded fail-red.
**Files to create/touch:**
- `docs/native/engine-arch-review-2026-07-05/NATIVE_COMPAT_LEDGER.md` (generated, committed)
- optionally `milo-native-engine/tests/` — a 1-case gtest asserting `NativeCompat::Table()` is
  non-empty and every row's `name` is unique (cheap coverage guard). Only if it slots into the
  existing gtest CMake cleanly; otherwise skip and rely on the python check.
**Approach:**
1. Run `python3 scripts/analysis/native_compat_census.py gen` → writes the ledger MD. Verify
   the summary line reports the default-ON-workaround count (~38 per §3.2) — this is the number
   §W5.3 drives to 0.
2. Run `check` → must exit 0 (registry covers every scanned flag, regen is clean). Record the
   green output.
3. **Fail-red demonstration (REQUIRED):** temporarily add `getenv("RB3_UNREGISTERED_DEMO_W06")`
   to a scratch line in one glue `.cpp`, run `check`, capture the NONZERO exit + the
   "not in registry" message into STATUS.md, then REVERT the scratch edit (do not commit it).
   Re-run `check` → back to 0.
4. Commit the ledger.
**Verification:**
- `python3 scripts/analysis/native_compat_census.py check; echo $?` → `0`.
- Fail-red transcript (nonzero + flag name) pasted into STATUS.md.
- `NATIVE_COMPAT_LEDGER.md` exists, has one row per flag, banner marks it generated.
**Commit:** rb3 repo, `W0.6:` prefix, ledger (+ optional engine gtest to milo-engine repo
separately). flock. Append STATUS.

## Exit criteria (measurable)

1. `milo-native-engine/src/platform/NativeCompatFlags.{h,cpp,gen.inc}` exist, compile into
   `milo-engine`, and expose a read-once `NativeCompat::Get()` accessor + typed table.
2. `rb3-native` AND `rb3-tests` build clean from `native/build-agent-W0.6` (pin-mismatch
   WARNING only). Any added gtest is green.
3. `scripts/analysis/native_compat_census.py` exists with `scan`/`gen`/`check`/`--selftest`;
   `--selftest` passes; `scan` finds ≥229 distinct flags and tags read-mode correctly for the
   two probes named in §Key-facts-4.
4. Exactly **5 call sites** rewired (across ≥2 files, spanning BOTH presence and truthy modes),
   each behaviour-identical for inputs {unset,"","0","1","x"}. No non-listed file touched.
5. `NATIVE_COMPAT_LEDGER.md` is generated (not hand-written), lists every flag with class +
   default + owner + faithful-status, and reports the default-ON-workaround count.
6. **`check` is GREEN** on the committed tree (registry ⊇ every scanned flag, regen clean) AND
   **demonstrably RED** when an unregistered `getenv` is present — the red transcript is
   recorded in STATUS.md (fail-red requirement).
7. `MILO_ENGINE_PIN` unchanged; commits obey MOVES-xor-CHANGES and per-repo flock/staging.

## Risks / conflicts

- **Contended file — `Rnd_Wgpu_RB3.cpp`:** owned by **W1.1** (WGSL externalization) and **W0.3**
  (draw-log golden), which chain on it this wave. It holds 113 `getenv`s and the juiciest
  default-ON gates, but **DO NOT rewire any of them here.** The 5-site demo is glue-only. The
  engine gate flags in that file still get **table rows** (via the census scan) so the ledger
  is complete — only the *call sites* are left for a later wave.
- **Other Wave-1 files:** W0.1 (skin golden), W0.2 (loud stubs), W0.4 (bone live-pose), W0.5
  (lineup gate) add new test/harness files and touch skinning/stub paths — none of the 5 chosen
  glue files (`rb3_gamewarm_native.cpp`, `rb3_heap_maint_native.cpp`, `rb3_platform_native.cpp`,
  `rb3_prefetch_native.cpp`) overlaps them. If a collision is discovered at implementation time,
  swap the offending site for another opt-out glue flag from the §3.1 Glue(17) list (e.g.
  `RB3_SFX_OGG_OFF` @ `rb3_xma_sidecar.h:100`, `RB3_XMA_PREFETCH_OFF` @ `:161`) — keeping the
  presence/truthy spread.
- **Semantic-drift landmine (see Key-facts-4):** the #1 way to silently fail this item is to
  resolve presence-mode flags with truthy logic (or vice-versa). S2's `FlagRead` per-flag mode
  + the {unset,"","0","1","x"} parity check is the guard. Treat any behaviour delta as a red.
- **CMake list churn:** adding one line to `MILO_ENGINE_PLATFORM_SOURCES` is the only engine
  build-graph edit; it does not touch the GPU-flavor partitions W1.1 depends on. Low collision.
- **Engine repo vs rb3 repo split:** S2 commits the module to `milo-native-engine` and the
  rewires to `rb3` as SEPARATE commits under their own locks. Do not cross-stage.
- **Web build untouched:** the module is in the always-built platform partition, so `rb3-web`
  would pick it up, but this wave never runs `scripts/web/build.sh` (hard-rule 5). New flags
  read via web-only files (e.g. `main_web.cpp` `RB3_SCREEN_BUNDLES_OFF`) still get ledger rows
  from the scan; their call sites are out of scope.
