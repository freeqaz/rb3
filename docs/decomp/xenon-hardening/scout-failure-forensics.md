# Scout: Failure Forensics on the Wrong Wii↔Xenon ghidriff Matches

**Date:** 2026-06-10
**Author:** scout-failure-forensics agent
**Inputs analyzed (all pre-existing, offline):**
- `build/SZBE69_B8/ghidra/ghidriff-xenon/eval_report.json` (judged holdout + dc3 pairs w/ verdicts)
- `…/ghidriff-xenon/json/bank8_target.elf-42264e.gzf-rb3_xenon_default_xex.gzf.ghidriff.matches.json` (all 2645 pairs)
- `…/ghidriff-xenon/ghidriff.log` (cascade counts; the canonical BSim-OFF run is the *second* run, log lines 1041–1167)
- `orig/SZBE69_B8/files/band_r_wii.map` (Wii CW ground-truth names + sizes)
- `tools/ghidra/eval_xenon_matches.py` (eval logic; defines the judged set)
- Two read-only pyghidra passes over the existing gzfs (see Reproduce)

**Bottom line:** The two string hashers (`StringsRefsHasher`, `StrUniqueFuncRefsHasher`)
fail for ONE reason — a single shared class-name string whose **multiset key is
non-unique** across the binary; the 1:1 survivor logic pairs the wrong leftover.
The `VTCombinedReference` 0.324 is **understated by the eval**: ≥7 of its 25 "wrong"
are eval/oracle artifacts (Wii↔Xbox class renames + an arity-normalize bug), not
ghidriff errors. And the BinDiff oracle is itself unreliable (sim=1.0 on
byte-identical stub shapes) for exactly the functions where SRH is judged 0.000.

---

## 0. Judged-set provenance (so the next agent trusts the counts)

`precision_by_match_type` in eval_report.json is tallied over a **judged set of 100
pairs**: 32 from holdout (`recovered_correct`/`recovered_wrong`) + 68 from dc3
high-confidence bindiff (`high_conf=true` AND verdict∈{agree,disagree}). A pair
contributes to every match-type it carries. Verified reproduction matches the report
exactly:

| match type | judged | correct | wrong | precision |
|---|---:|---:|---:|---:|
| OVERALL | 100 | 44 | 56 | 0.440 |
| VTCombinedReference | 37 | 12 | 25 | **0.324** |
| ExactInstructionsFunctionHasher | 31 | 29 | 2 | 0.935 |
| StringsRefsHasher | 26 | 0 | 26 | **0.000** |
| Implied Match | 4 | 3 | 1 | 0.750 |
| StrUniqueFuncRefsHasher | 2 | 0 | 2 | **0.000** |

Pool sizes (all 2645 pairs, from matches.json): SeedMatch 1186, VTCombinedReference
**722**, StringsRefsHasher **610**, ExactInstructions 63, StrUniqueFuncRefsHasher 45,
Implied 11, ExactMnemonics 4, SwitchSig 3, SymbolsHash 1.

> **Caveat that dominates everything below:** SRH has **zero judged-correct pairs** —
> all 26 judged are wrong, and none of the holdout's 146 entries were matched via
> SRH. So we have *no positive evidence* for SRH on this run. Every "would-have-been
> precision" for an SRH gate is measured against a 26/0 split: gates can only be
> scored on how many *wrong* they kill, NOT on correct-pairs preserved. The recall
> cost is estimated from the broader 610-pool's key-shape distribution, not measured.

---

## 1. Failure-mode taxonomy (with counts)

### 1A. StringsRefsHasher — 26/26 wrong = "shared single-string multiset, survivor mispairing"

I recomputed the actual string-ref keys for all 610 SRH + 45 StrUnique matched
functions on BOTH gzfs (read-only pyghidra; `forensics/recompute_strkeys.py` →
`strkeys_out.json`). Findings on the 26 judged-wrong SRH pairs:

- **26/26** reference **exactly one** distinct string (`nuniq==1`), and the Wii
  string == the Xenon string (e.g. both reference `"LayerDir"`). The key *matched
  perfectly*; the functions are still different functions.
