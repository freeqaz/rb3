# Session 2026-05-26 — Normalized-masking auditor refinements

Follow-up to `2026-05-26-objdiff-metric-audit.md`. The first audit pass left
~50 in-scope REVIEW candidates of which dispatches confirmed ~10 real bugs
(wrong vtable slot, wrong struct size/field, wrong constants, wrong logic) and
the rest were three identifiable noise classes. This pass adds three filters
to `scripts/analysis/audit_normalized_masking.py` so those classes stop
surfacing as REVIEW.

Read-only run, no fleet impact (the auditor never builds).

## Mental model — what `value_sig` was already doing

`value_sig(opcode, typed_args)` produces a register-agnostic, frame/SDA-blind,
pool-symbol-collapsed signature `(opcode, tuple-of-value-tokens)` per
instruction. The auditor accumulates two multisets — one for target, one for
base — then takes the symmetric difference. A pure register-rename +
instruction-reorder nets to zero; only genuine value differences survive.

The token-extraction rules already in place before this refinement:

- **Frame/SDA** displacements (`[r1, r2, r13, sp, rtoc]`) on D-form loads/
  stores: drop the immediate. Stack-layout differences are benign.
- **Bare numeric branch targets** (`b 0xaac`): drop. Layout noise.
- **Pool symbols** (`@F_xxx`, `@D_xxx`, `@stringBase0`, `floatBase`,
  `doubleBase`): collapse all to the literal token `"POOL"` via `norm_sym`.
- **Anonymous-namespace discriminators** (`@unnamed@...`, `@...`): strip.
- **Compiler renumberings** (`__FUNCTION__$58088`, `foo__123`): strip the
  trailing `\$\d+$` or `__\d+$`.
- **Reloc with addend**: when a relocation symbol is present, the immediate
  argument is its addend (a pool-section offset) — drop the imm, keep the
  symbol. Pure-immediate instructions keep their imm; that's where genuine
  member offsets and constants live.

The categorizer then buckets each surviving sig:

| Category | Trigger | Disposition |
|---|---|---|
| `frame` | no value tokens after frame/SDA stripping | benign |
| `pool` | only sigs of `("sym", "POOL")` | benign |
| `reloc_target` | any non-POOL `("sym", X)` token | REVIEW (wrong callee/global) |
| `member` | pure-imm on a LOADSTORE opcode | REVIEW (wrong struct field / vtable slot) |
| `addr_const` | pure-imm on an ADDR_CALC opcode (`addi/addic./subi/addis`) | REVIEW (wrong address calc) |
| `constant` | pure-imm on any other opcode (`cmpwi`, `li`, `ori`, etc.) | REVIEW (wrong literal) |

## The three filters added in this refinement

### FILTER 1 — Assert/fail call windows

**Motivation.** `MILO_ASSERT(...)`, `MILO_FAIL(...)`, `HANDLE_CHECK(...)`,
and the `Notify` family all expand to a stereotyped 4–5-instruction prelude
before the actual `bl MakeString` / `bl Notify` / `bl Fail` / `bl __assert`
call:

```
li   r5, LINE                    # source line number — drifts on edit
addi r4, r3, @stringBase0+OFF    # format string — pool offset drifts
... __FUNCTION__$NNN address ... # function-name discriminator
bl   MakeString<...>             # or bl Fail__5Debug...
```

All three move on every editing pass without indicating a semantic difference.
The first audit pass surfaced multiple Tour/WaitingUserGate/SongStatusMgr
functions as REVIEW solely because their `li r5, LINE` shifted between
target and base.

**Implementation.** `find_assert_windows(instrs, side)` linearly scans one
side's instruction stream for any `bl` whose `args` text contains one of:

```python
ASSERT_CALL_FRAGMENTS = (
    "MakeString",        # MILO_ASSERT/MILO_FAIL format-string builder
    "Notify__5Debug",    # Debug::Notify
    "Fail__5Debug",      # Debug::Fail
    "__assert",          # raw libc assert
)
```

For each match at index `idx`, it adds `{idx-5 .. idx}` to a blocked set
(constants `ASSERT_WINDOW_LOOKBACK=5`, `ASSERT_WINDOW_LOOKAHEAD=1`). The
main loop unions both sides' windows.

**Semantics in `diff_one`.** Inside a window, **pure-immediate** sigs are
dropped. Sigs that contain a `Symbol` token survive — a wrong-callee reloc
inside an assert window is still real (the `__FUNCTION__` pool symbol is
already collapsed to a constant `__FUNCTION__` via `norm_sym`, so it doesn't
trip).

