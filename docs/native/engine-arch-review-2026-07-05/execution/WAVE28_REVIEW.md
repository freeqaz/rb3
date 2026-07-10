# WAVE 28 — Pre-Dispatch Review (Fable, adversarial)

**Under review:** `WAVE28_KICKOFF.md` (draft, `b3793f51`) against
`WAVE27_CLOSEOUT_REVIEW.md` (Q1/Q6/E1-E7, `09cca9e8`), W27-CROWD/STATUS.md (+E1-E5),
W27-PROP-PROBE/STATUS.md (+E6-E7), README.md Wave-27/28 section, and the
WAVE27_KICKOFF.md COORDINATOR ACCEPTANCE block (A1-A10). Method: static reads + grep
of the RAW sources (probe sites, resolution code, asset tree, engine classjson) —
no builds, no runs, no code changes.

**VERDICT: READY-WITH-AMENDMENTS** — blocking: **A2, A3, A5, A7**.
Non-blocking but fold in: A1, A4, A6, A8.

The kickoff faithfully implements close-out Q6 (discriminator-first, all four STEP-0
items present, checkpoint-before-fix binding, lever A/B fork, E-C2 parked, PROP tail
optional). The E1-E5 errata are correctly summarized (verified against
W27-CROWD/STATUS.md:133-173). Prior-state header verified: pin `be401ec`
(native/CMakeLists.txt:74), census 410 (NativeCompatFlags.gen.inc getenv-row count =
410), recalibration landed in `a47d82f0` (N=8 runs, gate GREEN). The four blocking
amendments are precision fixes, not redesigns.

---

## (a) STEP-0 item 1 — probes exist; interleave is real; recipe needs one pin [A1]

**Verified — all cited probes exist at the cited sites, env-gated as claimed:**

- `CHARDRV_ENTER` — src/system/char/CharDriver.cpp:245 (gated `CHARDRV_PROBE`, :234)
- `CHARDRV_CLEAR` — CharDriver.cpp:273
- `CHARDRV_PLAY` — CharDriver.cpp:431
- `CHARDRV_DIE` / `CHARDRV_LIFE` — CharDriver.cpp:533 / :542 (unsampled transition
  detector via `gPrevFirst`, :529-537)
- `CHARDRV_REPLACE` — CharDriver.cpp:848; `CHARDRV_REPLACE_BT` gated `CHARDRV_BT`
  at :856-862, `backtrace_symbols_fd(bt, n, 2)` → fd 2
- Panel probe (`RB3_CROWD_PANEL_DBG`): UIPanel.cpp:54,:67 (CheckLoad/CheckUnload
  `mLoadRefs`), UIScreen.cpp:559,:576 (Load/UnloadPanels markers), PanelDir.cpp:244
  (Enter+triggers), BandScreen.cpp:66,:73,:87 (interstitials).

**Interleave verdict: ONE stderr stream, ordering = call order — the kickoff's "one
boot" instruction is sound.** The CHARDRV family writes `fprintf(stderr, ...)`
directly; the panel probes use `MILO_LOG` → `Debug::Print` (src/system/os/Debug.cpp:467)
→ `OSReport` → native shim `vfprintf(stderr, ...)` (native/src/rvl_shims.cpp:30-35).
Both land unbuffered on the same fd in emission order; the BT symbol dump also goes to
fd 2. So the mechanical recipe is: **one boot, all three env vars
(`RB3_CROWD_PANEL_DBG=1 CHARDRV_PROBE=crowd CHARDRV_BT=1`), capture stderr to ONE
file** — this is exactly what W27's `combined-boot.log` already demonstrated (close-out
Q1 cites REPLACE lines at combined-boot.log:597-603).

**Gap (A1, non-blocking):** the PANELDBG lines carry **no beat/frame stamp** (verified:
UIPanel.cpp:55, UIScreen.cpp:560/:577 log only name+refs), while every CHARDRV line
logs `beat=%.3f`. Kill-frame attribution therefore rests on line adjacency alone.

