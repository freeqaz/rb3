# Wave 17 — Pre-dispatch Review (Fable adversarial pass)

**Reviewer:** Fable subagent (2026-07-07). **Input:** `WAVE17_KICKOFF.md` (draft),
`RETROSPECTIVE/plans/{INDEX.md, PLAN-R1-dolphin-probe.md, PLAN-R2-skinning-fixtures.md,
PLAN-R3-uidump.md}`, `RETROSPECTIVE/{ROADMAP.md, OPTIONS.md §4}`, `HANDS-FIX/STATUS.md`,
`WAVE16_REVIEW.md` (A7 tally); plus direct tree re-verification (not inherited from the plans):
engine @ `51640ff` (== `MILO_ENGINE_PIN`, `native/CMakeLists.txt:74`; verified `rev-parse HEAD`;
one concurrent uncommitted `M src/platform/FxSendNative.cpp` — same artifact Wave 16 noted, leave
untouched): `src/platform/Rnd_Wgpu_RB3.cpp` (`:4736` HANDS_ATTACH block header, `:4872` INSTR_B,
`:5055-5057` shard-guard envs, `:5130` HEADMAT_DBG, `:5247-5289` DrawMesh drawlog capture,
`:5313` DrawLogOn, `:5324` `RecordDrawLog` def, BeginRenderPass `:1935,2246,2298,2348`),
`src/platform/RB3PostProc.cpp:88-93` (menuDepthOp), `NativeCompatFlags.classification.json`
(`:504,:511` DRAWLOG rows, `:1418-1421` ROWFIX default-ON); rb3:
`src/system/bandobj/BandCharacter.cpp:1372-1374` (REPOINT getenv, default-OFF),
`src/system/rndobj/Text.cpp:1721` (`RndText::DrawShowing`), `native/src/rb3_replay_api.cpp:1-64`
(/api/call contract + `RB3_REPLAY_API` gate), `native/src/rb3_http_server.cpp:356-374,395-396`,
`native/tests/test_helpers.cpp:76-78` + `orig-assets/extracted/` (present),
`native/tests/goldens/drawlog/splash_screen.json` (`"count": 792` + fixedclock-residual sidecar),
`scripts/native/keyboard-to-gameplay.py:154,299-308` (`--game-burst`); out-of-repo:
`rb3/Rock Band 3 (USA).wbfs` (4,112,515,072 B), `~/code/milohax/dolphin` (HEAD `aabea5b` +
`bc3b1f5` W7 hook; `build/Binaries/dolphin-emu-nogui` on disk),
`dolphin/Source/Core/Core/Boot/Boot.cpp:478-483` (`SetDefaultDisc`/`MAIN_DEFAULT_ISO`),
`milo-trace/milo_trace/capture/{dolphin.py, wii_symbol_map.py}` (on disk),
`orig/SZBE69_B8/files/band_r_wii.map` (`__vt__8CharBone` 80bfeaa8 `:63301`,
`__vt__13CharServoBone` 80c05d60 `:63938`, `TheTaskMgr` 80cacb98 `:80889`, `TheBandDirector`
80d16c9c `:88891`).

## VERDICT: **dispatch-with-amendments**