- **23/26** are tiny **`ClassName`/`Type` accessors**, Wii size **76 bytes** (the
  `return StaticClassName/Type` stub). The other 3 are bigger (`Poll__UITrigger` 516B,
  `StartScroll__UIList` 648B, `SendScrollSelected__ScrollSelect` 868B).
- **0/26** have the Wii class name == the dc3 class name (verified): ghidriff says
  `LayerDir`, BinDiff says `HamListRibbon`; `EndingBonus`→`RndMat`; `CamShot`→`RndMovie`; …

**Why a perfect-key match is still wrong — the mechanism (proved):** I counted, over
the *whole* Xenon and Wii programs, how many functions reference each of the 26
key strings (`forensics/string_global_uniqueness.py` → `string_uniqueness_out.json`):

- The key string is referenced by **2–9 Wii functions** (median **3**); `wmult>1`
  for **23/26**. Example: `"CamShot"` is referenced by 3 Wii funcs (ClassName, ctor,
  …) and 1 Xenon func. `"user_login"` by **9** Wii funcs.
- Ghidra's `MatchFunctions(one_to_one=True, one_to_many=False)` accepts a hash bucket
  only when exactly one unmatched function remains on each side. Earlier cascade
  stages (seeds, exact, VT) drained the *other* referencers of that string, leaving
  ONE Wii + ONE Xenon survivor — **but not the corresponding pair**. The leftover Wii
  `ClassName::CamShot` got bucketed with whatever Xenon `"CamShot"`-referencer was
  last standing.
- So the docstring warning ("DO NOT RUN with one_to_many=TRUE") is a *red herring for
  this run* — the cascade tuple sets `one_to_many=False` (version_tracking_diff.py:71-72,
  79-80) and Ghidra honored it. The defect is subtler: **a multiset of shared class-name
  strings is not a discriminating key cross-compiler**, and 1:1-survivor acceptance
  launders a non-unique key into a confident-looking 1:1 match.

### 1B. StringsRefsHasher — sub-mode B: 3/26 are *eval/oracle errors*, not ghidriff errors

The 3 SRH pairs whose key string is **1:1-unique on BOTH sides** (`wmult==1 &&
xmult==1`): `ui_trigger_complete`, `component_scroll_start`, `component_scroll_select`.
These are structurally forced single-string matches that are **almost certainly
CORRECT** (Wii `Poll__UITrigger` ↔ the only Xenon func referencing `ui_trigger_complete`).
The eval marks them wrong only because the BinDiff oracle (sim=1.0, conf≈0.96) names
that Xenon address `UITriggerCompleteMsg::Type` — and **BinDiff is unreliable here**:
it matches these by raw structural shape (all `return Symbol("…")` stubs look
identical), with no string evidence, so it pairs same-shape stubs arbitrarily. The
string evidence ghidriff used is the *better* signal. ⇒ SRH's true precision is **not
0.000**; at least these 3 are mis-scored. (Cannot raise above this without a cleaner
oracle, but the planner should not treat 0.000 as ground truth.)

### 1C. StrUniqueFuncRefsHasher — 2/2 wrong = same mode as 1A

`ClassName__RndParticleSys` (`"ParticleSys"`, 76B) and
`ClassName__InstrumentDifficultyDisplay` (76B). Both `nuniq==1` ClassName stubs. The
`ref_count` discriminator StrUnique adds to the key did NOT save them. Killed by the
same gates as 1A.

### 1D. VTCombinedReference — 25 wrong, three distinct sub-modes

The workhorse. 24 wrong are in the dc3 subset, 1 in holdout (`RVNATRelay` stem). Of
the 24 dc3-wrong (classifying by Wii-method-name vs dc3-method-name agreement):

