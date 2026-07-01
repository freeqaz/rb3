# T2 verification — score export + VT STL/template gate (adversarial review)

**Verifier:** Fable, 2026-06-10. **Implementer doc:** `task-T2-impl.md`. **Commit under review:**
`ghidriff:31a6f6c862ee1a791fd93a96dc58e0e6a342f832` (branch `rb3-improvements`), doc commit `rb3:d935f117`.
All verification OFFLINE (no ghidriff run, no service touched).

## Verdict

**CONFIRMED.** Every load-bearing claim was independently re-verified: the commits exist with the
claimed content, both test suites pass when re-run by me (18/18 + 10/10), my own independent
replay over the real 722-match VT pool reproduces exactly 19 exclusions including all 5
forensics-named internals with real container-param functions kept, the address-string
side-channel join is structurally sound (verified at the guard in the matched-list builder), the
matches-dict `{type: count}` shape audit holds, the CLI flag is wired end-to-end with the claimed
semantics, and the editable install makes the commit reachable from the gated runner. A
full-universe recall probe (my own, beyond the implementer's protocol) shows the exclusion gate
costs only **3 real-class functions out of 41,232** — no hidden recall destruction. Minor caveats
below; none load-bearing.

---

## What I checked and how

### 1. Commits exist and tree is clean
- `git show 31a6f6c --stat`: 7 files, +655/−55 — `vt_ref.py`, `implied_matches.py`, `bsim.py`,
  `version_tracking_diff.py`, `ghidra_diff_engine.py`, `__main__.py` (1 line),
  `tests/test_score_export.py` (393 lines). `rb3:d935f117` = the impl doc only.
- `git diff 31a6f6c..HEAD --stat` on ghidriff: only T1's `5b9cc4e` on top, touching only
  `correlators.py` + `tests/test_string_hasher_gate.py` (disjoint). `git status --short` on the
  7 T2 files: clean — HEAD content == commit content, nothing uncommitted.

### 2. Tests re-run by me (not trusted from the doc)
```
PYTHONPATH=<ghidriff-venv site-packages> python3 -m pytest tests/test_score_export.py -v \
    -o required_plugins= -o addopts= -p no:cacheprovider
```
**18/18 passed** (0.05s). `tests/test_fast_core.py`: **10/10 passed**. I read
`tests/test_score_export.py` in full — the tests exercise the REAL changed code paths
(`_accept_vt_candidates`, `_compile_exclude_patterns`, `build_function_match_entry` are imported
from the production modules, not reimplemented), with faithful doubles (FakeVTMatch exposes
exactly the four methods the helper calls; FakeAddrSet mirrors contains/add). The replay tests
are NOT skipped (artifact present) and assert hard numbers (`len(vt) == 722`,
`len(excluded) == 19`, the 5 named strings, kept-real survival).

### 3. Independent replay (my own script, not the test)
Loaded the real
`build/SZBE69_B8/ghidra/ghidriff-xenon/json/bank8_target.elf-...matches.json` (2,645 entries,
722 VT), applied `DEFAULT_VT_EXCLUDE_PATTERNS` myself, and PRINTED all 19 excluded names + all
74 "stl-ish but kept" names. Eyeball verdict: the 19 are all STL/ObjVector/template-internal
bodies (`_M_fill_insert_aux`, `__as__..._Rb_tree/map/ObjVector`, `__rs<...>`/`__ls<...>` template
stream ops, `insert_unique__..._Rb_tree`, `__unguarded_linear_insert<...>`,
`push_back__28ObjVector<CamShotCrowd>`, 2× `resize__`); all 5 forensics §1D names present. The
kept set includes every real container-PARAM function (`CollideList__13BandCharacter...`,
`DrawWidgets__9UIListDir...`, `PollLyricAnimations__10VocalTrack...`, `Mats__11TrackWidget...`,
`PropSync<...>`) plus all ctors/dtors and the non-template `__rs__9BinStream`/`__ls__FR9BinStream`
operators — exactly as claimed.

### 4. Recall probe over the FULL Wii name universe (beyond the impl's protocol)
The gate runs on the whole unmatched pool next run, not just the current 722 — the hunt was
"precision gates that also destroy recall". Applied the patterns to all **41,232** function names
from `config/SZBE69_B8/symbols.txt`:
- Would-exclude: **3,513 (8.52%)** — bucketed: 3,182 via the `stlpmtx_std`-owner pattern,
  241 `^__rs<`, 53 `^__ls<`, 34 ObjVector members, 3 `^resize__`.
- I sampled 25 of the 3,182 stlpmtx hits: ALL are genuine STL internals (owner is an
  `stlpmtx_std` type or a free template helper mangled `...__11stlpmtx_stdF...`). A structural
  check for hits where `stlpmtx_std` is matched outside owner position found **0**.
- The ONLY real-class collateral in the whole universe: `resize__6StringFUi`,
  `resize__Q26Quazal7qBufferFUiPCUcUi`, `resize__Q27RndMesh10VertVectorFib` — three vector-ish
  boilerplate bodies. **No recall destruction on real game functions.** The owner-anchoring
  claim holds: param-position `RQ211stlpmtx_std...`/`<Q211stlpmtx_std...` never matches the
  `__`-anchored pattern (verified both by regex reasoning and by the 74-kept list).

### 5. The address-string join (the classic silent-failure point)
`pair_scores` is keyed `(str(src), str(dst))` of Ghidra Address objects at accept time. The
writer joins with `(str(sym1.address), str(sym2.address))`. Verified sound by reading the
matched-list builder (`version_tracking_diff.py:340-352`): it does
`getFunctionContaining(match_addrs[0])` and **explicitly skips any match where
`func.entryPoint != match_addrs[0]`** — so `sym1.address` is the exact same Address keyed into
`matches`/`pair_scores`, and both sides stringify via the same `Address.toString()`. Implied
(`implied_match[0/1]` are entry-point Addresses) and BSIM (`bsim_match.sourceAddress`) use the
same form. Real artifact format check: `p1_addr` is `"8000fb10"` (NO `0x` prefix — see caveat b).

### 6. matches-dict shape audit (re-run the grep myself)
- Writers all still `{type: count}`: `vt_ref.py` (via `_accept_vt_candidates`),
  `implied_matches.py:256-257`, `bsim.py:135-136`, `decomp_correlate.py:95,266`,
  `version_tracking_diff.py` seed/hash loops.
- Shape readers untouched: `bsim.py:92` (`any(m_type in seed_match_types for m_type in m_types)`),
  `version_tracking_diff.py:183,231` (`Counter([tuple(x) for x in matches.values()])`).
- Matched-list consumers at `ghidra_diff_engine.py:1691,1775,1822` all unpack 3-element entries;
  matched entries stay `[sym1, sym2, m_types]`.
- `find_matches` arity: `version_tracking_diff` returns 4; `simple_diff.py:286` /
  `structural_graph_diff.py:353` return 3; the single caller (`ghidra_diff_engine.py:1683-1685`)
  uses `find_result[3] if len(find_result) > 3 else {}` — no unpack-crash path.

### 7. CLI wiring + reachability from the gated runner
- Flag chain verified: `add_ghidra_args_to_parser` (`ghidra_diff_engine.py:~423`) →
  `__main__.py:80` → engine init parse (`:228-230`) → `version_tracking_diff.py:252`
  (`exclude_patterns=self.vt_ref_exclude_patterns`) → `_compile_exclude_patterns`.
- Semantics tested live in the ghidriff venv python: omitted → `None` → 14 default patterns;
  `''` → `[]` → exclusion disabled (`if compiled_excludes:` falsy); `'foo,bar'` → `['foo','bar']`.
- Reachability: the venv's `__editable___ghidriff_1_0_0_finder.py` points at
  `/home/free/code/milohax/ghidriff/ghidriff`; `venv/bin/python -c "import ghidriff.vt_ref"`
  resolves to the fork file and sees the 14 patterns. `run_ghidriff_xenon.sh` passes
  `--vt-ref-correlators --vt-ref-min-score 9.5 --implied-min-ratio 0.9 --no-decomp-correlate`
  (lines ~241-246) — so BOTH score paths (VT product, Implied gated ratio) are exercised on the
  next gated run, with default exclusions active (runner does not pass the new flag). The
  per-accept INFO log line exists in `_accept_vt_candidates` (log-only sweep claim holds).
- Implied gated path read in full (`implied_matches.py:225-263`): `ratio` is always computed
  under the gate (cache-miss path computes it; `f1/f2 is None` → 0.0 → gated out), so every
  gated accept carries `accepted_ratio` — the "all real Implied matches will carry a ratio at
  0.9" claim is correct.

### 8. Accept-loop regression vs the original code
Diffed old inline loop vs `_accept_vt_candidates`: identical sort key (−product,
addr-string tiebreak), identical gate ORDER (min_score NaN-inverted → too_short → already_matched),
STL gate inserted after already_matched (so reject counters keep their old meanings), identical
matches/p1_matches/p2_matches updates. Only behavioral deltas: per-accept log DEBUG→INFO and the
new `stl/template excluded: N` field in the summary line — both claimed and intended.

---

## Caveats (minor, none refuting)

a. **`^resize__` slightly exceeds the "STL/template internals" framing.** It excludes any
   function literally named `resize` regardless of owner. Universe-wide collateral is 3 real-class
   boilerplate bodies (`String::resize`, `Quazal::qBuffer::resize`, `RndMesh::VertVector::resize`);
   1 of the 19 current-pool exclusions (`resize__Q27RndMesh10VertVector`) is an Hmx class, not
   STL. Negligible cost and arguably desirable (vector-ish boilerplate with the same trivial-collision
   property), but the impl doc's "all true internals" is a touch generous on that one name.
b. **Address format in the doc examples is `0x`-prefixed; the REAL artifact uses bare hex**
   (`"8000fb10"`). The join itself is immune (both sides use `str(Address)`), but **T4 must not
   assume a `0x` prefix** when post-processing `p1_addr`/`p2_addr`. Flagging since the PLAN §3 and
   impl-doc examples could mislead.
c. **The 0.324→0.393 precision gain is forensics' projection, not re-measured** — correctly
   attributed in the impl doc; cannot be re-measured without the human-gated run. The exclusion's
   mechanical effect (19 matches removed, 5 of them judged-wrong) is verified; the precision
   delta is not independently confirmable offline.
d. **`stlp_priv`-owned internals are NOT covered** by the patterns (false negatives possible at
   the margin); completeness, not correctness. Add patterns later if forensics on the next run
   shows them leaking through.
e. **`__main__.py` one-line scope deviation** — verified it is exactly one line (`:80`) and is
   required for the flag to function; no concurrent task touches that file (T1's commit confirmed
   disjoint).
f. `decomp_correlate` accepts carry no scores (its `Decomp Match`/ratio is not exported) — out of
   T2's brief and the stage is `--no-decomp-correlate`'d in the runner; noting for completeness.

## For the next agent
- T4: read VT product via `entry.get('scores', {}).get('VTCombinedReference', {}).get('product')`;
  treat `scores` and each per-type key as optional; addresses are BARE hex strings (caveat b).
- The first scored matches.json only exists after the human-gated re-run (PLAN §6). After it,
  sanity-check: every `VTCombinedReference`-typed entry should carry a VT score (the accept path
  always records when `pair_scores` is threaded — it is, unconditionally, in
  `version_tracking_diff.py:249-252`); every `Implied Match` entry should carry `ratio` given
  `--implied-min-ratio 0.9`.
- If the VT pool baseline changes, `test_replay_stl_exclusion_over_real_vt_pool`'s `== 722` /
  `== 19` assertions need a baseline update (the test docstring says so).
- Nothing open from this verification; T2 is safe to consume.
