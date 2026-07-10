# WAVE 29 — PRE-DISPATCH REVIEW (Fable, adversarial)

**Under review:** `WAVE29_KICKOFF.md` at HEAD `6c803adb` (kickoff + coordinator
pre-work commit), against `WAVE28_CLOSEOUT_REVIEW.md` (E1-E6 + Q8),
`W28-CROWD-OWNER/RECHARTER.md`, both W28 STATUS files, `W28-PROP-FIX/PLAN.md`,
and the code the charter makes factual claims about (CharDriver.cpp,
rb3_game_input.cpp, boot-to-song.py, analyze_prop_ab.py, NativeCompatFlags census).

## VERDICT: **DISPATCH-WITH-AMENDMENTS** (A1-A3 blocking; A4-A10 advisory)

The charter faithfully carries the Q8 menu (kickoff §menu matches README
§Wave 29 menu verbatim), quotes the RECHARTER acceptance target set verbatim
(kickoff:100-109 == RECHARTER.md:60-69), carries the E2 hypothesis label
(kickoff:111-114), rules E-C2 (Part C), and keeps the process rails. Three
findings must be amended before dispatch — two are exactly the W28-A2/E4
overclaim classes this review exists to catch.

---

## A1 — BLOCKING (W28-A2 class): `--part vocals` does not exist; Part B as chartered cannot run

The kickoff asserts the harness supports `--part {guitar,bass,drums,vocals,keys}`
(kickoff:23-24) and Part B mandates `boot-to-song.py --part vocals` (kickoff:142-143).
The engine rejects every part but guitar, on BOTH verb paths:

- HTTP path: `native/src/rb3_game_input.cpp:1080-1085` — `if (sym != "guitar") {
  *err = "unsupported part '" + sym + "' (only 'guitar')"; return false; }`
- Scripted path: `rb3_game_input.cpp:433-439` — same guard ("Only `guitar` is
  supported today"), and `PadVerbEnqueuePrimary` (:748-757) presses only Confirm on
  the default focus — there is no part-scroll machinery at all.
- `boot-to-song.py:151` ignores `k.verb()`'s return (`keyboard-to-gameplay.py:84-85`
  returns the HTTP status; nothing checks it), so a vocals run would silently strand
  on `part_difficulty_screen` and FAIL at the 150s gameplay deadline. The
  coordinator's verify run (kickoff:26-28) used the DEFAULT part=guitar — the vocals
  path was never exercised. The `--part` choices list (`boot-to-song.py:44,57`) is an
  overclaim in the script too.

**The good news:** Part B's evidence does not need a vocals-part run. The vocalist
is on stage in every run; W28's flag-ON guitar/expert runs already produced the
`mic.ikhand`/`mic_stand.ikhand` probe rows (WAVE28_CLOSEOUT_REVIEW.md Q6, E5/E6),
and `band-closeup-capture.py --member vocals` is supported
(band-closeup-capture.py:223-225).

**Amendment (adopt one; (a) recommended):**
> (a) **Part B re-chartered to supported tooling:** the E6 mic-chain A/B runs
> `boot-to-song.py --song beastandtheharlot --part guitar --fixed-clock` (the
> vocalist is on stage regardless of played part; W28 ON runs prove the mic rows
> fire), with vocalist closeups via `band-closeup-capture.py --member vocals`.
> Part D's "guitar + vocals runs" becomes "guitar + one additional SUPPORTED part
> run (bass or drums, if `part:` supports it — else two guitar runs in different
> venues) + `--member vocals` closeups". Kickoff:23-24 and boot-to-song.py's
> `--part` help text are corrected to state only `guitar` is engine-supported today.
> `part:` verb extension is OUT of Wave-29 scope (a Wave-30 tooling item).
>
> (b) *(only if the coordinator does it PRE-dispatch, verified end-to-end like the
> guitar run)* extend `kVerbPart` in rb3_game_input.cpp with view/highlight-aware
> scrolling (NOT a blind DDown count — choose_part list contents depend on song
> parts + controller breed), re-verify `--part vocals` reaches gameplay, and only
> then keep Part B's wording. rb3_game_input.cpp remains outside both lanes' write
> sets either way.