- **Sub-mode V1 — platform twins (6/24): NOT ghidriff errors.** Same method, the
  Wii/Xbox class is a *rename of the same source class*, so the eval's name-key join
  can't bridge them:
  `SetTex@WiiMovie↔DxMovie`, `CheckShotOver@BandCamShot↔HamCamShot`,
  `IsDownload@BandSongMetadata↔HamSongMetadata`, `Select@WiiPostProc↔NgPostProc`,
  `ComputeElbowPullAndQuat@BandIKEffector↔HamIKEffector`, `SetFrame@QuatKeys` (see V2).
  For identity-porting these are the **correct** answer.
- **Sub-mode V2 — eval normalize/arity bug (≥1/24): NOT a ghidriff error.**
  `SetFrame__8QuatKeysFff` ↔ `?SetFrame@QuatKeys@@UAAXMMM@Z`: same class, same method,
  bindiff sim=1.0 conf=0.993. Judged disagree only because
  `normalize_demangled` produced arity 2 (Wii `Fff`→2 float params parsed) vs 3 (MSVC
  `MMM`). Detail string in eval: `('QuatKeys',),'SetFrame',2 != ...,3`. Same function;
  the Xbox build likely just took an extra float. **Eval false-disagree.**
- **Sub-mode V3 — genuinely wrong (≈17/24).** Of these, **5 are STL/ObjVector/template
  internals** matched to other STL internals (`push_back__ObjVector<CamShotCrowd>` ↔
  `_Copy_Construct<CamShotCrowd>`, `_M_fill_insert_aux<MidiChannel>` ↔
  `_Param_Construct<MidiChannel>`, `__as__ObjVector<EventTrigger::Anim>`, etc.). The
  rest are unrelated (`Copy__UIListDir` ↔ `UpdateADSR@StreamReceiver360`;
  `cbForUnrecoveredErrorRetry` ↔ `main`; `__dt__JsonObject` ↔ `json_tokener_reset_level`).

**⇒ VT's effective precision counting V1+V2 as correct is ~(12+7)/37 ≈ 0.51, not 0.324.**
The 0.324 is an artifact of demanding class-name *equality* under a Wii→Xbox rename.

### 1E. The score threshold was NON-BINDING (read this before tuning `--vt-ref-min-score`)

From the canonical run (ghidriff.log:1164):
```
VTCombinedReference: 2792 candidates -> accepted 722
   (below min_score: 0, already matched/taken: 1942, shorter than min-func-len: 128)
```
`below min_score: 0` ⇒ **the 9.5 floor culled zero candidates.** 722 = 2792 − 1942
(already taken by seeds/exact) − 128 (too short). Raising `--vt-ref-min-score` from
9.5 has no effect until it crosses the *actual* score of the lowest accepted
candidate — which we don't have (scores aren't in matches.json). **Action for the
planner: instrument vt_ref.py to log per-accept scores before tuning the floor;
9.5 is currently a no-op.**

---

## 2. Per-gate would-have-been precision (on the judged subset)

Reproduce: `python3 docs/decomp/xenon-hardening/forensics/gate_analysis.py`

### SRH gates (26 judged, ALL currently wrong — gates scored on wrong-killed only)

| gate | kept | correct | wrong | killed | note |
|---|---:|---:|---:|---:|---|
| NONE (baseline) | 26 | 0 | 26 | 0 | prec 0.000 |
| **distinctive-string-count ≥ 2 (nuniq≥2)** | 0 | 0 | 0 | **26** | kills 100% of wrong |
| 1:1-unique key (wmult==1 && xmult==1) | 3 | 0 | 3 | 23 | keeps the 3 sub-mode-B (likely-correct, mis-judged) |
| exclude key-string ref by >1 Wii func | 3 | 0 | 3 | 23 | identical effect to 1:1 here |
| min Wii size ≥ 120 B | 3 | 0 | 3 | 23 | keeps the 3 non-stub pairs |

**Recall cost of the SRH gates (estimated from the 610-pool, NOT measured):**
- `nuniq≥2` keeps **128/610 (21%)**, drops the 482 single-string matches (incl. **233**
  76-byte ClassName/Type stubs). Among single-string matches, the *correct* ones are
  exactly those whose string is globally 1:1-unique (sub-mode B). So `nuniq≥2` is
  **over-aggressive**: it would also drop legitimately-unique single-string matches.
