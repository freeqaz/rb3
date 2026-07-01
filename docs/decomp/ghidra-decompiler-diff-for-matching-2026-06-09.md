# Using Ghidra (and m2c) as a *semantic diff* to match functions faster — findings + plan (2026-06-09)

**Question (from jw):** is there a way to use Ghidra as something like objdiff to help us
*match* functions faster during the decomp loop?

**Short answer:** Yes — but not as an objdiff replacement. objdiff is already optimal at
what it does (asm‑exact "did I match"). The high‑value move is a **decompiler‑level semantic
diff of *your build* vs *the target***, used as a **triage classifier**:

> *Is this sub‑100% function logically identical (→ permuter‑class: stop hand‑reasoning,
> send to permuter or mark at‑limit) or actually different (→ a real source bug, and here's
> where)?*

That verdict is the determination that currently burns the most time in our at‑limit notes.
And we already own ~90% of the infrastructure to produce it — it's just pointed at the wrong
binary pair.

---

## 1. The reframe: two different diff axes (we've only used one for matching)

| Tool | What it diffs | Granularity | Purpose |
|---|---|---|---|
| **objdiff** | your build `.o` ↔ **target** | asm‑exact instruction LCS | "did I match" — the inner loop |
| **Our whole Ghidra stack** (`run_version_tracking.sh`, `patchdiff_vt.py`, `run_ghidriff.sh`) | **Bank 5 ↔ Bank 8** | structural / fuzzy | port DWARF *types/names* onto the target |

Every bit of Ghidra binary‑diffing muscle we've built — Version Tracking, the patchdiff
Bulk correlators, ghidriff, the rename‑recovery plan — runs on the **Bank5↔Bank8** axis
(*understanding* the target). **Nothing currently points a decompiler diff at build↔target.**
That is the unexploited capability.

---

## 2. What objdiff already covers (don't reinvent it)

`objdiff-core/src/diff/code.rs` is purely instruction‑level:

- Patience‑LCS over **opcode sequences**, then per‑arg penalties: reg=5, imm=1, replace=60,
  insert/delete=100. `match_percent = (1 − diff_score/max_score)·100`.
- It already emits **`match_percent_normalized`**, which *masks register swaps and
  branch‑offset swaps*. So the **pure regswap case is already a solved permuter‑class signal**:
  `normalized==100 && raw<100` ⇒ regalloc only, full stop.
- It already has a **branch graph** (`branch_from`/`branch_to`) and the milohax fork adds
  `--include-data` + an MSVC frame‑pointer‑anchor compensation pass.
- `run_diff_inspect` layers clusters / regswaps / diagnose on top.

objdiff never decompiles. It cannot tell you whether two *non‑regswap* mismatches are
logically equivalent.

---

## 3. The gap Ghidra/m2c fills

The expensive residual in our at‑limit corpus is **not** regswap — it's **FPR scheduling,
peephole, CSE, sdata2 layout** (the Synth / Vec.h / Mesh.cpp "2,450 variants, 0 wins"
families; `UpdateScrolling`; `CharEyes::Highlight`; …). There, `normalized%` is still <100,
but a **decompiler reconstructs the same high‑level expression on both sides** — because
scheduling / CSE / peephole differences vanish under SSA + structured‑CFG recovery.

A decompiler diff returns **"logically identical"** where objdiff can only say *"still 96%,
keep guessing."* That is exactly the "permuter‑class, mark at‑limit, move on" gate that today
costs thousands of permuter variants to reach empirically.

---

## 4. Empirical de‑risk (2026‑06‑09) — the signal is real

Tested Option A (m2c on both sides) on `UpdateScrolling__10VocalTrackFf`
(80.15% in report.json; memory documents it at‑limit 79.96%, "control flow IS correct").

Pipeline that **works today**, zero new infra:

