# W0.6b — Classify the game-root `Unknown` NativeCompat flags

**Wave 5, Lane B (parallel to Lane A).** Planner: Opus. Engine pin `609efb7` (do NOT bump).
Parent: `../WAVE5_KICKOFF.md` (COORDINATOR ACCEPTANCE §D1/D2 binding) + `../WAVE5_REVIEW.md`
("W0.6b — the brief's premise is stale…", authoritative) + `../README.md` (hard rules 1–8).

## Objective

The census scanner already covers `rb3/src/system` (landed `a537c2a3`); `native_compat_census.py
check` already exits 0 at 318 flags. That part is DONE — **do not touch the scanner**. The real,
remaining gap: **91 game-root flags** are present in the registry only as `FlagClass::Unknown /
"unclassified"` rows in `NativeCompatFlags.gen.inc` because they have **no entry** in the curated
sidecar `NativeCompatFlags.classification.json`. This item authors a classification row
(`probe|workaround|feature|perf`) for **every one of the 91**, so a subsequent `gen` produces zero
game-root `Unknown` rows.

**Definitions (from the sidecar `_schema` + review):**
- **probe** — pure debug/diagnostic dump or archaeological investigation knob. No shipped behavior;
  default-OFF; `faithfulStatus: "n/a: <what it probes>"`. Names ending `_DBG`/`_PROBE`, plus dump
  knobs, are almost always this — but confirm the call site only prints/collects, never alters draw.
- **workaround** — a shipped native hack standing in for a real fix (usually default-ON with an
  `_OFF`/`_NO_` opt-out; ON means the workaround is active). `faithfulStatus: "not-live: <hack>
  default-ON"` (or default-OFF for opt-in hacks). This is the bulk of the `RB3_NO_*` / `*_FIX_OFF`
  family.
- **feature** — a real, intended runtime toggle for a port feature (e.g. `MILO_HEADLESS`), not a
  stand-in for a Wii-faithful path.
- **perf** — a performance knob (a value threshold or an optimization on/off) whose only effect is
  speed/memory, not visual/logic fidelity (e.g. `RB3_LOADER_*_MS` budgets, frustum cull).

**Verify at the call site — do NOT classify from the name alone.** The name is a strong prior; the
call site is ground truth. Cross-reference `~/.claude/.../MEMORY.md` topic files where a flag has a
documented history (e.g. `RB3_SKIN_RTT` → C8-faces memory, `RB3_WALKON_SNAP_OFF` → walk-on memory,
`RB3_HANDS_BIND_FIX`/`RB3_SKEL_REBIND_FULL` → W2.2/W0.1 STATUS files).

## Verified ground facts (re-grepped 2026-07-06 — cited lines fresh)

- **The 91 flags + their exact call sites** are captured in
  `execution/W0.6b/census-snapshot.json` (a `native_compat_census.py scan` snapshot; each flag's
  `files` field lists `path:line`). The authoritative source is a live re-scan (below).
- **All 91 are currently absent from the sidecar.** Verified by joining the scan against
  `NativeCompatFlags.classification.json` (89 existing classified entries, none of them game-root).
- **Sidecar format** (`milo-native-engine/src/platform/NativeCompatFlags.classification.json`):
  a single JSON object; rows are `"FLAG": { "class": "...", "owner": "...", "faithfulStatus": "...",
  "default": "on|off", "read": "presence|truthy|value" (optional) }`. New rows are inserted **before
  the closing `}`**, appended after the last existing row (add a trailing comma to the prior last
  row). `class`/`owner`/`faithfulStatus` are required; `default` overrides the scanner's name-based
  guess; `read` overrides the scanner's context guess (set it when the guess is `unknown`).
