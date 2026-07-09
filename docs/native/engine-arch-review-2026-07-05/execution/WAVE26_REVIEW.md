# Wave 26 — Fable pre-dispatch review

**Reviewer:** Fable. **Target:** `WAVE26_KICKOFF.md` (rb3 `34811b9f`).
**Verdict: DISPATCH-WITH-AMENDMENTS (A1–A9 binding).** All code anchors below re-derived from
source at HEAD, not trusted from the kickoff.

## Q1 (R-A) — CROWD merge trigger: dup-load or legitimate deferred selection?

**The destruction site is now PINNED in code, and it changes the expected verdict.**
`FileMerger::NotifyFileLoaded` (`src/system/char/FileMerger.cpp:343-364`) unconditionally calls
`Merger::Clear()` (`FileMerger.cpp:30-52`) on the slot **before** merging a newly completed load;
`Clear()` does `delete mLoadedObjects.front()` for every object the slot's PREVIOUS load merged in,
then `RemoveSubDir`s its `mLoadedSubdirs`. That is the only wholesale-delete path in the merge
machinery and it exactly produces the observed signature (7× `~Object`→`Replace(this,NULL)`
at `src/system/obj/Object.cpp:131` → `CharDriver::Replace`→`DeleteClip`, then DIE, then a second
`Enter` on all 8 — `W25-CROWD/evidence/step0-probe-trace.txt:21-35`). So beat 2.433 is a **second
completed load through the SAME Merger slot** that previously delivered the crowd clip bank.

**Dup vs legit is decidable by one log line, and the evidence leans LEGIT.** `NeedsLoading`
(`FileMerger.cpp:165-174`) already refuses to reload when `mLoaded == mSelected` — a true duplicate
can only fire via (i) the `unk29` force bit or (ii) a **FilePath compare that fails on native**
(path-normalization/case/root divergence — the one clean "native duplicate-load" mechanism
available). But the W25 observation that the post-merge bank is a **player-only sub-bank with zero
crowd clips** (STATUS:37-40) is strong evidence the second load is a **different file** — i.e. a
legitimate deferred selection (dircut/per-shot anim class: `directed_cut_N` slots,
`src/system/bandobj/BandCharacter.cpp:3954-3997`; selection via `BandWardrobe::AddDircut`/
`HarvestDircuts`, `BandWardrobe.cpp:351-382`). A same-file dup re-merge would reproduce an
equivalent bank, not a crowd-less one. **The kickoff's PREFER ordering (suppress first) should be
inverted: plan for re-fire; suppress only if the discriminator proves same-file** (→ A1).

**Native-specific divergence point:** the code path is shared; the divergence is **completion
timing**. `StartLoadInternal` (`FileMerger.cpp:94-129`): sync callers block in
`while(!mFilesPending.empty()) TheLoadMgr.Poll()`; async callers queue on `TheFileMergerOrganizer`
(`FileMergerOrganizer.cpp:122`) and complete whenever the loader gets frames — and native adds the
per-frame loader budget (`src/system/utl/Loader.cpp:571`, `RB3_LOADER_BUDGET_MS`), which is exactly
how a merge that Wii finishes before the vignette's beat 0 slips to beat 2.433 mid-clip. STEP 0
should log the async flag at the triggering `StartLoad` plus who fired it — the existing
`gNativeStartLoadTag` instrumentation (`BandCharacter.cpp:46, 3366-3394`, tags at `:4184/:4214/
:4253`) already exists for this; extend it, don't duplicate it.

**Real anchors for the lane:** `FileMerger.cpp:30-52` (Clear = the kill), `:165-174` (NeedsLoading
= the dup discriminator), `:176-200` (AppendLoader — log `mName`, `mLoaded`, `loading` here),
`:343-364` (NotifyFileLoaded), `:365-372` (proxy-merge branch + `find->SyncObjects()`), `:397-406`
(PostMerge / `on_post_merge`), `Object.cpp:131`, `obj/Utl.cpp:223-296` (MergeObject/Recurse ref
rewiring — where an `mClips` ObjPtr can get re-pointed). One extra probe: identify who re-`Enter`s
all 8 drivers at beat 2.433 (`CharDriver::Enter`, `CharDriver.cpp:191`) — it replays
`mDefaultClip` (`:227-228`), which is nil here, so it's currently a no-op but is the natural
re-fire seam if the fix wants it.

