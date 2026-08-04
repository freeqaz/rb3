# `ninja -n` returns a FALSE NEGATIVE here — use `tools/ninja-dry`

**Investigated and fixed 2026-08-04.** This is the *fifth* distinct way this repo
can hand back a convincing-but-wrong "nothing changed" reading, after the
absolute-path depfiles ([worktree-setup.md](worktree-setup.md#header-edits-in-a-worktree--fixed-2026-08-04-was-a-silent-false-negative)),
the objdiff normalized masks, the missing `base_path`, and the stale batch DB.

## Do this

```bash
tools/ninja-dry                  # would ANYTHING rebuild?      exit 0 clean / 1 pending / 2 can't-answer
tools/ninja-dry -q src/...       # a specific target, verdict line only
tools/tests/test-ninja-dry.sh    # regression test (hermetic, <2s, does not touch the RB3 build)
```

Do **not** use bare `ninja -n` to decide whether an edit invalidated anything.
It is not merely noisy — under a condition that occurs routinely it reports
success while hundreds of translation units are dirty.

---

## The defect

`ninja -n` rebuilds the generated manifest before it looks at anything else,
and in dry-run mode it then **deliberately gives up**:

```c
/* ninja v1.13.2, src/ninja.cc, NinjaMain::main loop */
if (ninja.RebuildManifest(options.input_file, &err, status)) {
  // In dry_run mode the regeneration will succeed without changing the
  // manifest forever. Better to return immediately.
  if (config.dry_run)
    exit(0);
  // Start the build over with the new manifest.
  continue;
}
```

That is the whole mechanism. Of the four candidate explanations put to this
investigation:

| candidate | verdict |
|---|---|
| (a) ninja re-execs itself and loses the dry-run state | **partly** — this is the *non*-dry-run path (`continue`). Dry run never gets there. |
| (b) the `generator = 1` marking makes `-n` skip dependents | **no.** `generator` only exempts an edge from the command-hash dirty check. |
| (c) the manifest is genuinely rewritten so a second pass sees a fresh graph | **no.** In dry-run nothing is executed, so build.ninja is never rewritten. |
| (d) something else | **yes — an explicit, intentional `exit(0)`.** |

Ninja's reasoning is sound in isolation: a dry run cannot actually regenerate
the manifest, so restarting would loop forever. The cost is that the caller gets
**exit status 0 and no error**, which is byte-for-byte what a genuine "nothing to
do" looks like.

### Why repeating the check doesn't rescue you

A dry run never runs the generator command and never writes `.ninja_log`. So the
manifest stays exactly as stale as it was, and **every subsequent `ninja -n`
returns the same empty answer**. From the caller's seat the generator edge *is*
permanently dirty. That is what makes this indistinguishable from a real
negative and gets it recorded as one.

Only a **real** ninja run clears it.

### But the edge is NOT "perpetually dirty" in the manifest's own terms

The working hypothesis handed to this lane was that `configure.py`'s edge is
inherently, permanently dirty. **That is refuted.** Measured: one real
`ninja build.ninja` settles it, and it stays settled across repeated runs.

The edge's declared inputs are honest — it lists exactly what `configure.py`
reads:

```ninja
rule configure
  command = $python configure.py $configure_args
  description = RUN configure.py
  generator = 1
build build.ninja: configure | build/SZBE69_B8/config.json configure.py $
    tools/project.py tools/ninja_syntax.py config/SZBE69_B8/config.json $
    config/SZBE69_B8/objects.json
```

so there is no "make the generator honestly clean" fix to apply — it already is.
The problem is that those inputs change *often*:

* `config/SZBE69_B8/objects.json` is edited by ordinary decomp work (marking a
  file matched), and `configure.py` / `tools/project.py` by build-tooling work;
* a **fresh worktree** starts stale by construction (see below).

One further detail worth knowing, because it defeats the folk model of ninja
staleness: since **ninja ≥1.11 the `.ninja_log` records the command's
*completion time*, not the output's mtime**, and that recorded value is what is
compared against inputs. So an edge whose command does not touch its output
(here: `download_tool.py` no-ops when the tool already exists) still goes clean
after one run. Constructing a *genuinely* non-converging generator needs an
input with a **future** mtime — ninja's own diagnostic for that state is
`manifest 'build.ninja' still dirty after 100 tries, perhaps system time is not
set`. `tools/tests/test-ninja-dry.sh` T4 builds exactly that case.

### The fresh-worktree instance is the one that burned a lane

`tools/setup-worktree.sh` ends by running `configure.py`, which *writes*
build.ninja — but writing it does not satisfy the edge. Ninja compares its
**recorded** mtime against the edge's inputs, and a fresh worktree pairs
git-stamped (`now`) copies of `configure.py`, `tools/project.py`,
`tools/ninja_syntax.py` and `config/<VER>/*.json` with a `.ninja_log` seeded
from main carrying *older* recorded times — or, with `--cold-cache`, no log at
all.

Measured A/B, same `--cold-cache` conditions, same base tree:

```
setup-worktree.sh BEFORE:   ninja -n -> 3 edges, exit 0
                            [1/3] TOOL build/tools/dtk
                            [2/3] SPLIT config/SZBE69_B8/config.yml
                            [3/3] RUN configure.py
                            ...while 1310 edges were genuinely pending.

setup-worktree.sh AFTER:    ninja -n -> 1310 edges, no truncation.
```

Measurement lanes live in fresh worktrees. The very first "did my edit
invalidate anything?" they ask is the one guaranteed to lie.

---

## Control matrix (real RB3 build, worktree `wt-ninjadry`, 2026-08-04)

Denominator: **1310 edges** in the default-target closure (`ninja -t commands |
wc -l`). `src/system/obj/Object.h` is named by **724 of 1616** depfiles.
Every row was measured from a fully-converged tree and restored afterwards.

| # | edit | raw `ninja -n` | truth | `tools/ninja-dry` |
|---|---|---|---|---|
| A | nothing | 0 edges, exit 0 | nothing pending | `CLEAN 0/1310` (exit 0) ✓ |
| B | header `Object.h` (724 TUs) | 727 edges, exit 0 | 727 | `PENDING 727/1310` (exit 1) ✓ |
| C | single `.cpp` (`UILabel.cpp`) | 4 edges, exit 0 | 1 MWCC + REPORT + PROGRESS + SYNC | `PENDING 4/1310` (exit 1) ✓ |
| D | `configure.py` | 1 edge (`RUN configure.py`), exit 0 | manifest + the PROGRESS edge (which really does depend on configure.py) | `PENDING 1/1310` (exit 1) ✓ |
| E | `objects.json` | 1 edge (`RUN configure.py`), exit 0 | manifest only | `CLEAN 0/1310` (exit 0) ✓ |
| **F** | **`objects.json` + header** | **1 edge, exit 0** | **727 edges — 724 TUs** | **`PENDING 727/1310` (exit 1)** ✓ |
| **G** | **`objects.json` + single `.cpp`** | **1 edge, exit 0** | **4 edges** | **`PENDING 4/1310` (exit 1)** ✓ |

**Rows E, F and G produce identical raw output and identical exit codes**, yet
row E has no work and rows F/G have 727 and 4 edges of real work. There is no
way to tell them apart from what `ninja -n` prints. `tools/ninja-locked` used
to make it worse by appending a 40-line progress dashboard to every dry run,
burying the one line that was there; that is now suppressed for `-n`/`--dry-run`.

Row C's "4" is correct, not slop: the 3 extra edges are the REPORT / PROGRESS /
`SYNC decomp.db` tail that every source change drags along. Row D's "1" is the
PROGRESS edge, which genuinely lists `configure.py` as an input.

### The check is shown to FAIL, not just to pass

`tools/tests/test-ninja-dry.sh` — hermetic scratch ninja project, 18 assertions:

| test | asserts |
|---|---|
| T1 | clean tree → `CLEAN`, exit 0, denominator printed |
| T2 | one dirty input → `PENDING 1/2`, exit 1 |
| T3 | stale manifest + 2 dirty edges → raw `ninja -n` **hides them and exits 0** (the defect, asserted as still present), while ninja-dry says `PENDING 2/2` |
| T4 | **perpetually dirty generator (future mtime) → ninja-dry exits 2 `FAILED` and does NOT say `CLEAN`** |
| T5 | manifest with no generator edge → normal verdict, not a spurious FAILED |
| T6 | `ninja-dry` and `ninja-locked` still derive the same lock path |

T4 is the point. It went through two wrong constructions first — `touch trigger`
before the manifest write, then after it — **both of which converged**, making
the test vacuously green while a real hole sat in the tool. Only a future mtime
defeats ninja's recorded-completion-time scheme. T3 is deliberately written to
fail loudly if a future ninja fixes the underlying behaviour, so the tool gets
re-evaluated rather than carried forever.

---

## What was implemented

1. **`tools/ninja-dry`** — materialise the manifest for real (`ninja
   build.ninja`; 0.13s when current, 4.7s worst case), **assert** convergence
   via ninja's own `no work to do` sentinel with a bounded retry, run the dry
   run, **re-assert** convergence, then report a verdict *with a denominator*.
   Never reports an empty result as a pass; exits 2 without a verdict when it
   cannot answer. Takes the same per-build-dir `flock` as `ninja-locked`
   (phase 1 is a real build, so it must be serialised). **Not read-only** — that
   is unavoidable: while the manifest is stale ninja's in-memory graph is the
   *old* graph, so no answer derived from it is trustworthy anyway.
2. **`tools/ninja-locked`** — one-line change: `-n` / `--dry-run` joins the
   subtool list that skips the trailing progress dashboard.
3. **`tools/setup-worktree.sh`** — runs `ninja build.ninja` after `configure.py`
   so a new worktree is dry-runnable from the start.
4. **`tools/tests/test-ninja-dry.sh`** — the regression test above.

Lock isolation was re-verified after the changes: holding
`<worktree>/.ninja-build.lock` blocks both `ninja-dry` and `ninja-locked -n`
from the worktree root *and* from a subdirectory of it, while a run against an
unrelated build dir proceeds immediately. T6 guards the deliberate duplication
of the 19-line derivation block (`ninja-dry` cannot simply call `ninja-locked` —
nesting the `flock` on the same file deadlocks; and `ninja-locked` is on the
whole fleet's hot path, so making it `source` a helper adds a failure mode for
19 lines of savings).

## What was deliberately NOT done

* **No reintroduction of `deps = "gcc"`.** `-t deps` / `-t missingdeps` are the
  ninja-native way to interrogate dependencies, and both are dead ends here
  *because* this repo keeps `.ninja_deps` empty on purpose — the binary deps
  cache is what produced the rebuild-everything failure mode, and its removal is
  also what makes the repo-relative depfiles load-bearing (see CLAUDE.md
  "Concurrent Builds"). Do not "fix" the deps log to make a subtool work.
* **No attempt to make the generator edge "honestly clean".** It already is; its
  declared inputs match what `configure.py` reads. Suppressing the edge would
  trade a false negative for a wrong build.
* **No patch to ninja.** The `exit(0)` is intentional upstream behaviour with a
  real justification. Wrapping it is the correct layer.
* **No change to any other ninja invocation.** `tools/ninja-locked` remains the
  only way to build.

### Which `-t` subtools are and aren't affected

All `-t` subtools dispatch at `RUN_AFTER_FLAGS` / `RUN_AFTER_LOAD` /
`RUN_AFTER_LOGS`, every one of which is **before** the manifest-rebuild attempt
— so subtools are immune to the early exit. None of them answers "what is
dirty", though: `targets`, `rules`, `commands` and `graph` ignore dirtiness
entirely, and `deps` / `missingdeps` read the empty deps log. `-t commands` is
still useful as an honest **denominator** — it enumerates every edge in the
requested closure, dirty or not — and that is what `ninja-dry` uses.

## Existing worktrees

`tools/ninja-dry` works in a worktree created before this fix — verified
`PENDING 1310/1310` in a pre-fix `--cold-cache` worktree. **No worktree needs
recreating.** If you prefer to repair one permanently:

```bash
cd <worktree>
tools/ninja-locked build.ninja     # one real run settles the generator edge
```

## Zero-cost sanity check

If `ninja -n` prints `RUN configure.py` — with or without `TOOL`/`SPLIT` above
it — **the answer you are reading is void**, regardless of the exit code.
Re-ask with `tools/ninja-dry`.

## Related false negatives in this repo

* [worktree-setup.md](worktree-setup.md) — reflinked depfiles named main's
  headers, so a header edit in a worktree rebuilt nothing. Fixed 2026-08-04.
  **This lane's results depend on that fix**: rows B and F above only produce
  727 edges because the worktree's depfiles were normalized first.
* objdiff normalized masks / missing `base_path` → per-function 0% and 0-function
  units that look like regressions.
