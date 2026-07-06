# Wave 7 — Pre-dispatch Review (Fable adversarial pass)

**Reviewer:** Fable subagent, 2026-07-06. Scope: `WAVE7_KICKOFF.md` vs the diagnosis artifacts
(W3.3, W4.2, W2.8, W2.1-flip-blocker §Backlog, W3.2) + live source spot-checks (engine
`Rnd_Wgpu_RB3.cpp` / `RB3MaterialBinder.cpp` / `rb3_postproc.wgsl.inc`, rb3 harnesses, goldens,
`Utl.cpp`, git state of both repos).

## VERDICT: **dispatch-with-amendments**

The lane structure, oracle-first sequencing (Lane B), and the W3.3→WASH ordering are sound, and the
kickoff represents the Wave-6 diagnosis artifacts faithfully (verified against each STATUS.md and
today's source line numbers). But there is one hard **dispatch precondition** (uncommitted Wave-6
close-out, A1), one **stale factual premise** in the WASH lane (A2 — the "unlanded W0.3d part-b
patch" does not exist; it landed in Wave 5), and two **flip-semantics breaks the A5 sweep missed**
(A8). None require redesign; all are brief-level amendments.

---

## Amendments

### A1 (BLOCKER) — Commit the Wave-6 close-out BEFORE dispatch; the flip's rb3 side is entirely uncommitted

Verified in the working trees right now:

- `MILO_ENGINE_PIN` in `native/CMakeLists.txt:74` is still **`8e7eddd`** (pre-flip); engine HEAD is
  `1b045d9` (flip = `fced18b`, one behind). The kickoff's own header says the pin bump follows the
  re-golden commit — that commit has not happened.
- `git status` (rb3): **modified-uncommitted** — `native/tests/goldens/drawlog/splash_screen.json`
  (888→792, +5185/−8833), `splash_screen.fixedclock-residual.json` (regenerated post-flip, N=36 per
  its own `_comment`), all **three inverted harnesses** (`w21flip-dolphin-ab.py`, `w21flip-ui-ab.py`,
  `wash-measure.py`), `NATIVE_COMPAT_LEDGER.md`, `REFACTOR_PLAN.md`, `execution/README.md`.

Consequences if dispatched as-is: (a) the WASH lane will edit `wash-measure.py` **on top of its
uncommitted inversion** — a clobber/ordering hazard the hard rules can't protect against; (b) any
worktree-based work (Lane D; `tools/setup-worktree.sh`) checks out the **committed** state — 888-draw
golden + non-inverted harnesses — against a post-flip engine, so drawlog/A-B gates misfire in ways
that look like real regressions; (c) every lane build warns on the pin mismatch. **Fix:** coordinator
lands the close-out commits (goldens+sidecar, harness inversions, ledger/README/plan) and bumps the
pin to `1b045d9`, then dispatches.

### A2 (R-B answer) — The WASH ranked-prior premise is stale: there is NO unlanded W0.3d part-b patch

The kickoff (and the Wave-6 backlog line it inherits) says prior #1 is "async asset residency — the
unlanded W0.3d part-b patch". Verified false on two axes:

- The only staged W0.3d part-(b) artifact was `W0.3d-fix.patch` (`src/system/rndobj/Utl.cpp`
  `SortDraws` material-NAME tie-break — see `W0.3d/STATUS.md:95-198`), and it **LANDED in Wave 5**
  as rb3 `76f51077` (verified: `git show 76f51077` touches `Utl.cpp` +26; live at `Utl.cpp:192`).
- That fix is **order-only and fixed-clock-gated** (`RB3FixedClockActive() &&
  !RB3DrawSortDeterministicOff()`): it does not touch async asset/texture **residency** at all, and
  it is inert in `wash-measure.py`'s real-time boots and in normal user runs.

So: **R-B = no, the matrix has no landing dependency — run it now.** But rewrite prior #1 as "async
asset/texture residency at capture — **no fix exists, staged or otherwise**; the landed SortDraws
tie-break is order-only + fixed-clock-gated and cannot help". If the matrix implicates residency,
the fix is NEW work in the engine loader/worker path (`ThreadCall_Native.cpp`, `DataFile.cpp:786`
chain per W0.3d.S2) — files **not** in Lane A's declared list; fence that follow-up explicitly when
it materializes.

### A3 (R-A answer) — W3.3-fix must be flag-first (registered flag, coordinator flip), not conditional same-step default-ON

Yes, one-step default-ON is too aggressive, and the kickoff's own escape hatch ("default-ON only if
the sweep is unambiguous") samples the wrong population. Verified mechanics: the current per-channel
ceiling (`rb3_postproc.wgsl.inc:186-191`) engages whenever ANY channel > 0.82; the proposed
luma-keyed version engages only when **luma** > 0.82. Frames with a hot single channel but sub-knee
luma (saturated stage-light colors — common outside song-start) are rolled today but go
**uncompressed to the final clamp** under the patch: a behavior change on a population the songMs
0–25s sweep never samples (SP overlay, big rock endings, hot menus). Land it behind a registered
flag (default-OFF), run S2 + let the S3 WASH matrix double as broad hot-frame coverage (it runs
gameplay boots at ~21s with the fix ON), then coordinator flip — the same discipline every other
behavior change in this campaign followed, and the Wave-5 flip-hold showed why.

Two accuracy notes for the brief: (i) the "staged patch" is a **comment-only proposal file**, not a
git-appliable patch (`staged-luma-preserving-ceiling.patch` is 100% `#`-prefixed); the brief should
say "implement per the staged proposal", not "apply the patch". It does, however, quote the current
shader block **byte-exactly** (verified against `rb3_postproc.wgsl.inc:186-191`), so implementation
is mechanical. (ii) The DC3 zero-blast claim holds: `rb3_postproc.wgsl.inc` is included only by
`RB3PostProc.cpp:118`, an RB3-only TU.

### A4 — The WASH matrix needs a W3.3-OFF control arm (and pinned flag hygiene)

The kickoff's "first question: did W3.3-fix already collapse the wash rate?" is unanswerable from a
matrix run only on the fixed build — the Wave-6 rates (OFF 2/7 / ON 4/7) were measured on a
different build and protocol. Add a **5th config: W3.3-fix flag OFF, everything else default**, at
the same N≥6, as the in-experiment baseline. Also spell out: (a) all matrix configs run the
placement contract at **default (ON)** — do not inherit `wash-measure.py`'s OFF/ON `STATES` arms
(line 260) when adapting it; (b) the 4 isolation configs run with the W3.3 flag ON (per A3 it will
not be default yet).

### A5 (R-C answer, part 1) — Lane B step 0: re-measure the EXISTING `RB3_HANDS_BIND_FIX` branch under the new BL-A2 oracle before writing any new rebake

The mechanism shape is plausible but has an untested cheap experiment sitting in the tree: W2.2's
`RB3_HANDS_BIND_FIX` (default-OFF, `BandCharacter.cpp:1385`) was judged "no benefit" under a
provably rotation-blind origin metric (W2.8's key finding). Once BL-A2 exists, an ON/OFF A/B of that
existing flag against the far-vertex metric is one boot each and decisively sorts the mechanism
question: if it moves the metric, BL-A1 is an extension + flip decision, not new code; if it does
not, the brief's deeper risk is live — the C8 doc (`CHAR_SKINNING_DEFORM_INVESTIGATION.md:149-158`)
says the animated basis divergence includes a **pose-varying** component (`calcOffset=true` "drifts
as the bone moves away from the rebind pose"), and NO static invBind rebake, however rotation-aware,
can fix a pose-varying basis error — that would escalate to the pose-pipeline itself. Make this
step-0 explicit so BL-A1 doesn't burn an Opus lane re-deriving a landed negative. The tripwire
framing in the kickoff (RB3_BOUND_REBAKE 200-460u = STOP signature; bind-pose-capture is the
distinguishing thesis) is otherwise faithful to the W2.2/WAVE3_REVIEW history.

### A6 (R-C answer, part 2) — BL-A2 spec gaps: pose source and RefSkinVertex availability

(a) The oracle must measure under an **animated pose** (θ>0): at bind pose R·sin(0)=0 and the
far-vertex metric is structurally GREEN — it cannot satisfy the kickoff's own "fail-red on today's
build" requirement. The brief must name the pose source (W0.4-style clip-driven pose or a captured
live gameplay pose) **through the real rb3 band load path** — the Wave-3 review's exact catch
(engine goldens run in the DC3-context suite and never exercise the band-rebind path) applies
verbatim here. (b) "Reusing the W0.1 RefSkinVertex path" needs one word of honesty: `RefSkinVertex`
is a **`static` function inside engine `tests/test_skin_golden.cpp:164`** — rb3-tests cannot link
it. Either duplicate the ~40-line LP64 reference skinner into `native/tests/` (fine, self-contained)
or export it via an engine test header (an engine-repo edit — out of every Wave-7 fence). Say which.

### A7 (R-D answer) — Lanes A and C are genuinely file-disjoint; two caveats to write into the fence

Verified: the floor is exactly at `RB3MaterialBinder.cpp:145-149` today (`if (isLikelyUiText)` at
:145, three `std::max(0.6f, …)` lines); it is a separate TU (W1.3 extraction) whose only coupling to
Lane A is **header includes** (`RB3PostProc.cpp:18` and `Rnd_Wgpu_RB3.cpp:25` include
`RB3MaterialBinder.h`); no shared mutable statics (the binder's statics are function-local caches;
the postproc ceiling reads none of them). Caveats: (i) header edits (`RB3MaterialBinder.h`,
`GameRenderHook.h`) are shared compile surface — additive-only, flag in STATUS; (ii) if W4.2 takes
its option 2 (focus-signal plumbing), the signal originates game-side in `rb3_render_hook.cpp` /
`QueryDrawMaterialPolicy` (`isLikelyUiText = isTextMeshHeur || matPolicy.isUiText`,
`RB3MaterialBinder.cpp:105`) — that file is currently **unowned by any Wave-7 lane**; add it to Lane
C's fence explicitly. Also pin `RB3_HUB_MENU_QUAD_HIDE=OFF` during the W4.2 A/B captures — the
news-ticker readability gate shares the main_hub screen with S3's quad flip, and an interleaved flip
would confound the "both directions" judgment.

### A8 (R-E answer) — Two flip-semantics breaks the A5 sweep missed, plus two minor staleness items

- **`scripts/native/crowd-bone-gate-capture.py` `--no-placement-contract` is silently broken.**
  Lines 83-84: it merely *omits* the `RB3_PLACEMENT_CONTRACT=1` opt-in — post-flip the contract is
  ON anyway, so the "W1-era identity placement" arm (docstring :21) no longer exists. The A5 sweep
  judged this file "non-inverting, harmless" by looking only at the default path. Fix: the option
  must set `RB3_PLACEMENT_CONTRACT_OFF=1`.
- **`scripts/native/placement-gate-capture.py` fail-red contract is inverted.** It sets no contract
  env at all (verified: `env = dict(os.environ)` :159, no contract key) and its docstring gate
  ("On the UNCHANGED … build this is RED", :23; exit-1 = "the current build's fail-red", :36) is now
  backwards — the default build is oracle-GREEN. It escaped the A5 sweep because it never mentions
  the flag name. Anyone re-running the documented `--expect-red` audit post-flip gets a confusing
  result. Fix: docstring + an explicit `--contract {on,off}` (OFF arm → `_OFF=1`).
- Minor: `w21flip-ui-ab.py:13` docstring still says "flag-OFF = no env" (code at :171 is correctly
  inverted); `_w32-boxambient-ab.py`'s Wave-6 OFF/ON scores (`W3.2/captures/scores.json`) were
  captured contract-OFF-by-default — post-flip re-runs are contract-ON on both arms, so **Lane D
  must re-baseline rather than diff against the Wave-6 numbers**.
- Checked clean: `NativeCompatFlags.{gen.inc,classification.json}` rows :223-225/:91-92 already
  carry the post-flip text ("live … default-ON … opt out via _OFF"); ledger :232-234 regenerated
  (uncommitted, → A1); `native/tests/test_placement_oracle.cpp` is env-independent (parses capture
  logs); lineup goldens untouched (correctly — flip passed the lineup gate); no rb3-tests source,
  golden, or doc-as-executable beyond the two scripts above depends on contract-OFF-by-default.

### A9 — Lane D's "Dolphin A/B on 2-3 venues" is NOT cheap as written; scope it in the S1 plan

Every "Dolphin A/B" in this campaign to date compares native captures against **pre-existing static
shots** (`w21flip-dolphin-ab.py:66` hardcodes `c8-ground-truth-2026-07-01/dolphin-shots/gp_00.png`);
there is no harness that drives Dolphin, and the existing shot inventory is char-closeups + one
venue's gameplay — not a 2-3-venue lighting set. Fresh matched-frame venue captures are a manual,
human-gated effort. The S1 plan should choose: (a) decide from existing ground truth
(dolphin-shots + `images/retail-screenshots/`) + the already-measured venue-probe data — likely
sufficient, since W3.2's own STATUS says per-pixel Lambert for point-spots "is arguably *more*
accurate" and option (iii)/(b) is "defensible on current data"; (b) file an explicit human capture
request as the decision's prerequisite; or (c) prototype-first and only then ask for captures. Do
not let a plan-only stage attempt to build a Dolphin harness. (The kickoff's framing of W3.2's
refutation is otherwise faithful: the ZERO-directional finding is **measured** — 23 environs, 27
point + 1 fakespot — while the "point-light gap" itself is an open question, correctly hedged with
"it may be strictly better".)

### A10 — Model tiers: tag them; only W3.2b is tagged today

Recommend (mirrors the Wave-6 pattern): **Opus** — Lane A S2 verify + the WASH matrix
measurement/verdict (statistical discipline, the campaign's highest-value adversarial stages), BL-A2
oracle design, BL-A1 impl; **Sonnet** — W3.3-fix S1 impl (mechanical shader edit against a
byte-verified quoted block), W4.2 S1 impl + capture packaging, hub-quad flip package (S3), the
matrix's boot-farm runner. Lane D S1 stays Opus (decision memo).

---

## Sequencing / fences — residual notes (no further amendments)

- Lane A internal order (W3.3-fix → WASH matrix) is correct: same mechanism space, and the matrix
  needs the fixed build (+ the A4 control arm).
- Lane B oracle-first is correct and is the direct application of W2.8's gate-blindness finding.
- Lane C's fence line is complete once A7(ii) adds `rb3_render_hook.cpp`.
- Lane D: add the one missing fence sentence — worktree/branch only, no mainline engine edits except
  append-only `classification.json` under its flock; rebase `wave6-boxmap-proto` onto post-Lane-A
  HEAD before any measurement that will be compared against Lane-A-built captures.
- The kickoff's "three harnesses already inverted" claim is true in the working tree (verified
  :186-190/:171-174/:260) — but uncommitted, which is A1.
- Backwards-premise scan: none found beyond A2. The four "where we are" complaint attributions match
  their STATUS artifacts (W3.3 grey = composite over-exposure with 3-way flag matrix proof; W4.2
  floor at :145-149 confirmed live today; W2.8 shard mechanism + oracle blindness; wash =
  flip-independent with ranked prior).

---

## Appendix — what I checked in source (all verified 2026-07-06)

| Check | Result |
|---|---|
| `Rnd_Wgpu_RB3.cpp:3064` | `kPlacementContractDefaultOn = 1` ("coordinator flip, Wave 6"); `_OFF` wins at :3067; engine log has `fced18b` then HEAD `1b045d9` |
| `native/CMakeLists.txt:74` | `MILO_ENGINE_PIN` = `8e7eddd…` — **pre-flip, not yet bumped** |
| rb3 `git status` | goldens (splash 792 + residual sidecar), 3 harnesses, ledger, README, REFACTOR_PLAN all **modified-uncommitted**; engine tree carries only the known unrelated `FxSendNative.cpp` edit |
| `splash_screen.json` (working tree) | `count: 792`; sidecar `_comment` documents post-flip refit from N=36 boots |
| `rb3_postproc.wgsl.inc:165-193` | flash-guard comment + per-channel Reinhard block **byte-identical** to the block quoted in `staged-luma-preserving-ceiling.patch`; final `clamp()` at return present; file included only by `RB3PostProc.cpp:118` |
| `staged-luma-preserving-ceiling.patch` | exists; **comment-only proposal**, not git-appliable |
| `RB3MaterialBinder.cpp:105,145-149` | `isLikelyUiText = isTextMeshHeur \|\| matPolicy.isUiText`; floor block exactly at :145-149 |
| Include graph | `RB3MaterialBinder.h` included by `RB3PostProc.cpp:18`, `RB3Quad.cpp:20`, `Rnd_Wgpu_RB3.cpp:25` — header-only coupling, no shared statics |
| `Utl.cpp:192-198` + `git show 76f51077` | W0.3d-fix SortDraws name tie-break **landed** (Wave 5), gated `RB3FixedClockActive() && !RB3DrawSortDeterministicOff()` |
| `w21flip-dolphin-ab.py:186-190`, `w21flip-ui-ab.py:171-174`, `wash-measure.py:260` | OFF arms set `RB3_PLACEMENT_CONTRACT_OFF=1` (inverted, uncommitted); ui-ab docstring :13 stale |
| `crowd-bone-gate-capture.py:83-84,196` | `--no-placement-contract` only omits the opt-in → **broken post-flip** |
| `placement-gate-capture.py:23,36,159` | no contract env anywhere; documented default-RED fail-red contract now inverted |
| Repo-wide `RB3_PLACEMENT_CONTRACT` grep (rb3 + engine, minus review docs) | full hit list triaged above; no test source/golden/doc-as-gate beyond the two scripts flagged |
| `NativeCompatFlags.gen.inc:223-225`, `classification.json:91-92`, ledger :232-234 | post-flip rows live (ledger uncommitted) |
| `CHAR_SKINNING_DEFORM_INVESTIGATION.md:104-158` | kickoff/W2.8 mechanism quotes faithful; `calcOffset=true` pose-drift note underpins A5 |
| `tests/test_skin_golden.cpp:164` (engine) | `RefSkinVertex` is `static` in the engine test TU — not reusable from rb3-tests without duplication/export |
| `test_placement_oracle.cpp` | env-independent (log/JSON-driven) |
| `w21flip-dolphin-ab.py:64-66` | "Dolphin A/B" = comparison against static `gp_00.png`; no Dolphin-driving harness exists in `scripts/native/` |
