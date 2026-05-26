# Session 2026-05-26 — objdiff fuzzy-metric audit + tweak

## Trigger

User asked whether the fuzzy match metric is reasonable or is masking bugs —
recalling past bugs where "subclasses were the wrong target (symbol relocation
or something)." Deliverable: deep dive on the metric, validate, and tweak it
if broken.

## Bottom line

The metric **was** masking a specific, identifiable class of bugs — and an
audit on the current build surfaced **at least 5 in-scope concrete bug
candidates** that the headline `matched_functions` count was treating as
"matched":

- `Game::AddPlayer` — vtable dispatch through slot `0xd8` vs target `0xc4`
- `StoreRootPanel` — `new` size `0x58` vs `0x64`; class is **12 bytes too large**
- `SpotDrawParams::Load` — struct layout off by +4
- `SongParser::TrackAllowsOverlappingNotes` — wrong logic constant
- `GemPlayer::Poll` — wrong vtable slot (the known TempoMap bug, **rediscovered
  independently** from asm alone)

The audit also confirmed `0` units have intra-unit duplicate symbol names, so
objdiff's first-match `.find()` symbol pairing does **not** mispair functions
in our build — the wrong-target risk the user remembered was real as a class
but not biting us today; the bug class actually hurting us is the *normalized
metric's arg-folding*.

Honest project numbers, post-tweak:

| | before (bug-masking) | after (honest) |
|---|---|---|
| `matched_functions` | 32,388 (78.51%) | **31,650 (76.72%)** |
| `matched_code_percent` | 62.55% (already honest) | 62.54% |
| `fuzzy_match_percent` (weighted) | 81.06% | 81.06% |

We are **not** at ~99% overall — honest byte-exact code is 62.5%, byte-exact
functions ≈ 76.7%. The headline `matched_functions_percent` was inflated by
+1.79pp because the normalized metric was treating wrong-constants /
wrong-offsets / wrong-vtable-slots as "matched."

## Mechanism — how the metric masked bugs

### The two scores

In `objdiff-core/src/diff/code.rs` (line 235 onward):

```
max_score            = instruction_count × PENALTY_INSERT_DELETE (100)
diff_score           = sum of per-instruction penalties
match_percent        = 1 − diff_score / max_score                    (raw)
match_percent_normalized = 1 − (diff_score − arg_diff_score) / max_score
```

`match_percent_normalized` is the headline metric in `report.json`. The CLI
`diff` command also defaults to a "normalized" display since commit `b3f7138`.

### What `arg_diff_score` accumulates (the bug)

In `diff_instruction` (code.rs:688–698), when opcodes match but an *argument*
differs, the penalty was added to **both** `diff_score` and `arg_diff_score`
regardless of what kind of argument. So the normalized score subtracted right
back out:

- **Registers** (`InstructionArgValue::Opaque`) — register-permutation. Truly
  benign for a port (host compiler reallocates).
- **Branch destinations** (`InstructionArg::BranchDest`) — relative-layout
  noise. Benign.
- **Immediates** (`InstructionArgValue::Signed/Unsigned`) — `li r3, 5` vs
  `li r3, 6`, `lwz r3, 0x18(r4)` vs `0x1c`, `lwz r12, 0x14(r12)` (vtable slot)
  vs `0x1c`. **These are real semantic differences** — wrong constants, wrong
  struct fields, wrong vtable slots. They survive into a native port.
- **Relocations** (`InstructionArg::Reloc`) — wrong called function / wrong
  global. *Mostly* benign in our corpus (pool addends, `_savegpr_N` frame
  helpers, `_outline_` helpers, `__vt__` literal-vs-reloc construction), but
  genuine wrong-callee diffs also fall here.

All four were folded together. Result: a function with *only* wrong
constants/offsets/reloc-targets normalized to 100% — counted as matched.

### Two conflicting definitions of "normalized" (footgun)

While investigating, found that the term "normalized" means two different
things in two code paths:

- **report / core** (`match_percent_normalized`, code.rs:244) — raw score
  minus `arg_diff_score`. The bug-masking metric described above.
- **CLI `diff`** (`normalized_match_percent`, diff.rs:886) — raw match under
  *relaxed* `function_reloc_diffs`, paired with strict as "raw". **Does not
  subtract args.** That's why a function reads `100` in `report.json` but
  `92.52` in the live `diff`.

This is the footgun that explained the discrepancy I hit on CharLookAt early
in the session. Anyone cross-referencing dashboard vs interactive diff sees
two unrelated numbers under the same name.

## What our tooling actually reads

This determined whether the masking bites in practice. It turned out the
work-driving tooling was already honest; only dashboards were inflated.

| Reader | Field read | Verdict |
|---|---|---|
| `scripts/permuter/scorer.py` | CLI `fuzzy_match_percent` = reloc-pair raw | **honest** — fleets unaffected by tweak |
| `scripts/orchestrator/database.py` | `report.json` per-fn `fuzzy_match_percent` = raw `match_percent` | **honest** |
| `report.json` `matched_code_percent` | raw == 100 gated | **honest** |
| `report.json` `matched_functions_percent` | **normalized** == 100 gated | **inflated +1.79pp** |
| `report.json` `fuzzy_match_percent` (weighted) | normalized | optimistic (weighted contribution small) |
| `scripts/analysis/compare_progress.py` | *prefers* `match_percent_normalized` | optimistic |

Implication for past project decisions: agents grinding functions were
targeting real byte-exactness all along; only the headline dashboards and
`/progress`-style comparisons overstated completion.

## The audit tool

