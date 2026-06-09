# wave-07 — on-stage band ANIMATION (static skeleton) — DC3-COMPARE (READ-ONLY)

Canonical investigation: `docs/native/CHAR_SKINNING_DEFORM_INVESTIGATION.md` `### wave-07
DC3-COMPARE`. This file = the per-wave detail + exact DC3 citations + probe plan.

## The question

After wave-06 the on-stage band RENDERS coherently (the gender-bind shard is rebaked away),
but the skeleton is STATIC: `XBONE_TRACK` shows `bone_R-upperArm` worldPos byte-identical
across 1424 draws over a whole song, MALES TOO, WITH clip+IK env-flags on. The crowd DOES
animate (own per-character skeletons). Why is the on-stage band static while DC3's native
build animates its dancers on the SAME shared Milo char engine?

## Answer (READ-ONLY trace, both trees)

The shared per-frame char-anim engine is IDENTICAL in both games and is NOT the gap:

```
Character::Poll()                       char/Character.cpp  (RB3 :217 / DC3 :422)
  → RndDir::Poll() iterates mPolls
    → CharDriver::Poll()                char/CharDriver.cpp:339   (shared, both games)
        f17 = mBeatScale * TheTaskMgr.Beat()
        if (mFirst)  mFirst = mFirst->PreEvaluate(f17, ...)      // advance clip
        if (mFirst)  mFirst->Evaluate(...);                       // frame -> values
                     mFirst->ScaleAdd(*mBones, Weight())          // write CharBones
    → CharBonesMeshes::PoseMeshes()     char/CharBonesMeshes.cpp:98  (shared)
        writes mBones values into the skeleton RndTransformable LocalXfms,
        resolved by NAME via CharUtlFindBoneTrans(name, Dir())   char/CharUtl.cpp:183
```

The hinge is `mFirst` in `CharDriver::Poll`: it is the head of the playing-clip list. **If no
clip is playing, `mFirst == nullptr`, the whole `if (mFirst)` block is skipped, `mBones` is
never written, and `PoseMeshes` poses the BIND pose every frame → byte-identical static.**
"clip+IK on" (i.e. `RB3_NO_CLIP`/`RB3_NO_IK` env unset) only means the drive code is not
env-DISABLED; it does NOT mean a clip is actually PLAYING. The band has no clip to play.

### Who is supposed to PLAY the band's clip (RB3)

The band's per-song choreography is the `song.anim` `RndPropAnim`, advanced by the venue
camera tick, which fires `play_group`/`set_play` messages on each `BandCharacter`:

```
WorldDir::Poll()                        world/Dir.cpp:158
  → HandleType(select_camera_msg)       // venue "world"-type TypeDef dispatch
    → $banddirector select_camera
      → BandDirector::OnSelectCamera()   bandobj/BandDirector.cpp:1376
          mPropAnim->SetFrame(TheTaskMgr.Seconds(kRealTime)*30, 1)   :1384
            → song.anim PropKeys evaluate -> play_group / set_play SymbolKeys
              → BandCharacter::OnPlayGroup / OnSetPlay   bandobj/BandCharacter.cpp:1810/1838
                → SetState(group, ...) -> PlayMainClip(...)           :1323 / :189
                  → unk454->Play(clip, ...)  -> a PLAYING CharClipDriver  :271
```

`mPropAnim` is bound in `BandDirector::OnFileLoaded` when `sym==song`
(`BandDirector.cpp:1202`, `dir->Find<RndPropAnim>("song.anim", false)`). If that Find fails,
an EMPTY fallback PropAnim is built (`unk110`, :1208-1224) with ONLY shot_bg/intensity keys
— **NO per-member `play_group` char-clip keys** → the band would never receive a group.

### DC3 had this EXACT bug and fixed it (the comparison)

