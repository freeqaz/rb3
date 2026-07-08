# T1-FRAMETRACE — PLAN (Wave-19, Lane F)

**Tool:** T1 frame-timeline tracer, **ATTRIBUTION mode** + wash co-sampler v2.
**Charter:** `RETROSPECTIVE/OPTIONS.md` §6.2 (rb3 `c4395043`) + `WAVE19_KICKOFF.md` Lane F
(`d93fa894`) as amended by `WAVE19_REVIEW.md` A2/A3/A5/A6 (`fb1f00e5`, **binding**).
**Author:** PLAN AUTHOR (Opus). **Status:** FOR FABLE REVIEW → implementer.
**Base:** rb3 HEAD `fb1f00e5`, engine pin `beb89e5`. NO default flips, NO pin bumps, NO engine edits.

> **NAMING BOX (review F9, binding).** This lane targets the **FRAME-ASSIGNMENT TIMING**
> axis — *which frame* async work lands on and *what each frame's consumers emit*. It is
> **NOT** R4's ledger `order` axis (DirLoader/DataLoader completion **SEQUENCE**), which
> already PASSES 10/10 (`R4-DETERMINISM/LEDGER.md`). The new `frameAssign` axis reads the
> same completion markers R4's `order` axis reads, but binds each completion to its **frame
> index** (`kind:name@frame`) — a different hash from `order_sig`'s `kind:name` sequence.
> Docs and code comments must keep these distinct; a lane that re-proves boot-loader
> sequence determinism has failed its charter.

---

## 0. Shape (5-sentence orientation)

T1 adds three **read-only measurement** axes — `frameAssign`, `songClock`, `emitTimeline` —
to the existing `[LOADDET]` marker family and grades them in a new `loaddet_gate.py
--timeline` mode, keyed on PIE/ASLR-stable module offsets and `kind:name` strings. It is an
**attribution instrument only**: it MEASURES the still-unpinned frame-assignment timing
residual that the R4 stream-position seam does not touch, and it does **not** introduce any
pinning/serialization mechanism (that is T3, whose GO/NO-GO and mechanism choice are chosen
FROM T1's attribution output). The wash co-sampler is rewritten as v2 (per-frame join +
≥N-distinct-covariate refusal + midrank AUC) as T1's first consumer, with a binding fail-red:
v2 must mechanically REFUSE the committed 2-cluster `wash_natural.json`. All new emission is
behind a new default-OFF flag `RB3_LOADDET_TIMELINE`, so every existing `RB3_LOADDET_PROBE`
consumer (R4 ledger, white_regrade, wash v1) sees a byte-identical marker stream, and the
MWCC Wii-match build is untouched (all new code is `#ifdef HX_NATIVE`). **Riskiest
assumption:** that the eng_hot capture window carries enough live divergence signal on
`emitTimeline`+`songClock` for gate-1 to fire (the boot window is H-TIMING-REFUTED per A2.2,
so gate-1 MUST run on eng_hot) — see §7 A-1.

---

## 1. What I verified in the tree (vs assumptions)

Every anchor below re-derived by symbol at HEAD `fb1f00e5` (rb3) / `beb89e5` (engine).

### 1.1 The probe TU — `native/src/rb3_loaddet_probe.cpp` (VERIFIED)
- `RB3LoadDetFrameTap(int frame)` **:117** — per-frame tap; emits `[LOADDET] frame=N
  gdraw=M`; calls `FlushAttrib(frame)` **:89** when `gRB3LoadDetAttribOn` (**:54**).
- `FlushAttrib` emits, per distinct caller PC, `[LOADDET] attrib frame=N pc=P **off=0x..**
  sym=S mod=M draws=U` — **`off` is `pc - dli_fbase`, a module-relative offset** (:103) →
  **PIE/ASLR-stable by construction**, resolved offline via addr2line. This is the
  `emitTimeline` key source; **no new attrib code needed**.
- `RB3LoadDetComplete(name, kind)` **:142** — emits `[LOADDET] complete frame=N gdraw=M
  kind=K name=F`, keyed on `gRB3TraceFrame`. Call sites: `DirLoader.cpp:743` (kind="dir"),
  `DataFile.cpp:837` (kind="data"), both inside `#ifdef HX_NATIVE` (**G3-safe pattern**).
