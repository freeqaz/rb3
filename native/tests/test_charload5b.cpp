// char-Load "5b" diagnostic — capture the FIRST failure when loading the
// big-endian head.milo (char/main/head/<gender>/gen/head.milo_xbox) on the
// little-endian native host.
//
// BACKGROUND. BandHeadShaper::Init (src/system/bandobj/BandHeadShaper.cpp:137)
// hard-gates head-milo loading on HX_NATIVE (`_tmp0 = false;`) because three
// char-serialization Load functions are believed not byte-correct when reading
// the BE head.milo on LE:
//   - CharClip::Load            (version-desync;            src/system/char/CharClip.cpp BEGIN_LOADS(CharClip))
//   - CharBonesSamples::Load    (string-len overflow;       src/system/char/CharBonesSamples.cpp:457/459)
//   - operator>>(CharBones::Bone) (string-len overflow;     src/system/char/CharBones.cpp:1353)
//
// This test does NOT remove that gate. It reproduces the load via the EXACT
// call BandHeadShaper.cpp:142 makes — DirLoader::LoadObjects(FilePath("char/
// main/head/male/gen/head.milo"), 0, 0) — and captures the first MILO_FAIL /
// MILO_ASSERT message (+ its source line) so the fix step knows where the load
// breaks.
//
// FAILURE CAPTURE. We do NOT use MILO_TRY/MILO_CATCH: on a 64-bit host that path
// is broken — Debug::Fail does `longjmp(TheDebugJump, (int)msg)` (Debug.cpp:173),
// truncating the 64-bit char* to a 32-bit int, so MILO_CATCH recovers a garbage
// pointer (verified: deref SIGSEGVs at 0x5784c040). Instead we install a
// Debug modal callback (SetModalCallback) — the non-try Debug::Fail path calls
// `mModalCallback(fail, modalMsg, false)` (Debug.cpp:224) with the FULL valid
// message. The callback copies it into a global and longjmps back here (our own
// jmp_buf, full 64-bit pointer never crosses the truncating longjmp), keeping
// the gtest binary alive so we report the exact first-failure text.
//
// The container itself (zlib-blocked Milo) is decompressed by ChunkStream; the
// inner per-object streams flow BE through the three Load functions above. We
// register a broad obj/rndobj/char factory set first so the loader reaches the
// real char objects (Character -> CharClip -> CharBonesSamples) rather than
// stopping early on an "unknown class" default-to-ObjectDir.

#include "test_helpers.h"

#include <csetjmp>
#include <cstring>

#include "obj/Object.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "utl/FilePath.h"
#include "utl/Symbol.h"
#include "os/Debug.h"
#include "os/System.h"

// --- obj / rndobj leaf + container factories the head.milo tree references ---
#include "rndobj/Dir.h"
#include "rndobj/Mesh.h"
#include "rndobj/MeshAnim.h"
#include "rndobj/MeshDeform.h"
#include "rndobj/Trans.h"
#include "rndobj/Tex.h"
#include "rndobj/TexBlender.h"
#include "rndobj/TexBlendController.h"
#include "rndobj/Mat.h"
#include "rndobj/Group.h"
#include "rndobj/MatAnim.h"
#include "rndobj/TransAnim.h"
#include "rndobj/PropAnim.h"
#include "rndobj/EventTrigger.h"

// --- char factories (the 5b subjects + their containers) ---
#include "char/Character.h"
#include "char/CharClip.h"
#include "char/CharClipSet.h"
#include "char/CharCollide.h"
#include "char/CharLipSync.h"
#include "char/CharInterest.h"
#include "char/CharFaceServo.h"
#include "char/CharWeightSetter.h"
#include "char/CharHair.h"
#include "char/CharServoBone.h"
#include "bandobj/BandFaceDeform.h"

// ---------------------------------------------------------------------------
// First-failure capture via a Debug modal callback (see header comment for why
// MILO_TRY can't be used on 64-bit). The callback runs INSIDE Debug::Fail with
// the full valid message; it stashes it and longjmps back to the test.
// ---------------------------------------------------------------------------
static jmp_buf s_failJmp;
static bool s_failJmpArmed = false;
static char s_failMsg[4096];
static bool s_failCaptured = false;

static void CaptureFailModal(bool &fail, char *msg, bool /*b*/) {
    // Record the first failure message (full, valid pointer).
    if (!s_failCaptured) {
        s_failCaptured = true;
        std::strncpy(s_failMsg, msg ? msg : "(null)", sizeof(s_failMsg) - 1);
        s_failMsg[sizeof(s_failMsg) - 1] = 0;
    }
    fail = false; // don't PlatformDebugBreak()
    if (s_failJmpArmed) {
        s_failJmpArmed = false;
        std::longjmp(s_failJmp, 1); // unwind back to the test, keep the binary alive
    }
}