Note for (a): bass/drums are ALSO unsupported today — the sweep text must not
assume any non-guitar part works without a verified pre-work change.

## A2 — BLOCKING (E4 class): Part A's acceptance bar misquotes the committed analyzer

Kickoff:136-138 states the bar as "`skip=0` AND `0` dst>30u entries for **ALL
ikhands**". The committed analyzer checks exactly THREE ikhands:
`analyze_prop_ab.py:21` — `FOCUS = ("strum.ikhand", "fret.ikhand",
"right_hand.ikhand")` — and its verdict loop (:77-86) iterates FOCUS only, printing
`ACCEPTANCE ({label}): PASS|FAIL` (:86) with exit 0/1. "ALL ikhands" is neither
A8(ii) (WAVE28_KICKOFF.md:221-226: "strum/fret/right_hand **skip=0** and
`dst_from_hand` **0 entries >30u**") nor what the tool measures — mic/mic_stand/feet
rows exist in the logs and would make a literal "ALL ikhands" bar unachievable and
judgment-laden. This is the exact E4 lesson (W28-PROP-FIX/STATUS.md:90-100): never
paraphrase an acceptance bar the tool decides.

**Amendment (adopt verbatim):**
> **Part A acceptance bar (replaces "for ALL ikhands"):** A8(ii) verbatim carry —
> same harness/song/window as W27 (`W26-PROP/run_prop_probe.py`, beastandtheharlot
> guitar/expert ~18s), reach clamp default-ON, flag-ON: strum/fret/right_hand
> **skip=0** and `dst_from_hand` **0 entries >30u**. The mechanical decider is the
> committed `analyze_prop_ab.py` printing `ACCEPTANCE (ON): PASS` (exit 0) on the
> flag-ON log. Anything else is labeled **PARTIAL** with the numbers (E4/E5).

## A3 — BLOCKING: Part C's deletion list is incomplete, and the deletion abuts Lane 1's live probe

Kickoff:148-150 enumerates "getenv helper, CrowdKeepState snapshot in Play, the
Poll re-arm block". Grep of HEAD shows FIVE rb3-side sites; the kickoff list misses
two, one of which would not compile if left behind:

| # | site | lines (src/system/char/CharDriver.cpp) | in kickoff? |
|---|---|---|---|
| 1 | W25 comment block + `struct CrowdKeepState` + `gCrowdKeep()` + `gCrowdClipKeepEnabled()` | 42-84 | partially ("getenv helper") |
| 2 | **dtor E-C3 prune block** (`gCrowdKeep().find/erase(this)`) | 107-115 | **MISSING** (dangling refs → compile error if #1 removed without it) |
| 3 | Play() snapshot (`gCrowdClipKeepEnabled() && strncmp(...,"crowd",5)`) + its comment | 421-437 | yes |
| 4 | Poll() re-arm block + its 50-line comment | 691-741 | yes |
| 5 | census row: engine-side `NativeCompatFlags.classification.json:1690` + generated `gen.inc:141` | — | correctly COORDINATOR-ONLY (kickoff:152-153) |

Cross-lane hazard: site 3 (:421-437) sits immediately above the `CHARDRV_PLAY` /
`CHARDRV_PLAY_BT` probe block (:438-466) that Lane 1's STEP-0(i) depends on. An
imprecise deletion could break the probe mid-wave while Lane 1 is booting against it.

**Amendment (adopt verbatim):**
> **Part C site list is BINDING:** delete CharDriver.cpp lines-equivalent sites
> 1-4 above (comment blocks included; the W26 "root-cause correction" comment at
> :52-67 is superseded by the W28 fifth narrative and goes too). The
> `CHARDRV_PLAY`/`CHARDRV_PLAY_BT`/`CHARDRV_*` probe blocks are byte-identical
> before/after. Post-edit mechanical gates, all three in STATUS:
> `grep -c "RB3_CROWD_CLIP_KEEP\|gCrowdKeep\|CrowdKeepState" src/system/char/CharDriver.cpp` == 0;
> `grep -c CHARDRV_PLAY_BT src/system/char/CharDriver.cpp` == 1;
> batch_objdiff on `Play__10CharDriverFP8CharClipifff` (100.0%) and
> `Poll__10CharDriverFv` (93.54%) == baseline. Engine-side census row removal +
> regen + the resulting pin bump are coordinator-only at close-out.

## A4 — ADVISORY: ownership matrix gaps + serialization rule

File collisions between the lanes are clean on paper (Lane 1 has zero CharDriver
writes; arbitration pre-ruled). Gaps: (i) `native/src/rb3_game_input.cpp` is owned
by NEITHER lane (relevant only if A1 option (b) is taken — keep it coordinator-only);
(ii) `scripts/native/boot-to-song.py` mid-wave bugfixes have no owner; (iii) both
lanes will ADD scripts under `scripts/native/` — fine, but say "new files with
distinct names only". Temporal hazard: both lanes share one build tree/binary while
Lane 2 edits CharDriver.cpp — inert by construction (default-OFF flag + dead
default-OFF deletion) but Lane 1's evidence should be provenance-stamped. Proposed
rule in the acceptance block below.

## A5 — ADVISORY: Lane 1 flag-naming constraints (census 411 near-misses)

`NativeCompatFlags.gen.inc` has 411 rows. Near-miss families the new trigger-layer
name must avoid: `RB3_NO_CROWD_REBIND` (default-ON, inside the protected
Crowd.cpp oracle — the A5/W28 semantic-collision precedent), `CROWD_REBIND_PROBE`,
`RB3_CROWD_PANEL_DBG`, `RB3_NO_CROWD_INTRO`, `RB3_CROWD_CLIP_KEEP` (being removed),
`RB3_HUB_*` family, and the released `RB3_HUB_CROWD_CLIPBIND`. Proposed constraint:
name matches `RB3_(SCENE|VIGNETTE)_TRIG*` vocabulary (the layer STEP 0 is expected
to name), contains none of `CLIPBIND|REBIND|CLIP_KEEP|CROWD_`, and
`grep -c '"<NAME>"' ../milo-native-engine/src/platform/NativeCompatFlags.gen.inc`
== 0. Chosen ONCE at the STEP-0 checkpoint, no mid-lane renames (A5 precedent). If
STEP 0 names a different layer, the vocabulary follows the named layer — with the
same grep gate.

## A6 — ADVISORY: A/B determinism mechanics unstated for Parts B/D

The harness has `--fixed-clock` (boot-to-song.py:69-70) and the kickoff mentions the
flag exists (:24) but never says WHEN it is required. HTTP verb arrival is
wall-clock even under fixed clock, so shot indexes do not align across runs.
Amendment: all A/B pairs (Part B mic A/B; Part D seed triage) run `--fixed-clock`
and pair screenshots by nearest `songMs` from `health.jsonl`, never by shot index
(the W-glow camera-desync false-positive lesson). Additionally, per E5
(W28-PROP-FIX/STATUS.md:101-109): Lane 2 owns CharIKHand.cpp and SHOULD parameterize
the `[IK_CLAMP]` 300 / `[PROP_DST]` 120 line caps via env (probe-only edit, gated
under the existing dbg envs, no new census name if read through the existing
`RB3_IK_CLAMP_DBG`/`RB3_PROP_DST_DBG` values) so mic-chain OFF/ON rows are
comparable rates, not window shares.

## A7 — ADVISORY: Part D needs a bounded-effort rule

Retail-pairing skepticism for the two coordinator seeds is present and correct
(kickoff:39-46, 158-163). Missing: an effort cap so the sweep cannot eat the lane.
Amendment: ≤3 boot-to-song runs + ≤2 closeup runs total; seed triage FIRST; at most
the top 6 anomalies get full entries (symptom/evidence/retail-pair/suspected
layer/proposed lane) — everything else is a one-line Wave-30 menu candidate. Part D
stops when Parts A-C gates are still unmet (fix work outranks findings-gathering).

## A8 — ADVISORY (verified, no amendment): CHARDRV_PLAY_BT covers STEP-0(i); no bypass

Verified at HEAD: the probe sits inside `Play(CharClip*,...)` (CharDriver.cpp:
457-463) within the existing CHARDRV_PLAY block, gated `CHARDRV_PROBE` name-match ×
`getenv("CHARDRV_BT")` (presence, not `=1` — dispatch prompts should say "set", not
"=1", for precision), `#ifdef HX_NATIVE`. ALL play paths route through this
overload: `Play(const DataNode&)` :472-479 → :476, `PlayGroup` :482-493 → :492, the
Poll starved-replay paths :677-689, and the default-clip play :256. The only
CharClipDriver construction NOT via Play is the copy path (:292), which emits no
CHARDRV_PLAY either — and W28's raw log shows the beat-0 cityscape plays DID emit
CHARDRV_PLAY (7 lines, raw 457-463), so the working reference traverses exactly the
probed site. Wii gate already re-verified by the coordinator (kickoff:36-38).
The "static milo listing is top-level-only" constraint for STEP-0(iii) is still
operative: `scripts/milo/mip_strip.py:399` `parse_dir_entries` is the W28-A4-pinned
top-level-only parser; runtime `/api/dta/eval` dump remains the pinned method.

## A9 — ADVISORY: checkpoint paths for Lane 2 unnamed

Rule 1 (kickoff:174-176) names Lane 1's `/tmp/wave29-checkpoints/crowd-step0-{i,ii,iii}.json`
but Lane 2's discriminator results (Part B verdict, Part A pre-fix baseline) have no
named paths. Amendment: `/tmp/wave29-checkpoints/gameplay-{prop3-baseline,micAB,sweep}.json`,
check-first semantics per rule 1; A7 raw-log mechanics apply to every
`engine.log` used as evidence (gzip into `W29-GAMEPLAY/evidence/raw/`).

## A10 — ADVISORY: base-SHA precision in dispatch prompts

Kickoff:3 says "Base: rb3 `ab1e5461`" (the close-out commit), but the pre-work the
lanes depend on (boot-to-song.py, CHARDRV_PLAY_BT) exists only at `6c803adb`.
Dispatch prompts must pin base = `6c803adb` (or later) so no lane starts from a tree
without the probe/harness.

---

## PROPOSED COORDINATOR ACCEPTANCE BLOCK (append to WAVE29_KICKOFF.md; WINS over draft text)

1. **CA1 (adopts A1(a)):** Part B = mic-chain A/B via `--part guitar` runs (vocalist
   on stage regardless; W28 ON-run mic rows are the precedent) + `band-closeup-capture.py
   --member vocals` closeups. Part D = guitar runs + `--member vocals`/`--member all`
   closeups; NO non-guitar `part:` runs are chartered (engine supports only `guitar`,
   rb3_game_input.cpp:1080-1085). boot-to-song.py `--part` help corrected (Lane 2,
   script-only). `part:` verb extension = Wave-30 tooling candidate, not this wave.
2. **CA2 (adopts A2):** Part A bar, verbatim A8(ii): flag-ON "strum/fret/right_hand
   **skip=0** and `dst_from_hand` **0 entries >30u**" — decided mechanically by the
   committed `analyze_prop_ab.py` printing `ACCEPTANCE (ON): PASS` (exit 0). Any
   other outcome is labeled **PARTIAL** with the numbers.
3. **CA3 (adopts A3):** Part C deletion = the five-site table in WAVE29_REVIEW.md A3
   (incl. the dtor prune block :107-115 and struct/map decl :42-84). Gates:
   `grep -c "RB3_CROWD_CLIP_KEEP\|gCrowdKeep\|CrowdKeepState"` == 0 in CharDriver.cpp;
   `grep -c CHARDRV_PLAY_BT` == 1; batch_objdiff `Play`/`Poll` == baseline
   (100.0 / 93.54). Census row + regen + pin bump coordinator-only at close-out.
4. **CA4 — Lane 1 acceptance (verbatim, RECHARTER.md §W29 ACCEPTANCE TARGET SET):**
   > 1. **Target drivers:** the 8 `char/crowd/crowd_{male,female}0N` `main.drv` CharDrivers,
   >    while `main_hub_screen` is active and their `mClips` resolves to
   >    `streetslomo_clips.milo` (assert the PathName, not just the count).
   > 2. **Animating criterion:** each target driver has `CHARDRV_PLAY` of a `playerN_{f,m}`
   >    clip AFTER beat 2.433 and `FirstPlaying() != NULL` (`animating > 0`) sustained on
   >    main_hub — NOT the transient cityscape `crowd1-5` plays at beat 0.
   > 3. **Do NOT** count the splash/cityscape crowd (crowd1-5, sv8) as main_hub walkers; it
   >    correctly animates during splash and correctly dies at the transition. Any census that
   >    measures `animating` at/around beat 2.433 without pinning `mClips==streetslomo_clips`
   >    is measuring the wrong crowd (the W23 ambiguity that caused this supersession chain).

   E2 carry: `playerN_{f,m}` remains a HYPOTHESIS until STEP-0(i) confirms the
   mechanism; if the proven clip names differ, the target set updates WITH EVIDENCE
   in STATUS. Lever-B-style honest re-charter at the STEP-0-named layer remains a
   valid full success (W28 precedent).
5. **CA5 (adopts A5):** Lane 1 flag name: matches the STEP-0-named layer's
   vocabulary (expected `RB3_(SCENE|VIGNETTE)_TRIG*`), contains none of
   `CLIPBIND|REBIND|CLIP_KEEP|CROWD_`, grep-0 against the 411-row
   `NativeCompatFlags.gen.inc`, chosen ONCE at the STEP-0 checkpoint.
6. **CA6 (adopts A6):** all A/B evidence runs use `--fixed-clock`; screenshot pairs
   matched by nearest `songMs` (health.jsonl), never shot index. Lane 2 MAY
   parameterize the E5 probe caps (CharIKHand.cpp, probe-only, no new getenv name).
7. **CA7 (adopts A7):** Part D bounded: ≤3 boot runs + ≤2 closeup runs; seeds first;
   ≤6 full findings entries; remainder = one-line Wave-30 candidates; NO fix code.
8. **CA8 — file-ownership matrix (binding):**

   | surface | Lane 1 (CROWD-TRIGGER) | Lane 2 (GAMEPLAY) |
   |---|---|---|
   | `src/system/world/`, vignette/scene load, PanelDir/WorldDir eventanm/trigger | WRITE | — |
   | `src/system/ui/` (only if STEP 0 names it; prewarm gate) | WRITE | — |
   | `src/system/char/CharDriver.cpp`, `CharClip*.cpp` | READ-ONLY (probes exist) | WRITE (Parts A/C) |
   | `src/system/char/CharIKHand.cpp` | READ-ONLY | WRITE |
   | `scripts/native/boot-to-song.py` (+ its bugfixes) | READ-ONLY (may run) | WRITE |
   | `scripts/native/` NEW files (distinct names) | ADD | ADD |
   | `native/src/rb3_game_input.cpp` | — (coordinator-only) | — (coordinator-only) |
   | protected: `Crowd.cpp:884-1000` oracle, RndMesh loader, sidecar/goldens, census/classjson, `native/CMakeLists.txt` pin, `rb3_session_trace.cpp`, engine `FxSendNative.cpp` | NO TOUCH | NO TOUCH |

   **Serialization rule:** Lane 2's CharDriver.cpp commits (Parts A/C) must leave
   every `CHARDRV_*` probe block byte-identical (CA3 grep gate) and may land at any
   time — they are behavior-inert flag-OFF. Lane 1 records `git rev-parse HEAD` of
   its build tree inside each STEP-0 checkpoint JSON so evidence↔binary provenance
   survives concurrent Lane 2 lands. Both lanes: git ops under
   `flock /tmp/rb3-git.lock`, stage BY PATH only.
9. **CA9 (adopts A9/A10):** Lane 2 checkpoints at
   `/tmp/wave29-checkpoints/gameplay-{prop3-baseline,micAB,sweep}.json` (check-first).
   Dispatch prompts pin base `6c803adb`+ and QUOTE this block's bars verbatim (E4
   rule) — including CA2's analyzer line and CA4's three-item target set.

---

*Every line-number claim above was read at HEAD `6c803adb` during this review.
Reviewer: Fable pre-dispatch (Wave-29). No kickoff edits made; no code written.*