- `LoadDetOn()` **:60** gates on `getenv("RB3_LOADDET_PROBE") || getenv("RB3_LOADDET_ATTRIB")`.

### 1.2 The async-file seam — `src/system/utl/Loader.cpp` (VERIFIED + CORRECTION)
- `FileLoader::LoadFile()` **:932**; `mFile->ReadDone(asdf)` **:934**; on true → `mState =
  &FileLoader::DoneLoading` **:941**. `mFile->ReadAsync(...)` issued at **:921**.
- `drainToEmpty = (mPeriod >= 1e29f) || RB3FixedClockActive()` at **:622** (web arm) and
  **:729** (native arm) — W0.3b full-drain-under-fixed-clock, exactly as A2 states. ✓
- **CORRECTION to A2 (VERIFIED, disclose to implementer):** the review directs the
  `frameAssign` marker at "the LW-1 async-file arrival seam". Native `AsyncFile` is
  **synchronous**: engine `AsyncFile_Native.cpp` `AsyncFileNative::_ReadDone()` **returns
  `true` immediately** (:77; header :1-2, :18 "actually synchronous"). So on native the
  `ReadDone`→`DoneLoading` flip fires the **same poll** the read was issued; the marker's
  run-to-run frame variance comes from *queue-issue timing* (when the loader reaches the
  front, which depends on prior loaders), **not** I/O latency. The dominant `frameAssign`
  divergence carrier in the gate window is therefore the **ThreadCall/DataLoader "data"
  completion** (`DataFile.cpp:837`, worker-parse timing under `RB3_LOADDET_JITTER`), not the
  FileLoader flip. **Decision:** still add the FileLoader marker per the review directive
  (completeness of the LW-1 picture, and queue-issue variance is real), but document it as a
  secondary/completeness signal and drive gate-1's assertion off the carriers with signal in
  eng_hot (`emitTimeline`+`songClock`; `frameAssign` disclosed with its event count).

### 1.3 seam mechanics — engine (VERIFIED, read-only, NOT edited)
- `ThreadCall_Native.cpp`: `LoadDetWorkerJitter()` **:38** (env `RB3_LOADDET_JITTER`, worker
  sleeps 0..N µs before each dispatch, default-OFF), `LoadDetSerialize()` **:56** (gated
  `RB3_FIXED_CLOCK && RB3_LOAD_DETERMINISM`); when serialize is on the pending job is run
  **inline on main** (:205ff) so the worker↔main alloc race is removed → **seam-ON pins the
  gRand STREAM (R4's axis)** but does **not** pin frame-assignment timing (T1's three axes).
- Confirms A2: ThreadCall is inline under the seam; the timing axis is the residual.

### 1.4 songMs source — A3 decision = **option (b)** (VERIFIED)
- `native/src/rb3_http_handlers.cpp` `RB3HttpServerPoll(int frame)` **:1007**, runs once per
  frame pre-Draw, computes `songMs` from the null-guarded
  `TheGame->GetBeatMaster()->GetAudio()->GetTime()` chain **:1027-1032**, then
  `NotifyFrame(frame, screen, songMs)` **:1033**. This file is **git-clean** (verified) — a
  safe host for the songMs sample.
- **A3 hazard avoided:** `native/src/rb3_session_trace.cpp` is DIRTY (`M`, +10, the
  `CurrentSongMs` external-linkage fix, 2026-07-07) and the hazard note forbids staging it.
  T1 takes **A3 option (b)**: sample songMs at the clean `RB3HttpServerPoll` site into a
  probe-owned marker; **T1 never touches `rb3_session_trace.cpp`** and never declares
  `extern float CurrentSongMs()`.

### 1.5 Rand.{h,cpp} ownership — review A5 (VERIFIED, supersedes kickoff/my prompt)
- **`src/system/math/Rand.{h,cpp}` is Lane F's file this wave (review A5).** My dispatch
  prompt's collision line said "W-ISO owns Rand.h/.cpp tags"; that reflects the pre-review
  kickoff — **the review moved Rand.cpp ownership to F**, and per the conflict rule the
  review governs. **T1 needs NO Rand.cpp edit:** `emitTimeline` derives entirely from the
  existing per-PC attrib stream (`FlushAttrib`, §1.1) because the attrib tap fires BEFORE the
  redirect (`RB3_LOADDET_ATTRIB_TAP()` at macro :168, invoked ahead of `RB3_LOADDET_REDIR`
  in each wrapper, e.g. `RandomInt` :189-192) → **isolated private-stream draws ARE per-PC
  counted**. Ownership is declared only to keep Lane I out of the file; the file is expected
  to stay unmodified.

