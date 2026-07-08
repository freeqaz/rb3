# T1-FRAMETRACE — PLAN REVIEW (Fable, adversarial)

**Target:** `T1-FRAMETRACE/PLAN.md` @ rb3 `a712d5a6` (plan commit; plan states base `fb1f00e5`).
**Reviewed against:** `RETROSPECTIVE/OPTIONS.md` §6.2 + §4 ten lints (rb3 `c4395043`),
`WAVE19_KICKOFF.md` Lane F (`d93fa894`), `WAVE19_REVIEW.md` A2/A3/A5/A6 (binding),
the dispatch validation contract (fail-reds 1–3), the collision matrix, and the CURRENT
tree — **every anchor below re-derived by symbol at HEAD `a712d5a6` (rb3) / `beb89e5` (engine)**.

**VERDICT: APPROVE-WITH-AMENDMENTS** — amendments R1–R6 below are BINDING on the
implementer. No redraft: the plan's anchor fidelity is the best I have audited this
campaign (see §2 — essentially every file:line claim reproduced exactly), the design is
charter-faithful, and the collision statement is consistent with the committed W-ISO plan.
But the plan under-implements one leg of the BINDING validation contract (R1), leaves
gate-2's arm unspecified in a way that can structurally null the gate (R2), asserts a
refusal-block shape the committed red-baseline file will not produce (R3), states one
false mechanism claim (R4 — "pre-Draw"), and carries two non-runnable exit assertions
(R5). All are one-paragraph fixes to milestones/exits, not design changes.

---

## 1. Amendments, ranked (plan claim / evidence / binding correction)

### R1 (HIGH) — the binding fail-red (2) OFF-arm leg is missing; it is also the only arm where `frameAssign` can be asserted

- **Plan claim:** M3 (gate-1) runs **N=6 seam-ON** eng_hot boots and asserts divergence on
  `emitTimeline`+`songClock` only; `frameAssign` is "disclosed, not asserted" (PLAN §5 M3,
  §7 A-1). M4 (gate-2) is a jitter differential with **no arm specified**.