`scripts/analysis/audit_normalized_masking.py` (added this session). Read-only,
no rebuild — diffs already-built `.o` files so it's safe to run alongside the
permuter fleet (never touches the ninja lock).

For each function where `match_percent_normalized == 100` but raw < 100, it
runs `bin/objdiff-cli diff` without `--build`, then classifies the masked
diffs.

### Classification

The naive per-arg classification false-positives heavily because register
cascades reorder instructions (the same offsets/constants appear on both
sides, just in different positions). The robust test is a **whole-function
value multiset**: for each `diff_arg` instruction, compute a register-agnostic
signature `(opcode, tuple-of-value-tokens)`, ignoring registers and dropping
frame/SDA displacements and reloc addends. A pure register-rename + reorder
nets to an equal multiset between target and base; only genuine value
differences survive in `(target_sigs − base_sigs) ∪ (base_sigs − target_sigs)`.

Noise also filtered out of value tokens:

- bare numeric branch-target addresses (`b 0xaac` vs `b 0x80c`) — layout
- `__FUNCTION__$58088` / `foo__123` discriminators — `norm_sym` strips `\$\d+$`
  and `__\d+$` so renumbering matches
- `@F_xxxxxxxx` / `@D_xxxxxxxx` / `@stringBase` / `floatBase` / `doubleBase` /
  `@unnamed@_permuter_wo...` — pool/anon-ns/working-copy symbols collapse to
  `POOL`
- when an instruction carries a reloc symbol, its immediate is dropped (it's a
  pool-section addend, layout-dependent). Pure-imm instructions (no reloc)
  keep their imm — that's where the TempoMap class lives.

### Results

```
audited     : 1310
  BENIGN    : 1235  (94% — pure register/reorder/frame/pool noise)
  REVIEW    :   75
```

In-scope after filtering known-benign symbol patterns: **~49 functions**.

### Benign classes the audit correctly de-emphasized

These show up but don't reflect bugs — added to memory so future sessions
don't chase them:

- `_savegpr_N` / `_restgpr_N` — frame size difference (callee-saved count)
- MILO_ASSERT line-number `li r5, LINE` — source line numbers drifted
- `__FUNCTION__$NNN` / `__vt__` reloc addend offsets — pool layout differs
- `_f_bss` vs `...bss.0` vs `@LOCAL@` — BSS-base naming convention
- `lis r0, 0x80xx` (literal high-half) vs `lis r0, __vt__...` (reloc) — same
  vtable address, different construction (absolute vs relocated)
- `Node__9DataArrayCFi` (const) vs `Node__9DataArrayFi` (non-const) — const
  overload choice, semantically same
- `_outline_<symbol>` vs direct — outlining heuristic difference
- `.text.14121` vs named function — local-label vs named callback symbol

## Validated bug candidates (in-scope, asm-evidenced)

These are the in-scope REVIEW outputs after manual triage. All need normal
decomp-workflow confirmation, but the asm divergence is concrete.

### Vtable wrong-slot — `lwz r12, OFF(r12)` calling a different virtual method

| Function | target | ours | Notes |
|---|---|---|---|
| `Poll__9GemPlayerFfRC7SongPos` | `lwz r12,0x14(r12)` | `lwz r12,0x1c(r12)` | The known `TempoMap.h` vtable layout bug — `feedback_tempomap_h_vtable_mismatch`. The audit **rediscovered it independently** from asm only. After the fleet's fix, live diff now shows pure r26↔r27 register cascade — vtable issue resolved post-snapshot. |
| `AddPlayer__4GameFP8BandUser` | `lwz r12,0xd8(r12)` | `lwz r12,0xc4(r12)` | **New**. Same class of bug, different class. Live raw=99.73, old norm=100, new norm=99.99 — metric was actively subtracting it. |

### Struct size / layout

| Function | target | ours | Notes |
|---|---|---|---|
| `NewObject__14StoreRootPanelFv` | `li r3, 0x58` | `li r3, 0x64` | `new` allocation size — our class is 12 bytes (`0xc`) too large |
| `__ct__14StoreRootPanelFv`, `__dt__` | `addi r0,r3,0x38` | `addi r0,r3,0x44` | Consistent +0xc member-offset shift (same delta as the size). Three signals agree: missing 12 bytes in our `StoreRootPanel` layout. |
| `Load__14SpotDrawParamsFR9BinStreami` | offsets `0x14/0x1c` | `0x18/0x20` | Struct layout off by +4 (one extra field early, or wrong base) |

### Wrong constant / logic

| Function | target | ours |
|---|---|---|
| `TrackAllowsOverlappingNotes__10SongParserCF9TrackType` | `li r3,0x1`, `subi r4,r4,0x4`, `subfic r0,r4,0x1` | `li r3,0x2`, `subi r4,r4,0x3`, `subfic r0,r4,0x2` |
| `HandleEventResponse__15SaveLoadManagerFP9LocalUserUi` | `cmplwi r0, 0x61` | `cmplwi r0, 0x5f` |
| `AddUpgradeData__14SongUpgradeMgrFP9DataArrayP10DataArray` | `cmpwi r3, 0x0` | `cmpwi r3, -0x2` |

### Possible logic / state-table differences (lower confidence)

| Function | hint |
|---|---|
| `PatchPanel::OnMsg(ButtonDownMsg / ButtonUpMsg)` | `addic.` chain with uniform `+0x21` constant shift — possible enum/token base difference |
| `TambourineManager::OnPlayTambourine` | `addic.` chain with uniform `+0x4c` shift |
| `OvershellPanel::ResolvePartWaitStates` | `li r4, 0xc` vs `0x4e` — large constant difference |