No false premise this wave — I went looking for one in the usual place (plan-vs-tree drift and
the M1s) and did not find it. Both declared engine ranges resolve **by symbol, exactly, at
today's HEAD == pin** (R-C: `:4736` is the `RB3_HANDS_ATTACH_PROBE` block header, `:5324` is
`void BandRnd::RecordDrawLog(...)`); every filesystem/tool/map claim gating R1's M1 is live on
disk (R-A); R2's known-bad arm is real — the flag exists in-tree default-OFF and Wave-16's
committed evidence shows the tear (`HANDS-FIX/STATUS.md` "DECISIVE" PNGs); the eleven-defaults
tally reconciles (Wave-15's ten per WAVE16_REVIEW A7 + `RB3_ROWFIX` flipped default-ON at the
Wave-16 E1 flip, classjson `:1421`); all ten OPTIONS §4 lints exist as numbered and none is
dropped by the kickoff. The amendments are workability fixes, the largest being R-B: as
written, poll-for-commit under-specifies WHERE Lane U develops its engine half while it waits —
and the cheapest correct shape is to make R2's engine probe commit an **M1-exit deliverable**
(it is M1's only implementation artifact anyway) while Lane U develops in an isolated engine
worktree from wave start, sequencing only the LANDING. One literal rule conflict needs a carve-
out: "refuted flags UNSET" as carried would forbid R2's `bad-torn` capture arm, which must set
`RB3_HANDS_AUTHORED_REPOINT=1` to produce the known-bad fixture (A3).

---

## Amendments

### A1 (HIGH, Lane S + Lane U / answers R-B, kickoff `:34-38`,`:46-50`) — keep the lanes parallel; sequence only the LANDING, and pull R2's engine commit forward to M1-exit

Poll-for-commit is adequate **for landing** but the kickoff leaves two gaps:

1. **Lane U's M1 IS its engine edit** (PLAN-R3 §4 M1 = sidecar struct + capture +
   `?prov=1` serialization, "NO game-side hooks yet"). If Lane U interprets "rebases after
   Lane S's engine commit exists" as "don't start engine work until then", its M1 — the
   wave's go/no-go — blocks at wave start on another lane's schedule. If instead both lanes
   edit the **shared** engine checkout concurrently, that is exactly R2's risk R-d
   (concurrent uncommitted engine edits — and the shared tree is ALREADY dirty today:
   `M src/platform/FxSendNative.cpp`, untouched-by-rule since Wave 16).
2. Nothing bounds how long Lane U polls. R2's engine block is required only by its M1
   captures, but the plan text ("§3.2 … commit engine first") reads as a lane-end act.

**Concrete shape (adopt into the kickoff):**
- **Lane S:** the `RB3_PALETTE_DUMP` probe block is M1's only code artifact — commit it to
  engine master (flock, stage ONLY `Rnd_Wgpu_RB3.cpp` + its classjson rows; do NOT stage
  `FxSendNative.cpp`) **at M1 capture time, before the M1 GO/NO-GO is even adjudicated**.
  The block is additive + env-gated, so it is landable regardless of the metric verdict, and
  this minimizes Lane U's wait to hours, not a lane.
