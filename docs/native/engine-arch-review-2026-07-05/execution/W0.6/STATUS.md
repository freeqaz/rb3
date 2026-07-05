# W0.6 — STATUS log

Append-only. One `## <subtask-id> — done|partial|blocked` section per subtask, with commit SHAs and blockers. Update under `flock /tmp/rb3-docs.lock`.

## W0.6.S1 — done

**Commits:**
- rb3 `525fdde8` — `scripts/analysis/native_compat_census.py` (scan/gen/check/--selftest tool).
- milo-native-engine `54f9c50` — `src/platform/NativeCompatFlags.classification.json` (curated sidecar).

**What was built:**
- `scan`: regex-walks `getenv("NAME")` (and `std::getenv`) under `milo-native-engine/src` +
  `rb3/native/src` (`.cpp/.h/.mm`), guesses read-mode per flag from a small trailing-lines
  context window (`presence` / `truthy` / `value` / `unknown`, majority-vote across sites),
  emits deterministic JSON. Both named probes from PLAN.md's Key-facts-4 verify correctly:
  `RB3_GAMEWARM_OFF` -> `presence`, `RB3_NO_SFX` -> `truthy` (confirmed against the real trees,
  not just fixtures).
- `gen`: joins scan union sidecar -> C brace-init rows (`NativeCompatFlags.gen.inc` shape)
  + a Markdown burn-down ledger with class/default/owner/faithful-status/sites columns and a
  default-ON-workaround summary line. Sidecar-absent flags get `FlagClass::Unknown` (never
  invented). Verified against the real repos with `--gen-inc-out`/`--ledger-out` scratch
  overrides (not written to the real committed paths — those need `NativeCompatFlags.h/.cpp`
  from S2 first, so I didn't leave generated artifacts sitting uncommitted in the engine tree).
- `check`: re-scans, diffs the scanned flag-name set against the committed `.gen.inc`'s names
  (grep, no compile), and separately checks regen-cleanliness against the sidecar. Verified
  all three states for real: (a) RED against the real (currently absent) target path — expected
  pre-S2, `.gen.inc` doesn't exist until S2 creates the registry module; (b) GREEN against a
  scratch registry matching the current scan+sidecar; (c) fail-red demo — injected
  `getenv("RB3_UNREGISTERED_DEMO_W06")` into a scratch `native/src/_scratch_w06_demo.cpp`,
  `check` reported nonzero + the exact flag name, then the scratch file was deleted (never
  staged/committed) and `check` returned to green. Transcript:
  ```
  check: FAIL — 1 getenv flag(s) not in registry (/tmp/NativeCompatFlags.gen.inc):
    - RB3_UNREGISTERED_DEMO_W06
  check: FAIL — /tmp/NativeCompatFlags.gen.inc is stale (regen would differ). Run `gen`.
  check: FAIL — /tmp/NATIVE_COMPAT_LEDGER.md is stale (regen would differ). Run `gen`.
  exit: 1
  --- re-run after revert ---
  check: OK — 224 scanned flags all present in registry, regen clean.
  exit: 0
  ```
- `--selftest`: 14/14 hermetic checks pass (temp-dir fixtures, no dependency on the real
  trees) — covers presence/truthy/value scan classification, sidecar join (classified vs
  Unknown fallback), gen row/ledger shape, and both the green and fail-red `check` states.

**Sidecar (`NativeCompatFlags.classification.json`):** 82 curated entries — 35 `probe`
(the 20 flags PLAN.md names verbatim + 15 more obvious `*_PROBE`/`*_DBG`-family flags the
scanner turned up, e.g. `SKIN_CLAMP_PROBE`, `SKEL_REBAKE_PROBE`, `RB3_LIGHT_PROBE`,
`RB3_VENUE_PROBE`, `MILO_DEBUG_ARM_CHAIN_DIR/FRAME` — a bounded, naming-unambiguous extension
of PLAN.md's "dozens more … naming families" allowance, not invented) + 47 `workaround`
(30 engine incl. the skinning + hub-layout families, 17 glue — exactly PLAN.md's
Authoritative flag classification list, verbatim). All 82 keys confirmed present in the real
scan (`missing from scan: []`). The remaining 142 of 224 scanned flags are `class=unknown`
by design (NEEDS-CLASSIFICATION rows for later burn-down waves).

**Deviation from PLAN.md (recorded, not silently expanded):** PLAN.md's exit criterion #3 says
"scan finds >=229 distinct flags." MEASURED reality: `140` distinct engine-root flags +
`89` distinct glue-root flags = `229` when summed **per-root without cross-repo dedup**
(this matches lane 06 §3.1's own "Engine: 140" / "Glue: 89" figures exactly). But 5 flag
*names* are used in **both** roots (`MILO_HEADLESS`, `MILO_HEIGHT`, `MILO_WIDTH`,
`RB3_RENDER_DBG`, `RB3_SHARPEN_DBG`), so the **globally-deduped union** — which is what a
single registry needs (one row per name, per the Design section's "one row per flag" /
`NativeCompatFlag` table intent) — is `224`, not `>=229`. `scan` globally dedupes (files/sites
merge across roots for a shared name) since that's what `gen`/S2's table actually needs; the
alternative (229 rows, with 5 names duplicated per-root) would produce two conflicting
registry rows for the same env var, which is the exact hazard PLAN.md's Key-facts-4 warns
about. Total call sites: 296. Everything else in the exit criteria (selftest passes, both
named probes' read-modes correct) holds as specified.

**What remains (not this subtask):** the registry module (`NativeCompatFlags.h/.cpp`), running
`gen` for real onto the committed engine/rb3 target paths, the 5 call-site rewires, and the
committed ledger + gtest coverage guard are S2/S3.

**Blockers:** none.