> **A1 (amend):** permit the lane two one-line probe edits adding
> `beat=%.3f` (`TheTaskMgr.Beat()`) to the `UIScreen::UnloadPanels` marker and the
> `UIPanel::CheckUnload` UNLOAD line; add `src/system/ui/UIScreen.cpp` to the owned
> files as **probe-line-only** (it hosts two of the W27 probe sites but is absent from
> the kickoff's owned list, which names only UIPanel/PanelDir).

## (b) STEP-0 item 2 — "probes exist" is an OVERCLAIM; the swap question is NOT currently observable [A2 — BLOCKING]

The clip-set resolution mechanism, from source:

- **The member:** `ObjPtr<ObjectDir> mClips` — CharDriver.h:87. Also
  `ObjPtr<Hmx::Object> mDefaultClip` (:92), `Symbol mClipType` (:105).
- **Where it binds:** deserialization, `bs >> mClips` — CharDriver.cpp:890 →
  `ObjPtr::Load(bs, warn, dir=NULL)` — ObjPtr_p.h:536-543: reads the serialized NAME
  (e.g. `"clips"`) and resolves `mOwner->Dir()->FindObject(buf, false)`. The winner
  among same-named candidates is decided by `ObjectDir::FindObject` — Dir.cpp:531-542:
  own entry table first, then **`mSubDirs` in index order, first match wins**
  (parentDirs=false here). I.e. **which of the two same-named `clips` sets a driver
  binds is decided by subdir/merge ORDER at load time.**
- **Post-load mutation sites:** `SetClips` — CharDriver.cpp:294-297 (also reached by
  `SYNC_PROP_SET(clips, ...)` :1053); direct assignment in the copy path (:285,
  `COPY_MEMBER(mClips)` :935). Nothing else writes mClips.
- **`play_clip` resolution:** `play_clip` is a **Character** handler
  (Character.cpp:896 → `OnPlayClip` :923-930) → `mDriver->Play(node,...)` →
  `FindClip` (CharDriver.cpp:374-385) → `MyFindClip` (:342-372): if the DataNode is
  `kDataObject` it uses the embedded object pointer and **ignores mClips entirely**;
  if symbol/string it does `mClips->FindObject(name, false)`. So play_clip resolves a
  clip NAME *inside* the already-bound set (or bypasses the set) — it never re-picks
  the set.
- **Group path:** `PlayGroup` → `mClips->Find<CharClipGroup>(cc, false)`
  (CharDriver.cpp:456).

**Is "does any driver's mClips SWAP at the kill" observable with existing probes? NO.**
The only probe that prints `mClips` is the `[CHARDRV]` line at CharDriver.cpp:502-516
(`clips=%p clipsName='%s' nclips=%d`), and it is **%60-sampled** (`if ((n++ % 60) == 0)`,
:503) — this sampling is precisely why the close-out ruled the swap question "not
decidable from the %60-sampled logs" (WAVE27_CLOSEOUT_REVIEW.md Q1(b)). Likewise the
**E5 serialized default-clip name is not observable**: `ObjPtr::Load` consumes the
name into a stack buffer and discards it (ObjPtr_p.h:538-543); `CHARDRV_ENTER` logs
only the RESOLVED pointer's name (CharDriver.cpp:245-249). The kickoff's STEP-0 header
"(≤1 day; probes exist)" is therefore true only for items 1 and 4.

> **A2 (amend, BLOCKING):** correct the STEP-0 header to "probes exist for items 1
> and 4; items 2-3 require two small new probe additions inside owned
> CharDriver.cpp", and specify them:
> 1. **mClips-swap detector (unsampled):** a `gPrevClips` per-driver transition
>    detector in `Poll`, same pattern as the existing `gPrevFirst`/`CHARDRV_DIE`
>    detector (CharDriver.cpp:529-537), logging
>    `[CHARDRV_CLIPSWAP] dir=... from=%p'%s' to=%p'%s' beat=...` on any change —
>    PLUS one line in `SetClips` (:294) so swaps that occur between polls are
>    attributed to their caller (the copy path :285 is covered by the Poll detector).
> 2. **E5 serialized-name probe:** at LOADS (CharDriver.cpp:923), capture the
>    serialized default-clip STRING (Tell/peek-ReadString/Seek-back around
>    `mDefaultClip.Load`, or an HX_NATIVE-gated manual read+resolve replicating
>    ObjPtr_p.h:536-543), logging
>    `[CHARDRV_DEFCLIP] dir=... serialized='%s' resolved=%p`. BinStream has
>    Tell/Seek (BinStream.h:44-55,:82).
> Both are env-gated HX_NATIVE probe-class, inside the owned file — no scope change.
> Also fold E3 into item 2's deliverable: per-driver `CHARDRV_PLAY` counts (why does
> `crowd_female04` never Play — 7 PLAY lines, LIFE firstSet=0).