### 1.6 The scripts I own (VERIFIED)
- `scripts/native/loaddet_gate.py`: `parse_boot_log` **:123** already parses `frames{}`,
  `attribByOff{off:{frame:draws}}`, `completes[(frame,kind,name)]`; `ledger_for_arm` **:300**
  (`order_sig` **:326** = R4's order axis, DO NOT alter); `grade_external_logs` **:180**
  (A2/F6 VOID discipline); `resolve_offsets` **:234** (addr2line offline). Regexes: `FRAME_RE`
  :55, `ATTRIB_RE` :57 (captures `off`), `COMPLETE_RE` :58.
- `execution/R4-M4/wash_cosample.py`: `parse_phase` **:72** (`cur_frame`-attributed
  LIGHTVAL = the stale join), `auc` **:118** with **tie-blind argsort ranking** (:127, the
  AUC-0.000 artifact), `seam_env` **:59** (imports `wd.ARMS["eng_hot"]`).
- **Red baseline `R4-M4/evidence/wash_natural.json` (VERIFIED degenerate):** 89 shots,
  `fx_emit_win` has **exactly 2 distinct values [1290, 63425]**, `light_changes_win` 2
  values [65, 97]; yet v1 reports `separates:true`/`instrument_validated:true` with **AUC
  0.0** — the exact false-validation v2 must refuse.

### 1.7 classjson (VERIFIED)
- `RB3_LOADDET_PROBE` is catalogued in engine
  `src/platform/NativeCompatFlags.classification.json` **:539** (`class:probe`,
  `read:presence`, `faithfulStatus:"…read rb3-side…"`). No getenv-scanning validator script
  exists in `src/platform/` (only `.h/.cpp/.json`), so an rb3-side flag row is documentation
  the coordinator regenerates into gen.inc at close-out. The new `RB3_LOADDET_TIMELINE` row
  mirrors this format (§4).

---

## 2. Design — markers, flag, axes

### 2.1 New flag (default-OFF): `RB3_LOADDET_TIMELINE`
Read once (cached) rb3-side in the probe TU. **Why a new flag rather than reusing
`RB3_LOADDET_PROBE`:** the new markers must NOT perturb the marker stream that R4's ledger,
white_regrade, and wash v1 parse/diff when they run `RB3_LOADDET_PROBE=1`. In particular the
`frameAssign` marker must **NOT** reuse `RB3LoadDetComplete(...,"file")` — that would add a
row to `completes[]`, which `order_sig` (:334) hashes, **regressing R4's already-10/10 order
axis**. A dedicated flag + dedicated marker keywords guarantee zero perturbation. The timeline
gate sets `RB3_LOADDET_PROBE=1 RB3_LOADDET_ATTRIB=1 RB3_LOADDET_TIMELINE=1` together.

### 2.2 New markers (all rb3-side, all `#ifdef HX_NATIVE`, all gated on TIMELINE)
Add to `native/src/rb3_loaddet_probe.cpp` a `TimelineOn()` helper (cached getenv, same shape
as `LoadDetOn`) and:

| Marker | Emitted from | Line format | Feeds |
|---|---|---|---|
| file-arrival | `Loader.cpp` FileLoader flip (§2.3) via new `RB3LoadDetFileArrive(name)` | `[LOADDET] filearrive frame=N gdraw=M name=F` | `frameAssign` |
| songMs sample | `rb3_http_handlers.cpp` `RB3HttpServerPoll` via new `RB3LoadDetSongMs(float ms)` | `[LOADDET] songms frame=N ms=X.X` | `songClock` |

- **Distinct keyword `filearrive`** (not `complete kind=file`) → no existing regex consumes
  it (`COMPLETE_RE` matches `complete `). New regexes added in §2.4.
- `RB3LoadDetSongMs(float ms)` reads `gRB3TraceFrame` **internally** (extern, already used by
  `RB3LoadDetComplete`) for its frame key, so songMs shares the one frame axis regardless of
  the `frame` param `RB3HttpServerPoll` was handed. (ASSUMPTION A-2, §7.)
- `emitTimeline` needs **no new marker** — it is derived from existing `[LOADDET] attrib`
  lines (`off`, `frame`, `draws`).
- `frameAssign` also consumes the **existing** `[LOADDET] complete` lines (dir/data) — but
  only inside the new `--timeline` grader, keyed as `kind:name@frame` (never touching
  `order_sig`).

### 2.3 Edit site — `src/system/utl/Loader.cpp` (the one src/system edit; G3-critical)
Inside `FileLoader::LoadFile()` (:932), at the `ReadDone`-true branch just before/at
`mState = &FileLoader::DoneLoading;` (:941), add:
```cpp
#ifdef HX_NATIVE
        { extern void RB3LoadDetFileArrive(const char *); RB3LoadDetFileArrive(Loader::mFile.c_str()); }
#endif
```
Mirror the exact `#ifdef HX_NATIVE { extern …; call; }` shape of `DirLoader.cpp:742-744`.
**G3:** HX_NATIVE is never defined in the MWCC Wii build → the FileLoader symbols are
byte-identical → `batch_objdiff` on `system/utl/Loader.cpp` == baseline (§6.3). (`Loader.cpp`
is already `NonMatching` in objects.json:1447; G3 requires *no regression*, not a match.)

### 2.4 Grader — `scripts/native/loaddet_gate.py --timeline`
Add, without touching `ledger_for_arm`/`order_sig`:
- Regexes: `FILEARRIVE_RE = r"\[LOADDET\] filearrive frame=(\d+) gdraw=(\d+) name=(\S+)"`,
  `SONGMS_RE = r"\[LOADDET\] songms frame=(\d+) ms=(-?\d+(?:\.\d+)?)"`.
- Extend `parse_boot_log` to also collect `fileArrivals[(frame,name)]` and
  `songMs{frame:ms}` (behind the existing `attrib` capture path; unknown lines already
  ignored by other callers).
- Three axis signatures (all md5 of a canonical string; **each disjoint from `order_sig`**):
  - **`frameAssign`** = `_md5("|".join(f"{kind}:{name}@{frame}" for (frame,kind,name) in
    sorted(completes)) + "#" + "|".join(f"file:{name}@{frame}" for (frame,name) in
    sorted(fileArrivals)))`. Binds every completion/arrival to its frame. *Distinct from
    `order` (which drops the `@frame`).*
  - **`songClock`** = the frame at which songMs first crosses each of a fixed checkpoint
    ladder (e.g. every 2000 ms over the gate window): `_md5("|".join(f"{cp}:{first_frame_ge(cp)}"
    …))`. Directly encodes the "21003 @ 3585 vs 5095" evidence.
  - **`emitTimeline`** = `_md5` of the per-frame, per-`off` emission sequence over the gate
    window: `"|".join(f"{fr}:{off}:{draws}" for fr in sorted(frames) for (off,draws) in
    sorted(attribByOff@fr))`. Keyed on module `off` (PIE-stable).
- New `--timeline` mode: parse a set of eng_hot boot logs (launch N boots via `boot_measure`
  in an **eng_hot arm** — see §2.5 — or `--grade-logs` external), compute the three sigs per
  boot vs boot[0], report per-axis `PASS/DIVERGE` + **per-axis event/sample counts** (lint-8
  disclosure: a quiet `frameAssign` with 0 completion events is "no signal", not a pass).
  Write `timeline-<tag>.json` (`allow_nan=False`). VOID discipline inherited: grade the exact
  measurement boots' own logs; never discard-and-rerun (A2/F6).

### 2.5 Gate capture regime — eng_hot (A2.2, BINDING)
The boot window is H-TIMING-REFUTED (all completions land by frame 2, 10/10 identical, per
`W0.3d-b/STATUS.md:69-77`). Gate-1 MUST run on the **eng_hot window** (song-load → gameplay),
where the divergence lives (songMs@frame, InitParticle emission swing). Reuse the
`RB3_VENUE_*` eng_hot arm from `WHITE-fix/white_discriminate.py` `ARMS["eng_hot"]` (:70), the
same regime wash_cosample already uses. Add a `--regime eng_hot` switch to the timeline mode
that folds those env vars into `boot_measure`'s arm env, and drive to a post-load songMs
target (the wash sweep pattern) rather than the boot-window K offset.

---

## 3. Wash co-sampler v2 — `execution/R4-M4/wash_cosample.py` (rewrite)

Per §6.2 (F1 correction, verbatim) — three required changes, **in this order**:

1. **Per-frame join (not stale poll window).** Build the co-sample from the T1 per-frame
   timeline table `{frame → (fx_emit, light_state, songMs, hi_frac_if_shot)}` and join each
   screenshot to **its own frame's row** from that table. Replace the v1 `window(fx_draws,
   fr, win)` trailing-sum (which collapses to 2 clusters when fx_draws is bursty) with the
   per-frame emission value at (and, if a smoothing window is kept, strictly around) the shot
   frame, read from the same ledger `emitTimeline` uses. The stale `cur_frame`-attributed
   LIGHTVAL join (parse_phase :77-91) is replaced by the timeline record's per-frame light
   state.
2. **≥N-distinct-covariate assertion (the lint that refuses F1).** Before any AUC: for each
   covariate, count distinct values across the joined shots. If any covariate has
   `< N_MIN_DISTINCT` (default **5**; ≥ the "2 clusters" degeneracy with margin), emit
   verdict `DEGENERATE` and set `instrument_validated=False` with an explicit
   `refused: {covariate: n_distinct}` block. **Do not compute/return a separation claim on a
   degenerate covariate.**
3. **Midrank AUC/U.** Replace the tie-blind `ranks[order]=arange` (:127) with average-rank
   (midrank) assignment for ties (scipy `rankdata(method='average')` or an inlined midrank),
   so ties yield the true AUC (~0.32) not 0.000.
4. **Light-POSITION amplitude signal — added ONLY AFTER 1-3** (F1 ordering requirement); it
   is a new covariate subject to the same ≥N-distinct gate.

**Offline re-grade mode `--regrade <json>`:** read a stored `wash_natural.json`-shape file's
`off_boot.shots[]` covariates and run steps 2-3 with **no boot** — the mechanical regression
test for fail-red (3).

**capture_lints wiring (A6):** wash v2 imports `scripts/native/capture_lints.py` (Lane I's
new file: `allow_nan=False`, black-frame/luma-0 exclusion with disclosure, attempt
disclosure). **Landing order (A6):** Lane I lands `capture_lints.py` as its first checkpoint
commit; if T1 reaches v2 first, T1 **stubs the import** (`try: import capture_lints except:
local fallbacks`) and re-wires in one line when Lane I's file lands. T1 does NOT create
`capture_lints.py`.

---

## 4. classjson row (append-only, under `/tmp/milo-engine-classjson.lock`)

Append to engine `src/platform/NativeCompatFlags.classification.json` (NO gen.inc regen —
coordinator regenerates once at close-out):
```json
 "RB3_LOADDET_TIMELINE": {
  "class": "probe",
  "owner": "render/determinism",
  "faithfulStatus": "n/a: Wave-19 T1 frame-timeline tracer (attribution mode) — gates the NEW per-frame filearrive + songms markers so existing RB3_LOADDET_PROBE consumers (R4 ledger/order axis, white_regrade, wash v1) keep a byte-identical marker stream; read rb3-side (Loader/http_handlers/loaddet probe taps). Default-OFF, presence-read; ATTRIBUTION-only, no pinning (that is T3)",
  "default": "off",
  "read": "presence"
 },