- **Evidence:** The dispatch validation contract (BINDING) reads: *"(2) the three axes must
  go RED on an OFF-arm run under induced contention+jitter and GREEN-or-honestly-graded on
  the seam-ON arm."* The plan implements the seam-ON half (M3, honestly-graded-RED is the
  assertion) but has **no OFF-arm run anywhere**. And the OFF-arm is precisely where
  `frameAssign` has structural signal: seam-ON, `LoadDetSerialize()` (engine
  `ThreadCall_Native.cpp:68`, inline dispatch at :205-:214) runs DataLoader parse jobs
  inline on main, pinning `data:<name>@frame` to the issue frame — the plan's own §1.2/§1.3
  says so. Seam-OFF, the worker path (`ThreadCall_Native.cpp:99`, `LoadDetWorkerJitter()`
  :38 — env-gated on `RB3_LOADDET_JITTER` alone, independent of the seam flags, verified)
  makes the "data" completion frame scheduler-dependent — the H-TIMING mechanism
  `W0.3d-b/STATUS.md:69-77` left open for the *gameplay* window ("the venue/song async
  loads that stream DURING gameplay were not exercised").
- **Binding correction:** Add to M3 (or as M3b) an **OFF-arm sub-run**: env =
  `RB3_FIXED_CLOCK=1` (required — see R4: `gRB3TraceFrame` only advances under
  `gRB3TraceActive || RB3FixedClockActive()`, App.cpp:551/:557-:563; a flagless arm would
  key every marker `frame=0`), `RB3_LOAD_DETERMINISM` **unset**, `RB3_LOADDET_PROBE=1
  RB3_LOADDET_ATTRIB=1 RB3_LOADDET_TIMELINE=1 RB3_LOADDET_JITTER=200`, N≥3 eng_hot boots,
  optionally under CPU contention (a background load) per the contract's
  "contention+jitter". **Assert all THREE axes DIVERGE (RED)** on this arm, with per-axis
  event counts (lint 8). The gate window for `frameAssign` must include the **song-load
  phase** (the boot log contains it; do not clip the window to post-load frames only, or
  loader completions may be 0-count and the assertion vacuous — if in-window completion
  count is 0, lengthen the window, disclosed). This upgrades `frameAssign` from "disclosed
  shrug" to a real per-arm assertion without weakening M3's seam-ON leg.

### R2 (MEDIUM-HIGH) — gate-2 (M4) must specify its arm; run seam-ON it structurally cannot show the named `data:@frame` move its own example uses

- **Plan claim:** M4: "a reference set at low/zero jitter vs one boot with an injected
  perturbation (`RB3_LOADDET_JITTER` bump)… the injected boot must show a **named** axis
  element move (e.g. a specific `data:<name>@frame` shifts…)". No seam arm stated.
- **Evidence:** Seam-ON, jitter fires **inline on the main thread**
  (`ThreadCall_Native.cpp:208`, "keep the fail-red jitter meaningful here too") — it is a
  main-thread sleep; the data completion still lands on the issue frame, so
  `data:<name>@frame` **cannot** move. Under `RB3_FIXED_CLOCK`, loader drain is
  wall-clock-independent (`Loader.cpp:622,:729` drainToEmpty, verified), so a main-thread
  sleep perturbs mostly the **audio clock vs frame** relation (songMs from
  `MasterAudio::GetTime()`, real-time, not fixed-clock-pinned — which is why songMs 21003
  lands on frame 3585 vs 5095 in the committed evidence).
- **Binding correction:** Run gate-2 on the **OFF-arm** (`RB3_FIXED_CLOCK=1`, seam unset —
  same env family as R1), where worker jitter shifts `data:<name>@frame` directly; controls
  at `RB3_LOADDET_JITTER=0`, injected boot at a high value (e.g. 2000). If the implementer
  also wants a seam-ON differential, the pre-registered expected carrier there is
  **songClock ladder shift** (main-thread sleep vs real-time audio clock), NOT
  `frameAssign` — pre-register the expected carrier per arm so a quiet `frameAssign`
  seam-ON is not misread as gate failure.

### R3 (MEDIUM) — M5's refusal-block assertion will fail as written: the red-baseline file has FOUR degenerate covariates, not two

- **Plan claim:** M5 exit: `--regrade …wash_natural.json` returns
  `refused:{fx_emit_win:2, light_changes_win:2}`.
- **Evidence (re-derived from the committed file):** `R4-M4/evidence/wash_natural.json`
  `off_boot.shots[]` (89 rows) distinct-value counts: `fx_emit_win` **2** [1290, 63425] ✓,
  `light_changes_win` **2** [65, 97] ✓ — but ALSO `songms` **2** [20944.5, 36688.1] and
  `frame` **2** [5019, 5079]. (Outcome/label fields: `hi_frac` 87, `mean_luma` 89, `class`
  3.) An exact-dict assertion on two keys fails when v2 correctly reports four.
- **Binding correction:** (a) Define the **covariate set** explicitly in v2 (covariates =
  join inputs: `fx_emit_win`/per-frame emission, `light_changes_win`/light state, `songms`,
  `frame`; outcomes `hi_frac`/`mean_luma`/`class` are exempt from the distinct-gate — an
  87-distinct outcome is healthy). (b) M5's fail-red assertion becomes: verdict
  `DEGENERATE`, `instrument_validated:False`, and `refused ⊇ {fx_emit_win:2,
  light_changes_win:2}` (superset, not equality). The 2-distinct `frame` field is itself
  the smoking gun of the stale join (89 shots on 2 frames) — v2 refusing on it is correct
  behavior, not an accident to assert away.

### R4 (MEDIUM) — false mechanism claim: `RB3HttpServerPoll` is NOT "pre-Draw"; the truth is better (A-2 becomes VERIFIED) but has two preconditions the plan must state

- **Plan claim (§1.4):** "`RB3HttpServerPoll(int frame)` :1007, runs once per frame
  **pre-Draw**". §7 A-2 treats gRB3TraceFrame currency as an assumption.
- **Evidence:** The file-header comment ("Called once per frame BEFORE Draw()",
  `rb3_http_handlers.cpp:1006`) is **stale**: the only call site is `src/App.cpp:909`,
  **after `RunOneFrame(frame)`** returns (post-Draw/EndDrawing; `PollScreenshots` follows
  at :910). This *resolves A-2 as verified-true*: `RB3TraceSetFrame(frame)` runs at the top
  of the same `RunOneFrame(frame)` iteration (App.cpp:551 trace-active arm, :563
  fixed-clock arm; `rb3_session_trace.cpp:913-914` assigns `gRB3TraceFrame = frame`), so
  `gRB3TraceFrame == frame` when the songMs sample fires. Post-frame sampling is also
  *better* aligned to wash v2's screenshots (captured at the same iteration's
  `PollScreenshots`).