## (d)+(b) Lever A is worded at the WRONG LAYER [A3 — BLOCKING]

The kickoff's lever A says "fix `play_clip`/driver clip resolution so they resolve
the RESIDENT copies". Per the source walk above, `play_clip` never chooses the set —
the set choice is made **at milo load time** by `ObjPtr::Load` → owner-dir
`FindObject` with first-match subdir ordering (Dir.cpp:535-542), or by a later
`SetClips`/copy, or the trig may reference the clip **object directly**
(`MyFindClip` kDataObject branch, CharDriver.cpp:345-347) so that no name resolution
happens at all at play time. A lane that implements the lever as written would patch
`FindClip`/`Play` and miss a load-order or object-reference divergence.

> **A3 (amend, BLOCKING):** reword lever A to: *"fix the driver's clip-set
> BINDING so the resident streetslomo drivers resolve/hold the RESIDENT `clips`
> copy — at whichever layer the STEP-0 ownership-chain dump names: (i) `mClips`
> load-time resolution (ObjPtr::Load → FindObject subdir ORDER / namespace —
> an ordering or dir-scoping fix is in-scope), (ii) a post-load `mClips` swap
> (SetClips/copy path), or (iii) the trigger's direct object reference
> (MyFindClip kDataObject path). play_clip-time re-resolution is a permissible
> mechanism only if the dump shows that is where Wii diverges."* Same carve-out
> and fallback rails unchanged.

## (c) STEP-0 item 4 — identity-check evidence paths verified, with two caveats [A4]

- `orig-assets/extracted/config/vignettes.dta` EXISTS; the sv3 backdrop `dyn_file`
  is **campaign-conditional** (lines 4-28): fresh-save `TRUE` branch → `("sv3_a")`
  only; advanced campaign levels randomize over sv3_b×4/sv3_a/sv7_*. A deterministic
  fresh-boot native run loads **sv3_a**. Interstitial mapping at :129-136 (sv3/sv8).
- `orig-assets/extracted/ui/main/main_hub.dta:744` —
  `(panels meta sv3_panel main_hub_panel accomplishments_status_panel)`. (Note the
  file is `ui/main/main_hub.dta`, not `ui/*.dta` top-level.)
- Milo containers: `orig-assets/extracted/world/vignette/shell/gen/{sv3_a,sv3_b,sv8_a}.milo_xbox`
  all present; hub milo at `ui/main/gen/main_hub.milo_xbox`.
- **Tooling:** static top-level entry listing exists —
  `scripts/milo/mip_strip.py` `read_container` + `parse_dir_entries` (mip_strip.py:399-425,
  returns ordered `(className, objName)`); DC3's `inflate_milo.py`
  (dc3-decomp/scripts/milo/) for the container blocks. **Nested payloads (streetslomo
  inside sv3_a) are packed and NOT statically listable** — runtime dump via
  `/api/dta/eval` remains the pinned method (W27-A4 carried; `PathName()` — Utl.cpp:30
  — prints a full ownership chain in one call, use it in any dump probe).