```bash
# build side asm: dtk already disassembles our .o into the SAME ".fn SYMBOL, global"
# format m2c consumes from the target.
build/tools/dtk elf disasm build/SZBE69_B8/obj/band3/bandtrack/VocalTrack.o  /tmp/bld.s
# target side asm already exists:
#   build/SZBE69_B8/asm/band3/bandtrack/VocalTrack.s
python3 ../m2c/m2c.py --target ppc -f $SYM --passes 4 --deterministic-vars  build/.../VocalTrack.s > tgt.c
python3 ../m2c/m2c.py --target ppc -f $SYM --passes 4 --deterministic-vars  /tmp/bld.s            > bld.c
# extract just the fn body, then diff
```

Result: **~92 changed lines / 1711**, and **every one is the same systematic noise** — the
target side resolved the rodata string pool (`temp_r3 + 0x7C9`, `"VocalTrack.cpp"`) while the
build `.o` side emits `&@stringBase0 + 0x7C9` / `@STRING@…` / `@7120` for the **same offset**.
Control flow, arithmetic, calls, and struct field accesses (`->unk6C`, `->unk70`) are
**identical**.

Conclusions:
- The both‑sides m2c diff **correctly identifies this at‑limit function as logically
  identical** — i.e. it would have told us "permuter‑class, don't grind" in seconds.
- The noise is **systematic** (string/global base‑pointer symbolization + temp suffix
  numbering), therefore **normalizable**, not random. The classifier needs a normalizer that
  canonicalizes base references; after it, diff ≈ 0 ⇒ permuter‑class.
- Raw whole‑file diff is useless (m2c dumps every referenced global → dominated by
  string‑vs‑byte‑array encoding). You **must** extract the single function body first.

### The normalizer contract (what makes A a clean signal)
Canonicalize, on each side, before diffing:
1. String/data base pointers: `temp_rN_xxxx + OFF`, `&@stringBase0 + OFF`, `@STRING@<fn>@k`,
   `@<addr>` → a single canonical token, e.g. `DATA(OFF)` keyed by the **offset**, not the
   carrier register/symbol.
2. `temp_rN_yyyy` / `var_rN_yyyy` / `phi_rN` suffix numbers → positional placeholders
   (`--deterministic-vars` gets most of the way; strip residual numeric suffixes).
3. `@LOCAL@<fn>@name` → `LOCAL(name)`.
4. Whitespace / `?`‑typed forward decls of temps.

Then: **0 residual diff ⇒ IDENTICAL (permuter‑class)**; **non‑zero residual on control flow /
operators / call targets ⇒ DIFFERENT (source bug, here)**.

---

## 5. Three leverage options (ranked by ROI)

### Option A — m2c both‑sides triage (cheapest; de‑risked above)
- Inputs already exist: target asm in `build/SZBE69_B8/asm/`, build asm via `dtk elf disasm`
  on `build/SZBE69_B8/obj/**.o`.
- m2c flags: `--target ppc --passes 4 --deterministic-vars` (`--decomp` adds `--noise=low
  --show-offsets`). No "matching mode"; comparison is ours.
- Cost: function body extraction + the normalizer above + a `diff`. ~a day of glue.
- Wire a verdict line into `bin/analyze-function`.
- **Risk:** m2c text is noisier than Ghidra (regalloc → variable name drift); the normalizer
  is load‑bearing. AST‑diff is the robust upgrade over text‑diff.

### Option B — Ghidra decompiler diff via the live 8001 service (cleaner signal)
- Ghidra's decompiler normalizes harder than m2c (real dataflow/SSA, less name noise).
- The primitive is **already written**: `ghidriff/ghidriff/decomp_correlate.py` decompiles two
  functions, normalizes the signature (`remove_code_sig`), and compares pseudo‑C text.
- Plan: load our build (`.o` set or `main.elf`) as a **third program** alongside
  `bank8_target` in the pyghidra project (the `gamecube_dol` transcode + multi‑program project
  already support this), then reuse that primitive build↔target.
- Standing service (`tools/ghidra/pyghidra-service.sh`, port 8001) decompiles on demand,
  <50 ms cached. `mcp_client.py` is the client.
- **Risk:** the build changes every recompile → re‑import cost; project‑lock coordination
  (headless import vs live service). Heavier than A.

