# T3 Verification — BSim patch deploy + runner hardening (2026-06-10)

**Verifier:** Fable (adversarial).
**Claim under test:** `task-T3-impl.md` ("COMPLETE — all deliverables landed and verified offline").
**Verdict: PARTIAL** — patch deployment + jar-swap + `--no-decomp-correlate` + fork-default
GHIDRA_INSTALL_DIR are all CONFIRMED and reachable from the runner, but the third named
deliverable ("BSim-ON guidance") is functionally broken: the documented recommended
invocation (`RB3_XENON_BSIM=1`) **crashes ghidriff at argparse** (proven by replaying the
exact CMD through the real parser), and the new comment's toggle semantics are inverted
(unset = OFF, not ON; `=0` = ON-then-crash, not off). The impl doc's load-bearing claim
"the extra `1` argument is ignored by ghidriff" is **refuted by direct test**.

---

## 1. CONFIRMED — Ghidra fork branch `bsim-xenon-patches`

Checked in `/home/free/code/milohax/ghidra`:

- `git log --oneline master..bsim-xenon-patches` → exactly the 2 claimed commits:
  `1220f13915` (topk) and `eebbd8ba3a` (parallel-agg). Branch base = master tip
  `9434f1c110`; master untouched.
- `git diff --stat master..bsim-xenon-patches` → exactly 3 files
  (BSimProgramCorrelatorMatching.java 205±, BinningSystem.java 44±, FunctionNode.java 19±),
  259 insertions / 9 deletions — matches the impl doc. The "4 expected files" in the
  brief was a miscount (BSimProgramCorrelatorMatching is touched by both patches); the
  implementer's explanation is correct.