- **Recommended SRH gate: `nuniq≥2 OR (single string AND that string is 1:1-unique
  across both programs)`.** This kills all of sub-mode A (the shared-key survivors)
  while *keeping* sub-mode B (the structurally-forced unique-string matches). Requires
  computing a global per-string ref-count map at hash time (cheap: ghidriff already
  builds `func_str_map`; invert it to `string→#funcs` once per program). The
  `MIN_STRING_LEN` and ClassName-stub problem then takes care of itself: a class name
  referenced by ≥2 funcs (ClassName + ctor + …) is exactly the non-unique case.

### VT gates (37 judged, 12 correct / 25 wrong)

| gate | kept | correct | wrong | killed | precision |
|---|---:|---:|---:|---:|---:|
| NONE (baseline) | 37 | 12 | 25 | 0 | 0.324 |
| **exclude STL/ctor/dtor/ObjVector internals** | 28 | 11 | 17 | 9 | **0.393** |
| min Wii size ≥ 128 B | 22 | 4 | 18 | 15 | 0.182 ⬇ |
| min Wii size ≥ 256 B | 11 | 1 | 10 | 26 | 0.091 ⬇ |

- **Size gates HURT VT** (opposite of SRH): correct VT matches skew *small*; wrong ones
  skew large. Do NOT add a VT min-size floor.
- **exclude STL/template internals** is the only structural gate that helps VT
  (0.324→0.393), and it costs little recall: STL/ObjVector internals are only **36/722
  (5%)** of the VT pool (plus 55 ctor/dtor). Recommend excluding `stlpmtx_std`/
  `_Copy_Construct`/`_Param_Construct`/`ObjVector<…>::{push_back,resize,operator=}`
  candidates from VT acceptance — they collide trivially cross-compiler (every
  `vector<T>` copy looks alike) and carry no porting value.
- The biggest VT win is **not a gate but the eval**: crediting V1 platform-twins and
  fixing the V2 arity-normalize bug raises measured VT precision to ~0.51. See §4.

---

## 3. Quantified "how far is the wrong answer" (the forensics question)

For the 26 SRH + 2 StrUnique wrong pairs (where Wii ground-truth class is known):
- **Same class? 0/28.** Always a *different* class.
- **Same TU/file? No** — `LayerDir`(LayerDir.o) ↔ `HamListRibbon`; `CamShot`↔`RndMovie`.
  The wrong target is an unrelated class that *coincidentally shares one short string*.