### Option C — BSim for easy‑win discovery (optional, low priority)
- BSim **is installed** (`ghidra/Ghidra/Features/BSim`, `support/bsim` CLI). Decompiler
  feature‑vector similarity DB.
- Use: "find target functions structurally near ones we've already matched → batch easy‑win
  candidates," and near‑duplicate clustering.
- **But** we already have 41,680 exact name matches, so discovery value is low — our own
  `docs/decomp/ghidra-capabilities-2026-06-09.md` reached the same "LOW priority" call.
- It is *not* a per‑function permuter‑class classifier (that's A/B); evaluate separately.

### Not worth it
Repointing the VT/patchdiff **byte/instruction** correlators at build↔target — their
similarity scores just duplicate objdiff's `%`. Only the **decompiler** diff is worth
repointing.

---

## 6. rb3‑xenon (Xbox 360 / MSVC PPC) benefits too

The decompiler‑diff triage **generalizes cleanly** to the Xenon side at `../rb3-xenon`:
- The build‑side asm path is the same shape: `jeff` (the Xenon dtk fork) splits/disassembles
  XEX→COFF, so a `jeff`‑disasm of the build `.obj` feeds m2c just like `dtk elf disasm` does
  here.
- m2c supports MSVC‑PPC heuristics; `decomp-synth` already drives objdiff‑scored permutation
  on the MSVC side, so a "logically identical?" gate would prune its search the same way.
- Ghidra Option B is identical in shape (load build + target, diff pseudo‑C); BSim works on
  any architecture.
- **Action:** mirror whichever of A/B wins here into `../rb3-xenon` (and the shared
  `decomp-synth`), keyed off `jeff` for the build‑side asm.

---

## 7. Honest ROI

This is a **sharper triage signal for the ambiguous middle**, not a revolution — regswap‑only
cases are already handled by objdiff's `normalized%`, and many at‑limit calls are already known
permuter‑class. The win is converting *"2,450 permuter variants to prove it's hopeless"* into
*"decompiler says identical in 50 ms."* Prototype A cheaply, **measure against our existing
at‑limit corpus**, then decide whether B's cleaner signal is worth its heavier setup.

---

## 8. Infra inventory (what already exists)

- **m2c** both‑sides: works today; `bin/decompile` shows the invocation. `--deterministic-vars`,
  `--noise=low`, `--decomp`, `--show-offsets` available. No AST/IR dump (`--dump-ast` absent) →
  diff is on C text (or roll our own tree‑sitter AST diff).
- **dtk** `elf disasm` → build `.o` → `.fn SYMBOL, global` asm (m2c‑ready). Verified.
- **Ghidra decompiler diff primitive**: `ghidriff/ghidriff/decomp_correlate.py` (exact pseudo‑C
  equality after `remove_code_sig`).
- **pyghidra‑mcp fork**: `import_binary`, `decompile_function` (batch, cached SQLite),
  multi‑program projects; `gamecube_dol.py` transcodes DOL→symbolized BE‑PPC ELF.
- **Standing service**: `tools/ghidra/pyghidra-service.sh` (port 8001) holds `bank8_target` +
  `band_r_wii`; `mcp_client.py` client; ~<50 ms cached decompiles.
- **BSim**: present in the Ghidra install (`support/bsim`, `Features/BSim`).
- **decomp-synth**: guided permuter (132 patterns, objdiff‑scored, m2c/Ghidra seeds) — the
  consumer that a permuter‑class gate would feed.

## 9. Validation experiment (the "Science!" run) — design

Goal: measure whether the build↔target decompiler diff correctly classifies permuter‑class
vs source‑fixable, for A and B, and probe C.

Corpus (labeled):
- **Positives (expect IDENTICAL)** — documented at‑limit / permuter‑class:
  `UpdateScrolling__10VocalTrackFf` (✓ de‑risked), `Highlight__8CharEyesFv` (82.76%),
  `ComputeDeformWeights__12BandCharDescCFPf` (77%), `UpdateVolumes__14StandardStreamFv`,
  `Poll__8CharEyesFv`, a Vec.h/Mesh family fn.
- **Sanity (expect IDENTICAL trivially)** — a few 100% functions; classifier must not emit a
  false "different."
- **Discrimination (expect DIFFERENT)** — mismatched pairs (fn X target vs fn Y build) +
  optionally one worktree‑mutated 100% fn (flip a comparison) as a gold real‑divergence case.

Metrics per approach: verdict vs label (confusion matrix), residual‑diff size, wall‑clock,
and A‑vs‑B agreement. Output: which approach to harden into a `bin/` tool, and whether to
mirror into rb3‑xenon.

*(Results appended by the workflow run.)*

---

## Validation results (2026-06-09)

Ran the labeled corpus (10 cases: 6 permuter + 2 sanity-100 + 2 discrimination) through the
Option-A classifier (`scripts/analysis/semantic_diff_classify.py`), and independently probed
Option B (Ghidra pseudo-C diff), Option C (BSim), and the Xenon port of Option A. Each verdict
was adversarially judged (function bodies re-extracted, residual lines hand-classified
real-logic vs noise).

### 1. Option-A accuracy — confusion matrix

| | verdict IDENTICAL | verdict DIFFERENT |
|---|---|---|
| **expect IDENTICAL** (8) | 6 (TP) | **2 (FN — both classifier-noise)** |
| **expect DIFFERENT** (2) | 0 (FP) | 2 (TN) |

- **8/10 correct.** **Zero false-IDENTICAL** — the gate never rubber-stamped a divergent pair.
  Both discrimination/impostor pairs (UpdateShifts-vs-UpdateGems @280 residual; Poll-vs-Highlight
  @719 residual) classified DIFFERENT with genuine real-logic residual. The conservative direction
  is the safe one: a permuter-class gate must never green-light a real source bug, and it didn't.
- **Both misclassifications are the *same* normalizer bug, not genuine divergence.**
  `Highlight__8CharEyesFv` (36 residual) and `Poll__8CharEyesFv` (2 residual) are both known
  at-limit/permuter-class and were hand-verified logically identical (same call targets, operators,
  struct offsets, control flow). The false DIFFERENT comes from the `_RE_STRING_SYM` regex charset
  `[A-Za-z0-9_<>]` **excluding `,`**: a template-named assert symbol like
  `@STRING@__rf__36ObjOwnerPtr<10CharLookAt,9ObjectDir>CFv` matches only up to the comma → folds
  to `STR` and orphans the tail `,9ObjectDir>CFv@0`, manufacturing a bogus residual. Target side
  resolved the same rodata to an inline literal that normalizes cleanly. This is the *exact*
  string-pool noise family the contract targets — just an incomplete charset.
- Of the 6 true-IDENTICAL, two (`UpdateVolumes`, the sanity-100s) had **raw diff 0** before any
  normalization — strongest possible identity signal — and the rest collapsed cleanly
  (UpdateScrolling 96→0, ComputeDeformWeights 24→0, UpdateShifts 12→0). Runtimes 0.6–16s/fn.

### 2. Is A a trustworthy permuter-class gate? — YES, after one fix

The signal is real and the conservative bias is correct (no false-IDENTICAL across 10 cases incl.
2 adversarial impostors). It is **not yet trustworthy as an unattended gate** because of the comma
charset gap, which produced a 25% false-DIFFERENT rate concentrated entirely in template-heavy TUs
(both CharEyes cases). A false DIFFERENT is a cheap failure (it just sends a human to look) but it
defeats the "stop grinding automatically" purpose, so it must be fixed before auto-wiring.

**Normalizer gaps remaining (priority order):**
1. **(dominant) Add `,` and `$` to the `@STRING@` symbol charset.** Single highest-value fix —
   flips both FNs to correct IDENTICAL → 10/10. Template-arg commas in MWCC mangled assert symbols.
2. Strip extra parens around a resolved offset (`(DATA(0xb))` vs `DATA(0xb)`) — m2c pool-carry
   artifact seen in CharEyes AddString3D calls.
3. Catch target-only forward-decls already rewritten to a `DATA(base)` carrier
   (`? (*DATA(base))(...)`, `s8 *DATA(base);`) — `_RE_TEMP_DECL` only matches `temp/var/phi_rN`.

After fix #1 the corpus is 10/10. The tool already prints `sample_residual`, so even an
un-catalogued noise class surfaces for a human rather than silently mis-verdicting — the design is
sound, the charset is just incomplete.

### 3. Option B (Ghidra pseudo-C diff) vs A

**B ran and works** — and gives a *cleaner* signal than m2c. A standalone pyghidra harness
decompiled all 3 probed functions from both build `.o` and the bank8 target ELF (forcing
`PowerPC:BE:32:Gekko_Broadway`), and after token-stream normalization all three were >99%
token-identical with **zero real-logic divergence** — correctly PERMUTER-CLASS, matching known
at-limit status (ComputeDeformWeights 99.77%, Highlight 99.49%, UpdateScrolling 99.02%). Notably
B got `Highlight` right where A's charset bug failed — Ghidra's SSA/dataflow normalizes the assert
rodata without m2c's symbol-rendering asymmetry.

**But B is heavier and not worth fronting:**
- ~9s JVM cold-start + import *per process*; needs a long-lived target-ELF program to amortize.
- Real productionization gotchas found: the install auto-selects the **wrong** `PowerPC:BE:64:Xenon`
  language for both `.o` and ELF (must force Gekko_Broadway); the `:default` language-ID form
  *segfaults the JVM* on a `.o`; target ELF needs `analyze=False`, build `.o` `analyze=True`;
  `setrecursionlimit(8000)` required. No linked build ELF exists (only at 100%/`ok`), so the build
  side imports per-`.o`.
- A naive line-diff is misleading (Ghidra 80-col wrap + `undefined4`-vs-`char*` type inference);
  the token-stream comparator is mandatory (Highlight went 492→51→37 tokens as the diff got
  smarter). Same residual noise *family* as A (SDA `@LOCAL@` vs raw `r13+OFF`, savegpr helper
  names, `__D_`/`D_` const forms).

**Verdict:** B is the robust upgrade (decompiler dataflow > m2c text), but only after A's cheap fix
is exhausted. Keep B as the fallback for functions where m2c mis-renders (heavy paired-single /
unusual control flow) — its only remaining real normalizer win is mapping the build side's named
SDA `@LOCAL@`/`LOCAL_<fn>_<name>` symbol back to its r13 offset (collapses ~90% of UpdateScrolling's
246 residual tokens). **Validated only the IDENTICAL direction (3 at-limit fns); B still needs a
source-bug negative control before auto-trust.**

### 4. Option C (BSim) — SKIP (no)

Pipeline is **proven feasible** (built an H2-file BSim DB from one analyzed `.o` — 54/54 functions
signed and queryable — without touching the live 8001 project), but it is **not worth building.**
BSim's job is *discovering* target↔candidate correspondence when you lack names. RB3 already has a
complete 1:1 CodeWarrior mangled-name map (54/54 identical symbols in the spot-checked TU;
~41,680 exact matches project-wide), and `report.json` already gives exact per-function asm match%
for every named pair. There is **no discovery gap for BSim to fill**, and its fuzzy
feature-vector similarity is strictly noisier than the exact name map. Full both-sides ingest would
cost minutes-to-hours of analysis (~1,876 `.o` + 17MB ELF) for a capability we have for free.
Matches the existing LOW rating in `ghidra-capabilities-2026-06-09.md`. (Niche exception: same-side
near-duplicate clustering — not the cross-side classification we need.)

### 5. Xenon — feasible to mirror, YES

Option A ports cleanly to `../rb3-xenon`. Ran the full pipeline on real MSVC-PPC functions and got
parallel target/base m2c decompilations with correct verdicts:
- `?Int@Rand@@QAAHHH@Z` @99.72% → IDENTICAL/permuter (only diff = call-target rendering, normalizes
  to 0).
- `?Multiply@@YAXABVTransform@@0AAV1@@Z` @61.7% → DIFFERENT/source-bug (opcode streams diverge at
  index 10) — negative control confirms `--use-base` reads a genuinely different obj.

**Build-side-asm hookup — NOT jeff.** jeff only splits the XEX into the *target* asm. The build
side flows through **objdiff**: it reads both the dtk-split target `.obj` and our MSVC COFF base
`.obj`, and `rb3-xenon/tools/objdiff_to_m2c.py` converts per-symbol objdiff JSON into
m2c-parseable `.fn`-style asm, with `--use-base` emitting our build side. One symmetric pipeline:
```
bin/objdiff-cli diff -p . -u <unit> "<MSVC_SYM>" -f json --include-instructions \
  | python3 tools/objdiff_to_m2c.py [--use-base] | python3 ../m2c/m2c.py -t ppc --valid-syntax -
```
m2c already has merged Xenon support (PPC64+VMX128, MSVC `?...@@` parsing); `-t ppc` works. The
normalizer transfers almost verbatim — the systematic Xenon noise is call/data-symbol *resolution*
(target `fn_<addr>` vs base MSVC-mangled `?Int@Rand@@QAAHXZ`), the exact analog of the Wii
string-pool carrier noise (rule #1). Soft items: body extractor must key off the m2c signature line
(no `.fn` marker — objdiff_to_m2c emits `label:`); orchestrator must pass `-u <unit>`; set
`objdiff_to_m2c_path` in `rb3-xenon/decomp-synth.json` (currently `null`).

### 6. RECOMMENDATION

**Harden Option A into `bin/decomp-diff SYMBOL -u UNIT` first.** It is the cheapest, already
8/10 (10/10 after one charset fix), reuses 100% existing infra (dtk + m2c + the normalizer),
runs in seconds/fn with no JVM, and never false-greened a divergent pair across 2 adversarial
impostors. B is a cleaner but heavier fallback; C is dead.

Concrete next steps:
1. **Fix the normalizer charset** in `scripts/analysis/semantic_diff_classify.py`: add `,` and `$`
   to the `@STRING@` symbol regex (gaps #1–#3 above). Re-run the corpus → expect 10/10. This is the
   only blocker to trustworthiness.
2. **Wrap as `bin/decomp-diff SYMBOL -u UNIT`** mirroring `bin/decompile`: resolve unit → target
   asm (`build/SZBE69_B8/asm/...`) + build `.o` (`dtk elf disasm`), decompile both, extract the one
   body, normalize, emit `match% + residual tokens + VERDICT` (0 residual ⇒ PERMUTER_CLASS/at-limit;
   any control-flow/call-target/struct-offset residual ⇒ SOURCE_BUG, shown).
3. **Add a source-bug positive control** to the corpus (a worktree-mutated 100% fn, or the Xenon
   `Multiply` analog) — we validated DIFFERENT only via impostor pairs, not a true logically-diverged
   build; confirm the verdict flips for real before auto-trust.
4. **Wire the gate into the at-limit loop, advisory-first.** Emit the verdict line in
   `bin/analyze-function`. Do **not** auto-mark at-limit yet — let it gate *human attention* (skip
   hand-reasoning when PERMUTER_CLASS) for a few sessions; promote to a decomp-synth pre-filter
   (prune the permuter search when the gate says IDENTICAL) once the source-bug control passes.
5. **Mirror to rb3-xenon** after the Wii tool stabilizes: same classifier, swap the asm source to
   the `objdiff → objdiff_to_m2c.py [--use-base] → m2c` path, extend rule #1's carrier set to the
   `fn_<addr>` / `?...@@` flavor, set `objdiff_to_m2c_path` in `rb3-xenon/decomp-synth.json`.

Keep B's pyghidra harness (`scripts/decomp-diff-optionb/`) on the shelf as the fallback decompiler
for m2c-hostile functions; skip C entirely.

---

## UPDATE (2026-06-09, post-run) — normalizer fixed, corpus now **10/10**

The synthesis above prescribed "add `,` and `$` to the `@STRING@` charset" for 10/10. **That fix is
wrong** — proven empirically: a bare comma in the charset also eats the m2c **arg-separator** comma
(`@STRING@Foo, 0x1EB` → `STR 0x1EB`), which *regressed* `UpdateScrolling` from IDENTICAL→DIFFERENT.

The real discriminator: a **template** comma is followed by a non-space (`<10CharLookAt,9ObjectDir>`)
while m2c's **arg-separator** comma is comma+SPACE (`, 0x1EB`). The landed fix in
`scripts/analysis/semantic_diff_classify.py`:

1. `_RE_STRING_SYM = @STRING@[A-Za-z0-9_<>$]+(?:,[A-Za-z0-9_<>$]+)*(?:@\d+)?` — commas fold into the
   symbol only when immediately followed by name chars; a `, ` separator can't match and is preserved.
2. `_RE_DATA_PAREN` — strip redundant parens directly wrapping a DATA token (`(DATA(0xb))`→`DATA(0xb)`).
3. `_RE_DATABASE_DECL` — drop target-only forward-decls of a `DATA(base)` carrier
   (`? (*DATA(base))(...);`, `s8 *DATA(base);`); a real *use* never ends in `DATA(base);`.
4. `_RE_DATABASE_VAL` — a bare string-pool base used as a value (`DATA(base)`, no offset, not a
   struct deref) == a string pointer (`STR`), reconciling target's carried base with build's inlined
   `@STRING@`.

**Empirically re-run, all 10 cases:**

| | verdict IDENTICAL | verdict DIFFERENT |
|---|---|---|
| **expect IDENTICAL** (8) | **8 (TP)** | 0 |
| **expect DIFFERENT** (2) | 0 | **2 (TN)** |

**10/10.** All 8 at-limit/sanity functions → IDENTICAL (residual 0). Both adversarial impostors stayed
DIFFERENT with large residual (**277**, **718** — down only 1 line each from pre-fix), confirming the
four rules canonicalize *noise* without collapsing genuinely-divergent functions. Zero false-IDENTICAL,
zero false-DIFFERENT. The Option-A classifier is a trustworthy permuter-class gate on this corpus.

**Lesson:** an agent-prescribed fix that was never executed (the charset claim) was wrong; the empirical
re-run caught it. The remaining gate before *unattended* auto-trust is unchanged — a real source-bug
positive control (a worktree-mutated 100% fn), since DIFFERENT was validated only via impostor pairs.

---

## CORRECTION 2 (2026-06-09) — the "10/10" was measuring the WRONG thing. Honest negative result.

The source-bug negative control (the gate the user specifically asked for) exposed a **critical bug
in the classifier and therefore in everything above it**.

### The bug
`objdiff.json` defines `obj/` = **TARGET** objects (split from the original binary) and `src/` =
**BASE** objects (compiled from our source = our build). The classifier was reading **`obj/` for the
"build side"** — so it compared the **target against itself** (`asm/` split vs `obj/` object, both the
target). That is why everything looked clean: two renderings of the *same* binary are trivially
identical modulo dtk's split-vs-elf-disasm string rendering (the only thing the ~100-line normalizer
was actually cancelling). **It never looked at our compiled source at all.** The source-bug control
caught this: a deliberately mutated function still classified IDENTICAL, because the mutated `src/`
object was never read.

### The corrected comparison (src/ = our build  vs  obj/ = target), both via the same dtk path
Fixed in `semantic_diff_classify.py` (`resolve_build_obj` → `src/`, `resolve_target_obj` → `obj/`).
Re-validated with safe normalizations (all-register temp-suffix strip, `@F_/@D_` const-pool →
value token, declaration/comment drop):

| function | class | norm residual | verdict | expected |
|---|---|---|---|---|
| UpdateVolumes | permuter | 0 | IDENTICAL | ✓ |
| GetInSongTime | sanity-100 | 0 | IDENTICAL | ✓ |
| RefreshCurrentShift | sanity-100 | 4 (`(f32)` cast noise) | DIFFERENT | ✗ |
| Poll (CharEyes) | permuter | 110 | DIFFERENT | ✗ |
| UpdateShifts | permuter | 149 | DIFFERENT | ✗ |
| ComputeDeformWeights | permuter | 257 | DIFFERENT | ✗ |
| Highlight (CharEyes) | permuter | 387 | DIFFERENT | ✗ |
| UpdateScrolling | permuter | **1669** | DIFFERENT | ✗ |
| impostor pair ×2 | different | 215, 582 | DIFFERENT | ✓ |

**4/10.** And the failure is not fixable by more normalization: a *permuter-class* function
(`UpdateScrolling`, norm **1669**) carries **more** residual than a *genuinely different* impostor
(norm **215**). **The signal is not separable — you cannot threshold it.**

### Source-bug control: signal exists, but isn't separable from regalloc noise
Mutating `RefreshCurrentShift`'s guard (`!=` → `==`, a real semantic bug) and recompiling `src/`
produced norm **6** with the inverted branch visible in the residual
(`-if (temp_r5 != ...)` / `+if (temp_r5 == ...)`). So a real bug *does* surface. But the **unmutated**
same function is already norm 4 (cast noise), and the hard permuter functions are norm 100–1669 — so
the real-bug signal (Δ≈2 lines) is drowned by regalloc/stack noise on exactly the functions we care
about.

### Root cause — why the premise was wrong
The whole idea rested on "a decompiler normalizes away regalloc/scheduling." **m2c does not.** It
names locals by **register** (`temp_f31` vs `temp_f30`) and stack slots by **frame offset** (`sp9EC`),
so regalloc cascades and frame-size deltas — the *defining* feature of the at-limit functions — become
large *text* differences. Normalizing those away requires blanking register/slot identity (skeleton
alpha-renaming), which then also cancels real bugs (the negative control would fail). There is no safe
threshold in between.

**objdiff already does this better.** Its `match_percent_normalized` aligns instructions and masks
register/branch-offset swaps *at the asm level* — precisely the regalloc question — without
re-introducing naming noise. The decompiler-diff is strictly worse for the permuter-class question.

### Option B (Ghidra) is suspect for the same reason
The Option-B probe decompiled "the build `.o`" — almost certainly `obj/` (target) too, i.e. the same
target-vs-target comparison, which is why it also looked >99% identical. Ghidra likewise names locals
by stack offset, so it would hit the same frame-delta/regalloc noise. Its ">99%" numbers are **not
trustworthy** and would need re-validation against `src/` before any claim.

### What still stands (the real wins)
- **dtk bug fixed & owned** — `elf disasm` was missing the `detect_strings` pass that `dol split` runs
  (`../dtk` branch `milohax/elf-disasm-detect-strings`, patch in `docs/decomp/patches/`). Real upstream
  bug, PR-ready for encounter/decomp-toolkit.
- **pyghidra-mcp bug fixed & owned** — `.o` (e_entry==0) mis-detected as 64-bit Xenon + no
  language-id validation (`../pyghidra-mcp` branch `milohax/gekko-language-guard`, patch in
  `docs/decomp/patches/`).
- The negative control did its job — it is the reason we know any of this.

### Honest recommendation (supersedes §6 and the first "10/10" UPDATE)
- **Do NOT productionize this as a permuter-class gate.** It is not separable for regalloc-cascade
  functions, which are the entire point. Don't wire it into `bin/analyze-function` as a verdict, don't
  feed it to decomp-synth, don't mirror to rb3-xenon yet.
- `semantic_diff_classify.py` is left in its **corrected** form (src vs obj). It is, at best, a *noisy
  advisory* that's only clean on small functions — and for those, objdiff already says 100%/normalized.
- For "is this just regalloc?", **use objdiff's `match_percent_normalized` + `run_diff_inspect`** —
  it's the right tool and already exists.
- The dtk + pyghidra fixes are worth upstreaming on their own merits.