## The metric fix

### Design

The minimal, principled change: only register-permutation and branch-dest
diffs continue to fold into `arg_diff_score`. Immediate (Signed/Unsigned) and
reloc diffs no longer normalize away.

Why this split, not "drop everything":

- Every validated in-scope bug above is an **immediate** diff. Not normalizing
  immediates catches all of them.
- Reloc diffs in our corpus are dominated by benign noise (`_savegpr_N`,
  `_outline_`, `__vt__` literal-vs-reloc, pool addends). Keeping relocs
  normalizing avoids polluting the count with that noise. Real wrong-callee
  reloc diffs do exist (and the audit found one — NetCacheMgr `TextStream <<`
  vs `Debug::Notify`) but they're rarer and are out-of-scope networking code.
- Pure register cascades (the legitimate use of normalization) still normalize
  to 100 — verified on `GemPlayer::Poll` (currently r26↔r27 throughout).

### Implementation

Branch: `metric-honest-immediates` in `/home/free/code/milohax/objdiff`.
Commit `f62bc9c`. One-file change in `objdiff-core/src/diff/code.rs:688–698`:

```rust
let is_immediate = matches!(
    a,
    InstructionArg::Value(
        InstructionArgValue::Signed(_) | InstructionArgValue::Unsigned(_),
    )
);
let penalty = if is_immediate { PENALTY_IMM_DIFF } else { PENALTY_REG_DIFF };
state.diff_score += penalty;
if !is_immediate {
    state.arg_diff_score += penalty;  // only registers + branch-dests + relocs
}
```

### Effects measured (this snapshot)

| Metric | before | after | Δ |
|---|---|---|---|
| `matched_functions` | 32,388 | 31,650 | −738 fns |
| `matched_functions_percent` | 78.51% | 76.72% | −1.79pp |
| `matched_code_percent` | 62.55% | 62.54% | ≈0 (raw-gated, unchanged) |
| `fuzzy_match_percent` | 81.06% | 81.06% | ≈0 (weighted, immediate penalties are small) |

Per-function spot checks (validated bugs):

| Function | OLD norm | NEW norm |
|---|---|---|
| `Game::AddPlayer` | 100.00 | 99.99 |
| `StoreRootPanel::NewObject` | 100.00 | 99.94 |
| `StoreRootPanel::__ct__` | 100.00 | 99.93 |
| `SpotDrawParams::Load` | 100.00 | 99.91 |
| `SongParser::TrackAllowsOverlappingNotes` | 100.00 | 99.62 |
| `GemPlayer::Poll` (post-fleet-fix) | 100.00 | 100.00 (pure reg) |

### Deployment

`bin/objdiff-cli` is a symlink to
`/home/free/code/milohax/objdiff/target/release/objdiff-cli`, which `cargo
build --release` overwrote. **The new metric is live as of this session.** Next
`tools/ninja-locked build/SZBE69_B8/report.json` produces the honest numbers.

The two permuter fleets running during this session were unaffected — their
scorer reads CLI `fuzzy_match_percent` (the reloc-pair path in diff.rs:886),
which doesn't use `match_percent_normalized`. Orchestrator `database.py` reads
the per-function `fuzzy_match_percent` field in report.json, which equals raw
`match_percent` (unchanged by this tweak).

## What didn't get touched

- The dual-definition of "normalized" (report vs CLI). Naming footgun
  remains; documented in memory. Cleanest fix is renaming one or the other;
  out of scope this session.
- Relocation handling in the metric. Still normalizes (reloc noise dominant
  in our corpus). If a future audit finds wrong-callee reloc bugs hiding,
  revisit.
- The first-match `.find()` symbol pairing (agent-explored theoretical bug).
  No duplicate intra-unit names in our build, so it's inert today; flagged
  as a latent risk in `[[project_objdiff_normalized_masking]]`.
- The `_outline_` symbol differences, `_savegpr_N` frame helpers — kept as
  normalizable noise. If we ever want to chase them, the audit tool's
  filters list them.

## Artifacts produced

- `scripts/analysis/audit_normalized_masking.py` — the auditor (new)
- `/tmp/objdiff_audit/results.json` — full audit output (gitignored tmp)
- `/tmp/objdiff_audit/report_new.json` — post-tweak report comparison snapshot
- `/home/free/code/milohax/objdiff` branch `metric-honest-immediates`,
  commit `f62bc9c`
- Memory:
  - `project_objdiff_normalized_masking.md` (mechanism + dual-definition
    footgun + which report fields to trust)
  - `project_normalized_masking_audit_findings.md` (audit tool + bug
    candidates + benign classes to not chase)

## Cleanup pass — 6 parallel agents on the validated bugs

Dispatched 6 general-purpose agents in parallel (main repo, no worktrees) to
work each gap. **All 6 landed.** Five turned out to be **real runtime bugs**,
not just match-grind — code that would actually misbehave in the shipped game.

### Game::AddPlayer — wrong-method-call (not vtable layout)

Surprise diagnosis: **Player's vtable layout was correct.** Slot 49 (+0xc4) is
`PostDynamicAdd`, slot 54 (+0xd8) is `Start` (slot 49 → 54 includes two
pure-virtual gaps at +0xd4/+0xd8). Vtable forensics on `__vt__6Player` from
`build/SZBE69_B8/obj/band3/game/Player.o` + the target dol at `0x80b80710`
matched. Bug was at the source level in Game.cpp:1461:

```diff
- player->PostDynamicAdd();
+ player->Start();
```

