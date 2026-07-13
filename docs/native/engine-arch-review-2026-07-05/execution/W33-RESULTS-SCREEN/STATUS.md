# W33-RESULTS-SCREEN — STATUS (Lane 1)

Base SHA: `6186706e`. Build: BandCharDesc.cpp (shared decomp) + rb3_platform_native.cpp (native glue). Boot runs: 5 (bound ≤6).

## Acceptance (quoted verbatim from WAVE33_KICKOFF.md, CA-amended)

> - Full flow proof: boot → song → results → CONFIRM → back to shell screen, captured (screen-name evidence via the harness dta/uidump used by prior waves), run rc=0 (exit-trap teardown tolerated). This flow is currently IMPOSSIBLE — it is the headline deliverable.
> - V3(a): a deliberately-triggered assert on the fixed path renders its text (screenshot or stderr capture); the overshell "PLAY ON XBOX LIVE" `OvershellSlotState` MILO_FAIL either fixed (if native-only state bug) or documented-faithful (stubbed-online path) — do NOT paper over it silently.
> - V5: results screen shows a real artist string and numeric SOLO SCORE, or an honest separate-mechanism finding.
> - Gates: batch_objdiff on every touched shared-decomp unit — baseline-exact OR improved (state which per unit, with numbers; CA5 table governs); /data-diff for the V2 data symbol (CA4); drawlog-golden per CA2 (--fixed-clock --canonical-order, PASS 792); rb3-tests per CA3 (123/116/7/0 same-or-better); boot A/B if any new flag.
> - Bounds: ≤6 boot runs.

CA4 (V2): `sDrumVenueMappings` retail = 0x2C (11 ptrs) vs our 10; fix = append `""` sentinel, unconditional, NO physical-line add (`__LINE__` hazard :57/:358/:609). CA5 baselines: Debug 100.0, MakeString 100.0, OvershellSlotState 100.0, BandWardrobe 99.50284, BandCharDesc 99.318756. CA6: BandWardrobe → Lane 1; V3(b) file = OvershellSlotState.cpp:181.

## Self-grade vs quoted text: PASS (all bullets met)