- **`gen` is NOT run by this lane.** Per D2 (WAVE5_REVIEW §"Collision"), three Wave-5 lanes
  (this + the flip + W3.1a) all append to `classification.json`; running `gen` mid-wave would clobber
  a sibling's uncommitted rows in the generated `gen.inc`. **The coordinator runs ONE `gen` at wave
  end.** This lane commits `classification.json` rows only; it verifies by running `gen` to a
  **temp** output path (never the committed `gen.inc`/ledger).
- **Spot-checked call sites (confirm the priors):**
  - `world/Dir.cpp:151` `RB3_TV3_PLAY_OFF` — `sOff = getenv(...) ? 1 : 0; if (!sOff) b = true;` →
    opt-out **workaround**, default-ON (TV3 dir plays unless opted out).
  - `rndobj/Draw.cpp:70` `RB3_MENU_VOID_FIX_OFF` — opt-out **workaround** default-ON;
    `Draw.cpp:69` `MENU_VOID_SKIP` + `:68/:124` `MENU_VOID_DBG2` are **probes** (debug skip-list /
    sky-keyed dump).
  - `rndobj/Draw.cpp:101` `RB3_REFRACTION_FIX_OFF` — opt-out **workaround** default-ON
    (`bottom_square_refraction` cull fix).
  - `synth/MetaMusic.cpp:381` `RB3_METAMUSIC_SYNC` — truthy opt-in (`e[0] && e[0]!='0'`),
    default-OFF behavior toggle → **workaround** (meta-music sync stand-in), default-OFF.
  - `rndobj/Draw.cpp:201` `RB3_VENUE_FRUSTUM_CULL` — truthy opt-in, `if(!sOn) return false;` a pure
    culling optimization → **perf**, default-OFF.