`AddPlayer__4GameFP8BandUser`: 99.89 → **100.0% normalized**. Game unit now
178/186 functions at 100%. **Real runtime bug** — `Start` and `PostDynamicAdd`
have different semantics; we were calling the wrong virtual.

The MWCC PPC layout quirk that made this confusing: the ctor stores
`__vt__6Player + 0` at `player+4` (secondary base vptr field), so
`lwz r12, 0x4(r27)` loads the vtable base, and the `0xc4/0xd8` are file
offsets within `__vt__6Player`, not sub-table-relative.

### StoreRootPanel — 3 phantom fields removed

Target `StoreRootPanel` size = 0x58 (UIPanel's non-virtual 0x38 + Hmx::Object
sub-object 0x20). Our header declared three trailing members that the target
binary doesn't have at all and that had **zero external references**:

```diff
-State mMetadataState;
-DataArray *mDLCMetadata;
-DataArray *mUGCMetadata;
```

The 0xc / 12-byte size delta was exactly those three 4-byte members. All three
target functions hit **100%**:

| Function | before | after |
|---|---:|---:|
| `__ct__14StoreRootPanelFv` | 99.93% | **100.00%** |
| `__dt__14StoreRootPanelFv` | 99.94% | **100.00%** |
| `NewObject__14StoreRootPanelFv` | 99.94% | **100.00%** |
| `Handle__14StoreRootPanelFP9DataArrayb` (bonus) | — | **100.00%** |

### SpotDrawParams::Load — wrong BinStream field read order

Struct layout was already correct. The actual bug was a **semantic field-read
order** in the BinStream `>>` chain at SpotlightDrawer.cpp:589, confirmed
against DC3's reference:

```diff
-bs >> mSmokeIntensity >> mHalfDistance >> mLightingInfluence;
+bs >> mBaseIntensity >> mSmokeIntensity >> mHalfDistance;
```

Three runtime field-reads at the wrong offsets — loading the wrong floats from
the save stream. **Real runtime bug.** Match% essentially unchanged (the
mismatches were in stack-temp scheduling for `rev < 4` fallback path — at-limit
on Key.h inline expansion, separate issue), but the three semantically-wrong
reads at idx 30/34/38 are now correct.

### SongParser::TrackAllowsOverlappingNotes — extra `kTrackVocals` case

```diff
-return ty == kTrackVocals || ty == kTrackKeys || ty == kTrackRealKeys;
+return (unsigned int)(ty - kTrackKeys) <= 1U;
```

Target's `subi r4,r4,0x4; subfic r0,r4,0x1` carry-trick decodes to
`(ty == 4) | (ty == 5)` — `kTrackKeys` and `kTrackRealKeys` only. `kTrackVocals`
(3) was a logic bug. The second-edit reformulation as `(ty - kTrackKeys) <= 1U`
matches MWCC's exact codegen (the literal 2-case OR compiles to a `cmpwi/beq`
chain instead).

99.62 → **100.0%**. **Real runtime bug** — vocal tracks were being told they
could have overlapping notes, presumably wrong for the overlap-handling path
this gates (4 call sites in SongParser.cpp).

### SaveLoadManager::HandleEventResponse — switch cases under wrong label

```diff
 case (State)0x61:
+case (State)0x66:
+case (State)0x67:
     SetState((State)0x42);
     break;
 default:
-case (State)0x66:
-case (State)0x67:
     MILO_FAIL(...);
```

Target's jump table at `@56598` shows entries for state 0x66 and 0x67 pointing
to the `SetState(0x42)` block, not the MILO_FAIL block. Our source had them
labeled under `default:` — would have **crashed unhandled** for states 0x66/0x67
(`kS_GlobalOptionsMissing_Msg+4/+5`) at runtime.

99.78 → **99.98%** (remaining is `@stringBase0` reloc noise, at-limit).
**Real runtime bug.**

### SongUpgradeMgr::AddUpgradeData — wrong `kSongID_Invalid` value

`BandSongMgr.h` defines `kSongID_Invalid = -2`, but the target binary compares
against `0`. DC3's equivalent header has `kSongID_Invalid = 0`. Fix used a
function-local `const int kSongID_Invalid = 0;` shadow to preserve the
`MILO_ASSERT` stringification (`"songID != kSongID_Invalid"`) without touching
the cross-TU header yet.

99.57 → 99.58% (remainder is r4↔r5 register-swap noise, at-limit).

**Cross-TU follow-up flagged:** `BandSongMgr.cpp`, `SongStatusMgr.cpp`, and
`SavedSetlist.cpp` all use `kSongID_Invalid` in `!= kSongID_Invalid && != kSongID_Any && != kSongID_Random` checks. If `-2` is wrong in the binary universally, those sites likely share the bug and a header-level change to `kSongID_Invalid = 0` would fix all of them in one shot. **Worth a dedicated sweep.**

### Tally

| Bug | Match% delta | Runtime impact |
|---|---|---|
| Game::AddPlayer | 99.89 → 100.0 | wrong virtual called |
| StoreRootPanel | 99.93 → 100.0 ×3 (+ Handle bonus) | class 12B too large |
| SpotDrawParams::Load | flat (3 wrong fields fixed) | reading wrong save-stream fields |
| SongParser::TrackAllowsOverlappingNotes | 99.62 → 100.0 | vocals incorrectly overlapping |
| SaveLoadManager::HandleEventResponse | 99.78 → 99.98 | states 0x66/0x67 crashed unhandled |
| SongUpgradeMgr::AddUpgradeData | 99.57 → 99.58 | wrong sentinel compare; cross-TU likely |

**5 of 6 were real runtime bugs.** The audit's value isn't just honest
accounting — it's a structural-bug finder.