// Run `fn` with the modal callback installed; returns the first failure message,
// or nullptr if it completed without a MILO_FAIL/MILO_ASSERT.
template <typename Fn>
static const char *RunCapturingFirstFail(Fn fn) {
    s_failCaptured = false;
    s_failMsg[0] = 0;
    ModalCallbackFunc *prev = TheDebug.SetModalCallback(CaptureFailModal);
    const char *result = nullptr;
    s_failJmpArmed = true;
    if (setjmp(s_failJmp) == 0) {
        fn();
    } else {
        result = s_failMsg; // longjmp'd out of a failure
    }
    s_failJmpArmed = false;
    TheDebug.SetModalCallback(prev);
    return result;
}

// Register everything the head.milo object graph instantiates by class name.
// Idempotent (RegisterFactory just overwrites the map slot), so it is safe to
// call after the fixture's RegisterCommonFactories.
static void RegisterCharLoadFactories() {
    // rndobj leaves/containers seen in the head.milo type table
    // (CharCollide, Mesh, MeshDeform, TexBlendController, TexBlender, Trans, ...)
    RndDir::Init();
    RndMesh::Init();
    RndMeshAnim::Init();
    RndMeshDeform::Init();
    RndTransformable::Init();
    RndTex::Init();
    RndTexBlender::Init();
    RndTexBlendController::Init();
    RndMat::Init();
    RndGroup::Init();
    RndMatAnim::Init();
    RndTransAnim::Init();
    RndPropAnim::Init();
    EventTrigger::Init();

    // char objects. NOTE: Character is deliberately NOT registered here — its
    // ctor unconditionally `new CharacterTest(this)`, whose ctor hard-requires
    // RndOverlay::Find("char_test", true) (a dev-only overlay the minimal test
    // fixture doesn't register) and MILO_FAILs before any object stream loads.
    // Leaving Character unregistered lets the dir default to ObjectDir; the
    // INNER CharClip / CharBonesSamples leaves still decode (which is the 5b
    // serialization path we want to exercise).
    //   Character::Init();   // <- pulls in CharacterTest -> char_test overlay
    CharClipSet::Init(); // dir root of the *_clips.milo
    CharClip::Init();
    CharCollide::Init();
    CharLipSync::Init();
    CharInterest::Init();
    CharFaceServo::Init();
    CharWeightSetter::Init();
    CharHair::Init();
    CharServoBone::Init();

    // bandobj face-deform object embedded in the head milos (the "*.fdm"
    // BandFaceDeform leaves GetNum() counts).
    BandFaceDeform::Init();
}

class CharLoad5b : public EngineTestFixture {
protected:
    static void SetUpTestSuite() {
        EngineTestFixture::SetUpTestSuite();
        RegisterCharLoadFactories();
    }
};

// Reproduce the BandHeadShaper.cpp:142 PRIMARY load and capture the first
// failure. genderpath comes from SystemConfig("objects","BandCharDesc")
// .FindData("head_male_path") == "char/main/shared/head_male.milo" (per
// orig-assets char/char_objects.dta). DirLoader resolves it to
// char/main/shared/gen/head_male.milo_xbox (kPlatformXBox from the fixture).
// THIS is the milo the 5b gate blocks — it carries the CharClip /
// CharBonesSamples / CharBones::Bone streams. (The char/main/head/<g>/head.milo
// file is the SECONDARY head.mesh load done later by SetMeshAnim.)
TEST_F(CharLoad5b, LoadHeadMaleMilo) {
    const char *kPath = "char/main/shared/head_male.milo";

    ObjectDir *dir = nullptr;
    const char *failMsg = RunCapturingFirstFail([&] {
        dir = DirLoader::LoadObjects(FilePath(kPath), 0, 0);
    });

    if (failMsg) {
        // The fix step reads this: exact MILO_FAIL/MILO_ASSERT text + the
        // src line embedded in it (assert msgs are "<file>:<line>: ...").
        ADD_FAILURE() << "head.milo LOAD FAILED (first failure):\n  " << failMsg;
        return;
    }

    ASSERT_NE(dir, nullptr) << "DirLoader::LoadObjects returned null with no MILO_FAIL";

    // Prove the char-serialization path actually ran: the shared head milo
    // carries CharClip leaves (whose Load drives CharBonesSamples::Load +
    // operator>>(CharBones::Bone) — the named 5b functions). If they decoded as
    // real CharClip objects (not ReadDead-skipped), the load is byte-correct.
    int nClips = 0, nFaceDeform = 0;
    for (ObjDirItr<CharClip> it(dir, true); it; ++it) nClips++;
    for (ObjDirItr<BandFaceDeform> it(dir, true); it; ++it) nFaceDeform++;
    fprintf(stderr, "[5b] head_male.milo: CharClip=%d BandFaceDeform=%d\n",
            nClips, nFaceDeform);
    EXPECT_GT(nClips + nFaceDeform, 0)
        << "head milo loaded but no CharClip/BandFaceDeform decoded (objects skipped?)";
    RecordProperty("CharClips", nClips);
    RecordProperty("BandFaceDeforms", nFaceDeform);

    delete dir;
}