```
This is T1's **only** engine-side change (a catalog row, not code). All executable code is
rb3-side.

---

## 5. Milestones + exit criteria

**M1 — markers + flag (rb3 code).** Add `RB3_LOADDET_TIMELINE`/`TimelineOn()`,
`RB3LoadDetFileArrive`, `RB3LoadDetSongMs` to `rb3_loaddet_probe.cpp`; the HX_NATIVE call in
`Loader.cpp:941`; the `RB3LoadDetSongMs(songMs)` call after `rb3_http_handlers.cpp:1032`;
classjson row (§4, under lock). Build `rb3-native` in own build dir.
*Exit:* (a) a `TIMELINE=1` eng_hot boot emits `[LOADDET] filearrive` and `[LOADDET] songms`
lines; (b) a `PROBE=1` boot WITHOUT `TIMELINE` emits **zero** new lines — `diff` of the
`[LOADDET]`-line set vs a pre-M1 baseline boot shows only whitespace/ordering-invariant
equality (byte-identical marker family for R4/wash); (c) `grep -c filearrive\|songms` on a
plain boot (no flags) == 0. Checkpoint committed.

**M2 — three axes + `--timeline` grader.** Implement §2.4.
*Exit:* `--timeline --grade-logs <2 eng_hot logs>` emits `frameAssign`/`songClock`/
`emitTimeline` per-boot vs reference with per-axis PASS/DIVERGE **and event/sample counts**;
re-grading the SAME log twice yields byte-identical sigs (grader determinism);
`order_sig`/`ledger_for_arm` output on an R4 fixture is **unchanged** vs pre-M2 (proves no
conflation, naming box). `allow_nan=False` honored.

**M3 — gate-1 (instrument-detects-the-axis), eng_hot, N=6 seam-ON.** Run 6 seam-ON eng_hot
boots at `RB3_LOADDET_JITTER=200`.
*Exit / fail-red:* `emitTimeline` AND `songClock` must **DIVERGE** across the 6 boots (the
axis is real and unpinned by the current seam — the whole point of attribution mode);
`frameAssign` reported with its completion-event count (may be low in eng_hot — disclosed,
not asserted). **If all asserted axes are 6/6 identical → the instrument is blind → RED,
rebuild** (do not ship a blind instrument). Evidence: `evidence/gate1-eng_hot-seam-on.json`
+ the 6 boot logs, committed.

**M4 — gate-2 (injection fail-red).** Differential: a reference set at low/zero jitter vs one
boot with an injected perturbation (`RB3_LOADDET_JITTER` bump). Because the eng_hot floor may
already diverge, the assertion is **differential-attributable**: the injected boot must show a
**named** axis element move that the controls do not (e.g. a specific `data:<name>@frame`
shifts, or the `emitTimeline` per-frame delta for a named `off` changes) — proving the
instrument RESPONDS to the induced mechanism, not just ambient noise. Evidence:
`evidence/gate2-injection.json` + logs, committed.

**M5 — wash v2 + fail-red (3).** Rewrite per §3.
*Exit / fail-red (3):* `wash_cosample.py --regrade R4-M4/evidence/wash_natural.json` returns
verdict `DEGENERATE`, `instrument_validated:False`, `refused:{fx_emit_win:2, light_changes_win:2}`
(the ≥5-distinct gate fires on the committed 2-cluster file). Plus one live natural-venue
re-run producing either ≥N distinct covariates (join fixed) OR an honest
`still_degenerate` disclosure with counts (never a silent pass). capture_lints wired (or
stubbed per A6). Evidence: `evidence/wash_v2_regrade_refusal.json` + `evidence/wash_v2_live.json`.

**M6 — inertness + closeout.** (a) **G2 flag-OFF:** plain boot → 0 timeline lines + drawlog
byte-identical vs baseline; PROBE-only boot marker stream byte-identical (re-affirm M1b).
(b) **G3 Wii-match:** `batch_objdiff` on `system/utl/Loader.cpp` touched symbols ==
baseline (HX_NATIVE inert). (c) **PIE-stable keys:** two boots at different ASLR base (default
Linux ASLR) produce identical `off=` keys for the same consumer AND identical `frameAssign`/
`emitTimeline` sigs modulo timing (keys use `off`/`kind:name`, never raw `pc` — verified by
grep of the grader). Write `STATUS.md`, update checkpoint with all commit SHAs, commit
evidence.

---

## 6. Gates — each with its fail-red demonstration

| Gate | Assertion | Fail-red demonstration |
|---|---|---|
| **G1 instrument-detects** (M3) | eng_hot N=6 seam-ON: `emitTimeline`+`songClock` DIVERGE | If 6/6 identical → blind → RED/rebuild. Evidence names the divergent axis + counts. |
| **G2 injection** (M4) | injected jitter → a NAMED axis element moves in the injected boot, absent in controls | Un-injected controls do NOT show that element move → the instrument reflects the mechanism, not noise. |
| **G3 wash-refusal** (M5, BINDING) | `--regrade wash_natural.json` → `DEGENERATE`, refused, not validated | The file has exactly 2 distinct `fx_emit_win` values; ≥5-distinct gate fires; v1's false `validated:true`/AUC-0.0 is mechanically prevented. |
| **G4 flag-OFF inertness** (M6) | plain boot: 0 timeline lines, drawlog byte-identical; PROBE-only stream byte-identical | Any new line under PROBE-without-TIMELINE = FAIL (would regress R4/wash). |
| **G5 Wii-match** (M6) | `batch_objdiff` on `Loader.cpp` == baseline | Any touched-symbol delta = FAIL (HX_NATIVE leak). |
| **G6 PIE-stable keys** (M6) | keys are module offsets / kind:name, never raw pc | grep the grader for `pc=` use in a key = FAIL. |

---

## 7. Assumptions the reviewer should attack

- **A-1 (riskiest).** eng_hot carries enough live `emitTimeline`+`songClock` divergence for
  gate-1 to fire. *Basis:* the committed evidence (songMs 21003 @ 3585 vs 5095; InitParticle
  [2505,1755,2509]; WASHPROBE 19898–27938) is exactly the eng_hot window. *If wrong:* gate-1
  would be silent for lack of signal, not blindness — the plan's lint-8 event/sample counts
  distinguish these, and the mitigation is to lengthen the gate window / raise jitter, not to
  weaken the assertion. `frameAssign` is deliberately NOT in the gate-1 assertion because
  loader completions may be quiet post-load (§1.2).
- **A-2.** `gRB3TraceFrame` is current (this-frame) when `RB3HttpServerPoll` runs, so the
  songMs marker shares the frame axis. *Basis:* the existing `RB3LoadDetComplete` markers
  already key on `gRB3TraceFrame` from arbitrary main-thread sites and work in R4. *If
  wrong:* fall back to `RB3HttpServerPoll`'s own `frame` param and reconcile in the grader.
- **A-3.** A new rb3-side flag needs only a classjson catalog row (no engine getenv site).
  *Basis:* `RB3_LOADDET_PROBE` is rb3-side and catalogued the same way; no validator script
  found (§1.7). *If wrong:* the fallback is to reuse `RB3_LOADDET_PROBE` gating with the
  distinct `filearrive`/`songms` keywords (byte-safe for the ORDER axis since neither is
  matched by `COMPLETE_RE`) and drop the new flag — at the cost of extra lines in PROBE-only
  streams (acceptable only if no consumer raw-diffs stderr; verify first).
- **A-4.** `batch_objdiff`'s baseline for `Loader.cpp` is reproducible pre-edit. *Basis:*
  G3 pattern used by every prior HX_NATIVE marker. Implementer captures the baseline BEFORE
  the M1 edit.

---

## 8. Collision statement (vs the other two Wave-19 lanes)

**T1-FRAMETRACE (Lane F) writes, and owns:**
- `native/src/rb3_loaddet_probe.cpp` (new markers/flag)
- `src/system/utl/Loader.cpp` (one HX_NATIVE `filearrive` call, G3-neutral)
- `native/src/rb3_http_handlers.cpp` (one `RB3LoadDetSongMs` call — clean file, A3 opt-b)
- `src/system/math/Rand.{h,cpp}` — **owned per review A5** (Lane I forbidden), **expected
  unmodified** (emitTimeline derives from existing tap)
- `scripts/native/loaddet_gate.py` — **F-only** (A6); adds `--timeline` + 3 axes; does NOT
  touch `ledger_for_arm`/`order_sig`
- `execution/R4-M4/wash_cosample.py` — **wholly F** incl. its capture_lints wiring (A6)
- engine classjson: append `RB3_LOADDET_TIMELINE` row only (append-only, under lock)

**T2-WORLDROI (Lane P):** sole ENGINE writer (`Rnd_Wgpu_RB3.cpp` prov sidecar ~:3377-:4130)
+ `uidump_query.py`. **Disjoint** from every T1 file. (Engine dirt is `FxSendNative.cpp`
only — disjoint from both.)

**W-ISO (Lane I):** the four venue-consumer TUs (`CharClipDriver.cpp`, `Crowd.cpp`,
`CharInterest.cpp`, `LightPresetManager.cpp` — function-scope guards per A4), the NEW
`scripts/native/capture_lints.py`, and `execution/R4-M4/white_regrade.py` wiring. **Lane I
does NOT touch `Rand.{h,cpp}` (A5) nor `loaddet_gate.py`/`wash_cosample.py` (A6).**

**Shared-import handshake (A6):** wash v2 imports Lane I's `capture_lints.py`; Lane I lands
it first; T1 stubs the import if it gets there first — one-line rewire either way. No
write-write overlap on any file.

**Hazards never staged:** engine `FxSendNative.cpp`; rb3 `rb3_session_trace.cpp`. T1's design
touches neither.