### Full flow proof — PASS
`flow_proof.py` (evidence/raw/flow_proof_engine.log.gz, evidence/flow_health.jsonl):
`song_select → part_difficulty → game_screen → {game jump 600000} → coop_endgame_screen → **CONFIRM** → accomplishments_newaward_screen → hint_goalcomplete_screen → **song_select_screen** (shell)`. Engine alive throughout, rc=0. Previously IMPOSSIBLE (CONFIRM SIGSEGV'd in NameToDrumVenue).

### V2 (primary) — FIXED (decomp-data correction, unconditional)
STEP-0 (evidence/lane1-step0-v2.json): DECOMP DIVERGENCE — retail table 0x2C=11 ptrs (symbols.txt:56210, map:61104), 11th entry = `""` sentinel (distinct reloc `@21543`, points into zero-pad after "none"; `*"" == 0` terminates the loop). Our table lost it → `NameToDrumVenue("")` (unload path via `BandWardrobe::OnUnloadVenue:953 SetTempoGenreVenue(Symbol(),Symbol(),"")`) walked to index 10 OOB → SIGSEGV.
Fix: appended `""` on `BandCharDesc.cpp:21` **in place** (no physical line added; :57/:358/:609 preserved). Verified:
- /data-diff `sDrumVenueMappings__12BandCharDesc`: **93.07359 → 100.0** (base_size 40→44, mismatch 0).
- batch_objdiff: `NameToDrumVenue` **100.0**, `DrumCallback` **100.0** (COMPLETE/COSMETIC).
- unit `main/system/bandobj/BandCharDesc` fuzzy **99.318756** = CA5 baseline-exact.

### V3(a) — FIXED (native strong-def; asserts DISPLAY)
STEP-0 (evidence/lane1-step0-v3a.json): `NetworkSocket_Stub.cpp` is `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE`, so `NetworkSocket::GetHostName()` resolved to the weak trampoline `__hmx_tramp_dta_125` (`xorl %eax,%eax; ret`) — returns an **unconstructed String**. `Debug::Modal` (Debug.cpp:377) formats it via `FormatString::operator<<(const String&) → str.c_str()` (MakeString.cpp:312) → SIGSEGV, masking **every** assert routed through Modal.
Fix: strong native `String NetworkSocket::GetHostName() { return String(""); }` in `rb3_platform_native.cpp` (same weak-stub-override class as the `PlatformMgr::GetName` precedent in that file; faithful to Wii `NetworkSocket_Wii.cpp:82`). ZERO shared-decomp/`__LINE__` touch → CA5 Debug/MakeString 100.0 untouched. Wins over `.weak` stub; also fixes HolmesClient callers.
Proof (`v3_overshell_proof.py`, evidence/raw/v3_overshell_engine.log.gz): fired the genuine `overshell:attempt_register_online` (main-thread `slot->Handle`, NOT dta/eval which sigsetjmp-skips Modal). Assert now RENDERS its full banner:
```
APP FAILED
OvershellSlotState 139 does not exist
...
ConsoleName:    Build: 100807   Plat: xbox
Lang: eng   SystemConfig: config/band_keep.dta
```
`ConsoleName:` line (empty hostname = GetHostName returned "") printed; **SIGSEGV count = 0** (see evidence/raw/grep_table.txt). Formatter completes; asserts display.

### V3(b) — DOCUMENTED-FAITHFUL (stubbed-online path; not papered over)
`OvershellSlotStateMgr::GetSlotState` (OvershellSlotState.cpp:181) MILO_FAILs when the online-register flow (`AttemptRegisterOnline → BeginOverrideFlow(kOverrideFlow_RegisterOnline) → GenerateCurrentState → GetSlotState(id)`) requests a state id not in the DTA-registered `mStates` (observed id 139). This is the **online-register path** — out of port scope (Nintendo WFC dead 2014; `TheServer` is a zeroed data stub, OvershellSlot.cpp:99). No native code edited; with V3(a) fixed the assert now **DISPLAYS honestly** ("OvershellSlotState 139 does not exist" + OSFatal) instead of silently crashing the formatter. NOT papered over (no dummy return added to GetSlotState — that would mask genuine missing-state bugs for legit callers). OvershellSlotState.cpp unit stays 100.0 (untouched). A real fix requires online-registration implementation (future scope).

### V5 — RESOLVED as separate-mechanism (artist correct)
`v5_results_uidump.py` /api/uidump on coop_endgame_screen (evidence/v5_results_labels.txt, evidence/raw/v5_uidump.json.gz):
- `artist.lbl` = **'Avenged Sevenfold'** (CORRECT — real artist string), `song.lbl` = 'Beast and the Harlot'.
- The visual-pass "j0"/"GO" were MISREADS of `highscore_1.lbl` = `'Previous Best: <alt>j</alt> 0'` and `solo_highscore1_label01.lbl` = `'Previous Best: <alt>G</alt> 0'` — the `<alt>…</alt>` rich-text icon-glyph markup on the leaderboard "Previous Best" lines (glyph + value 0), NOT artist/score-value corruption and NOT the V3(a) formatter family.
Finding: artist string is real & correct; the `<alt>` leaderboard-glyph rendering on Previous-Best lines is a distinct minor item, deferred (own charter). Satisfies "real artist string … or an honest separate-mechanism finding."

## Gates
| Gate | Result | Baseline |
|---|---|---|
| batch_objdiff `main/system/bandobj/BandCharDesc` | NameToDrumVenue 100.0, DrumCallback 100.0; unit fuzzy 99.318756 | CA5 99.318756 — baseline-exact |
| /data-diff `sDrumVenueMappings` (CA4) | 93.07359 → 100.0 (size 44=44, mismatch 0) | improved to target agreement |
| drawlog-golden `--fixed-clock --canonical-order` (CA2) | PASS 792 draws (302 known-residual, non-blocking) | CA2 PASS 792 — exact |
| rb3-tests (CA3) | 123 ran / 116 pass / 7 skip / 0 fail | CA3 123/116/7/0 — exact |
| boot A/B | N/A — no new flag added | — |

Only shared-decomp unit touched: BandCharDesc (baseline-exact). Debug/MakeString/OvershellSlotState/BandWardrobe: NOT edited (100.0/baseline untouched). Native glue rb3_platform_native.cpp: not a decomp unit.

## Coordinator note (default-ON decision for ratification)
V3(a) `GetHostName` override is unconditional native glue (matches the file's `PlatformMgr` ctor / `TheContentMgr` unconditional precedent), not a flag/probe and not a flip of an existing default — it provides a previously-weak-stubbed symbol its faithful Wii value. No getenv gate added (an opt-out that re-enables a formatter SIGSEGV is a footgun with no legitimate use). Flagged here per rule 8 for close-out ratification.
