# char-Load 5b — DIAGNOSTIC lane

**Goal of this lane:** build the diagnostic that captures the FIRST failure when
loading the big-endian head.milo on the LE native host, so the fix step knows
exactly where the load breaks.

**Headline result (NEGATIVE / surprising):** the three char-Load functions the
`BandHeadShaper.cpp:137` gate comment names — `CharClip::Load`,
`CharBonesSamples::Load`, `operator>>(BinStream&, CharBones::Bone&)` — are
**ALREADY byte-correct on the LE native host.** When the real BE head milos and
their clips milos are loaded via the exact `DirLoader::LoadObjects` path the gate
blocks, they decode cleanly with **no MILO_FAIL, no version-desync, no
string-len overflow**. This mirrors the 2026-06-08 BandFaceDeform::DeltaArray
finding (the suspected endian bug didn't reproduce — `ReadEndian` already
swaps each `>>` correctly).

The thing that ACTUALLY blocks the boot-time path is unrelated to serialization:
`Character`'s ctor unconditionally constructs a `CharacterTest`, whose ctor
hard-requires a render overlay (`char_test`) that isn't registered in a
menu-only boot → `MILO_FAIL("Could not find overlay \"char_test\"")` BEFORE any
object stream loads. See "Cascade / real blocker" below.

---

## Test artifact

- **File:** `/home/free/code/milohax/rb3/native/tests/test_charload5b.cpp` (uncommitted)
- **Wired into:** `native/CMakeLists.txt` rb3-tests source list (added
  `tests/test_charload5b.cpp` next to test_bandpatchmesh.cpp, ~line 641).
- **Build:** `cmake --build /home/free/code/milohax/rb3/native/build-native --target rb3-tests`  (builds clean)
- **Run:**   `RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted ./native/build-native/rb3-tests --gtest_filter='*CharLoad5b*'`
- **Result: 4/4 PASS, EXIT=0** (runs clean both individually and all-in-one-process):

```
[5b] head_male.milo:          CharClip=0  BandFaceDeform=6
[5b] head_female.milo:        BandFaceDeform=6
[5b] head_male_clips.milo:    CharClip=4
[5b] head_female_clips.milo:  CharClip=4
[  PASSED  ] 4 tests.
```

### What the 4 tests do
Each test runs the EXACT call `BandHeadShaper.cpp:142` makes —
`DirLoader::LoadObjects(FilePath(<path>), 0, 0)` — under `EngineTestFixture`
(RunBoot headless boot, `TheLoadMgr.mPlatform = kPlatformXBox` → resolves
`.milo_xbox`), then walks the loaded dir with `ObjDirItr<T>` to prove the char
objects actually DECODED (not ReadDead-skipped):

| Test | path (genderpath, from `char/char_objects.dta`) | exercises |
|---|---|---|
| `LoadHeadMaleMilo` | `char/main/shared/head_male.milo` | `BandFaceDeform::Load` ×6 (`*.fdm` head shapes) |
| `LoadHeadFemaleMilo` | `char/main/shared/head_female.milo` | `BandFaceDeform::Load` ×6 |
| `LoadHeadMaleClipsMilo` | `char/main/shared/head_male_clips.milo` | **`CharClip::Load` ×4 → `CharBonesSamples::Load` → `>>CharBones::Bone`** |
| `LoadHeadFemaleClipsMilo` | `char/main/shared/head_female_clips.milo` | **`CharClip::Load` ×4 → same chain** |

`CharClip::Load` (CharClip.cpp BEGIN_LOADS, gRev>0xC branch at CharClip.cpp:890)
calls `mFull.Load(bs)`/`mOne.Load(bs)` = `CharBonesSamples::Load`, which calls
`LoadHeader` → `bs >> mBones[i]` = `operator>>(CharBones::Bone)` (CharBones.cpp:1352).
So **CharClip=4 with zero warnings proves all three named functions ran correctly.**

---

## First failure captured (the literal answer to "where does it break")

There are TWO distinct "first failures" depending on factory registration:

### (A) With `Character` registered (the boot-time path BandHeadShaper hits)
```
FAIL-MSG: Could not find overlay "char_test"
```
- **Site:** `RndOverlay::Find` (`src/system/rndobj/Overlay.cpp:59`, `fail=true`).
- **Backtrace (gdb):**
  `RndOverlay::Find("char_test", true)`
  ← `CharacterTest::CharacterTest` (`src/system/char/CharacterTest.cpp:38`, member
     init `mOverlay(RndOverlay::Find("char_test", true))`)
  ← `Character::Character` (`src/system/char/Character.cpp:188`, member init
     `mTest(new CharacterTest(this))` — UNCONDITIONAL)
  ← `Character::NewObject` ← `Hmx::Object::NewObject`
  ← `DirLoader::SetupDir` (`DirLoader.cpp:472`) ← `DirLoader::LoadHeader`
     (`DirLoader.cpp:517`) — i.e. it fires when the loader instantiates the dir's
     ROOT object as `Character`, **before any object STREAM is read.**