- **Binding correction:** (a) Fix the plan/doc text ("post-RunOneFrame, same-iteration
  frame equality proven at App.cpp:551/:563 + :909") — do not propagate the stale
  "pre-Draw" comment into new docs (§4-lint false-premise class). (b) State the two
  preconditions: `gRB3TraceFrame` only advances when `gRB3TraceActive ||
  RB3FixedClockActive()` (App.cpp:557-:563) — all timeline captures need
  `RB3_FIXED_CLOCK=1`; the grader must flag a log whose songms/complete markers are all
  `frame=0` as VOID (mis-armed boot), never grade it. (c) The songms call site sits
  **behind `if (!TheRB3HttpServer) return;`** (`rb3_http_handlers.cpp:1018`) — songClock
  requires `RB3_HTTP=1`; the grader discloses songms sample count and a 0-sample songClock
  is "no signal"/VOID, never PASS (it fails safe in gate-1 — empty sigs are identical →
  RED — but must not silently pass an M2 grade).

### R5 (MEDIUM) — two exit assertions are not runnable as written

- **M1 exit (b):** "a PROBE=1 boot WITHOUT TIMELINE … `diff` of the `[LOADDET]`-line set vs
  a pre-M1 baseline boot shows only whitespace/ordering-invariant equality." Two *eng_hot*
  boots' `[LOADDET]` streams legitimately differ (that divergence is the lane's whole
  thesis); byte-comparison across separate boots is only valid in the **headless boot
  window**, where 10/10 identity is proven (`W0.3d-b/STATUS.md:69-77`). **Correction:**
  M1(b) = (i) on a PROBE-only boot, `grep -cE 'filearrive|songms'` == 0 AND the distinct
  marker-keyword set equals baseline's; (ii) optionally the byte-diff, but only on
  boot-window logs. (Exit (c) already covers the flags-off case.)