```python
def _is_pure_imm(sig):
    if sig is None: return False
    _, vals = sig
    return bool(vals) and all(v[0] == "imm" for v in vals)
if in_assert_window:
    if _is_pure_imm(ts): ts = None
    if _is_pure_imm(bs): bs = None
```

### FILTER 2 — `@stringBase0` pool-base offset detection

**Motivation.** `addic. r4, r3, 0x3e` reads at a glance like an enum compare,
but if r3 was set up via the canonical MWCC pool-base load idiom (`lis r3,
@stringBase0; addi r3, r3, @stringBase0`) then `0x3e` is just the offset of
some string inside the per-TU string-pool section, which re-orders with every
source edit. This was the **PatchPanel::OnMsg(ButtonDownMsg/ButtonUpMsg)
false positive** (uniform `+0x21` shift across the addic. chain) and the
**TambourineManager::OnPlayTambourine** false positive (uniform `+0x4c`).

The bare `value_sig` already handles the case where `@stringBase0` appears in
the same instruction's typed_args (e.g. `addi r4, r30, 0x42, @stringBase0`)
because the symbol token causes the immediate to be dropped. The case it
didn't handle: when the pool base was loaded into a register two instructions
earlier and the current `addic.` references it via register without the
relocation symbol appearing in its own typed_args.

**Implementation.** Two functions:

`find_pool_base_regs(instrs, side)` — per-side linear scan producing a dict
`{idx: frozenset_of_pool_regs_BEFORE_this_insn}`. State machine:

- `lis rB, @stringBase0` → mark `saw_lis[rB] = True`; clear rB from holding.
- `addi/addic./addic. rD, rB, @stringBase0[+K]` →
  - if rB is in holding (chained pool-base), `holding.add(rD)`.
  - else if `saw_lis[rB]` is pending (typical lis/addi pair), `holding.add(rD)`.
  - else (raw pool-addi without preceding lis), `holding.add(rD)` anyway —
    the `@stringBase0` reloc in this instruction's typed_args alone is
    sufficient evidence.
- Any other write to a register clears its pool-base status. Writes that
  don't modify a register's value (compares, stores, branches, ctr/lr
  moves) are explicitly listed so they don't clear it.

The snapshot is taken **before** the instruction is processed, so an `addic.
r4, r4, K` reads the pre-write pool state correctly. (Initial bug: I was
snapshotting after the write, which caused `addic. r4, r4, K` to clear r4's
status before it could be checked — false-positive REVIEW on TambourineManager
even with the filter on. Fix: move `pool_at[idx] = frozenset(holding)` to the
top of the loop body.)

`is_pool_offset_addr(opcode, typed_args, pool_regs_at_idx)` — returns True if
this is an `addi/addic./addic./subi` AND its base register (second `Register`
typed_arg) is in the pool set. The main loop uses this to drop both ts and bs
when the respective side's instruction is a pool-offset compute.

### FILTER 3 — Literal-vs-reloc address construction

**Motivation.** `lis r0, 0x80be; addi r3, r3, 0x3c08` (pure-immediate
two-instruction address construction) and `lis r0, __vt__...; addi r3, r3,
__vt__...` (relocated construction) resolve to the exact same final pointer —
only the linker discipline differs. Target sometimes emits one form, base the
other, depending on whether the compiler chose to fold the literal at compile
time or wait for the linker. This was the **Crowd::resize** false positive
where target had `lis r0, 0x80be / addi r3, r3, 0x3c08` and base had `lis r0,
__vt__29ObjPtr<...> / addi r3, r3, __vt__29ObjPtr<...>`.

**Implementation.** A pre-pass over diff_arg instructions builds the index
set `lit_vs_reloc_block`. For each diff_arg at index `idx`:

- Extract `Symbol` and `Signed/Unsigned` typed_args from both sides.
- Define `t_has_vt = any "__vt__" in target's Symbol args`, same for base.
- Case A: target has no Symbol args AND base has a `__vt__` Symbol AND
  target's immediate(s) fit in 16 bits (`-0x10000..0xffff`) — add idx.
- Case B: base has no Symbol args AND target has `__vt__` AND base's
  immediate(s) fit in 16 bits — add idx.