- **Scope boundary (accepted, do not widen):** `src/band3` (~21 additional `getenv` files) is NOT a
  census scan root (per W2.6's verifier). It is out of scope for W0.6b; record it as accepted, do
  not add band3 rows.
- **Shared-root flags count as game-root here.** Five of the 91 have `roots` spanning
  engine/glue too (`CAM_DBG`, `CHAR_DBG`, `RB3_PREWARM_DBG`, `MILO_HEADLESS`, `RB3_WEB_OFFMAIN_MIX`).
  They are still `Unknown` and still originate (partly) from the game scan-root, so they are
  classified here — one sidecar row covers all their roots.

## Subtasks

Three parallel classification lanes partition the 91 flags with **zero overlap** (exact lists
below), each writing its rows to a **private staging file** in `execution/W0.6b/` (no shared-file
contention). A fourth lane merges the three staging files into `classification.json` in **one**
`flock`'d append and verifies. Every implementer must first re-run
`python3 scripts/analysis/native_compat_census.py scan --json /tmp/w06b-<id>.json` and confirm its
assigned flags are still present + still `Unknown` (resume-safe; skip already-authored rows).

### W0.6b.S1 — char/skinning/bandobj behavior flags (23) — model: opus
**Goal:** classify the judgment-heavy skinning/rebind/deform/char-behavior flags where
workaround-vs-feature is a real call, cross-referencing the W2.2/W0.1/char-skinning memory.
**Files touched (write):** `execution/W0.6b/classified-S1.json` (staging; a JSON object of rows).
**READ-ONLY** on all `src/`. Do NOT edit `classification.json` directly (S4 merges).
**Exact flag set (23):** `RB3_BOUND_REBAKE`, `RB3_HANDS_BIND_FIX`, `RB3_INST_STRINGS_MODE`,
`RB3_MESH_FREE`, `RB3_NO_CLIP`, `RB3_NO_CROWD_REBIND`, `RB3_NO_DEFORM`, `RB3_NO_DEFORM_LOAD`,
`RB3_NO_FACE`, `RB3_NO_HEAD_REBIND`, `RB3_NO_HEAD_SHAPER`, `RB3_NO_IK`, `RB3_NO_INST_REBIND`,
`RB3_NO_POSEMESHES`, `RB3_NO_SKEL_REBIND`, `RB3_SKEL_REBIND_CALCOFF`, `RB3_SKEL_REBIND_FULL`,
`RB3_SKIN_FIX_OFF`, `RB3_SKIN_NOCACHE`, `RB3_SKIN_RTT`, `RB3_SKIN_TIMING`, `RB3_WALKON_SNAP_OFF`,
`SET_SKEL_REBIND`.
**Guidance (verify each at its call site):** the `RB3_NO_*` and `*_FIX_OFF` rebind/deform/IK/face
family are opt-out **workaround**s default-ON (ON = hack active). `RB3_HANDS_BIND_FIX` =
**workaround**, default-OFF experimental bind fix (W2.2: measured no benefit, NOT flipped).
`RB3_SKEL_REBIND_FULL` = **workaround**, the known-broken full-body rebind used as W0.1's fail-red
control (default-OFF; note it is broken in `faithfulStatus`). `RB3_SKIN_RTT` = **feature**
(default-OFF broken-path bypass gated behind the flag, per C8-faces memory). `RB3_SKIN_NOCACHE`/
`RB3_SKIN_TIMING` = **perf**/**probe** — read the site (a cache disable = perf; a timing print =
probe). `RB3_WALKON_SNAP_OFF` = opt-out **workaround** default-ON. `SET_SKEL_REBIND` /
`RB3_SKEL_REBIND_CALCOFF` — read `BandCharacter.cpp:1788/1110`: if it selects a rebind behavior it's
a **workaround**; if it only gates a probe print it's a **probe**. `RB3_INST_STRINGS_MODE` (value
read) — likely **feature** or **workaround**; judge by whether it selects a shipped behavior.
**Steps:** for each flag, open `src/system/<file>:<line>` from the scan; determine
class + default + read-mode from the code; write a sidecar row. Add `"see <memory-topic>"` to
`faithfulStatus` where a memory file documents it.
**Verify:** `python3 -c "import json;json.load(open('execution/W0.6b/classified-S1.json'))"` parses;
row count == 23; every key is in the S1 set; every `class` ∈ {probe,workaround,feature,perf}.

### W0.6b.S2 — ui/render/audio/loader/synth/world behavior + perf/value knobs (24) — model: opus
**Goal:** classify the non-char behavior flags (opt-out fixes, perf/value knobs, the one real
feature toggle).
**Files touched (write):** `execution/W0.6b/classified-S2.json` (staging). READ-ONLY on `src/`.
**Exact flag set (24):** `MILO_HEADLESS`, `RB3_APPLY_HANDLER_FIX_OFF`, `RB3_BILLBOARD_OFF`,
`RB3_CAM_FALLBACK_OFF`, `RB3_LOADER_BUDGET_MS`, `RB3_LOADER_MIN_YIELD_MS`, `RB3_LOADER_READAHEAD`,
`RB3_LOADER_YIELD_MS`, `RB3_MENU_VOID_FIX_OFF`, `RB3_METAMUSIC_SYNC`, `RB3_NO_CROWD_INTRO`,
`RB3_PREWARM_NEXT`, `RB3_PREWARM_SCREENS`, `RB3_REFRACTION_FIX_OFF`, `RB3_RESYNC_YIELD_OFF`,
`RB3_REVIEW_LIGHTER_FIX_OFF`, `RB3_SCROLLBAR_FIX_OFF`, `RB3_STREAM_BUF_SECS`,
`RB3_STREAM_PREPLAY_CAP_OFF`, `RB3_TV3_PLAY_OFF`, `RB3_VENUE_FRUSTUM_CULL`, `RB3_VENUE_SYNC`,
`RB3_WEB_OFFMAIN_MIX`, `VENUE_CAM_LOCK`.
**Guidance (verify each):** `MILO_HEADLESS` = **feature** (headless runtime mode; shared root).
`*_FIX_OFF` (`RB3_APPLY_HANDLER_FIX_OFF`, `RB3_MENU_VOID_FIX_OFF`, `RB3_REFRACTION_FIX_OFF`,
`RB3_REVIEW_LIGHTER_FIX_OFF`, `RB3_SCROLLBAR_FIX_OFF`) = opt-out **workaround**s default-ON
(spot-checked TV3/menu/refraction confirm the pattern). `RB3_TV3_PLAY_OFF`, `RB3_RESYNC_YIELD_OFF`,
`RB3_STREAM_PREPLAY_CAP_OFF`, `RB3_BILLBOARD_OFF`, `RB3_CAM_FALLBACK_OFF` = opt-out **workaround**s
default-ON. `RB3_LOADER_BUDGET_MS`/`_MIN_YIELD_MS`/`_YIELD_MS`/`_READAHEAD`, `RB3_STREAM_BUF_SECS` =
**perf** value knobs (numeric budgets; see incremental-load-perf memory). `RB3_VENUE_FRUSTUM_CULL` =
**perf** (opt-in cull, default-OFF, confirmed at `Draw.cpp:201`). `RB3_PREWARM_NEXT`/
`RB3_PREWARM_SCREENS` = **perf** (GPU/asset prewarm; see load-perf memory) unless the site shows a
correctness dependency → then **workaround**. `RB3_METAMUSIC_SYNC` = **workaround** default-OFF opt-in
(confirmed `MetaMusic.cpp:381`). `RB3_NO_CROWD_INTRO`, `RB3_VENUE_SYNC`, `VENUE_CAM_LOCK` = read the
site: an opt-out behavior hack = **workaround**; a debug camera lock print = **probe**.
`RB3_WEB_OFFMAIN_MIX` = read `AudioDevice_Web.cpp:673` + `StreamReceiver.cpp:46`: a web off-main
audio-mix mode → **feature** or **workaround** (shared root).
**Verify:** parses; row count == 24; keys ⊆ S2 set; classes valid.

### W0.6b.S3 — probe / debug-only flags (44) — model: sonnet
**Goal:** classify the pure diagnostic flags (mechanical: nearly all `class: "probe"`,
`default: "off"`, `faithfulStatus: "n/a: <what it probes>"`). Still confirm each site only
prints/collects and does not alter shipped behavior — if a site turns out to gate real behavior,
reclassify it as workaround and note the deviation in the staging file's `_note`.
**Files touched (write):** `execution/W0.6b/classified-S3.json` (staging). READ-ONLY on `src/`.
**Exact flag set (44):** `BAND_ANIM_BONE`, `BAND_ANIM_PROBE`, `BONE_CLEAR_DBG`, `BONE_LOAD_DBG`,
`CAMDIR_DBG`, `CAM_DBG`, `CBM_DBG`, `CBM_DBG2`, `CBS_DBG`, `CHARDRV_PROBE`, `CHAR_DBG`, `CLOCK_DBG`,
`CROWD_REBIND_PROBE`, `GAME_DBG`, `GEM_DBG`, `HEAD_REBIND_PROBE`, `IK_TGT_DBG`, `INST_REBIND_PROBE`,
`K9_APPLY_DBG`, `MENU_VOID_DBG2`, `MENU_VOID_SKIP`, `MESH_BONE_DBG`, `MILO_LOCALE_DBG`,
`MILO_SETTOKEN_DBG`, `PART_INIT_DBG`, `PART_MOVE_DBG`, `RB3_DUMP_STEMS`, `RB3_HAIR_DBG`,
`RB3_METAMUSIC_DBG`, `RB3_NOTIFY_ALL`, `RB3_PLACEMENT_PROBE`, `RB3_PP_PROBE`, `RB3_PREWARM_DBG`,
`RB3_READAHEAD_DEBUG`, `RB3_SKINFIX_DBG`, `RB3_STATS_DBG`, `RELOAD_PROBE`, `SERVO_PROBE`,
`SKEL_REBIND_PROBE`, `SKEL_REBIND_SKINPOS`, `STRIDE_PROBE`, `UISCREEN_DBG`, `VENUE_DBG`,
`VOIDCUT_DBG`.
**Guidance:** owner tag = the subsystem of the file (skinning / render/crowd / char / ui / synth /
os, matching the existing sidecar's tag style). `RB3_DUMP_STEMS` (synth stem dump), `RB3_NOTIFY_ALL`
(os/Debug notify), `RB3_READAHEAD_DEBUG` (loader debug) are dumps → **probe**. `MENU_VOID_SKIP` is a
debug skip-list → **probe**. `SKEL_REBIND_SKINPOS` / `SKEL_REBIND_PROBE` are skinning investigation
prints → **probe**.
**Verify:** parses; row count == 44; keys ⊆ S3 set; classes valid (expect all/nearly-all `probe`).

### W0.6b.S4 — merge + verify (single flock'd append) — model: sonnet
**Goal:** merge the three staging files into `classification.json` in ONE append, verify a clean
`gen` produces zero game-root `Unknown`, and write the STATUS.md summary. blockedBy S1, S2, S3.
**Files touched (write):** `milo-native-engine/src/platform/NativeCompatFlags.classification.json`
(append rows only, under flock); `execution/W0.6b/STATUS.md` (summary table, under rb3-docs flock).
**Steps:**
1. Under `flock /tmp/milo-engine-classjson.lock`: load S1+S2+S3 staging JSONs; assert the union is
   exactly the 91-flag set with no duplicate keys and no key already present in
   `classification.json`. Insert all 91 rows **before the closing `}`** of `classification.json`
   (add a trailing comma to the current last row; group them under a `// --- W0.6b game-root flags
   ---` blank-line separator matching the file's existing sectioning). Do NOT modify or reorder any
   existing row (append-only, hard rule; the flip + W3.1a lanes append their own rows separately).
2. Validate JSON parses (`python3 -c "import json;json.load(open(...))"`).
3. Verify with a **temp** gen (never the committed paths):
   `python3 scripts/analysis/native_compat_census.py gen --gen-inc-out /tmp/w06b-gen.inc
   --ledger-out /tmp/w06b-ledger.md`, then assert **zero** game-root `FlagClass::Unknown` rows:
   cross-check `/tmp/w06b-gen.inc`'s `Unknown` entries against the game-root flag list from a fresh
   `scan` — the intersection must be empty. (Some engine/glue-only `Unknown` rows may remain; those
   are out of W0.6b scope — report the count, do not classify them.)
4. `python3 scripts/analysis/native_compat_census.py --selftest` → expect `14/14 PASS` (hermetic;
   proves the tool logic still holds — must not regress).
5. `python3 scripts/analysis/native_compat_census.py check` → expect exit 0 (registry still ⊇ every
   getenv; note this compares against the COMMITTED gen.inc, which the coordinator regenerates at
   wave end — a mismatch here is expected until the coordinator's regen and is NOT a failure of this
   lane; record the observed exit + reason in STATUS.md).
6. Under `flock /tmp/milo-engine-git.lock`: stage ONLY `classification.json`
   (`git add src/platform/NativeCompatFlags.classification.json`) and commit
   `W0.6b: classify 91 game-root NativeCompat flags (probe/workaround/feature/perf)`.
   Do NOT `git add -A`; leave sibling `FxSendNative.cpp` and any Lane-A WIP untouched.
7. Under `flock /tmp/rb3-docs.lock`: write the flag→class→one-line-reason summary table to
   `execution/W0.6b/STATUS.md` (91 rows) + the verify results (temp-gen zero-Unknown, selftest,
   check exit + reason).
**Verify:** the four checks above; STATUS.md present with the 91-row table.

## Exit criteria (measurable)

1. **Every one of the 91 game-root flags has a non-`Unknown` classification authored in
   `classification.json`** — verified by: a temp `gen` (step S4.3) produces **zero game-root
   `FlagClass::Unknown` rows** (intersection of temp `gen.inc` `Unknown` set with the fresh-scan
   game-root set == ∅). This is the hard exit; the coordinator's wave-end `gen` (D2) will reproduce
   it against the committed paths.
2. `native_compat_census.py --selftest` → **14/14 PASS** (tool logic un-regressed).
3. `classification.json` parses as valid JSON and contains 89 (pre-existing) + 91 (new) rows, with
   **no existing row modified** (append-only; `git diff` shows only additions after the prior last
   row + the one trailing-comma edit).
4. `execution/W0.6b/STATUS.md` contains the 91-row flag→class→reason table and the verify log.
5. **No `gen.inc` / ledger regen committed by this lane** (D2 — coordinator regens once). Running
   `gen` to committed paths is a HARD-RULE violation for this item.

## Files touched (exact — coordinator cross-diffs)

- `milo-native-engine/src/platform/NativeCompatFlags.classification.json` — **append 91 rows**
  (S4 only, under `flock /tmp/milo-engine-classjson.lock` + `flock /tmp/milo-engine-git.lock`).
- `docs/native/engine-arch-review-2026-07-05/execution/W0.6b/classified-S1.json` — staging (S1).
- `docs/native/engine-arch-review-2026-07-05/execution/W0.6b/classified-S2.json` — staging (S2).
- `docs/native/engine-arch-review-2026-07-05/execution/W0.6b/classified-S3.json` — staging (S3).
- `docs/native/engine-arch-review-2026-07-05/execution/W0.6b/STATUS.md` — summary + verify log (S4).
- `docs/native/engine-arch-review-2026-07-05/execution/W0.6b/census-snapshot.json` — reference
  snapshot (already written by planner).
- **NOT touched:** `NativeCompatFlags.gen.inc`, `NATIVE_COMPAT_LEDGER.md` (coordinator regens);
  `native_compat_census.py` (scanner already extended `a537c2a3`); any `src/` source;
  `Rnd_Wgpu_RB3.cpp`; `src/App.cpp`; `FxSendNative.cpp`.

## Risks / conflicts

- **Multi-lane `classification.json` collision (D2 — the primary risk).** The Lane-A flip
  (`RB3_PLACEMENT_CONTRACT_OFF` + `RB3_PLACEMENT_CONTRACT` default change) and W3.1a (new lighting
  flag) also append to `classification.json`. Mitigation: this lane is **append-only under
  `flock /tmp/milo-engine-classjson.lock`**, never modifies an existing row, and **never runs
  `gen`**. All three lanes' rows coexist; the coordinator runs the single reconciling `gen` at wave
  end. If S4 finds a sibling lane has already appended rows (e.g. the placement flags), **do not
  touch them** — append the W0.6b block after them (hard rule 8).
- **Intra-lane JSON corruption.** Avoided by the staging-file design: S1/S2/S3 write private files;
  only S4 touches `classification.json`, once, under flock.
- **Lane-A sequencing is independent.** Lane A (`W0.3d-fix → W2.1-flip → W3.1a`) is sequential and
  edits engine `Rnd_Wgpu_RB3.cpp` / `WriteSceneUniforms`; W0.6b never edits those. The only shared
  artifact is `classification.json` (handled above). No source-file overlap.
- **`check` exit non-zero is expected mid-wave**, because `check` diffs against the *committed*
  `gen.inc` (which W0.6b deliberately does not regen). Do not "fix" it by regenerating gen.inc —
  record the reason (D2) and leave the regen to the coordinator.
- **Scope creep into `src/band3`.** Not a scan root; out of scope. Record as accepted, do not add
  band3 rows.
- **Mis-classification (probe vs workaround).** A flag that *looks* like a probe but actually gates
  shipped behavior would be wrongly marked `probe`. Mitigation: each subtask must read the call site,
  not the name; S3 explicitly re-checks that each "probe" site only prints/collects.
