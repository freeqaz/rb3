# WAVE 33 PRE-DISPATCH REVIEW — DISPATCH-WITH-AMENDMENTS

**Reviewer:** Fable pre-dispatch gate. **Kickoff under review:** `WAVE33_KICKOFF.md`
(committed `3ad234fa`; its header says "Base SHA d699d837" — lanes actually branch
from `3ad234fa`, the kickoff commit itself; trivial, noted). **Method:** fresh
code/asm/map verification + one rb3-tests run + three drawlog-golden runs at HEAD
(read-only; no source edits).

**Verdict: DISPATCH-WITH-AMENDMENTS.** The evidence base is accurately quoted
(V2 chain verified line-by-line against code, map, and the two committed
backtrace .txt files), the re-rank is legitimate, and the family-STOP paraphrase
matches the binding terms. But two acceptance legs are mechanically broken as
written — Lane 2's flag-bisect names a flag that DOES NOT EXIST, and the drawlog
gate FAILS at HEAD under the naive invocation — plus one wrong-instrument gate
(V2's fix is a DATA divergence; function fuzzy% cannot show it) and one ownership
gap. All fixable by adopting the CA block below verbatim.

---

## Verification record (what was actually checked)

- **V2 quote accuracy vs `W31-VISUAL-PASS/FINDINGS.md` (`7bb74623`): ACCURATE.**
  `sDrumVenueMappings` is a 10-entry table at `src/system/bandobj/BandCharDesc.cpp:18-21`;
  `NameToDrumVenue` loop at `:585-593` terminates only on `*entry != 0` at even
  indexes 0,2,4,6,8,(10=OOB); caller chain confirmed:
  `BandWardrobe::OnUnloadVenue` (`BandWardrobe.cpp:948`, passes `""` at `:953`) →
  `BandCharacter::SetTempoGenreVenue` (`BandCharacter.cpp:3365`, `mDrumVenue =
  NameToDrumVenue(cc)` at `:3368`) → crash. Both backtrace files are committed
  (`W31-VISUAL-PASS/evidence/v2_endgame_confirm_sigsegv_backtrace.txt`,
  `v3_xboxlive_assert_formatter_sigsegv_backtrace.txt`, in `git ls-files`).
- **Retail ground truth (STEP-0 branch (a) PRE-CONFIRMED):** the linker map
  (`orig/SZBE69_B8/files/band_r_wii.map`) gives
  `sDrumVenueMappings__12BandCharDesc` `.data:0x80BE508C` **size 0x2C = 11
  pointers**; `config/SZBE69_B8/symbols.txt:56210` agrees. Our table has **10**.
  Retail HAS the empty-string sentinel our transcription dropped. The decomp
  DIVERGES — kickoff fix-branch (a), no HX_NATIVE gate needed.
- **Wrong-instrument check:** `NameToDrumVenue__12BandCharDescFPCc` is **already
  100.0%** in `report.json` (unit `main/system/bandobj/BandCharDesc` = 99.318756).
  The divergence is DATA-side; function objdiff can neither regress nor improve
  on the sentinel fix. The milohax objdiff fork's `--include-data` data-symbol
  diff (`/data-diff`) is the correct verification instrument.
- **V3 path exists as described:** `Debug::Modal`'s MakeString call at
  `src/system/os/Debug.cpp:376-383` (backtrace frame Debug.cpp:377); crash frame
  `FormatString::operator<<(String const&)` snprintf at
  `src/system/utl/MakeString.cpp:312`. Both units (`main/system/os/Debug`,
  `main/system/utl/MakeString`) are **100.0** — shared-decomp edits must hold that.
  V3(b) assert site: `src/band3/meta_band/OvershellSlotState.cpp:181`
  (`MILO_FAIL("OvershellSlotState %d does not exist", id)`, unit 100.0).
- **W31 set_play mechanism (Lane 2 STEP-0):** commit `a3916764` — 5-site arg
  swap `SendMessage(_val.Sym(), inst)` → `SendMessage(inst, _val.Sym())` in the
  `BEGIN_PROPSYNCS(BandDirector)` block (`src/system/bandobj/BandDirector.cpp`
  ~`:2140-2153`, `{bass,drum,guitar,mic,keyboard}_intensity` SYNC_PROP_SET).
  Commit message, verbatim: "**No new flag, no HX_NATIVE gate on the fix** (only
  a read-only default-OFF RB3_SETPLAY_PROBE diagnostic)." W31 close-out confirms
  UNCONDITIONAL (an HX_NATIVE gate would have forked faithful behavior). The
  ledger has NO behavioral set_play flag — only the probe.