## Q2 (R-B) — re-fire leak/re-entrancy

Re-firing `play_clip` is safe **only** at these times/scopes:
- **Timing:** either the `on_post_merge` message (`FileMerger.cpp:397-406`) **with
  `msg[2] == true`** (`mFilesPending.empty()` — PostMerge otherwise immediately
  `LaunchNextLoader`s, and firing between chained merges re-enters the W25 UAF territory), or —
  safer — the **next `CharDriver::Poll`** (frame boundary), which is exactly the shape of the
  already-landed, crash-proven `RB3_CROWD_CLIP_KEEP` re-arm.
- **Scope:** `mClipType == Symbol("crowd")` (not dir-name matching) — W25's WorldCrowd A/B proved
  gameplay produces ZERO `clipType=='crowd'` CharDriver events (W25-CROWD/STATUS.md:93), so this
  scope is provably dormant in gameplay; the A7 leak (firing on non-crowd drivers) is excluded by
  construction.
- **Never** iterate or deref a foreign/cached bank during the merge frame — the reproduced SIGSEGV
  class (STATUS "why not fully fixed").

**Composition insight the kickoff misses:** the W25 re-arm already re-Plays from the driver's OWN
live `mClips` when starved. If the engine fix merely keeps `mClips` resolving to a crowd-bearing
bank (or re-points it), **the existing flag completes the recovery** — the engine fix may not need
to touch play at all. Evaluate E-C2 (flag removal vs promotion) at close-out accordingly; also do
the E-C3 `gCrowdKeep` pruning cleanup opportunistically.

## Q3 (R-C) — PROP mechanism + regression risk

**Mechanism: the attach/resolution family (V23-adjacent), with a live clip-binding alternative —
the lane must discriminate them.** Verified anchors:
- `BandCharacter::SyncObjects` (`BandCharacter.cpp:2905-2924`) reparents the 8 named bones incl.
  `bone_prop0-3` and `bone_mic_stand_bottom` to the character via `SetTransParent(this,false)` —
  only if `Find()` resolves them at that moment (same load-order-sensitivity class as the hands).
- `mInstDir` resolved at `BandCharacter.cpp:328-333`, polled after `Character::Poll` (`:905`);
  it is a separate tree, deliberately excluded from skeleton rebinding (`:1102-1104`).
- `RndTransProxy::Sync` (`src/system/rndobj/TransProxy.cpp:28-45`) `Find()`s `mPart` in the proxy
  dir and on a miss **silently** `SetTransParent(0,0)` (`:44`) — a load-order miss leaves the
  subtree unparented in resource-milo local space with no warning. That is precisely the observed
  signature (pick z=98 ≈ resource-local vs hand z=48.8; mic stand below floor).
  `BandWardrobe::SyncTransProxies` (`BandWardrobe.cpp:326-340`) wires venue proxies by member-name
  substring, called from `:222`/`:302` with no gating on instrument-load completion.
- Alternative (c): prop-bone chain is parented correctly but the bones' `LocalXfm` is never
  animated (prop-bone clip tracks unbound — the W25 hands parse-time-binding class). Discriminator:
  dump the full `TransParent` chain + world of `bone_pick_strum`/`bone_mic_stand_bottom` in-song —
  NULL/identity-rooted chain ⇒ attach/proxy gap; correct chain but static rest LocalXfm ⇒ binding.

**Match-neutrality:** all candidates are native load-order/timing gaps in shared code paths that
Wii sequences differently — fix is HX_NATIVE-gated workaround class unless the lane proves a true
shared bug; kickoff already says this. Endorsed.

