# W-ISO PLAN review (Fable, adversarial) — verdict: APPROVE-WITH-AMENDMENTS

**Target:** `execution/W-ISO/PLAN.md` @ rb3 `73610545` (plan commit; anchors claimed at
`fb1f00e5`). **Reviewed against:** OPTIONS.md §6.5 charter (`c4395043`), WAVE19_KICKOFF Lane I
(`d93fa894`), WAVE19_REVIEW A4/A5/A6 + R-A (`fb1f00e5`, binding where it amends the kickoff),
the ten §4 lints, the validation contract, the collision matrix, and the CURRENT tree — every
anchor below re-derived by symbol/measurement, not trusted from the plan.

**VERDICT: APPROVE-WITH-AMENDMENTS.** AM-1..AM-6 are BINDING on the implementer. The plan's
structure (M1 lints-first → guards → N=10 EXIT with fail-red → G3 → flag-OFF) is charter-correct
and its four anchors are exactly right; but M1's F7 threshold derivation cites a provably wrong
exemplar file that would gut the re-grade statistics, M4's PASS criterion is stricter than the
binding contract in a way that can mislabel a charter-satisfying outcome, and R-A iteration 2 is
missing the subtraction rule without which its single bounded round can be burned re-finding the
already-guarded four.

---

## Amendments (ranked by severity)

### AM-1 (HIGH — M1, F7 threshold): the cited exemplar is a BRIGHT frame; derive T from the NEARBLACK row instead

- **Plan claim (M1, `partition_black_frames`):** "Threshold `<T>` derived from the committed
  zero-chroma exemplar `R4-M4/evidence/wr_n10_ON02_WHITE_zerochroma.png` (implementer measures
  its `mean_luma`; pick `T` just above it)".
- **Evidence (measured this review):** that PNG's mean_luma = **198.8/255 ≈ 0.780 normalized**
  (640x360, BT.709 weights). It is the **WHITE wash zero-CHROMA exemplar** (committed
  `wr_n10.json` row `wr_n10_ON_02`: class=WHITE, mean_luma 0.7817, mid_sat 0.0486) — high
  luma, near-zero saturation. It is NOT a luma≈0 frame.
- **Failure if implemented as written:** T ≈ 0.79 excludes **19 of the 20 committed run frames**
  from hi_frac (every frame except ON_02 itself has mean_luma 0.48–0.76) — the F7 "exclusion"
  becomes a near-total population wipe, disclosed but statistically meaningless.
- **The correct fixture:** F2/F7's target frame is `wr_n10_OFF_01` — class **NEARBLACK,
  mean_luma 0.0000, hi_frac 0.00, mid_sat NaN** (committed in `wr_n10.json`; its PNG was not
  committed but the row is). That is the exact frame behind the closeout's **12.43→13.81
  restatement precedent** (`WAVE18_CLOSEOUT_REVIEW.md:184-191`: "exclude luma≈0 frames from
  hi_frac stats (mirroring the mid_sat NaN rule)"; recomputed here: OFF mean over 10 = 12.431,
  over 9 excluding the black boot = 13.81 ✓).
- **Correction (BINDING):** derive `T` from the NEARBLACK row: `T` must sit above 0.0 and far
  below the lowest legitimate committed frame (0.4798 = `wr_n10_OFF_02`). Set **T = 0.05**
  (anything in (0.0, 0.1] is defensible); document BOTH bounds (exemplar luma 0.0, lowest-legit
  0.4798) in the file docstring. The M1 selftest fixture (`[{mean_luma:0.0},{mean_luma:0.5}]`)
  is unchanged and now actually exercises the shipped threshold. Do not touch
  `wr_n10_ON02_WHITE_zerochroma.png` — it is the WHITE-class exemplar for a different finding.

### AM-2 (MEDIUM-HIGH — M4): fix two defects in the EXIT gate spec