- **Probes still exist at HEAD:** `RB3_SETPLAY_PROBE` (`BandDirector.cpp:276,912`),
  `RB3_BANDPERF_PROBE` (`BandCharacter.cpp:531,3930`), `RB3_BANDPERF_CLIPS`
  (`:425`), `RB3_BANDPERF_BT` (`:3936`). W32 §5.6 put SETPLAY/MIDIDRV probes on a
  retirement *clock*, not retired — and Lane 2 uses them as acceptance
  instruments this wave, so retirement is a W34-close-out decision at the earliest.
- **Gates run fresh at HEAD (`3ad234fa`, engine at pin `2ea8e34`):**
  - `rb3-tests` (rebuilt): **123 ran / 116 passed / 7 skipped / 0 failed**.
  - `drawlog-golden.py --fixed-clock` (plain): **FAIL** — 71 then 72 "unexpected"
    world-xfm divergences across two runs (run-varying; the W0.3c
    draw-submission-ORDER nondeterminism class).
  - `drawlog-golden.py --fixed-clock --canonical-order`: **PASS — 792 draws, 297
    known-residual divergences within bound**. This matches the W31/W32 PASS
    records' phrasing (W32: "792 draws, 287 known-residuals within bound"); the
    residual count drifts run-to-run and is non-blocking by design.
- **Web rider tooling:** `scripts/web/build.sh`, `native/web/server.py` exist;
  deployed build is FRESH vs HEAD (release+debug wasm 2026-07-12 08:26; the only
  later commit touching web, `d699d837`, is js-only and already deployed 21:48).
  Kickoff's rider text is a verbatim quote of W32 §6 item 3, and W32 §5.2's
  "should run FIRST in W33 pre-work" is satisfied by rider-first ordering. Note:
  the W32 coordinator addendum already discharged **release song_select** — the
  rider's novel surface is web GAMEPLAY (both builds) + debug song_select.
- **§6.8 family-STOP wording, exact:** W32 §6 item 8: "**Carried/blocked:** F5
  (needs fresh hypothesis + oracle), F6 (UIGRADE), SKEL/CROWD families CLOSED
  (STOPs binding); arg-order NOT renewable as a sweep (Lane D ruling) —
  behavioral-heuristic use only." The binding terms themselves live in
  WAVE31_CLOSEOUT_REVIEW.md Lane D: "**SKEL_FAMILY_STOP is BINDING: no recharter
  without a new hypothesis** (lint 6 — no 7th cell)." The kickoff's reopen
  condition (positive rotation-basis signature on the NEW band-char/set_play
  surface, justification in PLAN.md) is consistent with and stricter than "no
  recharter without a new hypothesis" — and FINDINGS.md already frames V1 as
  new-hypothesis evidence, not a re-open of the same cell. Paraphrase OK; only
  the citation is off by one document (advisory A5).
- **Re-rank:** W32 §6 is titled "recommendation — ordering rationale"; not
  binding. F2/F7→W34 carry contradicts no ruling. Documented in the kickoff. OK.

---

## Amendments

### BLOCKING

**B1 — Lane 2 STEP-0(i) is unexecutable as written: there is NO committed
set_play gate/flag to bisect.** The kickoff says "disable the W31 set_play
dispatch via its committed gate (find the exact flag ... do NOT guess)" — the
lane would burn its run budget searching for a flag that does not exist
(`a3916764`: unconditional faithful decomp fix; ledger has only the read-only
`RB3_SETPLAY_PROBE`). **Fix:** replace the flag-bisect with a *source-level*
A/B: throwaway worktree (`tools/setup-worktree.sh`), locally re-swap the 5
`SYNC_PROP_SET` intensity sites in `src/system/bandobj/BandDirector.cpp`
(~`:2140-2153`) back to `SendMessage(_val.Sym(), "<inst>")`, build there, NEVER
commit the swap. That exactly reproduces the W30-era idle-band control
(equivalently: build at `a3916764^`).