In the main loop, both `ts` and `bs` are unconditionally set to `None` at any
index in `lit_vs_reloc_block`. This is cleaner than the original "cancel by
matching leftover counts" approach I tried first — the leftover-counter
approach failed because target's pure-imm sig and base's `__vt__` sym sig
have DIFFERENT keys, so they never net via simple `Counter` subtraction; you
either have to cancel by tracking per-side indices then post-process, or
suppress at extraction time. Suppress-at-extraction time is what landed.

The low-half-fit check (`is_low_half_imm`) is a sanity guard: a real
wrong-callee bug would carry an immediate too large to be the low half of a
linker-pooled vtable address, so this should not cause false negatives. (`lis`
takes a 16-bit shifted-up half; `addi` an `s16` low half. Both fit
`-0x10000..0xffff`.)

### FILTER 4 — Pool-ordinal renumbering (verified, already worked)

`__FUNCTION__$58088` vs `__FUNCTION__$46643` already collapsed via
`norm_sym`'s `\$\d+$` strip:

```python
>>> norm_sym('__FUNCTION__$58088')
'__FUNCTION__'
>>> norm_sym('__FUNCTION__$46643')
'__FUNCTION__'
```

Confirmed with `value_sig` end-to-end. No code change needed; verified for
posterity.

## Before/after counts

| | first audit (per-session doc) | refined audit (this run) |
|---|---|---|
| audited functions | 1310 | 1154 (lower — build has progressed) |
| BENIGN | 1235 | 1104 |
| REVIEW | 75 | **50** |
| in-scope REVIEW | ~49 | **24** (after also dropping Wii-specific `os/{HolmesClient,Joypad,CacheMgr_Wii,PlatformMgr_Wii}` and `synthwii`) |

The drop from ~49 → 24 in-scope is **filters 1+2+3 doing their job** on the
PatchPanel/Tambourine/Tour line-number/pool-offset families, not the build
progressing.

### Spot-check matrix — known cases before vs after the refinement

| function | before refine | after refine | reason |
|---|---|---|---|
| `PatchPanel::OnMsg(ButtonDownMsg)` | REVIEW (`addr_const=6`) | **BENIGN** | filter 2 (pool-base `addic.`) |
| `PatchPanel::OnMsg(ButtonUpMsg)` | REVIEW (`addr_const=4`) | **BENIGN** | filter 2 |
| `TambourineManager::OnPlayTambourine` | REVIEW (`addr_const=2`) | **BENIGN** | filter 2 |
| `TourDescPanel::LoadIcons` | REVIEW (mixed) | **BENIGN** | filter 2 (pool addi w/ @stringBase0 already in typed_args) |
| `TourDescPanel::FinishLoad` | REVIEW | **BENIGN** | filter 1 (assert window) + filter 2 |
| `TourDescPanel::Handle` | REVIEW | **BENIGN** | filter 1 + 2 |
| `Crowd::resize<CharData>` | REVIEW (`reloc_target=4`) | **BENIGN** | filter 3 |
| `Game::AddPlayer` (real bug) | REVIEW (`member=2`) | **REVIEW** | unchanged — real wrong-vtable-slot |
| `SpotDrawParams::Load` (real bug) | REVIEW (`addr_const=2`) | **REVIEW** | unchanged — struct layout +4 |
| `StoreRootPanel::__ct__/__dt__` (real) | REVIEW (`addr_const=4`) | **REVIEW** | unchanged — +0xc layout |
| `SongParser::TrackAllowsOverlappingNotes` (real) | REVIEW | **REVIEW** | unchanged — logic bug |
| `SaveLoadManager::HandleEventResponse` (real) | REVIEW | **REVIEW** | unchanged |
| `SongUpgradeMgr::AddUpgradeData` (real) | REVIEW | **REVIEW** | unchanged |

## Top 15 in-scope REVIEW candidates after refinement

Sev = `10×reloc_target + 8×member + 5×constant + 3×addr_const`. In-scope
defined as `main/band3/*` or `main/system/*` excluding `rndwii`, Wii-specific
OS subdirs (`os/HolmesClient`, `os/Joypad`, `os/CacheMgr_Wii`,
`os/PlatformMgr_Wii`, `synthwii`), and SDK code.

