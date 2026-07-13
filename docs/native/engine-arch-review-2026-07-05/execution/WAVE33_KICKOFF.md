# WAVE 33 KICKOFF — RESULTS-SCREEN crashes (V2+V5+V3) + BAND-POSE regression (V1) + web validation rider

**Base SHA:** `d699d837` (rb3). **Coordinator:** Fable. **Lanes:** Opus (2) + Sonnet web rider via ultracode Workflow.
**User directive (2026-07-13, verbatim):** "Let's work on those now please and continue our work, Keep iterating here and dispatching waves" — "those" = the W31-VISUAL-PASS candidates (V1/V2) + Xenia follow-up.

**Coordinator re-rank vs the committed W32 menu (documented, not silent):** the
W32 close-out's Wave-33 menu (F2-PILL + F7-SIDEBAR top-2) predates the
W31-VISUAL-PASS findings (`7bb74623`). V2 is a root-caused FLOW-blocker (cannot
return to shell after any song) and V1 is a visual-blocker REGRESSION — both
outrank cosmetic HUD work. **F2-PILL and F7-SIDEBAR carry to Wave 34 unchanged**
(charters already drafted in WAVE32_CLOSEOUT_REVIEW.md §6.1/6.2). Menu item 3
(web full-policy validation, "run FIRST") is chartered here as the rider. Items
4-7 (parity audit, E6 taxonomy, F8, probe soak-retirement) remain
coordinator-cheap carries.

**Concurrent, NOT part of this workflow:** a coordinator-dispatched XENIA-HUB-CRASH
agent (disassemble PC `0x82BCEFE4`, the deterministic main_hub-load crash from the
2026-07-12 refcap pass). Lanes ignore it; it shares no rb3 source surfaces.

---

## Evidence base (read before self-grading)
`execution/W31-VISUAL-PASS/FINDINGS.md` (`7bb74623`) — V1-V6 with evidence paths
and the two committed symbolized crash backtraces (.txt, force-added).

## Lane 1 — W33-RESULTS-SCREEN (V2 primary; V3 formatter FIRST; V5 rider)

**V3(a) — fix FIRST (it masks everything else):** the `Debug::Modal`/`MakeString`
assert formatter ITSELF SIGSEGVs when formatting the V3 overshell assert — every
assert on that path dies silently. Discriminator: reproduce from the committed
backtrace; name the formatter defect (varargs? %s on non-string? MakeString buffer?).
Fix so asserts DISPLAY (native-side; if the defect is in shared decomp code, the fix
must be retail-faithful or HX_NATIVE-gated with byte-identical `#else`).

**V2 (primary, root-caused by the visual pass — verify then fix):**
`BandCharDesc::NameToDrumVenue("")` walks off the 10-entry `sDrumVenueMappings`
table (no sentinel; `OnUnloadVenue` passes `""` on the results-screen CONFIRM
unload path) → SIGSEGV; the shell is unreachable after any song. STEP-0
discriminator (blocking): what does RETAIL do — `bin/analyze-function` /
Ghidra Bank8 (`binary_name=bank8_target`) + rb3-xenon for `NameToDrumVenue`: does
retail's table have a sentinel/different bound, does retail guard "", or does
retail never reach this call natively-divergent? Fix at the layer named: (a) if
our decomp DIVERGES from retail (lost sentinel/wrong bound) — fix the decomp,
objdiff match must not regress (improvement expected); (b) if retail would crash
too on this input (native-only call path) — HX_NATIVE guard on the native-only
condition, byte-identical `#else`.

**V5 (rider):** results screen renders artist as `j0` and SOLO SCORE as `GO`
(non-numeric) — discriminator: where do the strings come from (Localize? MakeString
fmt? stale buffer?); plausibly the SAME formatter family as V3(a) — check after
V3(a) lands before chartering a separate fix.

