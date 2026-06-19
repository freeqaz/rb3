# char-Load 5b — un-gate attempt: BLOCKED on a downstream destructor crash

**Date:** 2026-06-11. **Verdict: BLOCKED** (do NOT lift the gate yet). The
serialization is byte-correct (CharLoad5b gtest 4/4), but lifting the
`BandHeadShaper::Init` gate so `gHeadMale`/`gHeadFemale`/`gVisemes[]` become
non-null introduces a **null-deref SIGSEGV during the first screen transition**
(splash → next), in the panel-unload destructor chain. The crash is a downstream
consumer of the head-shaper dirs, NOT a char-Load bug.

## What was tried (worktree, hand-reverted to pristine — no commit)

Replaced the two `_tmp0/_tmp1 = false;` HX_NATIVE gates with an env opt-out
(`if (getenv("RB3_NO_HEAD_SHAPER")) _tmp0 = false;`) so head shapes load by
default on native. Built `rb3-native` + `rb3-tests` at **-O0 Debug** (the
canonical native opt level — `RelWithDebInfo`/`-O>0` does NOT link: `inline`
fns in `TimeConversion.cpp` with `#pragma force_active on` are only emitted
out-of-line as weak symbols at `-O0` under clang; see `native/CMakeLists.txt`
~lines 987-1008, and the `OutfitConfig.cpp` -O0 pin).

## Verification result (A/B, identical harness)

| Test | Gated (RB3_NO_HEAD_SHAPER=1) | Un-gated (default) |
|---|---|---|
| `CharLoad5b` gtest | 4/4 PASS | **4/4 PASS** |
| Headless 5-frame boot | exit 0, 0 MILO_FAIL | exit 0, 0 MILO_FAIL (head milos load clean) |
| `song-end-test.py` (→ gameplay) | **reaches game_screen, song ends, PASS** | **SIGSEGV at nil, never leaves splash** |

- Un-gated 5-frame boot is clean: the SHARED `head_male.milo`/`head_female.milo`
  AND the secondary `char/main/head/{male,female}/head.milo` (SetMeshAnim) all
  load with **0 MILO_FAIL, 0 "String chars > 512", 0 "Can't dynamically load",
  0 "char_test" overlay fail**. The diagnostic's `force_milo_inline`/`char_test`
  worries did NOT fire. Only benign `Skinned mesh needs to be re-exported`
  NOTIFYs are new. So **BandHeadShaper::Init itself runs fine** un-gated.
- The crash is later: pressing `start.btn` on `splash_screen` triggers a screen
  transition whose panel unload tears down a dir graph that now contains a
  head-shaper dir, and `~Object()` derefs null.

## The exact downstream failure (for the fix step)

**Signal:** SIGSEGV (signal 11) at address `(nil)`, frame ~30, during the
splash → next-screen transition. Fault frame = `Hmx::Object::~Object()`
(`src/system/obj/Object.cpp:121`).

**Backtrace (symbolized, -O0 build):**
```
BandUI::Poll                         band3/meta_band/BandUI.cpp:201
UIManager::Poll                      system/ui/UI.cpp:583
BandScreen::Enter                    band3/meta_band/BandScreen.cpp:12
UIScreen::Enter                      system/ui/UIScreen.cpp
UIScreen::UnloadPanels               system/ui/UIScreen.cpp:575
UIPanel::CheckUnload                 system/ui/UIPanel.cpp:59
UIPanel::Unload                      system/ui/UIPanel.cpp
WorldDir::~WorldDir   (recurses)     system/world/Dir.cpp:64,70
PanelDir::~PanelDir                  system/ui/PanelDir.cpp:33
RndDir::~RndDir                      system/rndobj/Dir.h:34
ObjectDir::~ObjectDir                system/obj/Dir.cpp:621   <-- mSubDirs.clear()
  std::vector<ObjDirPtr<ObjectDir>>::clear()
  ObjDirPtr<ObjectDir>::~ObjDirPtr   system/obj/Dir.h:42
  ObjDirPtr<ObjectDir>::operator=    system/obj/Dir.h:143
ObjectDir::DeleteObjects             system/obj/Dir.cpp:599
ParallelGroupSeq::~ParallelGroupSeq  system/synth/Sequence.h:199
GroupSeq::~GroupSeq / Sequence::~Sequence
Hmx::Object::~Object                 system/obj/Object.cpp:121   <-- CRASH @ nil
```

**Root-cause hypothesis (strong):** `FindSubdir(dir, cc)` returns a **borrowed**
subdir pointer — `subdir->mSubDirs[0].Ptr()` — and `BandHeadShaper::Init` stores
it into the `gVisemes[]` `ObjDirPtr` array, then calls `gVisemes[i]->SetName("", 0)`
(BandHeadShaper.cpp:176). That viseme dir is ALSO still owned by its parent's
`mSubDirs` vector. When a screen panel-unload destroys an `ObjectDir`, its
`~ObjectDir` does `mSubDirs.clear()` (Dir.cpp:621), which destroys the same dir
that `gVisemes[]` (or the SetName reparent) also references → double-free /
use-after-free → `~Object()` derefs null. The crash is in the `mSubDirs`
`vector<ObjDirPtr<ObjectDir>>` teardown, exactly where a borrowed-then-reparented
viseme subdir would blow up.

## Where the fix likely lives (NOT in char-Load, NOT in BandHeadShaper::Init's load)

The fix is an **ownership/lifetime** correction on native, candidates:
1. `gVisemes[]` borrowing `mSubDirs[0].Ptr()` from a head dir that gets unloaded
   on a screen transition — make the viseme dirs not double-owned (e.g. keep the
   head dirs alive for the app lifetime and ensure they aren't pulled into a
   panel's unload graph), or hold them via a strong ref that the panel teardown
   respects.
2. The `gVisemes[i]->SetName("", 0)` reparent (BandHeadShaper.cpp:176) detaching
   the subdir from its parent's `mSubDirs` incorrectly on LP64.
3. The `ObjDirPtr<ObjectDir>` LP64 dtor/`operator=` path (Dir.h:42/143) during
   `mSubDirs.clear()` — verify it doesn't double-Release on native.

A native fix MUST be `#ifdef HX_NATIVE`-gated (or match-neutral) and re-tested
with `song-end-test.py` reaching `game_screen` AND a clean splash→menu
transition, not just the 5-frame boot (the 5-frame boot does NOT exercise the
crashing screen-unload path).

## Repro

```bash
# worktree at .claude/worktrees/ungate-5b (branch wt-ungate-5b), built -O0 Debug.
# Apply the env-opt-out gate edit, rebuild rb3-native, then:
BIN=native/build-ungate/rb3-native; DATA=orig-assets/extracted
# UN-GATED crashes:
python3 scripts/native/song-end-test.py --bin "$BIN" --data "$DATA" --verbose --keep-log
# GATED baseline passes:
RB3_NO_HEAD_SHAPER=1 python3 scripts/native/song-end-test.py --bin "$BIN" --data "$DATA" --verbose
```