// Same for the female head (different vert counts / object set) so the fix step
// has a second data point if the failures diverge by gender.
TEST_F(CharLoad5b, LoadHeadFemaleMilo) {
    const char *kPath = "char/main/shared/head_female.milo";

    ObjectDir *dir = nullptr;
    const char *failMsg = RunCapturingFirstFail([&] {
        dir = DirLoader::LoadObjects(FilePath(kPath), 0, 0);
    });

    if (failMsg) {
        ADD_FAILURE() << "head.milo (female) LOAD FAILED (first failure):\n  " << failMsg;
        return;
    }

    ASSERT_NE(dir, nullptr) << "DirLoader::LoadObjects returned null with no MILO_FAIL";
    int nFaceDeform = 0;
    for (ObjDirItr<BandFaceDeform> it(dir, true); it; ++it) nFaceDeform++;
    fprintf(stderr, "[5b] head_female.milo: BandFaceDeform=%d\n", nFaceDeform);
    EXPECT_GT(nFaceDeform, 0) << "head_female.milo loaded but no BandFaceDeform decoded";
    delete dir;
}

// The CharClip / CharBonesSamples / operator>>(CharBones::Bone) streams — the
// THREE functions the 5b gate names — live in the SEPARATE *_clips.milo, not the
// head_*.milo above (which carries only BandFaceDeform *.fdm shapes). This test
// drives them directly: head_<gender>_clips.milo holds CharClipSet + many
// CharClip leaves; each CharClip::Load reads mFull/mOne CharBonesSamples (which
// read CharBones::Bone name+weight). If these decode as real CharClip objects
// without a MILO_FAIL, the 5b serialization path is byte-correct on LE native.
TEST_F(CharLoad5b, LoadHeadMaleClipsMilo) {
    const char *kPath = "char/main/shared/head_male_clips.milo";

    ObjectDir *dir = nullptr;
    const char *failMsg = RunCapturingFirstFail([&] {
        dir = DirLoader::LoadObjects(FilePath(kPath), 0, 0);
    });

    if (failMsg) {
        ADD_FAILURE() << "head_male_clips.milo LOAD FAILED (first failure):\n  " << failMsg;
        return;
    }

    ASSERT_NE(dir, nullptr) << "DirLoader::LoadObjects returned null with no MILO_FAIL";
    int nClips = 0;
    for (ObjDirItr<CharClip> it(dir, true); it; ++it) nClips++;
    fprintf(stderr, "[5b] head_male_clips.milo: CharClip=%d\n", nClips);
    EXPECT_GT(nClips, 0)
        << "clips milo loaded but no CharClip decoded — Load may have ReadDead-skipped";
    RecordProperty("CharClips", nClips);
    delete dir;
}

TEST_F(CharLoad5b, LoadHeadFemaleClipsMilo) {
    const char *kPath = "char/main/shared/head_female_clips.milo";

    ObjectDir *dir = nullptr;
    const char *failMsg = RunCapturingFirstFail([&] {
        dir = DirLoader::LoadObjects(FilePath(kPath), 0, 0);
    });

    if (failMsg) {
        ADD_FAILURE() << "head_female_clips.milo LOAD FAILED (first failure):\n  " << failMsg;
        return;
    }

    ASSERT_NE(dir, nullptr) << "DirLoader::LoadObjects returned null with no MILO_FAIL";
    int nClips = 0;
    for (ObjDirItr<CharClip> it(dir, true); it; ++it) nClips++;
    fprintf(stderr, "[5b] head_female_clips.milo: CharClip=%d\n", nClips);
    EXPECT_GT(nClips, 0)
        << "clips milo loaded but no CharClip decoded — Load may have ReadDead-skipped";
    delete dir;
}