- **Not a serialization bug.** It's a missing dev-only render overlay (`char_test`,
  registered from a debug DTA the minimal menu boot doesn't load).

### (B) With `Character` NOT registered (what the test does → clean load)
The dir defaults to `RndDir` (a benign `NOTIFY: ... Character not registered,
defaulting to RndDir`), and the INNER `BandFaceDeform`/`CharClip`/
`CharBonesSamples` leaves all decode with **no failure at all**. This is the
realest available proof that the 5b serialization is byte-correct.

---

## Cascade / real blocker (for the fix step)

The 5b gate comment's premise (CharClip version-desync + CharBones string-len
overflow on LE) **did not reproduce**. The actual obstacle to un-gating
BandHeadShaper on native is the `Character → CharacterTest → char_test overlay`
hard-fail, plus a couple of env gaps the test had to register around:

1. **`char_test` overlay** — `CharacterTest`'s ctor (CharacterTest.cpp:38) does
   `RndOverlay::Find("char_test", true)`. The fix likely needs to either register
   the `char_test` overlay in the native boot, or HX_NATIVE-guard the
   `CharacterTest` construction / overlay lookup. (`Character::Character` always
   `new CharacterTest(this)` — Character.cpp:188.)
2. **`BandFaceDeform` factory** — must be registered (`BandFaceDeform::Init()`) or
   the head milos log `Can't make BandFaceDeform` and the `.fdm` shapes are
   skipped. The full app registers it via the normal BandInit path; a menu-only
   harness must add it.
3. **Paths matter:** `head_male_path` = `char/main/shared/head_male.milo`
   (resolves to `char/main/shared/gen/head_male.milo_xbox`). The `CharClip`/
   `CharBonesSamples` objects are in the SEPARATE `*_clips.milo`
   (`head_male_clips.milo`), NOT `head_male.milo` (which carries only the
   `BandFaceDeform` `.fdm` shapes). The `char/main/head/<g>/head.milo` file is a
   THIRD, secondary load (`SetMeshAnim` head.mesh).
4. **`force_milo_inline.dta`** (`#ifndef _SHIP`) lists `&char/main/*/*male/*/*.milo`
   etc., which `DirLoader`'s ctor (`DirLoader.cpp:241-247`) turns into
   `MILO_FAIL("Can't dynamically load milo files matching ...")`. The
   `char/main/shared/head_male.milo` PRIMARY path does NOT match those patterns
   (segment count differs), so it's fine — but the secondary
   `char/main/head/male/head.milo` DOES match `&char/main/*/*male/*/*.milo` and
   would fail. The fix step should load the SHARED head milos, not the per-gender
   `char/main/head/...` one (or strip the force-inline guard on native).

---

## Tooling note for the fix step: MILO_TRY/MILO_CATCH is BROKEN on 64-bit native

Do not use `MILO_TRY`/`MILO_CATCH` to catch a failure on native: `Debug::Fail`
does `longjmp(TheDebugJump, (int)msg)` (`src/system/os/Debug.cpp:173`), which
**truncates the 64-bit `const char* msg` to a 32-bit `int`**. `MILO_CATCH`
recovers a garbage pointer (verified: deref SIGSEGV at 0x5784c040).

The test captures the first failure cleanly instead via a **Debug modal
callback**: `TheDebug.SetModalCallback(cb)`; the non-try `Debug::Fail` path calls
`mModalCallback(fail, modalMsg, false)` (Debug.cpp:224) with the FULL valid
message. The callback copies it, sets `fail=false` (skip PlatformDebugBreak), and
`longjmp`s to the test's own `jmp_buf` (full pointer never crosses the truncating
longjmp), keeping the gtest binary alive. See `RunCapturingFirstFail()` in the
test. (This longjmp-truncation is itself a latent native bug worth a separate
HX_NATIVE fix — `longjmp` val should not be how the message propagates on LP64.)

---

## Match-neutrality note (for whoever writes the actual fix)

This lane added ONLY a test file + one CMake line. It did NOT touch any
`src/system/char/*` shared source, did NOT remove the BandHeadShaper:137 gate,
and did NOT commit anything. Per the constraint: any real fix in `src/system/char/*`
must be Wii-match-neutral OR `#ifdef HX_NATIVE`-gated. Based on this diagnostic
the fix is likely NOT in the three named char-Load functions at all — it is in
the `Character`/`CharacterTest`/`char_test`-overlay boot dependency and factory/
path wiring (candidates for HX_NATIVE guards), since the serialization already
round-trips correctly.