| sev | unit | symbol | shape | bug-class guess |
|---:|---|---|---|---|
| 48 | `system/rndobj/BoxMap` | `__ct__14BoxMapLightingFv` | `stw r0, 0x0, r8` vs `stw r0, 0x578, r3` (6 sites) | struct init: target uses temp ptr r8 + offset 0, base writes through r3 + 0x578. Possibly wrong ctor base or different aggregate init shape. |
| 40 | `system/obj/Utl` | `NextName__FPCcP9ObjectDir` | `bl _savegpr_26` vs `bl _savegpr_27` | known-benign `_savegpr_N` count mismatch (frame size diff). Filter target for future session. |
| 40 | `band3/tour/QuestFilterPanel` | `Mat__19QuestFilterProviderCFiiP10UIListMesh` | `bl _savegpr_27` vs `_savegpr_26` | same benign class. |
| 40 | `band3/meta_band/MusicLibrary` | `Text__12MusicLibraryCFiiP11UIListLabelP7UILabel` | `bl _savegpr_25` vs `_savegpr_26` | same benign class. |
| 26 | `system/beatmatch/SongParser` | `TrackAllowsOverlappingNotes` | `subi r4,0x4`/`li r3,0x1` vs `subi r4,0x3`/`li r3,0x2` | **real bug** — extra `kTrackVocals` (fix documented in prior session, not yet committed). |
| 24 | `system/rndobj/Mesh` | `SetVolume__7RndMeshFQ27RndMesh6Volume` | `lfs f1, 0x4c(r1)` vs `lfs f1, 0x4(r19)` | structural — stack-temp `0x4c(r1)` vs member-load `0x4(r19)`. Different stack vs through-pointer flow. Likely at-limit, not a bug. |
| 20 | `band3/meta_band/AccomplishmentPanel` | `FillSetlistWithAccomplishmentSongs` | `bl GetAccomplishmentProgress` vs `bl _outline_GetAccomplishmentProgress` | known-benign `_outline_` helper choice. |
| 20 | `system/os/System` | `SetSystemLanguage__F6Symbolb` | `bl Node__9DataArrayCFi` vs `bl Node__9DataArrayFi` | known-benign const-overload selection. |
| 20 | `band3/meta_band/BandSongMgr` | `GetSongIDFromShortName` | `cmpwi r0, 0x0` vs `cmpwi r0, -0x2` | **real bug** — `kSongID_Invalid` cross-TU sentinel (header `-2`, binary `0`). Flagged for sweep in prior session. |
| 20 | `system/speex/libspeex/nb_celp` | `nb_decode` | `li r6, 0x52a/0x576` vs `0x52d/0x579` | speex internal — third-party (`lib/`), out of port scope. |
| 16 | `band3/game/Game` | `AddPlayer__4GameFP8BandUser` | `lwz r12, 0xd8(r12)` vs `0xc4(r12)` | **real bug** — wrong virtual (`PostDynamicAdd` should be `Start`). Fix documented, not committed. |
| 16 | `band3/game/Singer` | `AddToFreestyleDeployment__6SingerFf` | `lfs f2, 0x60(r3)` vs `lfs f2, 0x5c(r3)` | struct field +4 — Singer's freestyle data member missing/shifted. |
| 16 | `band3/meta_band/BandStorePanel` | `Enter__14BandStorePanelFv` | `lwz r3, 0x0(r3)` vs `lwz r3, 0x4(r3)` | member offset 0 vs 4 — field-read order or wrong vtable construction. |
| 16 | `band3/game/Player` | `FinalizeStats__6PlayerFv` | `lwz r12, 0x10(r12)` vs `lwz r12, 0x18(r12)` | wrong vtable slot — 0x10 to 0x18 is two slots up (or wrong inherited sub-object). |
| 16 | `band3/meta_band/SongSort` | `BuildSetlistTree__11SetlistSortF...` | `lwz r12, 0xf0(r12)` vs `lwz r12, 0xc4(r12)` | wrong virtual method dispatch — large delta (0x2c bytes = 11 slots). |

Additional notable entries below the top 15:

- `band3/meta_band/StoreRootPanel :: __ct__/__dt__` (sev 12) — `addi r0, r3,
  0x38` vs `0x44`. Documented real bug (3 phantom fields removed in prior
  session, fix not committed).
- `band3/meta_band/SaveLoadManager :: HandleEventResponse` (sev 10) — wrong
  state compare 0x61/0x5f. Documented.
- `band3/meta_band/SongUpgradeMgr :: AddUpgradeData` (sev 10) — same
  `kSongID_Invalid` sentinel as BandSongMgr.