- **Lane U:** develop the engine sidecar in an **isolated engine worktree + own build dir
  from wave start** (Wave-16 Lane-F precedent; R2 §6 R-d names the same mitigation). M1's
  golden gate runs against the worktree build. Poll for Lane S's probe commit; when it
  lands, rebase the worktree (trivial — the blocks are ~330+ lines apart and additive),
  then land. If Lane S's commit has not appeared by the mid-wave checkpoint, escalate to
  the coordinator (who lands S's block from its checkpoint) rather than continuing to poll.
- Coordinator still does the ONE pin bump at wave end (M-1, unchanged). Note the soft pin:
  mid-wave rb3-native builds against post-pin engine HEAD only warn (`CMakeLists.txt:80-84`)
  — expected, not an error to "fix".

No hard-sequencing of Lane U's whole engine half into a separate stage is needed — that
would serialize the wave for a merge that is structurally trivial.

### A2 (MEDIUM, Lane D, kickoff `:18-23`) — name Lane D's writable surfaces explicitly (lint §4.5: diagnosis lanes get wide grants)

"Nothing in this lane touches rb3/engine source beyond what the plan declares (probe-side
additions only)" under-specifies where the probe LIVES. Per PLAN-R1 §3.2 the Wave-A
deliverables are **out-of-repo**: `tools/wii_bone_probe.py` + `struct_offsets.py` land in
`../milo-trace` (reusing `RspConnection`/`WiiSymbolMap`); the ~20-line `MILO_TRACE_FREEZE`
extension (only if tearing is observed) lands in the `../dolphin` fork; the rb3 repo gets
ONLY `execution/R1-DOLPHIN/{PLAN,STATUS,evidence/}` this wave (`rb3_bone_probe_native.cpp`
and `scripts/analysis/interbone_diff.py` are M3 = Wave B, not this lane's scope). Plus a
Dolphin scratch-user config dir (outside all repos). Write these grants into the lane brief
so the lane neither stalls on permission friction nor "helpfully" relocates the probe into
rb3. Commit discipline in milo-trace/dolphin: own commits, never touch `bc3b1f5`'s tree
state beyond the declared extension.

### A3 (MEDIUM, Lane S vs process rules, kickoff `:46-50`) — carve out the known-bad capture arm from "refuted flags UNSET"

`RB3_HANDS_AUTHORED_REPOINT` is a REFUTED flag (classjson: "not-live REFUTED-BY-VISUAL";
`HANDS-FIX/STATUS.md` disposition "default-OFF, do-NOT-flip"). R2's `bad-torn` fixture arm
**must set it** for one capture run (PLAN-R2 §3.1) — that is the entire point of the
permanent red test. A literal reading of the carried rule "refuted flags UNSET" forbids the
capture. Amend the rule line to: *"refuted flags UNSET in every default/gate-green
configuration; known-bad CAPTURE arms (PLAN-R2 §3.1 `bad-torn`) may set one transiently to
manufacture the fixture, never in a shipped or gate arm."* Verified the red arm is real:
flag read at `BandCharacter.cpp:1372-1374` (getenv, default-OFF), Wave-16 evidence
(`matched_zoom_burst08_burst12.png` — "OFF coherent hand vs ON torn spikes") committed under
`execution/HANDS-FIX/evidence/`.

### A4 (MEDIUM, Lane D / answers R-A wording, kickoff `:54-56`) — the budget is encoded in the plan, but the kickoff's paraphrase subtly misstates it; fix the paraphrase

PLAN-R1 §4 M1 encodes the go/no-go crisply: GO = interactive-scene boot + one sane named
CharBone matrix via **either** route; NO-GO = **neither route A (debug DOL + DefaultISO) nor
route B (retail wbfs boot)** boots to an interactive scene **within the ~1-day effort box**
→ priced report + STOP. The kickoff instead says "one day-equivalent of agent effort
**before retail-DOL fallback**" — which reads as a day burned on route A alone before B may
start. That is the boot-troubleshooting rabbit hole R-A worries about. Reword to match the
plan: *the ~1-day box covers M1 in total; switch A→B as soon as A's failure mode is
identified (likeliest: debug DOL rejects the retail ARK — PLAN-R1 §6.1), and the input
rabbit-hole has its own half-day timebox → Qt-savestate fallback (§6.5).*

### A5 (LOW, R-C bookkeeping, kickoff `:29-31`,`:35-36`) — declared ranges resolve exactly, but record R3's FULL same-TU site list so "disjoint" is checkable at merge time

Verified at engine HEAD == pin: `:4736` = `RB3_HANDS_ATTACH_PROBE` block header (spans with
INSTR_B to ~`:4990`), `:5324` = `RecordDrawLog`. No drift. Two bookkeeping notes:
- R3's engine edit is NOT a single range: besides `:5324` it touches the DrawMesh call site
  (`:5283`, signature extension), `WriteSceneUniforms` (~`:1380`, viewProj CPU copy), four
  `BeginRenderPass` sites (`:1935,2246,2298,2348`, pass-open helper), plus
  `RB3DrawLogDebug.h`, `RB3MaterialBinder.cpp`, `RB3PostProc.cpp:93`. All verified disjoint
  from R2's insertion region — but the kickoff's "declared disjoint ranges" contract should
  list the full site set (copy PLAN-R3 §2c/§3.2's list) so the disjointness claim is a
  checkable predicate, not a vibe.
- R2's block must not split the existing probe blocks that live between the two declared
  ranges: INSTR_B ends ~`:4990`, shard-guard envs at `:5055-5057`, `RB3_HEADMAT_DBG` at
  `:5130`. Insertion point = after the INSTR_B closing scope, inside the skinning path where
  `skinnedView`/`owner` are live — state that in Lane S's brief.

### A6 (LOW, Lane S, fixture-environment check) — no absent-asset dependency; one boot-semantics note

`rb3-tests` boots headless off `RB3_DATA` (default
`/home/free/code/milohax/rb3/orig-assets/extracted` — present, `test_helpers.cpp:76-78`),
but R2's Suites A/B/C need **no boot at all** (committed goldens + math free functions) and
Suite D SKIPs when its env dirs are unset — so CI is asset-independent, correct by design.
The asset-hungry step is CAPTURE, which uses `keyboard-to-gameplay.py --game-burst`
(verified `:154,:299-308`) — the exact protocol Wave 16 already ran successfully in this
environment. Carry one line into Lane S's brief: fixture tests must keep the
farvert/hands-bind pattern of NOT requiring `EnsureEngineInit` (host-libm trig gotcha,
`test_hands_bind_oracle.cpp:80-86`), so a fresh checkout without `orig-assets` still runs
Suites A/B/C green.

### A7 (LOW, Lane D, M3-time detail) — `/api/call` is POST and gated `RB3_REPLAY_API=1`

The native seam R1 §0/§3.6 relies on exists as claimed (`rb3_replay_api.cpp` header contract;
route registered at `rb3_http_server.cpp:396`), but it is enabled by `RB3_REPLAY_API=1`
independently of `RB3_HTTP` (`rb3_replay_api.cpp:57-64`). Not M1-gating (M1 is Wii-side
only); note it in the lane brief so the Wave-B harness sets both envs.

---

## Lane assessments

- **Lane D (R1):** dispatch. Every M1 premise re-verified live this pass: wbfs at repo root
  (4,112,515,072 B), `dolphin-emu-nogui` built from the fork with the W7 hook commit
  (`bc3b1f5` on `aabea5b`), `SetDefaultDisc` inserts `MAIN_DEFAULT_ISO` for DOL boots
  (`Boot.cpp:478-483` — the plan's `:480-482` cite resolves), RSP client + CW-map parser on
  disk in milo-trace, all four map anchors match the plan's addresses. M1 is genuinely the
  cheapest decisive step (boot + discovery are the only item-killing risks; everything else
  is assembly of proven parts). Amendments A2/A4/A7 apply.
- **Lane S (R2):** dispatch. M1 spot-checked: the dump probe is an assembly of parts already
  live within ~150 lines of the insertion point (Tier-1/Tier-2/INSTR_B verified at
  `:4736-4990`), and §2.3's claim that the committed Wave-16 logs are summary-only (hence
  NEW captures are required) is consistent with the STATUS record — the plan does not
  over-claim salvage. Amendments A1/A3/A5/A6 apply.
- **Lane U (R3):** dispatch. M1 golden premise verified: the 792-draw golden + fixed-clock
  residual sidecar exist (`goldens/drawlog/splash_screen.json` `"count": 792`), the endpoint
  and handler are where the plan says, `RndText::DrawShowing` at `Text.cpp:1721`, the W14
  LoadOp mechanism at `RB3PostProc.cpp:88-93`, classjson template rows at `:504/:511` (note:
  `:511` is `RB3_DRAWLOG_DUMP`, a sibling — the classjson lives in the ENGINE repo, so the
  `RB3_DRAWLOG_PROV`/`RB3_MENU_DEPTH_CLEAR` appends ride the engine commit). Amendment A1
  (worktree + landing sequence) is the load-bearing one.

## Answers to the kickoff's open questions

- **R-A (Dolphin go/no-go encoded?):** YES in the plan — PLAN-R1 §4 M1 has an explicit
  GO/NO-GO with a fallback trigger (route A stall → route B retail boot), a priced NO-GO
  deliverable (disc-rebuild cost estimate), and the ~1-day box from ROADMAP. Nothing in M1
  is known-false: all filesystem/tool/map/API premises re-verified live (see appendix). The
  kickoff's own PARAPHRASE of the budget is the one defect — it implies a day on route A
  before B; fix per A4. Secondary timebox (input automation, half-day → savestate fallback)
  is also already in the plan (§6.5).
- **R-B (poll-for-commit vs hard stage?):** poll-for-commit is the right mechanism, but
  only for the LANDING, and only if R2's engine commit is pulled forward to M1-exit and
  Lane U develops in an isolated engine worktree meanwhile — the concrete shape in A1. Do
  NOT hard-sequence Lane U's engine half as a separate post-S stage: its M1 is the engine
  spike, and the merge being sequenced is additive-disjoint (trivial rebase), so full
  serialization buys nothing and costs half a wave.
- **R-C (ranges still resolve? collisions?):** YES — verified by symbol at engine HEAD
  (== pin `51640ff`): `:4736` = HANDS_ATTACH block header, `:5324` = `RecordDrawLog`. No
  collision between the two declared regions (~330 lines apart, R2's block bounded above by
  the shard-guard at `:5055` if inserted immediately after INSTR_B). R3's edit set is wider
  than its declared range (five more sites in the same TU + three other TUs, all verified
  disjoint from R2's region) — record the full list per A5. One live hazard flagged: the
  shared engine tree carries an uncommitted `M src/platform/FxSendNative.cpp` today; both
  engine writers must stage only their own files (and A1's worktree rule removes the
  concurrent-edit exposure entirely).

## Source appendix (verified anchors)

| Claim | Anchor | Verified |
|---|---|---|
| Engine HEAD == pin | `git -C ../milo-native-engine rev-parse HEAD` → `51640ff2bf…`; `native/CMakeLists.txt:74` | ✓ |
| R2 range | `Rnd_Wgpu_RB3.cpp:4736` "RB3_HANDS_ATTACH_PROBE" header; INSTR_B `:4872-~4990` | ✓ exact |
| R3 range | `Rnd_Wgpu_RB3.cpp:5324` `void BandRnd::RecordDrawLog(`; DrawLogOn `:5313`; capture call `:5282-5283` | ✓ exact |
| Between-ranges blocks | shard-guard envs `:5055-5057`; `RB3_HEADMAT_DBG` `:5130` | ✓ |
| R3 other engine sites | BeginRenderPass `:1935,2246,2298,2348`; `RB3PostProc.cpp:88-93` menuDepthOp | ✓ |
| Engine tree dirty | `git status` → `M src/platform/FxSendNative.cpp` (pre-existing, Wave-16-noted) | ✓ |
| wbfs | `rb3/Rock Band 3 (USA).wbfs`, 4,112,515,072 B | ✓ |
| Dolphin | `~/code/milohax/dolphin` HEAD `aabea5b` + `bc3b1f5` W7 hook; `dolphin-emu-nogui` 23.6 MB | ✓ |
| DefaultISO insertion | `Boot.cpp:478-483` `SetDefaultDisc` reads `MAIN_DEFAULT_ISO` | ✓ |
| milo-trace client | `milo_trace/capture/{dolphin.py, wii_symbol_map.py}` on disk | ✓ |
| Map anchors | `__vt__8CharBone` 80bfeaa8; `__vt__13CharServoBone` 80c05d60; `TheTaskMgr` 80cacb98; `TheBandDirector` 80d16c9c | ✓ all four |
| Debug DOL | `orig/SZBE69_B8/sys/main.dol` 13,068,128 B | ✓ |
| /api/call | `rb3_http_server.cpp:396` POST; gate `RB3_REPLAY_API` `rb3_replay_api.cpp:57-64`; `rb3rc_capture_sweep` `rb3_replay_capture.cpp:357` | ✓ |
| REPOINT flag | `BandCharacter.cpp:1372-1374` getenv, default-OFF; classjson `b06720d` "REFUTED-BY-VISUAL" | ✓ |
| Wave-16 tear | `HANDS-FIX/STATUS.md` evidence list ("DECISIVE" matched-zoom PNGs); disposition default-OFF do-NOT-flip | ✓ |
| 792 golden | `goldens/drawlog/splash_screen.json` `"count": 792` + fixedclock-residual sidecar | ✓ |
| rb3-tests boot | `test_helpers.cpp:76-78` `RB3_DATA` → `orig-assets/extracted` (present) | ✓ |
| Capture protocol | `keyboard-to-gameplay.py:154` `--game-burst`, burst loop `:299-308` | ✓ |
| Text choke point | `Text.cpp:1721` `RndText::DrawShowing` | ✓ |
| classjson rows | engine `NativeCompatFlags.classification.json:504` RB3_DRAWLOG, `:511` RB3_DRAWLOG_DUMP, `:1418-1421` ROWFIX default-ON | ✓ |
| Eleven defaults | WAVE16_REVIEW A7 "ten verified" (nine W15 + PLAYER_NAME_FALLBACK) + ROWFIX W16 E1 flip = eleven | ✓ reconciled |
| Ten lints | OPTIONS.md `:218-252` — ten numbered items, none dropped by the kickoff | ✓ |
