# Sweep Roadmap — 2026-05-25

After the STL specialization sweep landed today (6 fns to 100%/near-100%; 4 TUs confirmed not-applicable), capturing the next set of patterns worth broadly sweeping. Project state: ~75.5% functions matched (31,163 / 41,254).

## In-scope partial-match population

From `report.json`, excluding `system/rndwii/` and `system/os/` (out-of-scope for the port):

| Band | Count | Sweep value |
|---|---|---|
| 100% | 27,146 | — |
| 99.9% – <100% | 767 | LINKED-noise, mostly artifacts |
| 99% – <99.9% | 583 | last-mile, scheduling drift |
| 95% – <99% | 607 | last-mile, scheduling + small structural |
| 90% – <95% | 238 | **sweet spot — structural fixes** |
| 80% – <90% | 142 | **sweet spot — structural fixes** |
| 50% – <80% | 65 | needs deeper rewrite |
| <50% | 757 | stubs / missing impl |

**Prime sweep target: 1,587 fns in the 80–99% band.** Plus 767 in 99.9+ for cheap LINKED audits.

## Phase 1 — Diff-pattern sweeps (medium effort, well-documented patterns)

Patterns where the partial diff itself identifies the candidate. Each sweep needs `run_diff_inspect` or a custom script to filter the partials, then dispatch a wave of Sonnet agents.