**Acceptance (mechanical):**
- Full flow proof: boot → song → results → CONFIRM → back to shell screen,
  captured (screen-name evidence via the harness dta/uidump used by prior waves),
  run rc=0 (exit-trap teardown tolerated). This flow is currently IMPOSSIBLE — it
  is the headline deliverable.
- V3(a): a deliberately-triggered assert on the fixed path renders its text
  (screenshot or stderr capture); the overshell "PLAY ON XBOX LIVE"
  `OvershellSlotState` MILO_FAIL either fixed (if native-only state bug) or
  documented-faithful (stubbed-online path) — do NOT paper over it silently.
- V5: results screen shows a real artist string and numeric SOLO SCORE, or an
  honest separate-mechanism finding.
- Gates: `batch_objdiff` on every touched shared-decomp unit — baseline-exact
  OR improved (state which per unit, with numbers); drawlog-golden PASS at
  HEAD-baseline (capture the CURRENT baseline count BEFORE edits — do not assume
  W30-era numbers); rb3-tests clean at HEAD-baseline; boot A/B if any new flag.
- Bounds: ≤6 boot runs.

**Owned surfaces:** `src/system/bandobj/BandCharDesc.cpp/.h`,
`src/system/os/Debug.cpp/.h`, MakeString/format utl files it names (enumerate in
PLAN before editing), overshell slot-state file if V3(b) turns native-only,
`execution/W33-RESULTS-SCREEN/**`. **READ-ONLY:** everything Lane 2 owns,
CharDriver/CharIKHand, WorldCrowd/Crowd.cpp (PROTECTED), RndMesh loader.

## Lane 2 — W33-V1-POSE (visual-blocker regression)

**Symptom (V1, both songs tested):** band members hit grossly wrong FULL-BODY
poses in gameplay (bassist folded face-down over the bench; vocalist bent
backwards, leg horizontal) — new since W30; A/B-proven NOT the W32 MIDIDRV fix.
Prime suspect: W31's set_play dispatch newly playing body performance clips that
expose a latent transform/basis bug on band characters.

