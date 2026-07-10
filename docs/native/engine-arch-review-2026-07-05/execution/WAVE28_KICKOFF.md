# WAVE 28 KICKOFF — CROWD clip-set OWNERSHIP (name the owner, then ONE lever)

Status: DRAFT (pending Fable pre-dispatch review → COORDINATOR ACCEPTANCE)
Coordinator: Fable (this session). Lane agents: Opus.
Prior state: rb3 HEAD `a47d82f0`+, engine pin `be401ec`, census 410, FOURTEEN defaults ON,
drawlog-golden 792 PASS (per-name eps recalibrated `_w27_residual_recalib.py`, fail-red
audit OK), rb3-tests 116/0.

## Why this wave

Wave 27 settled the UI question and reopened the ownership question:

- **PROVEN (W27, stands):** `sv3_panel` is RESIDENT across splash→main_hub — the
  interstitial→regular-panel refcount handshake (`mLoadRefs` 1→2→1) is faithful; the
  A7 revisit cycle is leak-free. There is NO ui/panel-residency lever.
- **REINSTATED (close-out E1, raw logs):** seven `CHARDRV_REPLACE` kills of
  `crowd1-5.clp/clip` fire at beat 2.433 — the W26
  `WorldDir::~WorldDir → CharClipSet dtor → Replace(clip,NULL)` mechanism reproduces —
  sourced at the **FAITHFUL splash-side panel unload** (`splash_panel`+`sv8_panel`
  refs→0 in the kill frame, E2).
- **OWNER CORRECTED (E2):** the crowd clips / walker proxies / walk meshes are
  raw-string-present in `sv8_a.milo` (the splash backdrop), NOT in sv3_a's raw strings.
  W26 mis-attributed the destroyed clip set to sv3/streetslomo.
- **Root cause reframed (E4):** clip-set ownership/binding divergence — there are TWO
  same-named `clips` sets (11 vs 8 entries); do the resident streetslomo drivers
  resolve `play_clip` against the splash-owned copy that faithfully dies?
- **E3:** only 7 of 8 drivers ever Play (`crowd_female04` never triggered).
  **E5:** `mDefaultClip` is serialized-only — NULL may be faithful; log the serialized
  name before treating it as a gap.

This is the FOURTH narrative correction on this bug (W23 verts=0 → W25 load-merge →
W26 sv3-unload → W27 sv8-owner). Per `WAVE27_CLOSEOUT_REVIEW.md` Q6 the wave is
**discriminator-first with checkpoint-before-fix BINDING**: NO fix code, of any kind,
before the STEP-0 checkpoint file exists with a verdict.

## Lane W28-CROWD-OWNER — charter

**Goal:** answer, with named objects and evidence, WHO owns the clip set the hub crowd
drivers actually resolve against and WHOSE crowd we have been measuring — then apply
exactly ONE lever (or re-charter, if the answer says the bug was mis-scoped).

### BINDING STEP 0 — ownership + identity discriminators (≤1 day; probes exist)

All four items; checkpoint to `/tmp/wave28-checkpoints/CROWD-step0.json` BEFORE any
fix code (fields: owner-verdict, identity-verdict, per-item evidence pointers, chosen
lever + why).

1. **Name the torn-down owner directly.** One boot with
   `RB3_CROWD_PANEL_DBG=1 CHARDRV_PROBE=crowd CHARDRV_BT=1` (probes already exist:
   `CharDriver.cpp` ~:245/:273/:533/:848-870; panel probe from W27). Interleave the
   beat-2.433 Replace BACKTRACE with the panel-unload markers and read off the
   torn-down `WorldDir`'s actual name/owner — no inference from raw-string greps.