**(a) The `== 0` sub-criterion is stricter than the BINDING contract — demote it.**
The contract's EXIT is "eng_hot OFF-arm ledger **stream** axis 10/10". The stream axis passes
on **boot-invariance vs the reference boot**, not on zero:
`loaddet_gate.py:367` — `"stream": {"value": s, "pass": s == ref["postAnchorDelta"]}`.
The plan's PASS adds "AND every boot's `axes.stream.value` (postAnchorDelta) `== 0`". A
residual consumer drawing a **boot-invariant nonzero** count would give 10/10 (charter
satisfied; the Wave-18 VOID precondition — the lane's deliverable — becomes satisfiable) while
failing the plan's gate, mis-triggering an R-A round and potentially ending in a
`HELD-residual-to-T1` label for a result that satisfies the charter. The committed evidence
also can't guarantee 0: the baseline deltas `[16,0,16]` don't arithmetically reconcile with
CharInterest's attributed `[4,5,4]` (boot 2: delta 0 with 5 un-isolated draws attributed —
window/F3 fuzz), so "all-zero after isolation" is a prediction, not a derivation.
**Correction:** gate on `summary.stream == "10/10"` AND `nParsed == 10` (as the contract
states). Record per-boot `axes.stream.value` in the evidence JSON; if it is a nonzero
constant, note it in STATUS as a named, boot-invariant residual — informational, NOT a gate
failure, NOT an R-A trigger.

**(b) The alternate fail-red ("guards present but the seam OFF then attribute") is invalid —
strike it.** Seam-OFF boots emit no `[LOADDET]` anchor/reseed/frame markers at all
(`RB3ReseedGRandAtAnchor` early-returns, Rand.cpp:57-59; the probe markers ride the seam), so
`parse_boot_log` returns `no_anchor_marker` → `nParsed=0` → a broken-harness red, not "the
stream axis fails again". The ONLY valid fail-red is the pre-guard binary reproducing
`stream < 10/10` with real spread. (See AM-5 for the cheap way to get it.)

### AM-3 (MEDIUM — R-A iteration 2): the attribution table still names the ISOLATED four — state the subtraction rule

The attrib tap fires **BEFORE** the redirect in every wrapper
(`RB3_LOADDET_ATTRIB_TAP(); RB3_LOADDET_REDIR(...)`, Rand.cpp:186-212; macro order by design —
WAVE19_REVIEW A2 relies on it so isolated draws stay per-PC-counted). Consequence the plan
never states: in R-A iteration 2, `attribution_table` over the failing boots will list
`charclip`/`crowditer`/`charinterest`/`lightpreset` PCs **with their full draw counts**, plus
R4-M2's five already-isolated sites (Part.cpp:623/:1061, CameraShot.cpp:265, Sequence.cpp:218,
CharEyes.cpp:526) — none of which reach gRand any more. An implementer reading the raw table
will "re-find" the guarded four and can burn the single bounded iteration concluding the
guards don't work. **Correction (BINDING):** iteration 2's residual set = attributed
post-anchor PCs **MINUS the union of all isolated sites (the four new + R4-M2's five)**; the
delta arithmetic compares postAnchorDelta spread against **non-isolated** consumers' per-boot
draws only. Write this into `iso_ledger_gate.py`'s R-A helper or the STATUS procedure verbatim.

### AM-4 (MEDIUM — M2 exit): "--validate still runs green" cannot pass pre-M3 — redefine the exit as mechanical

`--validate` exits 0 **only if ledger-clean** (`white_regrade.py:185-189`:
`clean = nParsed==n AND stream==n/n`, `return 0 if clean else 2`). At M2 the guards don't
exist yet, and the committed eng_hot baseline is stream 2/3 (`postAnchorDelta [16,0,16]`) —
the expected M2-time result is **exit 2 for reasons external to M2's wiring** (a lucky 3/3 at
N=3 is possible, making the criterion flaky in the other direction too). **Correction:** M2's
exit is mechanical: the validate run **completes** (boots captured, ledger printed, JSON
written), the new disclosure fields are present, the emitted JSON round-trips through a strict
`allow_nan=False` parser, and both refusals demonstrably fire (the NaN injection + a
`--refinish`-without-`--validate` invocation refusing). Ledger-cleanliness is M4's business.
Alternatively run the green-check after M3 — but then say so in the commit order.

