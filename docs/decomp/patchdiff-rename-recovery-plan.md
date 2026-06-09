# Bank 5↔Bank 8 ghidriff divergence — results & what we built (2026-06-09)

**STATUS: ghidriff run completed and validated against real output.** This doc
started as a *plan* for rename recovery; the plan's central premise (that renames
were a big recoverable DWARF pool) **did not survive contact with the data**. This
is the corrected, results-based version. Companion to
[bank-divergence-2026-06-09.md](bank-divergence-2026-06-09.md).

## TL;DR (what actually happened)

- Ran ghidriff (`tools/ghidra/run_ghidriff.sh`, `--no-symbols`, VersionTrackingDiff)
  on Bank 5 ELF vs the synthetic Bank 8 ELF. ~3.5 h. Output: a **2.9 GB** pdiff
  JSON (+ a 2.4 GB matches.json) — far too big for `json.loads`.
- Built `tools/ghidra/distill_ghidriff.py` — streams the pdiff once (ijson/yajl,
  ~18 s) into a small index keyed by the **Bank 8 mangled symbol** (joined by
  *address*, because ghidriff's names are Ghidra's *demangled* display names, not
  the CodeWarrior mangled symbols our maps use).
- **The win: an instruction-level divergence index.** 2,406 functions precisely
  classified MISLEADING (real rewrites — the BandHeadShaper traps), plus CAUTION /
  TRUST / NO_DWARF. Wired into `bank_divergence.py` → `analyze-function`'s banner.
- **The dud: rename recovery.** Raw "renames" looked huge (7,826) but were
  contaminated; only **~162 are port-safe**, and even those have *changed*
  signatures (limited DWARF value). **Do not build the patchdiff tier-4 tooling.**

## The divergence index (the deliverable that matters)

`distill_ghidriff.py` → `build/SZBE69_B8/ghidra/ghidriff/divergence_index.json`
(keyed by Bank 8 mangled symbol), classified m_ratio-led (mnemonic similarity ≈
algorithm/source-intent similarity):

| Verdict | Count | Meaning |
|---|---|---|
| TRUST (changed-but-reliable) | 4,262 | mnemonic stream near-identical |
| CAUTION | 1,025 | some divergence — cross-check m2c/objdiff |
| **MISLEADING** | **2,406** | real rewrite — ignore Bank 5 body, use m2c |
| NO_DWARF | 13,553 | Bank-8-only by structure |
| TRUST (implicit, identical) | ~44k | byte-identical matches, absent from the diff |

**Wired in:** `bank_divergence.py::lookup()` now prefers this index when present,
falling back to the body-size heuristic for symbols absent from it (correctly
TRUST for identical bodies). Refinement: a ghidriff "added" (NO_DWARF) entry whose
mangled name *is* in the Bank 5 map is re-classed **MISLEADING** — Bank 5 DWARF
*types* still port by name (`port_dwarf_types.py`), only the *body* is wrong-era.
This makes `Init__14BandHeadShaperFv` read MISLEADING (the canonical case), not
NO_DWARF. Every `warning_banner()` consumer (incl. `analyze-function`) upgrades
automatically; absent the index, behaviour is unchanged.

Regenerate after a new ghidriff run: `distill_ghidriff.py`. The index lives under
`build/` (gitignored) — regenerable, and the wire-in degrades gracefully without it.

## Why rename recovery is low-yield (validated, not assumed)

ghidriff's name-blind matching pairs a Bank 5 and Bank 8 function structurally; a
"rename" = matched pair with **different mangled symbols by address**. Raw count
looked like 7,826, but two contaminations collapse it:

1. **Cosmetic false-renames.** ghidriff's `diff_type` "fullname" fires on the
   *demangled* name, which differs between the real-DWARF Bank 5 and the synthetic
   Bank 8 ELF *for the same function*. Junk signal — dropped (use mangled-by-address).
2. **Trivial-body byte collisions.** Unrelated tiny functions compile to identical
   bytes (`SetBaseKerning__7RndFontFf → SetFrameMicPitch__6SingerFf`), and
   `ExactBytesFunctionHasher` pairs them. **This is the same trivial-duplicate
   problem that OOM'd Ghidra VT's `applyDuplicateFunctionMatches` step** (the 8 GB
   heap died there; box has 93 GB but the apply is O(n²)).

Robust filter = **base-identifier match** (`Foo__A → Foo__B`, same function, only
the param-mangling suffix changed) **AND body ≥ 16 bytes**:

```
7,826 raw  ->  4,659 true (diff mangled)  ->  174 base-match  ->  162 PORT-SAFE
```

And the irony: a port-safe rename means the *signature changed* (that's why the
mangle differs), so Bank 5's DWARF is the *old* signature — struct/class types come
across, the changed param is stale or skipped. Modest value. ghidriff already
surfaced the pairs (`renames.json`, `port_safe` flag) for free — **no patchdiff
correlator build is warranted.**

## Tools produced

| Tool | Purpose | Needs Ghidra? |
|---|---|---|
| `tools/ghidra/run_ghidriff.sh` | run the Bank5↔Bank8 ghidriff diff (uses the `../ghidriff` editable clone) | yes |
| `tools/ghidra/distill_ghidriff.py` | stream pdiff → `divergence_index.json` + `renames.json` | no (ijson) |
| `scripts/analysis/bank_divergence.py` | per-symbol verdict; **now prefers the ghidriff index** | no |
| `tools/ghidra/measure_rename_recovery.py` | tally how many Bank-8 funcs carry a ported signature (VT recovery state) | yes |
| `tools/ghidra/port_renames.py` | port Bank 5 DWARF onto the ~162 port-safe renamed funcs, by address pair | yes |

## Remaining / optional

- **Port the ~162 port-safe renames** (`port_renames.py`): only param-count-matching
  pairs, skipping already-typed funcs. Modest, optional.
- **`measure_rename_recovery.py`**: confirms what the (OOM-truncated) VT pass typed.

## Gotchas hit (carried into the doc so they don't bite again)

- **VT AutoVersionTrackingTask OOMs** at `applyDuplicateFunctionMatches` (O(n²)
  LinkedList scan + per-compare score-string reparse) on our 41k-function binary.
  More heap delays it; the dupe-function correlator is the hazard. Prefer exact-only
  VT + this ghidriff index over relying on VT's dupe/ref tiers.
- **Gekko Sleigh ldefs gets clobbered.** `PowerPC:BE:32:Gekko_Broadway` lives only
  in a patched `/opt/ghidra/.../ppc.ldefs` (`tools/ghidra/install-gekko-sleigh.sh`,
  needs sudo). The `GhidraXenon` extension fighting over `ppc.ldefs` reverts it to
  stock → "Language not found" on any Gekko program open. Re-run the installer.
- **pyghidra scripts need the service's env**: `GHIDRA_INSTALL_DIR=/opt/ghidra`,
  `JAVA_HOME=java-17`, and a CLEAN `GHIDRA_USER_HOME=/tmp/claude/ghidra_user_rb3`
  (the default `~/.config/ghidra` has GhidraXenon, which breaks Gekko). The scripts
  here set these via `os.environ.setdefault`.
- **ghidriff names are demangled** — join to mangled symbols by `address`, not name.
- **Don't parse `matches.json` (2.4 GB)** — the 11M `BulkBasicBlockMnemonicHash`
  pairs are noise; the pdiff's modified/added/deleted is all you need.