- **M6 exit (c):** "two boots at different ASLR base … produce … identical
  `frameAssign`/`emitTimeline` sigs **modulo timing**" — "modulo timing" is unfalsifiable
  (eng_hot sigs are *expected* to differ; nothing is left to compare). **Correction:**
  G6 = (i) `off=` values for a fixed set of recurring `sym=` names identical across 2
  boots (ASLR moved `pc=`, `off=` did not — the addr2line-offline pattern); (ii) grep of
  the grader proves no `pc=` field enters any sig/key (the plan's own grep check, keep it);
  (iii) drop the "sigs modulo timing" clause.

### R6 (LOW) — disclosures and line-drift corrections (fold into STATUS/docs, no design change)

1. **`filearrive` covers ONE of three DoneLoading entries.** `FileLoader` also reaches
   `DoneLoading` at `Loader.cpp:924` (open-fail, no bytes) and `:1016` (`LoadStream` — the
   caller-supplied `BinStream` path, ctor `:877-:881`). The ReadDone flip (:934-:941) is
   the right async-arrival site; document the other two as known-uncovered arrival paths so
   a future consumer doesn't read `frameAssign` as total-arrival coverage.
2. **`FlushAttrib` emits `frame - 1`** (`rb3_loaddet_probe.cpp:109` — draws belong to the
   frame that ENDED; `ATTRIB_RE`'s `(-?\d+)` already tolerates the frame ⁻1 row). The plan
   is silent on this. `emitTimeline` docs must state rows are end-of-frame attributed and
   the final in-flight frame of a capture is never flushed; the songms↔attrib join in wash
   v2 must use the attrib line's own `frame=` field (already off-by-one-corrected at
   emission), not log adjacency.
3. Line drift, non-material: `LoadDetSerialize()` is `ThreadCall_Native.cpp:68` (plan says
   :56; :56 is inside the jitter helper's comment block); `ARMS["eng_hot"]` is
   `white_discriminate.py:71` (plan says :70). Both mechanisms verified as claimed.
4. M3's `frameAssign` disclosure and R1's assertion both depend on the **gate-window
   definition** including song load (see R1); write the window bounds into
   `timeline-<tag>.json` so the counts are interpretable.

---

## 2. Anchor audit (what I reproduced — the plan's §1 is accurate)

Verified exactly as claimed, at HEAD: probe TU `rb3_loaddet_probe.cpp` —
`RB3LoadDetFrameTap` :117, `FlushAttrib` :89, `gRB3LoadDetAttribOn` :54, module-offset
computation :103 (PIE-stable `pc - dli_fbase`), `RB3LoadDetComplete` :142, `LoadDetOn` :60
(PROBE||ATTRIB). `Loader.cpp` — `LoadFile` :932, `ReadDone` :934, DoneLoading flip :941,
`ReadAsync` :921, drainToEmpty :622/:729; `Loader::mFile` is the base-class `FilePath`
(Loader.h:40) distinct from `FileLoader::mFile` (`File*`, Loader.h:139) — the plan's
`Loader::mFile.c_str()` qualification is correct and matches `DebugText()` :929-:931
(safe after `RELEASE(mFile)`). Existing markers: `DirLoader.cpp:742-743` ("dir"),
`DataFile.cpp:836-837` ("data"), both `#ifdef HX_NATIVE`. Engine (read-only):
`AsyncFile_Native.cpp:77` `_ReadDone() { return true; }` + :18 "actually synchronous" —
the plan's §1.2 CORRECTION (FileLoader flip is synchronous-on-native; queue-issue variance
only) is TRUE and correctly disclosed. `Rand.cpp` — `RB3_LOADDET_ATTRIB_TAP` macro :168,
tap-before-redirect in all four wrappers (:188-:189 et seq) → isolated-stream draws ARE
per-PC counted; per-PC is a finer-grained superset of the charter's "per-tag" counts
(aggregatable via addr2line), so zero-Rand.cpp-edit satisfies §6.2(iii) under review A5.
`loaddet_gate.py` — `FRAME_RE` :55 / `ATTRIB_RE` :57 / `COMPLETE_RE` :58 (neither matches
`filearrive`/`songms` lines — keyword isolation confirmed), `parse_boot_log` :123,
`grade_external_logs` :180, `resolve_offsets` :234, `ledger_for_arm` :300, `order_sig`
:326. `wash_cosample.py` — `seam_env` :59, `parse_phase` :72 (`cur_frame` join :77-:91),
`window` :106, `auc` :118 with tie-blind argsort :127. `wash_natural.json` — 89 shots,
`fx_emit_win` = exactly {1290, 63425}, fx `auc: 0.0` + `separates: true` +
`instrument_validated: true` (the false validation, reproduced). Engine classjson
`RB3_LOADDET_PROBE` row :539-:545. `objects.json:1447` `system/utl/Loader.cpp:
"NonMatching"`. Lint 9: `native/CMakeLists.txt:247` globs `src/system/utl/*.cpp` into the
native build ✓. Collision matrix cross-checked against the committed `W-ISO/PLAN.md`
(:303, :312): Rand.{h,cpp} F-only, `capture_lints.py` I-first + F-stub — **consistent,
no write-write overlap**; hazard files (`FxSendNative.cpp` engine-dirty,
`rb3_session_trace.cpp` rb3-dirty, both re-verified in `git status`) untouched by the
plan's file set. Naming box honored: `frameAssign` hashes `kind:name@frame` (new
keywords/flag; `order_sig` and its `completes[]` input untouched — the plan's refusal to
reuse `RB3LoadDetComplete(…, "file")` is the right call and is what keeps R4's 10/10
order axis unperturbed).

**TWELVE defaults / G3 / G5:** new flag default-OFF, all new code `#ifdef HX_NATIVE` or
native-TU-only; the single src/system edit mirrors the DirLoader precedent; MWCC never
defines HX_NATIVE → Wii bytes unchanged; G5 baseline-before-edit (A-4) stands. No default
flips, no pin bumps, classjson append-only under lock per process rules. ✓

**§4 lints:** 3 (oracle fail-reds: present, strengthened by R1-R3), 7 (evidence paths
named per milestone ✓), 8 (event/sample counts: present; R1/R4 extend to OFF-arm +
songms-count), 9 (flavor membership: verified above ✓).

---

## 3. Verdict

**APPROVE-WITH-AMENDMENTS.** R1–R6 are binding on the implementer; none changes the
design's shape (markers, flag, three axes, wash v2, milestones M1–M6 all stand). R1 and R2
add/redirect one gate arm each; R3–R5 fix exit-assertion wording to what the committed
evidence and the tree can actually produce; R6 is disclosure hygiene. The plan's §1.2
synchronous-AsyncFile correction and §1.5 Rand-ownership reading are both verified and
correctly folded — the review amendments (A2/A3/A5/A6) are all honored as binding.