- **Same size band? Yes, tightly** — 23/26 SRH wrong are 76-byte stubs matched to
  76/88-byte stubs (Wii 76 / Xenon 88; the +12 is Xenon's larger thunk/prologue).
  This is *why* they collide: identical tiny shape + identical single string.
- **Distance = "same micro-shape, same one string, unrelated class."** Not random
  garbage — a structured collision among the binary's population of trivial
  `return Symbol("Name")` accessors (the engine has hundreds).

For VT wrong: bimodal — **V1/V2 (7) are distance≈0** (same function, renamed class /
arity artifact); **V3 (18) are distance=large** (STL look-alikes, or fully unrelated
funcs sharing a reference-graph neighborhood). VT wrongness is **not** correlated with
a low score (the floor was non-binding) and is **anti**-correlated with size (bigger =
more wrong). The likeliest real driver is **seed-neighborhood density**: VT propagates
through call/data-ref edges from accepted matches, so a function surrounded by many
seeds gets many candidate edges; STL internals sit in dense, homogeneous neighborhoods
and mis-propagate. (Not directly measurable from matches.json — scores/edge-counts
aren't exported. Flag for instrumentation.)

---

## 4. Recommendations for the planner (calibration evidence summary)

1. **SRH/StrUnique gate (high confidence, kills 100% of the 26+2 wrong):** require
   either `nuniq≥2` OR a globally-1:1-unique single string. Implement by inverting
   `func_str_map` to a per-program `string→#referencing-funcs` map (one pass, already
   have the data in `get_defined_data`) and rejecting a single-string candidate unless
   that string maps to exactly 1 func in *each* program. This preserves sub-mode B and
   removes the entire 76-byte-ClassName collision class. File: `ghidriff/correlators.py`
   (`StringsRefsHasher.hash` / a new acceptance filter), or gate in the cascade.
2. **VT gate (modest, low recall cost):** exclude STL/ObjVector/template-internal
   candidates from VTCombinedReference. 0.324→0.393 on the judged subset, costs 5% of
   the pool. File: `ghidriff/vt_ref.py` acceptance loop (name-pattern reject).
3. **Do NOT add a VT min-size floor** — it inverts (size≥128 → 0.182). Size floors
   help only the string hashers' stub problem, and `nuniq≥2` already covers that.
4. **`--vt-ref-min-score 9.5` is currently a no-op** (`below min_score: 0`). Before
   tuning it, instrument vt_ref.py to log per-accept scores; the discriminating cut is
   somewhere above the lowest accepted score, which is unknown.
5. **Fix the eval before re-measuring VT**, or VT will keep looking worse than it is:
   (a) credit Wii↔Xbox class renames (WiiMovie↔DxMovie, Band*↔Ham*, *Wii↔*Ng/*Xbox) as
   agree — a class-rename alias table, or a method-name+param-shape join that ignores
   the class token; (b) fix `normalize_demangled` arity for `Fff`↔`MMM`
   (QuatKeys::SetFrame false-disagree). Net effect: VT measured precision ~0.324→~0.51.
6. **Treat BinDiff sim=1.0 as UNTRUSTWORTHY for ≤88-byte stub-shaped functions.** It
   drives the SRH 0.000 and ≥3 of those are actually correct. A cleaner oracle for
   tiny accessors would be string-content equality (which is what SRH already uses).

---

## For the next agent

**Read first:** this doc, then `tools/ghidra/eval_xenon_matches.py` (judged-set
definition, §0) and `ghidriff/version_tracking_diff.py:63-82` (cascade tuple — note
SRH/StrUnique run with `one_to_many=False`, §1A) + `ghidriff/correlators.py:364-398`
(`StringsRefsHasher.hash` = `hash(tuple(sorted(strings)))`, the multiset key).

**Artifacts in `forensics/` (reusable):**
- `recompute_strkeys.py` → `strkeys_out.json` — per-matched-func string multiset +
  size, both programs (read-only pyghidra, ~2 min under the FORK ghidra
  `GHIDRA_INSTALL_DIR=/home/free/code/milohax/ghidra/build/ghidra`).
- `string_global_uniqueness.py` → `string_uniqueness_out.json` — global
  `string→[func addrs]` for the 26 wrong key strings (proves wmult median 3).
- `gate_analysis.py` — reproduces every table above from the JSONs + the CW map.
  Run: `python3 docs/decomp/xenon-hardening/forensics/gate_analysis.py`.

**Still open / not done here (needs a gated re-run or live instrumentation):**
- VT per-accept **scores** and **edge/neighborhood density** are not in matches.json;
  §2/§3 hypotheses about score-floor and seed-density need vt_ref.py instrumentation
  (a logging change, then ONE gated re-run — do not start unprompted).
- The SRH `nuniq≥2 OR globally-unique` gate's **recall** is *estimated* (21% of pool
  kept), not measured — there are zero judged-correct SRH pairs. A holdout that
  samples ClassName accessors would measure it.
- Whether the 3 sub-mode-B SRH pairs + 6 V1 twins + 1 V2 are truly correct should be
  spot-confirmed against the Wii CW map / DC3 source by the planner before crediting
  them (I asserted correctness from string + method-name evidence, not from reading
  both decompilations).

**Hard constraints honored:** no full ghidriff run; no service restart; only two
short read-only pyghidra passes over the existing gzfs; no git add -A / commit / stash;
all new files are under `docs/decomp/xenon-hardening/forensics/` (mine).