- `system/rndobj/Text :: GetDistanceToPlane` (sev 10) — `li r29, 0x1` vs
  `0x0`. Real bool/logic constant.
- `system/dsp/PitchDetector :: __ct__` (sev 10) — `li r3, 0x64` vs `0x60`.
  Wrong constant; could be wrong alloc size or wrong default value.
- `band3/meta_band/BandSongMgr :: ContentDone` (sev 8) — `lwz r0, 0x0(r25)`
  vs `lwz r0, 0x78(r1)` — frame vs through-ptr; possibly stack-temp shape.
- `band3/meta_band/ContentDeletePanel :: OnMsg(UITransitionCompleteMsg)` (sev
  8) — `lwz r0, 0xc(r1)` vs `lwz r0, 0x0(r3)` — same shape, frame vs ptr.
- `world/SpotlightDrawer :: Load__14SpotDrawParamsFR9BinStreami` (sev 6) —
  documented +4 layout.

**Real bugs estimated in top 15 + immediate follow-on**: ~10 (`BoxMap` ctor,
`TrackAllowsOverlappingNotes`, `BandSongMgr` + `SongUpgradeMgr` kSongID,
`AddPlayer`, `Singer` field, `BandStorePanel` offset, `Player::FinalizeStats`
vtable, `SongSort::BuildSetlistTree` vtable, `StoreRootPanel` size,
`SaveLoadManager` state, `RndText::GetDistanceToPlane`, `PitchDetector` ctor).

**Known-benign retained noise**: ~5 (`_savegpr_N` ×3, `_outline_` ×1, const-
overload ×1).

## What this refinement does NOT filter

These remain on the REVIEW list as classes documented in the previous
session's "benign classes the audit correctly de-emphasized" list. Filtering
them is **out of scope for this session** because each carries a non-trivial
risk of suppressing real bugs:

- **`bl _savegpr_N` vs `_savegpr_{N±1}`** — different callee-saved-register
  count (frame-size delta). The asymmetry IS information: it shows the base
  is spilling more/fewer registers than target. But that's caused by register
  allocation, not by code semantics. Filter target: recognize the
  `_save?pr_/_rest?pr_` symbol family explicitly and treat reloc-target diffs
  on those as benign.
- **`bl _outline_FOO` vs `bl FOO`** — MWCC outlining heuristic difference.
  Filter target: when one side has `_outline_X` and the other has `X` at the
  same instruction index, treat as benign. Risk: a real wrong-callee bug
  could hide here if a similarly-named outline target exists.
- **`bl Node__9DataArrayCFi` (const) vs `bl Node__9DataArrayFi` (non-const)**
  — const-overload selection. Semantically benign in this codebase. Filter
  target: when the mangled names differ only in trailing `C` before the `F`
  arg-list, treat as benign.
- **`lis r4, .text.14121` vs `lis r4, VoiceTakeoverCallback`** — function-
  pointer literal vs named symbol (local label vs named callback).

If a future session needs these filtered, the pattern is identical to
filter 3: detect the symbol-name pattern at the same instruction index and
suppress both sides' sigs.

## Build/codegen edge cases noted during development

1. **`addi rD, rB, K, @stringBase0`** — the JSON encodes this as four
   typed_args: `[Register rD, Register rB, Signed K, Symbol @stringBase0]`.
   Because there's a symbol token present, `value_sig` already drops the
   immediate (the `vals = syms if syms else imms` line). The pool-base filter
   handles the OTHER case: `addic. rD, rB, K` with NO symbol — meaning rB
   must have been set up as a pool reg by a prior instruction.

2. **Multiple pool regs alive simultaneously.** `find_pool_base_regs` uses a
   `set` per index so multiple registers can hold the pool base in parallel.
   The Tour functions chain pool-base loads (`addi r30, r27, @stringBase0`
   etc) and we correctly track both.

3. **Snapshot ordering.** Snapshot must be BEFORE the instruction's write,
   because `addic. r4, r4, K` reads-then-writes r4 — if we snapshot after,
   `r4` is missing from the pool set when the filter check runs.

4. **Compares/stores/branches don't clear pool regs.** The else-branch
   exclusion list in `find_pool_base_regs` lists `cmpw{,i,l,li}`, all stores
   (`stw{,u}`, `stb{,u}`, `sth{,u}`, `stfs{,u}`, `stfd{,u}`, `psq_st{,u}`,
   `stmw`), branches, `bl`, ctr/lr moves, `mtspr`. Without these exclusions,
   a `cmpwi r4, 5` in the middle of a string-pool chain would falsely clear
   r4.