2. **Dump the ownership chains.** For (a) the 5 clips that Play, (b) BOTH `clips`
   sets (11 vs 8 — and whether any driver's `mClips` pointer SWAPS at the kill), and
   (c) the 8 `crowd_*` char dirs: dump `Dir()` parent chains up to the owning panel
   `WorldDir` (native runtime dump via `/api/dta/eval` or an env-gated probe — the
   milo files nest; ls/grep on raw strings is NOT ownership evidence, that's what
   burned W26).
3. **E5 discriminator.** Log each crowd CharDriver's SERIALIZED default-clip name at
   load time (before any Replace) — decide whether `mDefaultClip==NULL` post-kill is
   a native gap or faithful data.
4. **Wii-GT identity check (the charter-scoping one).** Are the `crowd_*` chars part
   of retail main_hub at all, or are they streetslomo's OWN (differently-named)
   walkers — i.e. is the crowd we have measured since W23 actually the SPLASH crowd
   that faithfully dies with the splash? Evidence: retail screenshots
   (`images/retail-screenshots/`), Xbox DTA / milo dir listings for main_hub's
   vignette, and the sv8-vs-sv3 dir dumps from item 2.

### Levers (exactly ONE, chosen from the STEP-0 verdict)

- **A. Cross-panel clip resolution fix:** if the hub's resident streetslomo drivers
  are wrongly bound to the splash-owned `clips` copy, fix `play_clip`/driver clip
  resolution so they resolve the RESIDENT copies. This is the faithful-restoration
  carve-out class (unflagged allowed ONLY with checkpointed discriminator evidence +
  the A6-style drawlog ruling: unchanged draw count, only world-field crowd-pose
  diffs; anything else → flag-gated default-OFF fallback).
- **B. Re-charter:** if the observed crowd is splash-owned and faithfully dies, the
  hub-walkers bug was mis-scoped since W23 — write the re-charter (what ARE
  streetslomo's own walkers, what state are they in natively, fold in the deferred
  `verts=0` / near-black thread) and STOP. A documented re-charter is a full lane
  success; do not force a fix onto the wrong crowd.

### Flag + scope rails (binding)

- Lever A flag name: `RB3_HUB_CROWD_REBIND` (final unless review amends), chosen ONCE
  at the STEP-0 checkpoint; `#ifdef HX_NATIVE`, byte-identical `#else`; default-OFF
  unless the A6 carve-out fires with countersign-ready evidence. **NO default flips,
  NO pin bumps by the lane** — coordinator only, at close-out.
- Owned files: `src/system/char/CharDriver.cpp` (probes exist; narrow writes allowed
  for lever A resolution only), `src/system/char/CharClip*.cpp` (read + narrow),
  `src/system/ui/UIPanel.cpp`/`PanelDir.cpp` (read-mostly; W27 proved these faithful),
  `src/band3/meta_band/{BandScreen,InterstitialMgr}.cpp` (read), native probe files.
  **NOT owned / DO NOT TOUCH:** the protected gameplay WorldCrowd/RndMultiMesh oracle
  (`Crowd.cpp:884-1000`), the proven-correct RndMesh loader, hands/finger family
  (CLOSED), FOREARM binding (CLOSED), `native/src/rb3_session_trace.cpp`, engine
  `src/platform/FxSendNative.cpp` (concurrent agents' files — never stage).
- `RB3_CROWD_CLIP_KEEP` (E-C2) stays IN PLACE during the lane; removal is re-ruled by
  the coordinator at close-out only.

### Gates (all in STATUS.md with evidence)

| Gate | Requirement |
|---|---|
| STEP 0 checkpoint | ALL FOUR discriminators + verdicts BEFORE fix code (`/tmp/wave28-checkpoints/CROWD-step0.json`) |
| batch_objdiff | touched decomp fns: HX_NATIVE-gated → exact equality with baseline; faithful-fidelity fix → ≥ baseline with `run_diff_inspect diagnose`; untouched → trivially baseline |
| drawlog-golden | `python3 scripts/native/drawlog-golden.py --fixed-clock --canonical-order` flag-OFF: 792 PASS (recalibrated residual is current — do NOT touch the sidecar; ambient-RED reruns escalate to coordinator) |
| rb3-tests | 116 pass / 0 fail |
| boot A/B flag-ON (lever A only) | splash→hub→song_select→gameplay reachable, crash-free; A7 revisit cycle: `mLoadRefs` returns to pre-cycle + crowd census re-passes AFTER revisit |
| Acceptance (lever A) | flag-ON: `animating>0` on the crowd drivers + lit walking figures in isolate captures (`/api/screenshot`) |
| Acceptance (lever B) | committed re-charter doc naming the real hub walkers + their native state, with the same evidence rigor as STEP 0 |
| Evidence honesty | STATUS excerpts must cite RAW log paths + line ranges; coordinator E1 greps RAW logs (W27 E1 lesson — curated excerpts are not evidence) |

### Process rails (standing)

- Build only via `tools/ninja-locked` / cmake with `flock /tmp/rb3-native-build.lock`.
- Headless: `RB3_HTTP=1 RB3_FIXED_CLOCK=1`, free ports, pgid-only cleanup (never pkill by name).
- Checkpoints: `/tmp/wave28-checkpoints/<stage>.json`, check-first / write-before-return.
- Commits under `flock /tmp/rb3-git.lock`; stage ONLY your own files BY PATH.
- Deliverables: `W28-CROWD-OWNER/{PLAN.md,STATUS.md,evidence/}` + focused commit(s).
- Report back (StructuredOutput): paths + short findings, NO large diffs inline.

## Optional tail: W28-PROP-FIX (concurrent, disjoint files; defer without guilt)

The real prop-hand fix, mechanism fully pinned by W26/W27: (1) bind/animate the
prop-tip clip tracks (`bone_pick_strum`, `bone_[LR]-tip_*` carry constant LocalXfm —
W27(b)); (2) redirect the target BEFORE the multi-target weight loop (W27 comment nit
marks the spot, `CharIKHand.cpp` ~:271); (3) break the mFinger re-projection feedback
(collapse target proven: `RB3_PROP_FINGER_BYPASS` takes over-reach 120-240u → 21-25u,
reach 20.3u). Flag-gated default-OFF (`RB3_PROP_POSE_FULL`, name final unless review
amends), building on the existing `RB3_PROP_POSE` scaffold. Owned:
`src/system/char/CharIKHand.cpp` + narrowly the clip-binding site the tip-track fix
requires (enumerate in PLAN before touching). Acceptance: with flag ON in-song, hands
track the strum/fret positions with clamp mode counts ~0 skip (matching the B column
of W27's A/B) AND visible hand-on-instrument in captures. Same gates table as the main
lane (batch_objdiff / drawlog flag-OFF / rb3-tests / boot A/B flag-ON).

## COORDINATOR ACCEPTANCE — BINDING (adopts WAVE28_REVIEW.md `350d3ebc`, A1-A8 ALL)

Where this block conflicts with the draft text above, THIS BLOCK WINS.

1. **A1 ADOPTED — interleave recipe pinned + probe-line grant.** One boot, all three
   env vars (`RB3_CROWD_PANEL_DBG=1 CHARDRV_PROBE=crowd CHARDRV_BT=1`), stderr to ONE
   file — both probe families reach fd 2 unbuffered in call order (CHARDRV `fprintf`
   direct; PANELDBG via `MILO_LOG→Debug::Print→OSReport→vfprintf` rvl_shims.cpp:30-35).
   The lane MAY make two one-line probe edits adding `beat=%.3f` (`TheTaskMgr.Beat()`)
   to the `UIScreen::UnloadPanels` marker and the `UIPanel::CheckUnload` UNLOAD line.
   `src/system/ui/UIScreen.cpp` is added to owned files **probe-line-only**.
2. **A2 ADOPTED — STEP-0 header corrected; two new probes specified.** Probes exist
   for items 1 and 4 ONLY. Items 2-3 require two small additions inside owned
   CharDriver.cpp, both gated under the EXISTING `CHARDRV_PROBE` env (no new getenv
   names → no census growth):
   (a) **mClips-swap detector (unsampled):** per-driver `gPrevClips` transition
   detector in `Poll` (same pattern as `gPrevFirst`/`CHARDRV_DIE`, CharDriver.cpp:
   529-537) logging `[CHARDRV_CLIPSWAP] dir=... from=%p'%s' to=%p'%s' beat=...`,
   PLUS one line in `SetClips` (:294) to attribute between-poll swaps to their
   caller (the copy path :285 is covered by the Poll detector). The %60-sampled
   `[CHARDRV]` line (:502-516) is NOT sufficient — that sampling is why W27 could
   not decide the swap question.
   (b) **E5 serialized-name probe:** at LOADS (:923) capture the serialized
   default-clip STRING (Tell/peek/Seek-back around `mDefaultClip.Load`, or an
   HX_NATIVE-gated manual read+resolve replicating ObjPtr_p.h:536-543), logging
   `[CHARDRV_DEFCLIP] dir=... serialized='%s' resolved=%p`.
   Item 2's deliverable also folds in E3: per-driver `CHARDRV_PLAY` counts (why
   `crowd_female04` never Plays).
3. **A3 ADOPTED — lever A reworded (supersedes the draft).** Lever A = *"fix the
   driver's clip-set BINDING so the resident streetslomo drivers resolve/hold the
   RESIDENT `clips` copy — at whichever layer the STEP-0 ownership-chain dump names:
   (i) `mClips` load-time resolution (`bs >> mClips` CharDriver.cpp:890 →
   `ObjPtr::Load` ObjPtr_p.h:536-543 → owner-dir `FindObject` with first-match
   subdir ORDER, Dir.cpp:531-542 — an ordering or dir-scoping fix is in-scope);
   (ii) a post-load `mClips` swap (`SetClips` :294-297 / copy :285); or (iii) the
   trigger's direct object reference (`MyFindClip` kDataObject branch :345-347,
   which bypasses mClips entirely)."* play_clip-time re-resolution is a permissible
   mechanism ONLY if the dump shows that is where Wii diverges. Carve-out and
   fallback rails unchanged.
4. **A4 ADOPTED — identity-check paths pinned.** `orig-assets/extracted/config/
   vignettes.dta` dyn_file is campaign-conditional (:4-28; fresh save → `sv3_a`
   deterministic) — the lane RECORDS which variant the boot loaded. Hub panel list:
   `orig-assets/extracted/ui/main/main_hub.dta:744`. Milos: `orig-assets/extracted/
   world/vignette/shell/gen/{sv3_a,sv3_b,sv8_a}.milo_xbox`, `ui/main/gen/
   main_hub.milo_xbox`. Static top-level milo listing via `scripts/milo/mip_strip.py`
   `parse_dir_entries` (:399-425) — nested payloads (streetslomo inside sv3_a) are
   NOT statically listable; runtime `/api/dta/eval` dump is the pinned method, and
   `PathName()` (Utl.cpp:30) prints a full ownership chain in one call. The single
   retail hub screenshot (`images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png`)
   proves figure PRESENCE only, never motion; decisive identity evidence = the
   runtime dir dumps + milo/DTA listings, video `mhKNp9uAT48` optional corroboration.
5. **A5 ADOPTED — flag renamed.** Lever-A flag = **`RB3_HUB_CROWD_CLIPBIND`**
   (semantic collision: `*_CROWD_REBIND` is bone-rebind vocabulary owned by the
   default-ON `RB3_NO_CROWD_REBIND` inside the protected oracle, Crowd.cpp:929-932).
   `RB3_PROP_POSE_FULL` stands (collision-clean). Names chosen ONCE at the STEP-0
   checkpoint; no mid-lane renames. Census rows + pin bump for any new getenv names
   are coordinator-only at close-out (the A2 probes add NONE — gated under
   CHARDRV_PROBE).
6. **A6 ADOPTED — drawlog × carve-out rule + prewarm + lever-B target set.**
   (i) Drawlog gate addendum: if the carve-out fires unflagged, an over-eps result
   whose divergences are EXCLUSIVELY world-field crowd-name values with count=792
   unchanged is the EXPECTED signature of a genuinely-animating crowd (the
   recalibrated eps was derived from the FROZEN crowd) — escalate to coordinator for
   countersign (coordinator re-derives eps at close-out); ANY count change or
   non-crowd field → revert to flag-gated default-OFF. The lane NEVER edits the
   sidecar. (ii) A10 prewarm boot reinstated: one run with `RB3_PREWARM_SCREENS=1`,
   conditional on ANY ui/*.cpp edit including probe lines. (iii) Lever B acceptance
   += the re-charter must NAME the acceptance target set (which chars/drivers
   constitute "hub walkers") for subsequent waves' census.
7. **A7 ADOPTED — evidence honesty hardened (three mechanics, all mandatory).**
   (1) Raw logs are DELIVERABLES: gzip the full raw stderr of every evidentiary run
   into `W28-CROWD-OWNER/evidence/raw/` (or, if genuinely too large, commit sha256 +
   byte count + a durable non-/tmp copy path in STATUS). (2) STATUS carries a
   per-log probe-count table: `grep -c` for EVERY probe tag emitted this wave
   (CHARDRV_ENTER/CLEAR/PLAY/DIE/REPLACE/REPLACE_BT/POP/STARVE/LIFE/CLIPSWAP/DEFCLIP
   + PANELDBG CheckLoad/CheckUnload/UNLOAD/UnloadPanels) — zeros and omitted rows
   are mechanically visible. (3) Coordinator greps the RAW artifacts BEFORE
   accepting any STATUS headline; excerpts are illustrations, never evidence.
8. **A8 ADOPTED — PROP tail arbitration pre-ruled + numeric acceptance.**
   (i) Ownership arbitration is pre-ruled for concurrency: `CharClip*.cpp` and
   `CharDriver.cpp` writes belong to the CROWD lane THIS WAVE. The PROP tail may
   NOT write either; if the tip-track binding fix requires it, the tail documents
   the exact site + needed edit in its PLAN/checkpoint and DEFERS that piece
   (defer-without-guilt) — CharIKHand.cpp-local work (mFinger feedback break +
   redirect-before-weight-loop) proceeds regardless. (ii) Acceptance is numeric:
   same harness/song/window as W27 (`W26-PROP/run_prop_probe.py`, beastandtheharlot
   guitar/expert ~18s), flag-ON: strum/fret/right_hand **skip=0** and
   `dst_from_hand` **0 entries >30u** (W27 B column: 0/64, 0/36, 0/56 skip/clamp),
   plus visible hand-on-instrument in `/api/screenshot` captures; any quoted
   medians computed by a committed script, not by hand (E6 lesson).

Dispatch readiness per review: READY with A2+A3+A5+A7 folded in (done above).