- **Retail GT is thin:** exactly ONE hub screenshot,
  `images/retail-screenshots/yt_mhKNp9uAT48_menu_hub.png` (360/PS3, static frame;
  README:26 describes "animated neon-street scene"). A static frame can establish
  figure PRESENCE in retail main_hub, **not motion**, and it is 360/PS3 (acceptable
  proxy per the project's Xbox-assets preference).

> **A4 (amend):** pin these exact paths in STEP-0 item 4; require the lane to
> RECORD which dyn_file variant the boot loaded (expect sv3_a on fresh save); state
> explicitly that the screenshot can only prove presence/absence of street figures,
> so the decisive identity evidence is the runtime dir dump (sv8 vs sv3 chains from
> item 2) + the milo/DTA listings, with the video source (`mhKNp9uAT48`, README:93)
> as optional motion corroboration.

## (e) Flag names [A5 — BLOCKING for the CROWD flag only]

- `RB3_HUB_CROWD_REBIND`: **no textual collision** (0 hits in rb3 src/, native/, engine
  src/, classjson) — but a **semantic collision**: "crowd rebind" is established
  vocabulary for the default-ON gameplay bone-rebind workaround
  `RB3_NO_CROWD_REBIND` + its probe `CROWD_REBIND_PROBE`
  (engine classjson rows; src/system/world/Crowd.cpp:929-932 — **inside the protected
  oracle range :884-1000 this lane must not touch**). A flag named `*_CROWD_REBIND`
  that has nothing to do with bone rebinding invites exactly the cross-attribution
  confusion this campaign keeps paying for.
- `RB3_PROP_POSE_FULL`: clean. No hits anywhere; `RB3_PROP_POSE`/`RB3_PROP_POSE_DBG`
  exist (classjson:1711,1718; CharIKHand.cpp:54,:58) but all reads are exact-match
  `getenv` — no prefix matching in NativeCompatFlags.cpp or call sites.

> **A5 (amend, BLOCKING):** rename the lever-A flag to **`RB3_HUB_CROWD_CLIPBIND`**
> (or `RB3_HUB_CLIPSET_BIND`) — it names the actual mechanism (clip-set binding) and
> stays out of the bone-rebind namespace. Keep `RB3_PROP_POSE_FULL`. Blocking because
> the name is chosen ONCE at the STEP-0 checkpoint with no mid-lane renames. Add a
> close-out reminder: every new getenv name (flag + the A2 probes, e.g.
> CHARDRV_CLIPSWAP/CHARDRV_DEFCLIP if separately gated — prefer gating them under the
> existing `CHARDRV_PROBE` to avoid census growth) needs a census row + pin bump,
> census 410→N, coordinator-only.

## (f) Gates [A6]

- **drawlog row is correct post-recalibration** (recalib landed `a47d82f0`, N=8,
  sidecar `native/tests/goldens/drawlog/splash_screen.fixedclock-residual.json`
  exists; "do NOT touch the sidecar" + ambient-RED escalation matches close-out Q4).
- **A7 revisit carried correctly** for lever A (boot A/B row: `mLoadRefs` returns to
  pre-cycle + census re-passes AFTER revisit — matches W27-A7's sharpened form).
- **Gap 1 — carve-out × recalibrated-eps interaction is unstated.** The recalibrated
  per-name eps was derived from the FROZEN crowd's jitter. If the A6 carve-out fires
  (unflagged fix) and the crowd genuinely ANIMATES, the world-field crowd-name values
  will exceed that eps by construction — the gate will read RED with exactly the
  signature the carve-out calls acceptable ("unchanged draw count, only world-field
  crowd-pose diffs"). The gates table says flat "792 PASS", which an unflagged
  animation-restoring fix cannot satisfy. Without a stated rule the lane will either
  argue around the gate (how gates die) or wrongly fall back.
- **Gap 2 — A10 prewarm rail dropped.** W27-A10 existed because the ui files host the
  prewarm system (UIScreen.cpp:157-380). W28 owns ui files read-mostly, but A1 above
  adds probe lines to UIScreen.cpp.
- **Lever B rigor:** adequate as drafted (STEP-0-grade evidence), with one addition —
  the re-charter must explicitly redefine the ACCEPTANCE TARGET SET (which named
  chars/drivers constitute "hub walkers" for future waves' census), so W29 doesn't
  inherit the W23 ambiguity that caused this whole supersession chain.

> **A6 (amend):** (i) add to the drawlog gate row: *"if the A6 carve-out fires
> unflagged, an over-eps result whose divergences are EXCLUSIVELY world-field
> crowd-name values with count=792 unchanged is the EXPECTED signature — escalate to
> coordinator for countersign (coordinator re-derives eps at close-out); any count
> change or non-crowd field → revert to flag-gated default-OFF. The lane never edits
> the sidecar."* (ii) reinstate the A10 prewarm boot (one run,
> `RB3_PREWARM_SCREENS=1`) conditional on ANY ui/*.cpp edit, including probe lines.
> (iii) lever B acceptance += "names the acceptance target set for subsequent waves".

## (g) Evidence honesty — not yet strong enough [A7 — BLOCKING]

The kickoff requires "STATUS excerpts must cite RAW log paths + line ranges;
coordinator E1 greps RAW logs". Two failure modes survive that wording:
(1) `/tmp` raw logs are volatile — if they evaporate before close-out, the
countersign grep is impossible (W27's countersign only worked because
`/tmp/w27-crowd/*.log` happened to still exist); (2) the W27 E1 excerpt did not
misquote lines — it **omitted** them, and a path+line-range citation of a curated
excerpt would still have passed.

> **A7 (amend, BLOCKING):** harden the Evidence-honesty gate to three mechanics:
> 1. **Raw logs are deliverables:** gzip the full raw stderr of every evidentiary run
>    into `W28-CROWD-OWNER/evidence/raw/` (they are line-oriented text; small when
>    gzipped) — or, if genuinely too large, commit `sha256 + byte count + absolute
>    path` in STATUS and copy the file to a durable location outside /tmp.
> 2. **Mandatory probe-count table in STATUS:** for each raw log, `grep -c` counts of
>    EVERY probe tag emitted this wave (CHARDRV_ENTER/CLEAR/PLAY/DIE/REPLACE/
>    REPLACE_BT/POP/STARVE/LIFE/CLIPSWAP/DEFCLIP + PANELDBG CheckLoad/CheckUnload/
>    UNLOAD/UnloadPanels) — a zero or an omitted row is then mechanically visible,
>    which is precisely the check that would have caught W27's omission of 7
>    REPLACE lines.
> 3. **Countersign order:** coordinator greps the RAW artifacts BEFORE accepting any
>    STATUS headline; excerpts are illustrations, never evidence.

## (h) PROP tail — concurrency and acceptance [A8]

- **Concurrency: conditionally safe, one real collision risk.** `CharIKHand.cpp` is
  disjoint from the CROWD lane. But the tail's second owned surface — "narrowly the
  clip-binding site the tip-track fix requires" — is unnamed, and the plausible
  candidates (CharClip/clip-track binding code) sit inside `CharClip*.cpp`, which the
  CROWD lane ALSO owns "read + narrow". Two lanes with concurrent narrow-write rights
  to the same files is how staging accidents happen.
- **Acceptance measurability: yes, headlessly** — the W27 A/B was fully headless:
  `RB3_IK_CLAMP_DBG` skip/clamp counts + `RB3_PROP_DST_DBG` >30u entries
  (CharIKHand.cpp:359,:415 area) via the committed harness
  `W26-PROP/run_prop_probe.py` (exists). "Hands track strum/fret" as worded is vague;
  the B column of W27's table is the concrete bar.

> **A8 (amend):** (i) the PROP tail must NAME the exact clip-binding file(s) in its
> PLAN before dispatch of any edit; if any named file intersects the CROWD lane's
> owned set (`CharDriver.cpp`, `CharClip*.cpp`), coordinator arbitrates ownership
> BEFORE either lane writes it — default assignment: `CharClip*.cpp` writes belong to
> the CROWD lane. (ii) make the acceptance bar numeric: same harness/song/window as
> W27 (`run_prop_probe.py`, beastandtheharlot guitar/expert ~18s), flag-ON:
> strum/fret/right_hand **skip=0** and `dst_from_hand` **0 entries >30u**
> (reproducing W27's B column: 0/64, 0/36, 0/56 skip/clamp), plus visible
> hand-on-instrument in `/api/screenshot` captures; medians (if quoted) computed by
> a committed script, not by hand (E6 lesson).

---

## Fidelity spot-checks (no amendment needed)

- Kickoff "Why this wave" bullets match the close-out errata verbatim in substance
  (E1 seven REPLACE kills / beat 2.433; E2 sv8 owner; E3 7-of-8; E4 reframe; E5
  serialized-only) — checked against W27-CROWD/STATUS.md:133-173.
- E-C2 park carried (kickoff :97-98 = close-out Q3).
- "FOURTH narrative correction" framing and checkpoint-before-fix binding = Q6.
- Cited probe line numbers in kickoff STEP-0 item 1 (~:245/:273/:533/:848-870) are
  accurate to the current tree.
- NOT-owned list correctly retains the protected `Crowd.cpp:884-1000` oracle, RndMesh
  loader, hands/FOREARM families, and both concurrent-agent files.
- `bin/analyze-function`-class decomp evidence is not needed for this wave's STEP-0
  (native-runtime question), so its absence from the kickoff is fine.

## DISPATCH READINESS

**READY-WITH-AMENDMENTS.** Blocking before dispatch:

| # | One-line summary | Blocking? |
|---|---|---|
| A1 | Interleave recipe pinned (one boot, ONE stderr capture — verified both probe families reach stderr); add beat stamps to 2 PANELDBG lines; add UIScreen.cpp probe-line-only to owned files | no |
| A2 | Fix "probes exist" overclaim: items 2-3 need two specified new probes (unsampled mClips-swap detector + E5 serialized-name capture); fold E3 per-driver PLAY counts into item 2 | **YES** |
| A3 | Reword lever A: binding divergence is load-order/ObjPtr-resolution/object-ref layer, not play_clip-time resolution — allow ordering/namespace fix | **YES** |
| A4 | Pin identity-check evidence paths (vignettes.dta campaign-conditional dyn_file → record variant; main_hub.dta:744; shell/gen milos; mip_strip.py parse_dir_entries top-level only; single static hub screenshot = presence not motion) | no |
| A5 | Rename `RB3_HUB_CROWD_REBIND` → `RB3_HUB_CROWD_CLIPBIND` (semantic collision with RB3_NO_CROWD_REBIND bone-rebind vocabulary in the protected oracle); RB3_PROP_POSE_FULL clean; census rows at close-out | **YES** |
| A6 | Drawlog gate: state the unflagged-carve-out over-eps escalation rule; reinstate conditional A10 prewarm boot; lever B must name the future acceptance target set | no |
| A7 | Evidence honesty hardened: raw logs committed (gzip) or sha256+durable copy, mandatory per-log probe-count table in STATUS, coordinator greps raw before accepting headlines | **YES** |
| A8 | PROP tail: name the clip-binding file in PLAN pre-edit + coordinator arbitration on CharClip*/CharDriver overlap (default: CROWD owns); numeric acceptance = W27 B-column reproduction (skip=0, 0 dst entries >30u) + screenshot | no |
