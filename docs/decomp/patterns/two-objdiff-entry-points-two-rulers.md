# `objdiff-cli diff` and `objdiff-cli report generate` were two different rulers

**Repo: rb3** (Rock Band 3, Wii, MetroWerks/PowerPC, `SZBE69_B8`). Numbers below
are this binary's. The same defect exists in `../rb3-xenon` and `../dc3-decomp`
with different populations — do not quote one repo's figure against another's
binary.

**Status:** cause found upstream, fixed in project config 2026-08-31, guarded by
`scripts/verify_ruler_agreement.py` and by a ninja edge gating `REPORT`.
Read this before comparing any per-function number against `report.json`.

⚠ **The finding was expected to weaken here and did not.** It was first measured
on MSVC/Xbox 360 targets, and `ppc.calculatePoolRelocations` was the obvious
candidate for "does not transfer to mwcc". It transfers in full: this repo has
the **largest byte population of the three** — 151 functions / **224,892 bytes**,
against dc3's 120,728 B and rb3-xenon's 55,604 B.

## The mechanism

The two CLI entry points carry **different hardcoded base configs**, and neither
is the schema default:

| | `report generate`<br>`objdiff-cli/src/cmd/report.rs:581` | `diff`<br>`objdiff-cli/src/cmd/diff.rs:1070`<br>(and `--batch` at `diff.rs:1807`) |
|---|---|---|
| `functionRelocDiffs` | `none` | `data_value` |
| `combineDataSections` | **true** | false (schema default) |
| `combineTextSections` | **true** | false (schema default) |
| `ppc.calculatePoolRelocations` | **false** | **true** (schema default) |

Both then layer `objdiff.json`'s `options` block on top. Since `ca01cdbfd`
(2026-08-12) that block set only `functionRelocDiffs`, so it fixed the ruler the
two paths argue about most visibly and left them disagreeing on the other three.

`ppc.calculatePoolRelocations` is the one that bites. It **synthesizes**
`R_PPC_NONE` relocations for pooled data loads —
`objdiff-core/src/arch/ppc/mod.rs:819 make_fake_pool_reloc`, reached from
`objdiff-core/src/obj/read.rs:708` — and the config schema calls them *"fake
relocations"* in as many words. They are reconstructed per object by walking that
object's control flow and looking the computed address up **in that object's own
symbol table**. A dtk-carved *target* obj (a whole linked data section, anonymous
`lbl_*` labels) and our per-TU mwcc *base* obj do not reconstruct the same set.

`reloc_eq` then charges the asymmetry:

```rust
// objdiff-core/src/diff/code.rs:1330-1338
(None, Some(_)) => return relax_reloc_diffs || name_check,   // base-only: forgiven
(None, None)    => return true,
_               => return false,                              // TARGET-only: CHARGED
```

A relocation present on one side and absent on the other is charged under
**every** `functionRelocDiffs` mode except `none` — `name_check` included. So a
synthesized *display annotation* that only one side reconstructs costs a real
point, and the charged row can be two **textually identical** instructions.

This is **upstream objdiff behaviour, not a fork bug**: the three extra
report-side values arrive in `0c9e552 "Combine sections when generating report"`
(Luke Street, 2025-05-07), which touched `report.rs` only. `bin/objdiff-cli` is
a symlink shared with `../rb3-xenon` and `../dc3-decomp`, so all three repos were
exposed, and the fix is config-only in each — **no tool rebuild**.

## Scope on this binary: 151 functions, 224,892 bytes

Whole-binary sweep, rb3 worktree at `f819f72ae`, full `./tools/ninja-locked`
completed before reading `report.json`, one objdiff-cli **4.2.8**
(`358c715835cc`, xxh3 `9b2bb6f1f3a21062`), `diff --batch` over every
uniquely-named function in the report:

* **comparable rows** (a real percent on both sides): **34,268**
* **disagreements attributable to the config split: 151 (224,892 bytes)**
* direction: `report` higher on **151**, `diff` higher on **0**
* magnitude: up to **7.50 pp**
* **2** of them (284 B) read exactly 100.0 in `report.json` and <100 through
  `diff` — the class where a lane refuses a promotion for a reason that does not
  exist:
  * `AddSongData__11BandSongMgrFP9DataArrayP10DataLoader11ContentLocT` (268 B),
    report 100.0 / diff 99.10448
  * `__VISetGamma1_0` (16 B), report 100.0 / diff 98.75

Not disagreement, and not counted as either:

* **6,949 unpaired rows** — `diff --batch` returns `null`, the report returns
  `0.0`. Both say "no base symbol"; that is agreement.
* **109 rows carrying `base_unit`** — the batch path's *disclosed* cross-unit
  COMDAT fallback. The report scores per-unit only; those two numbers answer
  different questions.

### Attribution

| config applied to `diff` | disagreements |
|---|---|
| as configured (only `functionRelocDiffs` pinned) | **151** |
| `+ ppc.calculatePoolRelocations=false` alone | **0** |
| `+ combineDataSections/combineTextSections=true` alone | **151** |
| all four pinned | **0** |

`ppc.calculatePoolRelocations` alone explains 151/151; the two `combine*` keys
explain none *here*. They are pinned anyway, because they are not inert in
general: on the sibling rb3-xenon tree, applying them **without** the pool key
adds two fresh disagreements. Pin all four together.

## The fix

One project-config change, in `tools/upstream/project.py`'s `options` block —
**both** CLI entry points layer it:

```python
"options": {
    "functionRelocDiffs": "name_check",
    "combineDataSections": True,
    "combineTextSections": True,
    "ppc.calculatePoolRelocations": False,
},
```

**It changes no recorded number.** Same worktree, full build before and after:

| | matched_functions | matched_code | matched_code_percent | fuzzy_match_percent |
|---|---|---|---|---|
| before | 31,942 | 7,219,476 | 63.155643 | 81.89174 |
| after  | 31,942 | 7,219,476 | 63.155643 | 81.89174 |

objdiff certifies this independently: the post-change `REPORT` run logged
**`Report cache: 1876 hits, 0 misses`**, and that cache key covers both the
object bytes and *the resolved config*. Zero misses across a full 1,314-edge
recompile means neither moved — which they must not, since the three values we
added are the ones `report generate` already hardcoded. The whole-binary re-sweep
after the change: **0** disagreements (34,268 examined, 109 `base_unit`, 6,949
unpaired).

## Consequences

* **The headline is not overstated by this.** The report path was never the lower
  of the two on any of 34,268 comparable functions. Nothing `report.json` counts
  as matched was being forgiven here.
* **Per-function readings below the headline were LOW**, on 151 functions
  totalling 224,892 bytes. Any AT_LIMIT reasoning taken over those row sets was
  taken over phantom rows.

## Why the pre-existing helper did not close it

`scripts/sync_objdiff.py` and `scripts/analysis/reclassify_at_limit.py` both pass
`-c functionRelocDiffs=none` explicitly, so they are on a *third* ruler and are
out of scope here. What was missing is that nothing made a bare
`bin/objdiff-cli diff` — the command in half the plan docs under `docs/plans/` —
agree with the grader. `objdiff.json`'s `options` block is the one place *both*
CLI entry points read unconditionally, so pinning there covers the callers a
Python helper cannot reach.

## The guard

```
python3 scripts/verify_ruler_agreement.py --check      # ~0.2 s config-pin assertion
python3 scripts/verify_ruler_agreement.py --selftest   # ~20 s, with negative control
```

`--check` reads the effective config out of `report.json`'s own
`provenance.diff_config` (authoritative by construction: it is not a description
of the config, it *is* the config the score was taken under) and asserts each
divergent key is pinned in `objdiff.json`.

`--selftest` re-runs the end-to-end comparison with
`-c ppc.calculatePoolRelocations=true`, restoring `diff`'s own default, and
**requires** that to produce disagreements. If it does not, it exits **5**
("vacuous"), names the rotted witness set, and tells you to re-derive with
`--all` — it does not report success from a probe that examined nothing.
Measured on the fixed tree: **1,095 witness functions, 1,095 agree as configured,
46 disagree under the control flip**.

**Wired into the build.** `tools/upstream/project.py` emits a `CHECK RULER
AGREEMENT` edge whose stamp is an implicit input of `REPORT`, so a regenerated
`objdiff.json` that lost the pins stops the build instead of silently drifting.
Verified by deleting one pin from `objdiff.json` and building: `ninja: build
stopped: subcommand failed`, exit 1, and `REPORT` never ran.

The edge uses `--check --pins-only`, which deliberately skips the `report.json`
cross-check. That cross-check is the stronger assertion, but it is legitimately
false for exactly one build — the first one after a deliberate ruler change,
whose job is to replace the report it would be checked against. Gating `REPORT`
on it would deadlock. The pins have no legitimate transient, so that is what the
build asserts; the cross-check stays a manual step.