**Regression question (the load-bearing one): the risk is real but NOT the one the kickoff names.**
Per W25 measurements there is **no in-song population of correctly-posed in-reach instrument
targets** — all fret/strum/mic targets measured 98-273u (weight=1), so the "clamp no-op-in-reach"
cases are essentially non-instrument targets that a prop-posing fix shouldn't touch. The actual
risk: today's acceptable in-song look comes from the clamp's `d > k·reach` **keep-clip-pose**
branch; a prop fix that brings a target within reach but a few units WRONG re-engages full-weight
IK and can look worse than the clip pose. Proof protocol → A5. Bounding: the clamp stays default-ON
(14th default), so the worst post-fix case degrades to today's behavior, never to the spike-fan.

## Q4 (R-D) — mic_stand reach=0

**Not a separate MeasureLengths/chain bug; do not touch it.** `MeasureLengths`
(`src/system/char/CharIKHand.cpp:497-511`) requires `mHand->TransParent()->TransParent()` — a
2-bone arm. A mic-stand ikhand is structurally not that, so `mAAPlusBB=0` is authored shape, and
the `:240` guard correctly routes it to the direct-set (no-elbow) path (E-F4). Under direct-set the
hand lands wherever the target is ⇒ **fixing `bone_mic_stand_bottom`'s pose fixes this case for
free**. In scope only as a PROP acceptance case (mic at mouth, stand on floor); no code item.

## Q5 (R-E) — GLOW reachability + a likely root cause the kickoff missed

**Reachable headless; keep the lane; do NOT defer.** Two proven paths: (1) natural — `autohit`
hits gems and builds streak (native/src/rb3_game_input.cpp autohit → GemPlayer/BeatMatcher
autoplay; already used by `scripts/native/song-end-test.py:61` and
`scripts/native/capture_song_gameplay.py:168`); (2) forced — `set_multiplier` handlers
(`StreakMeter.cpp:256`, `BandTrack.cpp:1000`) via `/api/dta/eval`. **Caveat:** direct
`set_multiplier` updates only the meter (label + `mNewStreakTrig`, `StreakMeter.cpp:97-130`); the
peak/glow state fires only via `BandTrack::SetStreak` (`BandTrack.cpp:328-345`: ≥4x non-bass →
`GemTrackDir::PeakState` `:676` → `peak_state.trig`; bass ≥6x → SuperStreak). Use natural autohit
build (or the SetStreak path) for the 4x visuals.
**Likely root cause already in-tree:** the native halo pass **deliberately excludes**
`gem_smasher_glow` unless `RB3_SMASHER_HALO=1` (`native/src/rb3_render_hook.cpp:285-309`, comment
B6). The lane's first A/B must be `RB3_SMASHER_HALO=1` vs OFF on a driven-combo frame — if that
closes S5, GLOW reduces to a flag-default decision + E1, not a fix.

## Q6 — scope/overlap + closures

- **File ownership is disjoint if enforced** (→ A8): CROWD = `char/FileMerger.cpp` +
  `char/CharDriver.cpp` (+ read-only `obj/Utl.cpp`); PROP = `bandobj/BandCharacter.cpp`,
  `bandobj/BandWardrobe.cpp`, `rndobj/TransProxy.cpp`, `char/CharIKHand.cpp` (probes only,
  reuse IK_TGT_DBG); GLOW = `native/src/rb3_render_hook.cpp` + scripts. Collision risk:
  both CROWD (StartLoad-origin tracing) and PROP (instrument rigging selects) want probes in
  `BandCharacter.cpp` — it belongs to PROP; CROWD consumes the existing `gNativeStartLoadTag` logs.
- **WorldCrowd oracle:** untouched by these files, and the crowd-scoped re-fire is provably
  gameplay-dormant (Q2). BUT a `NeedsLoading`/FilePath-compare change is GLOBAL (every merger:
  closet, dircuts, song load) → A3. RndMesh loader: untouched. Hands/binding closures: PROP fixes
  targets, not hand bones/binding — compliant. RB3_IK_REACH_CLAMP stays — kickoff explicit.
- **Sizing:** right. 2 meaty + 1 light, and GLOW may collapse to a flag decision after A7.

## Q7 — gates/lints