DC3 native dancers showed the identical symptom — "characters freeze during gameplay; song.anim
advances, beat works, venue loads, but characters stand motionless in rest pose" — and DC3
root-caused it to async-load timing + DTA-flow gaps that stop the choreography clip from ever
PLAYING, NOT a skinning/bind issue. Sources (all in `dc3-decomp/docs/sessions/`):

| DC3 gap | DC3 file/fix | RB3 parallel to probe |
|---|---|---|
| **#1 venue WorldDir lost `world` TypeDef → `select_camera` UNHANDLED → PropAnim never advances** (`Copy(kCopyFromMax)` skips TypeDef, `obj/Object.cpp:172`; `{$world set_type world}` DTA never fires natively) | `2026-03-17-song-anim-advancement.md`: fix = `SetType("world")` in `HamDirector::VenueEnter` | Does `BandDirector::OnSelectCamera` run + `mPropAnim->GetFrame()` advance? If not → set the venue WorldDir's `world` TypeDef at the RB3 venue seam (`BandDirector::EnterVenue`/`LoadVenue`, `BandDirector.cpp:593-720`). |
| **#2 HamDriver::Poll weight-bootstrap deadlock** — `Layer::mWeight` uninit → 0 on native heap → `mWeight>0` guard blocks `Eval()` forever → clip queued but never evaluated | `2026-03-23-character-animation-investigation.md` §1: HX_NATIVE bootstrap forcing `Eval(1.0f)` once | If a clip IS playing but `moved≈0`: check `CharDriver::Weight()` / `CharClipDriver::mBlendFrac`→`Sigmoid` bootstrapping to 0 on native (`mFirst->ScaleAdd(*mBones, f14)` with `f14==0` writes nothing). |
| **#3 post-merge deferred choreography init** — init runs ~frame 20 before async merge (~1600); never re-run → clip-player has no keyframes → 0 clips | `2026-03-17-character-animation-freeze.md`: post-merge `HamDirector::Initialize()` re-runs init; + routine-builder bypass → return pre-authored `song.anim` directly | Confirm the loaded `song.anim` carries per-member `play_group` keys (not the `unk110` empty fallback `BandDirector.cpp:1210`), and that those keys actually `Handle` on the BandCharacters. |
| **#4 Game beat-freeze** — audio-fail set `mRealTime=false` → `mAudio.GetTime()`=0 → beat pinned at 0 → clip frames never advance | `2026-03-23-...` §5: `mRealTime=true` + wall-clock offset | `OnSelectCamera` uses `TheTaskMgr.Seconds(kRealTime)` (:1379) — confirm that clock advances. (Lower suspect; RB3 audio works post wave-05.) |

DC3's dancers ANIMATE because (a) their CharDriver HAS a playing clip — the MoveDir/HamDriver
choreography assigns it every song frame — AND (b) DC3 fixed those four native-flow gaps so
the clip actually plays + advances. RB3 reuses the SAME `CharDriver`/`CharBonesMeshes`/
`PoseMeshes` engine; the only missing piece is the band-side equivalent of "a playing clip
gets assigned per song frame."