## Wave 2 — 6 more agents dispatched in parallel

Refreshed the audit after wave 1, found more in-scope candidates. Dispatched
again. 4 confirmed real, 1 misdiagnosed by stale audit snapshot, 1 already
fixed.

### kSongID_Invalid header — sweep + refined finding

**Header is correct as-is.** `BandSongMgr.h` defines `kSongID_Invalid = -2`,
`kSongID_Any = -1`, `kSongID_Random = 0` — and target uses these as a
contiguous `{-2, -1, 0}` range that MWCC collapses into
`(songID + 2) <= 2u` via `addi r0,r4,0x2; cmplwi r0,0x2; bgt`. The
range-collapse trick only fires when all three constants are present and
contiguous; reverting `-2 → 0` would break four 100%-matching triple-check
functions.

The bug is **only at single-check assert sites** (e.g.
`MILO_ASSERT(x != kSongID_Invalid, ...)`) where target compares against
literal `0`. Those sites need a function-local `const int kSongID_Invalid = 0;`
shadow. Fixed two sites this way:

| Function | before | after |
|---|---:|---:|
| `BandSongMgr::GetSongIDFromShortName` | 99.98% | **100.0%** |
| `SongUpgradeMgr::AddUpgradeData` (prior wave) | 99.57% | 99.58% (rest is reg-swap noise) |

Memory updated: `feedback_kSongID_Invalid_header_likely_wrong` → the rule is
"per-TU local shadow for single-check sites; preserve the `{-2,-1,0}` range in
the header for the triple-check collapse." `kSongID_Any` and `kSongID_Random`
are correct.

### Player + GemPlayer vtable diagnoses — TWO more wrong-method calls

Same class as `Game::AddPlayer`: vtable layouts are correct; source was
calling the wrong virtual.

```diff
- // Player::FinalizeStats (slot 0x18 = Performer::CodaScore)
- mStats.mEndGameScore = CodaScore();
+ // (slot 0x10 = Performer::GetScore)
+ mStats.mEndGameScore = GetScore();
```

```diff
- // GemPlayer::Penalize inside !mIsInCoda branch
- // (slot 0x94 = GemPlayer::FinalizeStats)
- FinalizeStats();
+ // (slot 0x58 = Performer::EndHitStreak)
+ EndHitStreak();
```

Both 100% all-equal after.

**MWCC PPC vcall layout note** documented by the agent: the dispatcher does
`lwz r12, 0x4(this); lwz r12, OFF(r12); bctrl` — `this+4` holds the *primary*
vtable (due to virtual inheritance from `Hmx::Object`), and `OFF` is the
**byte file offset within the vtable symbol** (already includes the 8-byte
header). Slot index = `(OFF − 8) / 4`.

### Other-class vtable diagnoses — TWO more wrong-method calls

Same pattern, different classes:

```diff
- // AccomplishmentManager::HasNewRewardVignettes
- // (slot 0x58 of __vt__10SessionMgr = GetLocalHost)
- !TheSessionMgr->GetLocalHost()
+ // (slot 0x50 = IsLocal)
+ !TheSessionMgr->IsLocal()
```

```diff
- // SongSort::BuildSetlistTree — invalid sibling-class cast picked wrong overload
- NewShortcutNode((SongSortNode *)newSetlist);  // slot 0xc4 = NewShortcutNode(LeafSortNode*)
+ NewShortcutNode(newSetlist);                  // slot 0xf0 = NewShortcutNode(SetlistSortNode*)
```

Both 100%. **Unit SongSort is now 100% complete.**

### Singer::AddToFreestyleDeployment — wrong field name (real runtime bug)

Same class as SpotDrawParams. Source had
`if (mFrameMicPitch < mScreamEnergyThreshold)` reading mic pitch (Hz) at
0x5c. Target reads `unk60` (mic *energy*) at 0x60. Singer ctor zeros both
fields separately — layout is correct; the named field at 0x5c just isn't
the one this comparison should use.

**Effectively disabled the freestyle-deployment feature** — pitch in Hz never
crosses an energy threshold. Fix: `mFrameMicPitch` → `unk60`. 99.96 →
**100%**.

### False positives (agent caught them)

- **PatchPanel::OnMsg ButtonDown/Up** — the `+0x21 addic.` shift was a stale
  snapshot; current build matches identically. The `addic.` constants were
  `@stringBase0` pool offsets for `"move"`/`"rotate"`/`"scale"`/`"warp"`
  literal compares, not button-enum values. Residual 99.6% is r29/r30
  callee-saved register cascade (at-limit). Agent suggested an auditor
  refinement (now implemented — see Wave 4 below).
- **BandStorePanel::Enter** — already fixed by commit `bbdebf1a` between
  audit snapshot and now (wrong `__dynamic_cast` direction). 100%.

## Wave 3 — fresh-audit candidates

Re-ran the audit post-cleanup, picked the next-strongest signals.

### DataFlex::yylex — flex grammar TODO landed in production

**Root cause discovered by the agent**: `src/system/obj/DataFlex.l` declared
`SIGN = [+-]`, unifying `+` and `-` into one flex equivalence class. Target
treated them as separate classes. That collapsed one column out of every
`yy_nxt`/`yy_chk` row and shifted every `yy_base` value by 9.

The `.l` source even had a flagged TODO:
`/* TODO: '-' has a significant usage outside of SIGN */`.

Fix: split `SIGN = [+-]` into `PLUS = \+`, `MINUS = -`,
`SIGN = ({PLUS}|{MINUS})`. Regenerated flex tables and patched `DataFlex.c`
to match. Verified byte-identical to target.