### AM-5 (MEDIUM — sequencing, §4 lint 10 + cost): produce the PREGUARD RED **before** landing M3

Plan order builds `iso_ledger_gate.py` at M4 and back-derives the RED via a `HEAD~1` worktree
rebuild. Two improvements, the first of which is a lint: **(a)** lint 10 says build the
instrument first and let it grade the diagnosis before the fix — running `iso_ledger_gate.py`
against the **current, pre-guard binary** at N=10 IS the diagnosis grade (does the Wave-18 N=3
attribution reproduce at N=10?) and doubles as `iso_ledger_n10_PREGUARD.json`, with zero extra
builds. **(b)** the worktree route does work (`tools/setup-worktree.sh:236-249` plants the
`.claude/worktrees/milo-native-engine` symlink precisely so `native/CMakeLists.txt:72`'s
`../../milo-native-engine` resolves), but it costs a full native configure+build and is only
needed if the RED wasn't captured pre-M3. **Correction (BINDING order):**
M1 → M2 → **build `iso_ledger_gate.py` + capture PREGUARD RED (N=10, current binary)** → M3 →
M4 GREEN re-run → M5/M6. Keep the worktree fallback for the case where M3 has already landed
when the RED is found missing. Bonus: the PREGUARD run also re-validates ASSUMPTION-B's input
(if N=10 pre-guard attribution names a fifth consumer, guard it IN M3, not in an R-A round —
the iteration budget stays intact).

### AM-6 (LOW — cluster, all binding but small)

1. **Guard comment text (M3 template):** "venue-path M1 divergent" is wrong provenance — M1
   was the DEFAULT-path attribution (Rand.h's own comment names the M1 set); the four new
   consumers were attributed by **R4-M4** on the eng_hot path. Say "eng_hot venue-path
   divergent (R4-M4 attribution), spread <N>". Keeps F9 discipline: this is stream-position
   count variance, not the order axis — the template's existing wording is otherwise fine.
2. **"graded (probe-OFF) boots" is factually wrong:** `seam_env` sets `RB3_BOOTRNG_PROBE=1`
   (`white_regrade.py:71`), so graded boots take the **:286 probe branch**, not :294. This
   *strengthens* A4's function-scope rule (both branches covered — placement conclusion
   unchanged); fix the parenthetical. Side effect to disclose in STATUS: under the guard the
   probe's `gdraw=` field freezes (redirected draws bypass `sGRandDrawCount`, Rand.cpp:122).
   Checked: no grader parses `[BOOTRNG] PRESET ... gdraw=` numerically (wash_cosample reads
   LIGHTVAL valhash only; loaddet_gate reads `[LOADDET]` only) — informational.
3. **M6 has no named evidence artifact** (§4 lint 7). Commit e.g.
   `W-ISO/evidence/m6_default_boot.txt`: health-check output + `grep -c "\[LOADDET\]"` == 0
   on a default-flags boot log. Also retitle M6 "flag-OFF **inert**" — the native binary is
   not byte-identical (the guard ctor call is compiled in); inert behavior is the true claim,
   byte-identity is M5's (Wii) property.