Right in outline; gaps fixed by amendments: CROWD's drawlog-792 is a MENU capture and does not
exercise dircut/closet mergers (→ A3 adds a boot A/B); PROP's clamp-fire gate must be a relative
drop, not absolute 0 (W25's documented run-to-run intermittency); PROP's G3 wording ("per the G3
case") must be pinned to case-1 byte-identical like CROWD's (→ A9); GLOW gains the
RB3_SMASHER_HALO step + the PeakState caveat (→ A7). CROWD's deferred near-black-material
discriminator ordering is correct as written.

## AMENDMENTS (binding)

- **A1 (CROWD):** STEP 0 must log, at `AppendLoader`/`NeedsLoading` (`FileMerger.cpp:176/:165`),
  the Merger `mName` + previous `mLoaded` + new `loading` FilePaths for the beat-2.433 load.
  **Expected verdict inverted from the kickoff's preference:** the player-only bank swap indicates
  a legit new selection → plan for RE-FIRE; choose SUPPRESS only if the paths prove same-file
  (then the fix is the FilePath compare/normalization, still flag-gated). Commit this log as the
  checkpointed discriminator artifact.
- **A2 (CROWD):** re-fire hook = `on_post_merge` with `msg[2]==true`, or next-frame
  `CharDriver::Poll`; scope = `mClipType=='crowd'`; never touch foreign banks in the merge frame.
  PREFER composing with the existing RB3_CROWD_CLIP_KEEP re-arm (fix the bank resolution; let the
  flag re-Play), then rule on E-C2 removal/promotion. Do the E-C3 pruning cleanup.
- **A3 (CROWD):** any change to `NeedsLoading`/FilePath comparison is game-global → flag-gated AND
  initially name-scoped, plus a closet-or-gameplay boot A/B beyond drawlog-792.
- **A4 (PROP):** the discriminator must separate parent-chain gap (`TransProxy.cpp:44` silent null;
  `BandCharacter.cpp:2905-2924` Find-miss) from clip-binding gap: dump the TransParent chain +
  world for `bone_pick_strum`/`bone_mic_stand_bottom`/fret targets in-song before any fix.
- **A5 (PROP):** regression gates: (i) per-ikhand pre/post target-distance histogram (reuse
  IK_TGT_DBG) — no ikhand with pre-fix `d ≤ reach` may move its target beyond epsilon; (ii)
  clamp-fire-count as a large RELATIVE drop over a fixed capture, not absolute 0; (iii) E1 judged
  per-instrument closeup (fret hand ON neck, pick at strings, mic at mouth) — ratio gates alone
  cannot distinguish clip-pose from IK-pose.
- **A6 (PROP):** mic_stand reach=0 is expected chain shape — do NOT modify `MeasureLengths` or
  extend the clamp to reach==0 hands; cover via the mic E1 acceptance only.
- **A7 (GLOW):** step 1 = `RB3_SMASHER_HALO=1` A/B (`rb3_render_hook.cpp:285-309`); build streak
  via autohit (proven harness); note direct `set_multiplier` does not fire PeakState — ≥4x visuals
  need the SetStreak path or natural build.
- **A8 (ownership):** `FileMerger.cpp`+`CharDriver.cpp` = CROWD only; `BandCharacter.cpp`+
  `BandWardrobe.cpp`+`TransProxy.cpp`+`CharIKHand.cpp` = PROP only; `rb3_render_hook.cpp` = GLOW
  only. CROWD reads `gNativeStartLoadTag` output rather than editing BandCharacter.cpp.
- **A9 (G3):** PROP's shared-engine edits pinned to G3 case-1: byte-identical `#else`,
  `batch_objdiff` == baseline exactly (same standard as CROWD's line).

## Verdict

**DISPATCH-WITH-AMENDMENTS (A1–A9).** The wave is correctly shaped (recon-first, discriminators
checkpointed, closures restated, hazard files named). The two load-bearing corrections: (Q1) the
merge-kill site is `Merger::Clear` via `NotifyFileLoaded`, and the bank-swap evidence points to a
legitimate deferred selection — so the lane should expect re-fire, not suppress; (Q3) the real
PROP regression risk is re-engaged IK toward near-but-wrong targets, gated by A5 and bounded by
the clamp staying ON.