**Manual-application fidelity (the impl doc's caveat 3) — VERIFIED, not taken on faith:**

- Reproduced the conflict in a throwaway worktree (`git worktree add --detach
  /tmp/bsim-verify-wt master`, removed afterwards): `git apply --check patch1 patch2`
  passes (checks each against pristine tree) but actually applying both FAILS on
  BSimProgramCorrelatorMatching.java hunk context; even GNU `patch -p1` (fuzz-tolerant)
  fails hunk #2 after topk. So manual application was genuinely necessary — the impl
  doc's account is accurate.
- Equivalence proven by payload comparison: the sorted multiset of `+`/`-` lines of
  `git diff master..1220f13915` is **byte-identical** to
  `rb3/scripts/ghidra/bsim-topk-cap.patch`, and of `git diff 1220f13915..eebbd8ba3a` to
  `bsim-parallel-aggregation.patch` (`diff /tmp/patch2.lines /tmp/commit2.lines` → empty).
- Structural spot-check of the branch file: serial `addAssociate` loop is gone; the only
  `addAssociate` calls are inside `NodeAssociateWork.apply()`; Phase A grouping
  (`pairsBySource` computeIfAbsent) at lines ~311-331, Phase B ConcurrentQ machinery at
  ~334-380, inner classes `NodeAssociateWork`/`AggregationCallback` at ~394/419.
- BinningSystem.java and FunctionNode.java on the branch are byte-identical to a clean
  combined-patch application.
- Working tree for tracked files is clean on the branch (only pre-existing untracked
  junk: commit-message.txt, pr-body.md, test-logs*.txt — not from this task).

## 2. CONFIRMED — jar-swap, and it is REACHABLE from the runner

Jar: `/home/free/code/milohax/ghidra/build/ghidra-dist/ghidra_12.2_DEV/Ghidra/Features/VersionTrackingBSim/lib/VersionTrackingBSim.jar`

- `.orig` backup exists (54,529 bytes, as claimed); patched jar = 52,532 bytes (smaller is
  fine — recompiled classes, likely no debug info).
- `jar tf` shows the new inner classes: `$AggregationCallback`, `$NodeAssociateWork`, `$1`
  (PAIR_RANK), `$2`, plus pre-existing `$MatchingCallback`.
- Entry-set diff orig-vs-new: the new jar is a **strict superset** (only the 3 new inner
  classes added; nothing dropped) — `jar uf` did not lose entries.
- The classes are genuinely the patched ones, not stale: `javap -p` on the in-jar
  `BSimProgramCorrelatorMatching.class` shows `MAX_PAIRS_PER_QUERY` + `PAIR_RANK`
  Comparator; `BinningSystem.class` shows `SAFETY_MAX_LOOKUP_CANDIDATES` +
  `AtomicInteger lookupSafetyCapCount` + `getLookupSafetyCapCount()`. The main class
  bytewise differs from `.orig`'s.
- Class file `major version: 65` (Java 21) — matches the dist's own
  `application.java.compiler=21`. Consistent.
- **Reachability chain verified:** runner default `GHIDRA_INSTALL_DIR=
  /home/free/code/milohax/ghidra/build/ghidra` is a symlink →
  `ghidra-dist/ghidra_12.2_DEV` (readlink confirmed) = the dist that received the swap;
  and `ghidriff/ghidriff/bsim.py:32` imports `ghidra.feature.vt.api.BSimProgramCorrelator*`,
  which lives in exactly this jar. The patches WILL be exercised by `--bsim` runs.

## 3. CONFIRMED — runner hardening commits + mechanics

- rb3 commits exist as claimed: `d53240b8` (only `tools/ghidra/run_ghidriff_xenon.sh`,
  28+/13-) and `20402177` (only the impl doc). Focused staging — git-safety respected.
- `bash -n tools/ghidra/run_ghidriff_xenon.sh` → exit 0.
- Default flipped at line 142: `GHIDRA_INSTALL_DIR="${GHIDRA_INSTALL_DIR:-/home/free/code/milohax/ghidra/build/ghidra}"`.
- `--no-decomp-correlate` present in CMD (line 246) and confirmed in every dry-run
  variant I ran; the installed venv ghidriff (`editable from /home/free/code/milohax/ghidriff`,
  branch rb3-improvements) knows the flag (`--help` shows
  `[--decomp-correlate | --no-decomp-correlate]`, 5 hits).

## 4. REFUTED — the BSim-ON guidance (lines 221-240 of the script)

The CMD array still uses the pre-existing pair of expansions (lines 239-240):

```bash
${RB3_XENON_BSIM:+--bsim}
${RB3_XENON_BSIM:---no-bsim}
```

Measured shell semantics (tested directly):

| Env | CMD words | Actual effect |
|---|---|---|
| unset | `--no-bsim` | BSim **OFF, silently** |
| `RB3_XENON_BSIM=1` | `--bsim 1` | **argparse crash** (see below) |
| `RB3_XENON_BSIM=0` | `--bsim 0` | attempts **ON**, then same crash |

**(a) The stray arg is NOT ignored — it crashes ghidriff.** I replayed the exact CMD tail
through the venv's real parser (`ghidriff.parser.get_parser()` +
`GhidraDiffEngine.add_ghidra_args_to_parser`; `parser.py:23-25` defines positionals
`old` nargs=1, `new` append nargs='+'):

```
-: error: unrecognized arguments: 1
ARGPARSE EXITED: 2
```