4. **Import-drift hazard, M4:** Lane F concurrently owns `loaddet_gate.py` and is adding axes;
   `iso_ledger_gate.py` imports it live. If F's landing changes the `grade_external_logs`
   return schema mid-lane, do NOT fork-copy the file — pin the incompatibility in
   STATUS/checkpoint for the coordinator (mirrors the plan's own §5 gate-change rule).
5. **`--refinish` becomes dead for full runs after M2** (the validate path never calls
   `rebuild_rows`, so "--refinish --validate still allowed" permits a no-op combination).
   Acceptable per F2 — but make the refusal message say why and what remains allowed
   (crash-recovery row rebuild for non-verdict inspection).
6. **Line-cite nits (non-blocking, symbol anchors already correct):** `off_hi`/`on_hi` are at
   `white_regrade.py:217-218` (plan: :216-217); the `RB3_LOADDET_REDIR` macro block is
   Rand.cpp:173-184 (plan: :176-180); the reseed fn spans Rand.cpp:57-67 (plan: :64-66).

---

## Verified clean (re-derived independently — no action)

- **All four anchors exact at HEAD:** CharClipDriver primary ctor `:10`, draw `:62`, body
  opens `:22`, copy ctor `:73` drawless ✓; `WorldCrowd::OnIterateFrac` `:1216`, Fisher-Yates
  `:1234`, and `:810`/`:812` confirmed in a different method (do-not-guard stands) ✓;
  `CharInterest::ComputeScore` `:124`/`:172` ✓; `LightPresetManager::PickRandomPreset` `:275`,
  probe `:286`, shipping `:294`, both single-draw ✓.
- **ASSUMPTION-A resolves to FACT:** all four TUs `#include "math/Rand.h"` directly
  (CharClipDriver.cpp:3, CharInterest.cpp:3, LightPresetManager.cpp:3, Crowd.cpp:14). Drop the
  fallback clause or mark it moot.
- **§3.3 zero-Rand-edits claim TRUE:** guard struct + four free-fn redirect hooks + lazy tag
  map + generic reseed all present (Rand.cpp:57-93, :173-212; Rand.h:63-80, inside
  `#ifdef HX_NATIVE`). MWCC never sees any of it → G3 safety is structural ✓. A5 honored
  (no Rand.* edit).
- **Lint 9 (flavor membership):** all four TUs in rb3-native's compiled object list
  (`native/build/CMakeFiles/rb3-native.dir/DependInfo.cmake`; world/char GLOBs at
  `native/CMakeLists.txt:246-262`) ✓.
- **M5 unit names exist verbatim in objdiff.json:** `main/system/char/CharClipDriver`,
  `main/system/char/CharInterest`, `main/system/world/Crowd`,
  `main/system/world/LightPresetManager` ✓. Representative-symbol-per-unit is adequate given
  any leak outside `#ifdef` fails the MWCC build itself (batch_objdiff builds first).
- **M4 harness surfaces exist with the claimed shapes:** `capture_arm(binpath, guard_on, n,
  target_ms, overshoot_ms, rawdir, tag)` returning rows with `log` paths;
  `grade(rows)`→`lg.grade_external_logs([r["log"]], K_FRAMES=300)`; `seam_env` carries
  `RB3_LOADDET_ATTRIB=1` ✓; `--validate` N-cap `min(a.n,3)` at :174 ✓; the three `json.dump`
  sites :187/:280/:281-282 ✓; mid_sat NaN rule :218-220 ✓; `attempts` is a discarded local ✓.
- **Draw-count table matches the committed evidence file** (`[8,0,8]/[7,0,7]/[4,5,4]/[1,0,1]`,
  spread 8+7+1=16) ✓ — noting the boot-2 reconciliation fuzz feeding AM-2a.
- **Collision statement** matches A5/A6/the kickoff matrix; no engine writes; no default
  flips; no pin bump; hazard files correctly excluded from staging ✓.
- **ASSUMPTION-C (over-capture) safe as argued:** nesting restores the outer stream
  (Rand.cpp:89-93); over-capture only removes variance from the shared count ✓.
- **ASSUMPTION-D:** `capture_arm`'s `n*4` retry cap handles engagement failures; note N=10 →
  up to 40 boots/arm of wall-clock, budget accordingly.

## Verdict

**APPROVE-WITH-AMENDMENTS** — AM-1 through AM-6 are BINDING on the implementer. No structural
redraft: milestones, ownership boundaries, R-A bounding, and the anchor set all stand. The
plan is dispatchable the moment AM-1 (threshold fixture), AM-2 (gate criteria + fail-red
validity), AM-3 (attribution subtraction rule), AM-4 (M2 exit), and AM-5 (RED-before-M3
ordering) are folded in as written.