**B2 — "drawlog-golden PASS at HEAD-baseline" is unattainable under the naive
invocation: the plain `--fixed-clock` gate FAILS at HEAD pre-edit** (71-72
unexpected divergences, run-varying — pre-existing order-nondeterminism, not any
lane's regression). **Fix:** acceptance in both lanes must name the exact
invocation `python3 scripts/native/drawlog-golden.py --fixed-clock
--canonical-order`; PASS baseline at HEAD pre-verified by this review = **792
draws** (297 known-residual within bound, non-blocking). Lanes must not debug,
"fix", or grade against the plain-mode FAIL.

**B3 — V2's objdiff gate is the wrong instrument for the expected fix.**
"objdiff match must not regress (improvement expected)" cannot fire:
`NameToDrumVenue` is already 100.0 and the divergence is in the `.data` table
(retail 11 pointers vs our 10). **Fix:** V2 acceptance = (i) append the `""`
sentinel entry to `sDrumVenueMappings` (data-decomp correction, branch (a),
unconditional, no HX_NATIVE gate), (ii) verify with the data-symbol diff
(`/data-diff` / objdiff `--include-data`) on `sDrumVenueMappings__12BandCharDesc`
(target size 0x2C), (iii) `batch_objdiff` functions baseline-exact (unit
99.318756, NameToDrumVenue 100.0). Line-count hazard is concrete in this file:
`DECOMP_FORCEACTIVE` at BandCharDesc.cpp `:57`, `:358`, `:609` — append the
sentinel WITHOUT adding a physical line (fits on line 21).

**B4 — Ownership gap on the V2 chain.** `BandWardrobe.cpp` (the `OnUnloadVenue`
caller) is owned by NO lane, and the alternate fix layer
(`BandCharacter.cpp:3368`) is owned by Lane 2 while the kickoff's STOP+HANDOFF
rule only covers the Lane-2→Lane-1 direction. **Fix:** assign
`src/system/bandobj/BandWardrobe.cpp` to Lane 1 (READ-ONLY for Lane 2); add the
symmetric rule — if Lane 1's STEP-0 names `BandCharacter.cpp` (or any
`src/system/char/` file) as the fix layer: STOP, checkpoint HANDOFF. (The
pre-confirmed sentinel fix needs neither; this closes the collision on paper.)
Also name V3(b)'s file explicitly: `src/band3/meta_band/OvershellSlotState.cpp`.

### ADVISORY

**A5 — STOP citation off by one document:** the kickoff cites "W32 close-out
§6.8" for the binding STOP; §6.8 only *carries* it ("SKEL/CROWD families CLOSED
(STOPs binding)") — the binding wording is WAVE31_CLOSEOUT_REVIEW.md Lane D
("no recharter without a new hypothesis (lint 6 — no 7th cell)"). Paraphrase
substance OK; cite both.

**A6 — V3(a) first-look hint (saves lane time):** the crashing MakeString at
`Debug.cpp:377` formats `NetworkSocket::GetHostName()` (native network is
STUBBED) and a `String version` into `%s` slots; check stub-returned
garbage/NULL BEFORE assuming a formatter-core varargs defect. Crash frame =
`MakeString.cpp:312` (`snprintf` in `operator<<(String const&)`). Both TUs are
100.0-matched Wii units — HX_NATIVE-gate any behavioral change there unless it
is a proven decomp divergence.

**A7 — rb3-tests baseline pre-computed (saves every lane a run):** at HEAD,
**123 ran / 116 passed / 7 skipped / 0 failed** (skips = real-capture oracle
fixtures). Any FAILED > 0 on a lane tree is lane-introduced.

**A8 — Web rider scoping:** deployed build verified fresh vs HEAD → expected
web builds = 0 (budget ≤2 stands as slack, not plan). Release song_select is
already discharged (W32 addendum, `web_song_select_policies.png`); grade the
rider on its novel surface: web GAMEPLAY debug+release, debug song_select.

**A9 — Probe retirement interplay:** Lane 2's acceptance uses
`RB3_SETPLAY_PROBE`/`RB3_BANDPERF_*` as instruments; W32 §5.6's suggested
retirement ("once their families survive one further wave") therefore cannot
fire at W33 close-out for SETPLAY (its family is under active V1 investigation)
— defer the keep/retire decision to W34 close-out. Coordinator obligation, no
lane action.

**A10 — Base-SHA nit:** kickoff header says base `d699d837`; the kickoff itself
is `3ad234fa` and lanes branch from that. Gates in this review ran at `3ad234fa`.

---

## PROPOSED COORDINATOR ACCEPTANCE (adopt verbatim in dispatch prompts)

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