Note: DC3's `HamCharacter::Poll` (HX_NATIVE, `hamobj/HamCharacter.cpp:957-974`) also forces
`SetShowing(true)` so `RndDir::Poll` runs the child pollables ("On native, always force
Showing(true) ... so animations advance"). **RB3 ALREADY HAS the equivalent** in
`BandCharacter::Poll` (`bandobj/BandCharacter.cpp:387-484`: `SetShowing(true); if (Showing())
{ ... Character::Poll(); ... } SetShowing(wasShowing);`), so that specific gap is NOT present
in RB3 — the band IS being polled. The missing piece is upstream: the clip-ASSIGN.

## Probe plan (probe-agent, the ONE that builds)

The decisive tool already exists in-tree — `BAND_ANIM_PROBE` in
`src/system/bandobj/BandCharacter.cpp:411-471`:

```
BAND_ANIM_PROBE='*'   # match all band members (or a substring of a member dir name)
BAND_ANIM_BONE=bone_R-upperArm.mesh   # optional; default is that bone
```

It prints, per member per ~30 frames:
`[BAND_ANIM] member=... grp=... mDriver=... clipType=... FirstPlaying=... clip='...' |
unk454=... u454clip='...' bones=... | bone='...' pre=(...) post=(...) moved=...`

`pre`/`post` straddle `Character::Poll()` (the actual skeleton-drive sweep); `moved` is the
worldPos delta. Decision tree:

1. **`clip='(none)'` and `grp='(none)'`** → no group/clip assigned → the choreography-assign
   gap (DC3 #1 or #3). Next: add a one-shot log in `BandDirector::OnSelectCamera` (does it run?
   what is `mPropAnim->GetFrame()` frame-to-frame?). Frame NOT advancing → venue `world`
   TypeDef / `select_camera` dispatch gap (DC3 #1). Frame advancing but band still `(none)` →
   the `song.anim` lacks per-member `play_group` keys or they don't `Handle` (DC3 #3 class /
   `unk110` empty-fallback at `BandDirector.cpp:1210`).
2. **`grp='<name>'` but `clip='(none)'`** → `PlayMainClip` is bailing: `BandCharacter.no_anim`
   DataVar set (`BandCharacter.cpp:190-192`), or `unk454->ClipDir()` null, or
   `CharClipGroup` `mGroupName` not found (`:200-208`), or `GetClip(mask)` returns null
   (`:229-246`). The probe's `grp=` + the existing `MILO_NOTIFY_ONCE` strings localize which.
3. **`clip='<name>'` but `moved≈0`** → clip playing, not driving the bone → weight-bootstrap-0
   (DC3 #2: check `CharDriver::Weight()`/`mBlendFrac`→`Sigmoid`) OR posed≠drawn instance (the
   wave-06 magnet split — but that desyncs males too, and the rebake already handles the
   female offset; lower-probability). `bones=` pointer + the wave-06 `XBONE` boneDir let you
   confirm whether the posed CharBones target the drawn magnet.

Expected outcome: branch 1 (`clip='(none)'`) — the band never gets a playing clip — and the
root is the venue `select_camera`/`song.anim`-advance gap, the same class DC3 fixed with
`SetType("world")` at VenueEnter. Port that pattern at RB3's venue seam.

## Files (read-only references)

RB3: `src/system/bandobj/BandDirector.cpp` (Poll :242, OnSelectCamera :1376, OnFileLoaded
:1199, EnterVenue/LoadVenue :593-720), `src/system/bandobj/BandCharacter.cpp` (Poll :319,
PlayMainClip :189, SetState :1323, OnPlayGroup :1810, BAND_ANIM_PROBE :411), `src/system/
char/CharDriver.cpp` (Poll :339), `src/system/char/CharBonesMeshes.cpp` (PoseMeshes :98),
`src/system/world/Dir.cpp` (Poll/select_camera :124-164), `src/App.cpp` (RunOneFrame
TheBandDirector->Poll :506, TheTaskMgr.Poll :531).

DC3: `dc3-decomp/src/system/hamobj/HamDirector.cpp` (VenueEnter SetType("world"); SongAnim
routine-builder bypass; post-merge Initialize), `dc3-decomp/src/system/hamobj/HamDriver.cpp`
(Poll weight bootstrap), `dc3-decomp/src/system/hamobj/HamCharacter.cpp` (Poll :957
SetShowing-force; SyncObjects :196), `dc3-decomp/src/system/hamobj/MoveDir.cpp`
(SetSongPlayClip :1291, Poll :607), `dc3-decomp/src/lazer/game/Game.cpp` (PostWaitStart
mRealTime). DC3 session docs: `2026-03-17-character-animation-freeze.md`,
`2026-03-23-character-animation-investigation.md`, `2026-03-17-song-anim-advancement.md`,
`2026-03-17-hamobj-native-hack-audit.md`.