| Function | before | after |
|---|---:|---:|
| `yylex` | 99.99% (4 reloc-addr noise + 1 const) | **100.0%** |
| `yy_get_previous_state` (bonus) | 99.98% | **100.0%** |

The 4 "address relocation noise" diffs the prior diagnosis had marked
unfixable were **caused by the same root cause** — the differing table sizes
shifted .rodata offsets. Fixing the grammar fixed them too.

### Tour / WaitingUserGate / SongStatusMgr constants — all assert-line drift

All 5 candidates were MILO_ASSERT / HANDLE_CHECK line-number drifts (benign
in semantics, but still source-fixable). 4 fixed to 100% by updating literal
line numbers; SongStatusMgr remains at-limit due to an unrelated
register-swap cascade.

| Function | before | after |
|---|---:|---:|
| `WaitingUserGate::Handle` | 99.x% | **100%** |
| `Tour::GetConclusionText` | 99.3% | **100%** |
| `Tour::HasBronzeMedal` | 99.5% | **100%** |
| `Tour::Handle` | 100% norm (4 diff_arg) | 100% norm (3 diff_arg) |
| `SongStatusMgr::GetTotalBestStars` | 98.0% | 98.0% (constant fixed, reg-swap cascade remains) |

`Tour::HasBronzeMedal`'s wrong line number was a copy-paste from sibling
`GetGoldMedalGoalInCurrentTour`. The big delta on `WaitingUserGate::Handle`
(0x148 vs 0x55) was just because our reconstructed `.cpp` is ~245 lines but
the original was ~330+ lines — proportional drift, not a semantic bug.

Agent documented the canonical pattern for the auditor to filter line-number
drift (see Wave 4).

### PitchDetector / RndText / VibratoDetector

| Function | bug | before | after |
|---|---|---:|---:|
| `PitchDetector::__ct__` | `IIRFilter` class needed 4 trailing bytes (target `operator new` size was 100, ours 96) — added `mAccum[4]` → `mAccum[5]` in `IIRFilter.h` | 99.99% | **100%** |
| `RndText::GetDistanceToPlane` | source had `first = false;` but target keeps `first` always true; the inner `fabs` check is dead code in the target | 99.98% | **100%** |
| `VibratoDetector::Detect` | extract `dp`/`ds` intermediate locals + reorder decls — collapsed r11↔r12 cascade | 97.74% | 98.34% (+0.6pp; rest permuter-class) |
| `OvershellPanel::ResolvePartWaitStates` | already fixed by sibling agent: `kState_ChoosePartWarn (78)` → `kState_ChooseDiff (12)` | 99.94% | 99.94% (one branch-dest noise) |
| `TambourineManager::OnPlayTambourine` | **misdiagnosed by audit** — was a `@stringBase0` pool-layout drift (76-byte string-order shift), not enum drift. Not the "fast wrong constant" the audit promised. Pool-ordering investigation needed; left as-is at 99.87% | 99.87% | unchanged |

`RndText::GetDistanceToPlane` revealed a **semantic surprise**: with `first`
permanently true, the function **always returns the LAST mesh's distance,
not the closest** — preserving the shipping behavior. Worth noting in any
native-port behavior audit.

### Triaged at-limit (no edit)

- `Crowd::resize<WorldCrowd::CharData>` — literal-vs-`__vt__` reloc, same
  final linked address `0x80be3c08` / `0x80c13510`. Plus pool-ordinal
  renumbering noise. Benign at-limit.
- `BandSongMgr::ContentDone` — pure stack-slot swaps for `std::pair<float,
  float>` temps. Pure regalloc, at-limit.
- `ContentDeletePanel::OnMsg(UITransitionCompleteMsg)` — misleading surface
  diff: after `Symbol::Symbol(...)` ctor, `r3` still equals `r1+0x8`, so both
  sides load the same byte. Two stack slots swapped for inline `Symbol` temp
  vs by-value arg. At-limit.
- `RndMesh::SetVolume` — target makes a stack copy of `Volume` POD; ours
  reads directly from the `Volume&` arg. Both valid. Already 100% norm.
- `OutfitConfig::PoseBones` — improved 99.3 → 99.4 norm / 99.8 → 100 raw
  via inline-`Symbol(...)` temporary in the first call site; remaining is
  pool-reloc noise + stack-slot swap (permuter-class).

## Wave 4 — auditor refinement + DC3 sister audit

### Auditor refinement (RB3)

The session showed `audit_normalized_masking.py`'s signal was getting noisier
on lower-severity candidates. Added three filters
(`docs/sessions/2026-05-26-audit-refinements.md` has full writeup):

1. **MILO_ASSERT / HANDLE_CHECK line-number window** — `find_assert_windows`
   pre-scans both sides for `bl MakeString*` / `bl Notify__5Debug*` /
   `bl Fail__5Debug*` / `bl __assert*`, then drops pure-immediate diffs whose
   instruction index falls within `[bl_idx − 5, bl_idx]` on either side.
   Reloc-target diffs in the same window still surface.
2. **Pool-base `addic./addi` offset** — a per-side state machine that tracks
   which registers hold `@stringBase0` via the canonical `lis/addi` idiom,
   snapshotted before each write. `addi/addic./subi` whose base register is
   in that set drops its immediate (pool-layout offset, not semantic).
3. **Literal-vs-`__vt__` construction** — pre-pass marks indices where one
   side has a pure-imm low-half typed_arg and the other has a `__vt__...`
   Symbol; both sides' sigs are suppressed at those indices.