**FAMILY-STOP constraint (binding):** the SKEL rotation-basis family is CLOSED
with a binding STOP (W32 close-out §6.8). V1 counts as candidate NEW evidence
(new surface: gameplay band chars via set_play body clips). The lane may reopen
that family ONLY if STEP-0(iii) positively names rotation-basis (signature:
axis-swapped/90°-rotated limb sets matching the family's prior evidence),
documenting the new-evidence justification in PLAN.md. Otherwise fix at whatever
layer STEP-0 names and do NOT touch SKEL-family code.

**STEP 0 — discriminators, checkpointed BEFORE any fix:**
(i) Reproduce V1 at pinned songMs (visual-pass evidence frames name song+time);
then flag-bisect: disable the W31 set_play dispatch via its committed gate (find
the exact flag in WAVE31_KICKOFF/CLOSEOUT docs + census ledger — do NOT guess) →
does V1 vanish? Also confirm W30-era boot (idle-only band) shows no V1.
(ii) Name the offending clip(s)+bones: RB3_SETPLAY_PROBE / RB3_BANDPERF_* + a
pose census at the bad frame (which characters, which clip playing, which bones
wrong — wrong DATA (clip) vs wrong TRANSFORM (apply path)).
(iii) Basis test: if wrong-transform, is the signature the rotation-basis class
(axis-swap/90°) or something else (bind pose, mirroring, parent chain)?
**THEN** one lever at the named layer, default-OFF unless it's a plain decomp
divergence fix (state which); A/B = songMs-matched screenshot pairs on BOTH
visual-pass songs showing natural poses, plus no regression of the W31
set_play acceptance (rhythm/solo clips still play — rerun its census).

**Gates:** batch_objdiff touched units baseline-exact-or-improved; drawlog-golden
at HEAD-baseline; rb3-tests; boot A/B flag-ON. Bounds: ≤8 boot runs.
**Owned surfaces:** the anim-apply layer it names in `src/system/char/` +
`src/system/bandobj/BandCharacter.cpp` (shared with nobody this wave),
`execution/W33-V1-POSE/**`. **READ-ONLY:** BandCharDesc (Lane 1), CharDriver
probes, Crowd oracle, RndMesh loader. If STEP-0 names a file Lane 1 owns: STOP,
checkpoint a HANDOFF note.

## Rider (Sonnet, run FIRST) — W33-WEB-VALIDATE (W32 menu item 3 verbatim)

> **Web full-policy validation pass:** song_select + gameplay on web (debug AND
> release) now that the render-hook family is live — screenshot sweep vs native,
> confirm F3 glyphs + B-family policies render and nothing regresses. Closes §5.2.

Findings only, NO fixes. Build via `scripts/web/build.sh` if needed (it is slow —
prefer the existing deployed build if fresh); serve `native/web/server.py`;
Playwright-style capture per `project_web_polish_2026_06_02` harness precedent.
Deliverable: `execution/W33-WEB-VALIDATE/FINDINGS.md` + evidence, committed.
Bounds: ≤2 web builds, ≤6 capture sessions.

---

## Binding process rules (carried verbatim — violations = lane rejection)
1. **E4:** dispatch prompts quote acceptance verbatim; lanes quote it back in STATUS.
2. **A7:** raw stderr gz committed under `<lane>/evidence/raw/`; per-log `grep -c`
   table for EVERY probe tag; coordinator E1 greps RAW. Symbolized backtraces:
   commit the addr2line transcript (W30-E2).
3. **Checkpoint-before-fix:** `/tmp/wave33-checkpoints/<lane>-<stage>.json`
   (buildSha, verdict, evidence paths), check-first; mirror into evidence dir.
4. **Discriminator-first.** 5. Builds: `tools/ninja-locked` (Wii) /
   `flock /tmp/rb3-native-build.lock cmake --build native/build-native --target
   rb3-native` (native); headless `RB3_HTTP=1`, free ports, `--fixed-clock` A/B.
6. **pgid-only cleanup — NEVER pkill by name** (concurrent Xenia + web agents live).
7. Git: stage ONLY own files BY PATH; `flock /tmp/rb3-git.lock`; never touch
   `native/src/rb3_session_trace.cpp`, engine `FxSendNative.cpp`, others'
   untracked files. No `add -A`/`.`/`-a`, no stash, no Co-Authored-By.
   Prefix `wave33(<LANE>):`.
8. **NO default flips / pin bumps / census regen by lanes** — coordinator, once,
   at close-out. Engine edits: commit in `../milo-native-engine` first, coordinator
   bumps pin.
9. New flags/probes: getenv-gated default-OFF `#ifdef HX_NATIVE`, byte-identical
   `#else`. **W30 lesson: DECOMP_FORCEACTIVE bakes `__LINE__` — preserve physical
   line count above it in shared decomp files.**
10. Honest PARTIAL/RECHARTER > gamed PASS; exit-trap teardown rc=-11 tolerated.
11. Baselines: capture drawlog/rb3-tests/objdiff baselines AT HEAD before editing —
    W31/W32 changed them since the last documented numbers.

## Coordinator close-out obligations
E1 raw-grep countersign → adversarial close-out review → append-only errata →
README results + Wave-34 menu (F2-PILL + F7-SIDEBAR lead; fold Xenia agent
findings) → single census regen + ≤1 pin bump → memory → user summary.

---

## COORDINATOR ACCEPTANCE (adopted verbatim from WAVE33_REVIEW.md `50efa082` — BINDING, overrides conflicting text above)

**CA1 (Lane 2, replaces STEP-0(i) flag-bisect).** The W31 set_play fix is
UNCONDITIONAL (commit `a3916764`; no flag, no HX_NATIVE gate; ledger carries
only the read-only probe `RB3_SETPLAY_PROBE`). A/B control = throwaway worktree
(`tools/setup-worktree.sh`) with the 5 `SYNC_PROP_SET` intensity sites in
`src/system/bandobj/BandDirector.cpp` (~`:2140-2153`;
`{bass,drum,guitar,mic,keyboard}_intensity`) locally re-swapped to the pre-W31
`SendMessage(_val.Sym(), "<inst>")` order — never committed, worktree deleted
after. V1 vanishing under the re-swap = set_play-exposed; persisting = other
layer.

**CA2 (both lanes).** Drawlog gate invocation is exactly
`python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order`;
HEAD baseline (pre-verified 2026-07-13 at `3ad234fa`): **PASS, 792 draws**
(known-residual count ~297, run-drifting, non-blocking). The plain
`--fixed-clock` mode FAILS at HEAD (71-72 unexpected, pre-existing
order-nondeterminism) — not a gate, not a lane regression, do not chase it.

**CA3 (both lanes).** rb3-tests HEAD baseline: **123 ran / 116 passed /
7 skipped / 0 failed**. Acceptance = same or better; any new FAIL/SKIP is
lane-owned.

**CA4 (Lane 1, V2).** Retail `sDrumVenueMappings__12BandCharDesc` = size
**0x2C (11 pointers)** at `.data:0x80BE508C` (map + symbols.txt:56210); our
table = 10 (`BandCharDesc.cpp:18-21`). Fix = append the `""` sentinel entry —
unconditional decomp-data correction, no HX_NATIVE gate — WITHOUT adding a
physical line (DECOMP_FORCEACTIVE `__LINE__` hazard at `:57/:358/:609`).
Verify: `/data-diff` (objdiff `--include-data`) on the symbol reaches
target-size/content agreement; `batch_objdiff` unit functions baseline-exact
(`main/system/bandobj/BandCharDesc` 99.318756, `NameToDrumVenue` 100.0).
Flow proof (boot → song → results → CONFIRM → shell) stands as chartered.

**CA5 (Lane 1, unit baselines for touched TUs).** `main/system/os/Debug`
**100.0**, `main/system/utl/MakeString` **100.0**,
`main/band3/meta_band/OvershellSlotState` **100.0**,
`main/system/bandobj/BandWardrobe` 99.50284, BandCharDesc 99.318756. Every
100.0 unit must remain 100.0 (HX_NATIVE-gate native-behavior changes,
byte-identical `#else`).

**CA6 (ownership).** Lane 1 += `src/system/bandobj/BandWardrobe.cpp` (READ-ONLY
for Lane 2); V3(b) file = `src/band3/meta_band/OvershellSlotState.cpp`
(MILO_FAIL at `:181`). Symmetric handoff: if Lane 1's STEP-0 names
`BandCharacter.cpp`/`src/system/char/*` as fix layer → STOP + HANDOFF note
(mirror of Lane 2's existing rule). Lane 2 baseline for its owned TU:
`main/system/bandobj/BandCharacter` 99.67018.

**CA7 (web rider).** Use the deployed build (verified fresh vs HEAD:
release+debug wasm 2026-07-12 08:26 + js `d699d837` deployed 21:48) — 0 builds
expected. Novel surfaces graded: web GAMEPLAY (debug AND release) + debug
song_select; release song_select cites the W32 addendum discharge rather than
re-earning it. F3-glyph + B-family language matches W32 §6 item 3 verbatim —
grade against that text.

**CA8 (instruments).** STEP-0(ii) probe strings confirmed live at HEAD:
`RB3_SETPLAY_PROBE` (`BandDirector.cpp:276,912` — `[SETPLAY_KEYS]`/
`[SETPLAY_SEND]` tags), `RB3_BANDPERF_PROBE` (`BandCharacter.cpp:531,3930`),
`RB3_BANDPERF_CLIPS` (`:425`), `RB3_BANDPERF_BT` (`:3936`, glibc-only). Probe
retirement (W32 §5.6) deferred to W34 close-out (A9).

---

_Read-only review: no source edits; gates run were rb3-tests (×2), drawlog-golden
(×3), one cmake rebuild under `/tmp/rb3-native-build.lock`. This document is the
only write._