### 1A. `__declspec(noinline)` IPA defeat sweep
- **Source**: [fixable-macros.md § __declspec(noinline)](patterns/fixable-macros.md#__declspecnoinline-to-defeat-ipa-inlining)
- **Signal**: 70–80% caller with N extra inline instructions matching a helper body in the same TU; helper itself is 100% but never called.
- **Find candidates**: scan diff_inspect output for "extra-instructions-clustered-at-call-site" pattern. Could automate via `scripts/analysis/diff_inspect.py`.
- **Past wins**: `nandComposePerm` (lifted `nandGetStatus` 74.9→100%, `nandGetStatusCallback` 18.5→100%); `UnsetRun` (`OSSuspendThread` 74.6→100%). Both in SDK, but pattern works anywhere with `-ipa file`.
- **Estimated yield**: 5–15 wins. Higher in math-heavy or container-heavy TUs where IPA inlines aggressively.
- **Effort**: medium — needs diff-pattern detector; each fix is a one-line `__declspec(noinline)` annotation.

### 1B. `#pragma fp_contract off` sweep
- **Source**: [fixable-fsel-fma.md](patterns/fixable-fsel-fma.md)
- **Signal**: target emits separate `fsubs` + `fmuls` but ours emits fused `fmsubs` (or similar `fmadds` vs `fmuls`+`fadds`).
- **Find candidates**: grep partial-match functions in math-heavy units (`system/math/`, `system/char/`, `system/world/`, `system/bandobj/`) for inline `Cross`/`Dot`/`Normalize` calls.
- **Past wins**: `Spotlight::CalculateDirection` 88.1→93.4%, `MakeRotMatrix(Vec3, Vec3, Matrix3)` 87.6→94.0%.
- **Estimated yield**: 10–25 wins across math/world/char.
- **Effort**: low — one pragma block per function. Can be wave-dispatched per TU.

### 1C. `#pragma pool_data off` sweep
- **Source**: [fixable-macros.md § pool_data off](patterns/fixable-macros.md#pragma-pool_data-off)
- **Signal**: callee-saved register cascade where target leaves BSS base un-hoisted.
- **Past wins**: `CacheWii::WriteAsync` 85→100%, `GemTrack::DrawTrackElements` 92→94%.
- **Find candidates**: 80–95% functions that access BSS globals (heuristic: function body grep for top-level `g`-prefixed names that are likely globals).
- **Estimated yield**: 5–15 wins. Lower than fp_contract — applies in narrower circumstances.
- **Effort**: low per fix, but candidate identification is heuristic.

### 1D. `ObjPtr<T>.mPtr` direct access sweep
- **Source**: [fixable-declarations.md](patterns/fixable-declarations.md) + `feedback_objptr_direct_mptr` memory.
- **Signal**: function calls `operator T*()` on `ObjPtr` multiple times in the same expression / nearby; target reuses one load, ours emits multiple.
- **Find candidates**: ~821 `ObjPtr<` references in scope. Filter to partial-match functions in 85–99% band whose decl/source uses ObjPtr fields.
- **Past wins**: `CharEyes::SetFocusInterest` 90.8→100%; sister `DataNode *nPtr = &n;` pattern lifted `OutfitConfig::InMilo` 94→99.5%.
- **Estimated yield**: 10–30 wins.
- **Effort**: medium — needs per-function source inspection; can't grep purely.

## Phase 2 — Mechanical text-pattern sweeps (low risk, single-edit wins)

Patterns that grep can identify directly. Each one is a one-shot rewrite per call site.

### 2A. `!streq(a, b)` vs `if (strcmp(a, b))` sweep
- **Source**: [fixable-operators.md](patterns/fixable-operators.md) — `!streq` materializes via `cntlzw+srwi.+bne`; `if (strcmp)` emits `beq` differently.
- **Find candidates**: ~59 `!streq` / `if(strcmp(` call sites in scope. Cross-reference against partial-match function list to find the ones in 85–99% units.
- **Past wins**: `OutfitConfig::InMilo` 94.2→99.5% (combined with `nPtr` pattern).
- **Estimated yield**: 5–15 wins; per-fix is small but cumulative.
- **Effort**: very low — one Sonnet agent can sweep an entire TU.

### 2B. `Symbol::operator==(const char*)` vs `streq` sweep
- **Source**: [fixable-operators.md](patterns/fixable-operators.md).
- **Find candidates**: only ~3 `streq(...Str(), "...")` literal-shape calls left. Broader `streq` usage is ~50 — most don't fit this exact replacement.
- **Past wins**: `BandWardrobe::GetPrefab` 85.2→99.2% (with other fixes).
- **Estimated yield**: 2–5 — population mostly already swept.
- **Effort**: very low.
- **Recommendation**: do it but expect a small return.

### 2C. MILO_WARN format-string / arg-order audit
- **Source**: [fixable-operators.md](patterns/fixable-operators.md), wave-session-2026-05-23 § MILO_WARN argument and format.
- **Signal**: a single missing `\n`, swapped `%s` arg order, or extra space shifts `@stringBase0` offsets — can ripple through every function in the TU.
- **Find candidates**: ~42 multi-`%s` MILO_WARNs in scope. Audit each against the target binary's string pool when its TU has clustered partial matches.
- **Past wins**: `RndMesh::SkinVertex` 98.4→100% (single arg-order swap in sister function `CamShotFrame::Interp`).
- **Estimated yield**: 5–20 — but each fix can be a cascade unlock (entire TU's string offsets shift).
- **Effort**: medium per audit, very high cascade potential when found.

## Phase 3 — Cascade sweeps (high yield per edit, needs care)

One edit potentially unlocks many functions. Higher risk of regression (especially header edits — must A/B with full rebuild per `wave-session-2026-05-23.md` process notes).

### 3A. DC3 logic-bug cross-reference sweep
- **Source**: wave-session-2026-05-23 § "Algorithmic bug found via DC3 cross-reference".
- **Signal**: function at 90+% where the residual diff doesn't fit any compiler-pattern shape.
- **Find candidates**: `scripts/dc3_compare.py --filter system/ --min-rb3 88 --max-rb3 99 --sort rb3` — DC3-100% / RB3-partial functions.
- **Past wins**: `RndConsole::OnMsg(KeyboardKeyMsg)` 93.3→100%; revealed 6 latent source bugs this session (VocalTrainerPanel, VocalTrackDir, UIManager, CharEyes, Movie::Impl, ObjPtr<T>::Load near-miss).
- **Estimated yield**: 10–25 plus real bug catches.
- **Effort**: per-function — best run as a Sonnet wave with each agent doing DC3 side-by-side for one TU.

### 3B. Inline container helper methods in headers (qVector, qChain, etc.)
- **Source**: [fixable-declarations.md](patterns/fixable-declarations.md) + `feedback_inline_container_methods` memory.
- **Find candidates**: container types with partial-match instantiation chains. Past wins lifted `qChain::~qChain` 42→99% and `Key::Key` ctor 41→100% by adding an inline body in the header.
- **Estimated yield**: per edit, potentially huge (one header change cascades to every call site). But also risky.
- **Effort**: low edit / high A/B-test burden. **Header edits require user approval and full rebuild verification.**

### 3C. `__less<T>` specialization sweep
- **Source**: [fixable-declarations.md](patterns/fixable-declarations.md) + `feedback_stl_less_specialization` memory.
- **Find candidates**: TUs calling `std::sort(vec.begin(), vec.end())` with default `less<T>` where sort helpers are partial. **Apply today's prerequisites: integer/pointer compare only, trivially-copyable type only, current diff must show bool-mat.** This narrows from "all default-sort TUs" to a much smaller candidate set.
- **Past wins**: explicit `__less<T>` specialization in TU forces `bl __less<T>` call instead of inlined bool materialization.
- **Estimated yield**: 5–15 wins.
- **Effort**: low — sister pattern to today's sweep, same prerequisites apply.

## Phase 4 — LINKED-noise audit (lots of fns, mostly noise but cheap)

The 767 functions at 99.9–100% are almost certainly LINKED/ICF artifacts. Most won't pay off, but cheap to triage.

- **Source**: [verifiable-icf.md](patterns/verifiable-icf.md).
- **Approach**: scripted promotion of 99.9+ % functions to LINKED status (per `feedback_linking_sweep` memory). Update `config/SZBE69_B8/objects.json` for fns that turn out to be valid LINKED-merged.
- **Estimated yield**: status-only churn; no real match% movement, but useful for accurate progress reporting.
- **Effort**: very low — already scripted.

## Recommended execution order

1. **Phase 2A** (`!streq` sweep) — lowest risk, single-message wave, ~1 hour.
2. **Phase 1B** (`#pragma fp_contract off` sweep, math/world/char TUs) — wave dispatch, ~2 hours.
3. **Phase 3A** (DC3 logic-bug cross-reference) — wave dispatch on 88–99% RB3 / 100% DC3 fns, ~3 hours. Highest information content.
4. **Phase 1D** (`ObjPtr.mPtr` sweep) — wave dispatch on ObjPtr-heavy partials, ~2 hours.
5. **Phase 1A** (`__declspec(noinline)` IPA sweep) — needs diff-pattern detector built first.
6. **Phase 2C** (MILO_WARN audit) — opportunistic, run when a TU shows clustered 95–99% mismatches.
7. **Phase 1C** (`#pragma pool_data off`) — opportunistic, lower hit rate.
8. **Phase 3C** (`__less<T>`) — combine with today's sweep follow-ups since prerequisites are same.
9. **Phase 3B** (header container inlines) — **needs user approval**; high yield but high risk.
10. **Phase 4** (LINKED audit) — background cleanup, anytime.

## Anti-patterns / do-not-retry

Logged from today's sweep so future agents skip:

- **STL comparator specialization on float-compare comparators** (MessageTimer MaxSort/ObjSort, BandPatchMesh SortByZ, CameraManager NameSort, Stats PartPercentageSorter, CharClipGroup Alphabetically). See [fixable-macros.md § Confirmed-not-applicable](patterns/fixable-macros.md#confirmed-not-applicable-do-not-retry-without-a-new-approach).
- **`_vector_sized.c` reserve forms** — confirmed dead-end in wave-session-2026-05-23. Per memory `feedback_vector_sized_reserve_dead_end`.
- **Adding `-d STLPORT`** — already globally defined; network stagnation has different cause. Per memory `project_stlport_network_define`.