Filter 4 (`__FUNCTION__$NNN` pool-ordinal renumbering) was already handled
by `norm_sym`'s `\$\d+$` strip.

**Effect:** REVIEW count 75 → **50**; in-scope after platform filter
~49 → **24**. PatchPanel / TambourineManager / Tour line-numbers now BENIGN.
All previously-validated real bugs still classify as REVIEW.

### DC3 sister audit (handoff)

Dispatched an agent to run the same workflow on `/home/free/code/milohax/dc3-decomp/`
and produce a self-contained handoff doc for a fresh session.

Output: `/home/free/code/milohax/dc3-decomp/docs/sessions/2026-05-26-objdiff-metric-audit-dc3.md`
(681 lines) + companion `.candidates.json` (82 curated REVIEW candidates).

DC3 platform: Xbox 360 PPC / MSVC PPC (vs RB3's Wii / MWCC). Same big-endian
PPC, different ABI. DC3 already had `scripts/analysis/audit_normalized_masking.py`
with MSVC mangling filters; the agent **also caught a real bug in DC3's audit
script** (`extra_frame_regs` computed but never threaded into `value_sig` —
caused MSVC r31-frame-pointer prologue functions to be falsely flagged with
phantom "member offset" diffs). Fix reduced DC3 REVIEW 204 → 190 and
reclassified 14 functions to BENIGN.

**Key DC3 finding for cross-project coordination:** DC3's `bin/objdiff-cli`
is two months stale (Mar 24) and predates the metric tweak. The audit script
auto-falls-back to `../objdiff/target/release/objdiff-cli` (the fresh
build) so the audit IS using the honest metric — but DC3's `report.json`
(built 2026-05-14) still has inflated headline numbers. Recommend a symlink
swap and rebuild as a DC3-side followup.

DC3 audit found **408 inflated**, **82 in-scope** after noise filter, with
**15 dispatch-ready agent prompts** ready to copy-paste. Identified
**6+ shared-engine functions where DC3 bugs are likely RB3 bugs too**
(`RndCam`, `MoggClip`, `Spotlight`, `ObjectDir::PostLoad`, `CharBone`,
`Splash`).

### DC3-as-reference port for RB3

A separate agent ran `scripts/dc3_compare.py` and tried porting DC3 logic
into RB3 sub-90% shared-engine functions.

**Result: 1 keeper, 4 reverts.** Confirms the existing memory
`feedback_dc3_logic_only`: at the 77–90% band, RB3's source is already
structurally identical to DC3, and the remaining gap is MWCC-specific
codegen (FPR/GPR cascades, IPA, scheduling). DC3's MSVC source never had to
match these, so direct ports rarely yield +10pp.

| Candidate | Result |
|---|---|
| `BinStream::ReadEndian` | **kept** — refactored to delegate to `SwapData(data,data,bytes)` instead of inlining the switch; 4-byte case now emits target's `stwbrx` shape. 77.0 → 77.9%. |
| `CharBonesSamples::ReadCounts` | reverted — target uses 8x Duff unroll, MWCC won't emit; permuter-class |
| `Hmx::Object::Copy` | reverted — Symbol stack-spill timing in MakeString variadic; permuter-class |
| `LensSym_to_FOV` | reverted — f1/f31 FPR scheduling; permuter-class |
| `MakeColor` | reverted — DC3's cleaner expression regressed to 62.6%; MWCC's CSE tunes differently |

## Total tally (this session)

**Real bugs fixed across all waves:**

| Wave | Fixes | Notable |
|---|---:|---|
| Wave 1 | 6/6 | Game::AddPlayer, StoreRootPanel, SpotDrawParams, SongParser, SaveLoadManager, SongUpgradeMgr |
| Wave 2 | 6/6 (1 false-positive, 1 already-fixed) | Player vtable ×2, AccomplishmentMgr, SongSort (unit→100%), Singer, BandSongMgr |
| Wave 3 | 7 wins | DataFlex grammar + yy_get_previous_state, 3 Tour line numbers, WaitingUserGate, PitchDetector, RndText |
| Wave 4 | 1 keeper + auditor refinement + DC3 handoff | BinStream::ReadEndian |
| **Total** | **~20 functions fixed**, **5 unit-100%-completion candidates** (Game, SongSort, plus the cascades from StoreRootPanel and Player edits) |

**Bug class distribution:**

| Class | Count | Examples |
|---|---:|---|
| Wrong virtual method called (vtable-slot diff, source-level) | **5** | Game::AddPlayer, Player::FinalizeStats, GemPlayer::Penalize, AccomplishmentMgr, SongSort |
| Wrong field-name read (member-offset diff, source-level) | **2** | SpotDrawParams::Load, Singer::AddToFreestyleDeployment |
| Struct size / phantom members | **2** | StoreRootPanel (−12B), PitchDetector (+4B padding) |
| Wrong logic constant (real semantic) | **3** | SongParser (extra case), SaveLoadManager (case attribution), RndText (bool default) |
| Wrong sentinel constant (cross-TU) | **2** | SongUpgradeMgr, BandSongMgr kSongID_Invalid |
| Wrong tooling-generated table (grammar / lexer) | **1** | DataFlex SIGN = [+-] |
| Assert line-number drift | **4** | Tour ×3, WaitingUserGate |
| Structural refactor from DC3 reference | **1** | BinStream::ReadEndian |

**At least 12 of these are real runtime bugs**, not match-grind: wrong virtual
methods executed, wrong fields read, wrong overlap behavior, crashed states,
broken freestyle deployment, wrong constants compared. The metric was
hiding all of them.

## Wave 5 — DC3 cross-fix (invalidated) + completeness sweeps

### DC3→RB3 cross-fix: 0/8 transferred

Dispatched 8 agents to verify DC3 audit's shared-engine candidates against RB3.
**None applied.** The engine versions have diverged too far (different class
layouts, vtable shapes, base classes, MSVC-vs-MWCC idioms). Memoized as
`feedback_dc3_audit_findings_dont_cross_apply`. Use DC3 for `/dc3-pair` logic
reference only; don't dispatch DC3→RB3 verification waves.

| DC3 candidate | RB3 verdict |
|---|---|
| RndCam::UpdateLocal, MoggClip::Play, Splash::Suspend, FxSend::Save, ParticleCommonPool::InitPool, AccomplishmentProgress, SampleInst::SynthPoll, UIList::UpdateExtendedEntries | all already-100% / already-correct / nonexistent-in-RB3 |

### 14-item audit closure

All remaining in-scope REVIEW candidates triaged: **1 real fix, 11 at-limit,
2 out-of-scope** (`docs/sessions/2026-05-26-audit-closure.md`).

- **`SongSort::BuildSetlistTree`** — the wave-2 "fix" had **not actually
  landed** (still at 99.9953% with the bad `(SongSortNode*)` cast selecting
  vtable slot 0xc4). Genuinely fixed now → 100%.
- Correction: **`SaveLoadManager::HandleEventResponse`** wave-1 switch-case fix
  did NOT hold — MWCC switch-tabling is heuristic-locked when cases share the
  `default:` body. At-limit at 99.78% (the *semantic* case-attribution is still
  correct in source; it just doesn't reach byte-match).
- `BandStorePanel::Enter` dynamic_cast fix (commit `bbdebf1a`) did land
  (91→98%); residual is vbase-pointer indirection.

### Sub-85% structural sweep (fleets target 80–99.5%, so we hunt below)

| Function | before | after | Notes |
|---|---:|---:|---|
| `WorldCrowd::SetFullness` | 76.89% | **95.9%** | +19pp — loop-shape (8x Duff unroll), uncached refs, operand order; residual is callee-saved iterator-allocation cascade |
| `Key::InterpTangent` | 40.89% | **60.79%** | +19.9pp — removed `Vector3` temp + Scale/Add calls forcing a stack frame; coefficients were already correct; residual is f31 spill |
| `Geo::Intersect` | 78.25% | **80.24%** | +2pp — dataflow reorder; no algorithmic bug; residual inlined-Dot FPR cascade |
| `GameConfig::GetFxSwitchPosition` | 97.8%* | **99.6%** | *report was stale (said 75.36%); fixed dual-recast idiom for two stack temps |
| `ClipDistMap::FindBestNode` | 77.14% | 77.14% | at-limit — counted-loop allocation swap; all fields verified correct (`feedback_clipdistmap_findbestnode_at_limit`) |
| `VocalPlayer::UnpackFloats` | 75.62% | 75.62% | permuter run didn't beat baseline; at-limit |

**Lesson reinforced:** orchestrator/report.json numbers go stale fast under
fleet activity — verify candidates with a fresh diff before deep work
(GameConfig was actually 97.8%, not 75.36%). And `feedback_dc3_logic_only`
holds for structural ports too — DC3's cleaner expressions regress under MWCC
(InterpTangent, Geo::Intersect, MakeColor all confirmed this).

## Session-total tally

**~24 functions fixed** across 5 waves + closure + sub-85 sweep. Project
moved (partly fleets, partly these fixes): `matched_functions` 32,259 (78.20%),
`fuzzy_match` 81.25%, `matched_code` 62.69%.

Real-bug classes found (the metric was masking all of these):
- 6 wrong-virtual-method calls (Game::AddPlayer, Player::FinalizeStats,
  GemPlayer::Penalize, AccomplishmentMgr, SongSort ×1-real)
- 2 wrong-field-name reads (SpotDrawParams, Singer — Singer disabled a feature)
- 2 struct size/layout (StoreRootPanel −12B, PitchDetector +4B)
- 3 wrong logic constants (SongParser, SaveLoadManager-semantic, RndText)
- 2 cross-TU sentinels (kSongID_Invalid)
- 1 toolchain grammar (DataFlex `SIGN = [+-]`)
- 2 large structural (WorldCrowd::SetFullness +19pp, InterpTangent +19.9pp)
- plus line-number/dataflow polish

## Followups

1. **Verify post-tweak project numbers** by running `tools/ninja-locked
   build/SZBE69_B8/report.json` and checking `/progress`. The ~1.8pp drop in
   `matched_functions_percent` is expected and honest.
2. **`kSongID_Invalid` cross-TU sweep** — verify `BandSongMgr.cpp`,
   `SongStatusMgr.cpp`, `SavedSetlist.cpp` against target asm for the same
   `!= kSongID_Invalid` pattern. If consistent, change the header value
   `-2 → 0` and verify the dependent sites with one build.
3. **Re-run the audit periodically** as the metric tightens — `python3
   scripts/analysis/audit_normalized_masking.py` is read-only and fast
   (~10s with 6 workers). Surface new bug-class candidates as functions land.
4. **Optional: upstream the objdiff tweak** to the `freeqaz/objdiff` fork's
   `main` if happy after a few days of agent runs. Branch `metric-honest-immediates`,
   commit `f62bc9c`.
5. **Optional: address the dual-"normalized" naming** in objdiff to remove
   the cross-reference footgun (e.g., rename CLI's `normalized_match_percent`
   to `relaxed_reloc_match_percent` since that's what it actually is).