So the impl doc's claim "the extra `1` argument is ignored by ghidriff (unknown
positional arg parsing)" is **false**, and the "Recommended full invocation" written into
the script comment (lines 234-238), the impl doc §"Recommended Next Run", and PLAN.md §6
(`RB3_XENON_BSIM=1 ./tools/ghidra/run_ghidriff_xenon.sh`) will abort the gated human run
at startup with exit 2. (Fail-fast at least — but the deliverable "BSim ON for the next
run" is not achievable through the documented interface.)

**(b) The new comment's semantics are inverted.** Lines 221-233 now claim "default ON,
… leave unset (default on) … RB3_XENON_BSIM=0 (force off via the else branch below)".
All three statements contradict the measured behavior in the table above. The worst
failure mode: a human follows "leave unset", the run completes WITHOUT BSim, and T3's
whole point — the first measured BSim precision — is silently missed. (The OLD comment,
"BSim default OFF … re-enable with RB3_XENON_BSIM=1", at least matched the unset
behavior; the new text makes the doc-vs-behavior gap strictly worse on exactly the knob
this task was about.)

The implementer's "pre-existing bug, out of scope" defense is half-true: the expansion
lines are pre-existing, but the task explicitly was to "revise the RB3_XENON_BSIM comment
… recommend ON … document the full recommended invocation" — recommending an invocation
that crashes, with an untested "ghidriff ignores it" justification, fails that deliverable.

**Minimal fix (one hunk, before the CMD array):**

```bash
BSIM_FLAG="--bsim"
[[ "${RB3_XENON_BSIM:-1}" == "0" ]] && BSIM_FLAG="--no-bsim"
```

then replace lines 239-240 with `"${BSIM_FLAG}"`. That makes unset/1 = ON and 0 = OFF,
matching the new comment as written.

## 5. Minor notes (not verdict-driving)

- **JAVA_HOME default is dead text, not a bug here:** the script (line 143) and PLAN §6
  recommend `java-17-openjdk`, but the fork dist declares `application.java.min=21`.
  This does NOT break on this machine: pyghidra 3.1.0's launcher (`launcher.py:309-331`)
  prefers `java` from PATH (system java = 26) and resolves the actual JDK via
  LaunchSupport against the dist's requirements, so JAVA_HOME=17 is never used. Worth
  cleaning up alongside the BSim fix to avoid confusing a future human; on a machine
  whose PATH java is <21 it WOULD matter.
- The ghidra repo's working tree is left checked out on `bsim-xenon-patches` (not
  master). Allowed by the brief ("do not touch master" = no commits to master, which
  held), but agents building the dist later should be aware.
- Jar-swap is disk-only (not in git) — impl doc caveat 5 correctly documents the
  rebuild-overwrites-it hazard and rollback (`cp "${JAR}.orig" "${JAR}"` — verified the
  .orig exists).
- BSim precision remains unmeasured (impl caveat 4) — correct, and inherent to the
  no-run constraint; nothing to refute.

## Verdict

**PARTIAL.** Two of the three deliverables are confirmed end-to-end with independent
evidence (patches on branch = byte-equivalent payload to the patch files + correctly
jar-swapped + reachable from the runner default; `--no-decomp-correlate` + fork
GHIDRA_INSTALL_DIR landed and dry-run-verified). The third deliverable — BSim-ON
guidance — is refuted as shipped: the documented recommended invocation exits 2 at
argparse (impl doc's "ignored by ghidriff" claim disproven by direct parser replay), and
the rewritten comment describes toggle semantics that are the opposite of the script's
actual behavior, creating a silent-BSim-OFF trap for the gated human run.

## For the next agent

- **Required fix before the gated run:** replace `run_ghidriff_xenon.sh` lines 239-240
  with a real conditional (see §4 fix) and re-align the comment; optionally drop the
  java-17 JAVA_HOME default (dist min is 21; pyghidra resolves via PATH/LaunchSupport
  anyway). One-hunk change; re-verify with the three dry-run env states in §4's table
  plus the parser replay snippet in this doc.
- T5/T4 owners touching the runner: the impl doc's "For the Next Agent" already flags
  the stray-arg issue as LOW severity — treat it as **REQUIRED** instead (it is a
  startup crash, not an ignored arg).
- Everything in §1-§3 can be trusted without re-checking; commands used are inline above.
- Artifacts left for inspection: /tmp/commit1.lines, /tmp/commit2.lines,
  /tmp/patch1.lines, /tmp/patch2.lines, /tmp/branch_BSPCM.java (throwaway worktree
  removed).