5. **`_is_pure_imm` accepts NON-empty all-imm tuples only.** A `()`-tuple sig
   (no value tokens — the "frame" case) is benign on its own merits; treating
   it as "pure imm" and then suppressing it would be a no-op anyway, but the
   defensive check is `bool(vals) and all(...)`.

6. **`is_low_half_imm` range.** `lis` immediate is a 16-bit value sign-
   extended then shifted left 16, so it fits `[-0x10000, 0xffff]` as a
   nominal scalar. The check `-0x10000 <= iv <= 0xffff` covers both signed
   and unsigned interpretations. Real wrong-callee bugs would carry full
   32-bit address constants, which would fail this check.

## Implementation history during this session

Three iterations of filter 3:

1. **First attempt — post-leftover cancellation.** Compute the symmetric
   difference, then for each leftover pure-imm sig, look up which side it
   came from and check if the OTHER side had a `__vt__` symbol at the same
   index. Cancel the count.
   - **Failed because** the `__vt__` symbol sig is ALSO a leftover on the
     other side (it doesn't get cancelled by the imm sig in the Counter
     subtraction since they have different keys). I'd cancel the imm side
     but the sym side still surfaced as `reloc_target`.

2. **Second attempt — track per-sig instruction indices, cancel both sides
   by index.** This works but is bookkeeping-heavy and conflates two
   independent concerns (sig extraction vs leftover cancellation).

3. **Final — pre-pass index set, suppress at sig-extraction time.** The
   `lit_vs_reloc_block` is computed once before the main loop and consulted
   per instruction. Both sides' sigs become `None` at any blocked index. Simple,
   correct, and consistent with the pattern used by filters 1 and 2.

Two iterations of filter 2:

1. **First attempt — snapshot pool state AFTER processing.** Failed because
   `addic. r4, r4, 0xf0` cleared r4 from holding before its own check fired.
   TambourineManager remained REVIEW.

2. **Final — snapshot BEFORE processing.** Move the `pool_at[idx] =
   frozenset(holding)` line to the top of the per-instruction body so a
   read-modify-write reads the pre-write state.

Filter 1 worked first try once I figured out which `bl` targets to match.

## Files touched

- `scripts/analysis/audit_normalized_masking.py` — added the filter helpers
  (`find_assert_windows`, `find_pool_base_regs`, `is_pool_offset_addr`,
  `is_low_half_imm`) and the inline filter-3 pre-pass; wired them into
  `diff_one`. Removed the dead leftover-cancellation code.
- `docs/sessions/2026-05-26-audit-refinements.md` — this doc.
- `/home/free/.claude/projects/-home-free-code-milohax-rb3/memory/MEMORY.md`
  — appended one-line addendum to the `Normalized-masking audit findings`
  entry pointing at this doc.
- `/tmp/objdiff_audit/results_refined.json` — full post-refinement audit
  output (gitignored tmp).

## Reproduce

```bash
python3 scripts/analysis/audit_normalized_masking.py --workers 8 \
    --out /tmp/objdiff_audit/results_refined.json
```

Takes ~10s on a fresh build. No `--build` flag, no ninja-lock contention.
Output: same console summary as the original auditor + JSON file for
post-processing.

## Suggested follow-ups

1. **Land the documented real-bug fixes** that surface in the top 15
   (AddPlayer, StoreRootPanel, SongParser, kSongID_Invalid sweep). Most were
   diagnosed in the previous session but never committed. The auditor is
   already a structural-bug finder; capitalize on it.
2. **Investigate the new high-sev entries**: BoxMapLighting ctor (sev 48),
   Singer::AddToFreestyleDeployment (struct field +4), BandStorePanel::Enter
   (member offset), Player::FinalizeStats (vtable slot), SongSort::
   BuildSetlistTree (vtable slot). These were not in the original session's
   list — likely new since the metric got honest.
3. **Optionally add filters 5+ for the remaining noise classes** (`_savegpr_N`,
   `_outline_`, const-overload, `.text.NNN`) once the high-value bug list
   above is worked. The doc above sketches each pattern.
4. **Re-run periodically** — `python3 scripts/analysis/audit_normalized_
   masking.py` is read-only and fast. Treat it as a structural-bug
   regression detector: if a function appears as REVIEW unexpectedly, the
   metric is hiding a new bug.
