// Subsystems-online tests — assert the headless engine actually boots on native
// and the core object/config subsystems are LIVE (not stubbed/null). Mirrors
// dc3-decomp/native/tests/test_subsystems.cpp, scoped to RB3's RunBoot spine.
//
// These guard the whole "refuse-to-init a subsystem" hack class: if someone
// re-defers a load or an LP64 fault regresses SystemInit, a boot test goes red.

#include "test_helpers.h"

#include "obj/Object.h"
#include "obj/Data.h"
#include "utl/Symbol.h"
#include "os/System.h"

extern DataArray *gSystemConfig;

class NativeSubsystems : public EngineTestFixture {};

TEST_F(NativeSubsystems, SystemConfigPopulated) {
    ASSERT_NE(gSystemConfig, nullptr) << "SystemInit must populate gSystemConfig";
    DataArray *objCfg = gSystemConfig->FindArray(Symbol("objects"), false);
    ASSERT_NE(objCfg, nullptr) << "SystemConfig(\"objects\") must resolve after boot";
    EXPECT_GT(objCfg->Size(), 1) << "objects config should have per-class type-defs";
}

TEST_F(NativeSubsystems, ObjectFactoryRegistered) {
    // RegisterCommonFactories registers ObjectDir; the loader constructs by name.
    Hmx::Object *o = Hmx::Object::NewObject(Symbol("ObjectDir"));
    ASSERT_NE(o, nullptr) << "ObjectDir factory must be registered (loader needs it)";
    EXPECT_STREQ(o->ClassName().Str(), "ObjectDir");
    delete o;
}

TEST_F(NativeSubsystems, SymbolInterningStable) {
    Symbol a("convergence_test_token");
    Symbol b("convergence_test_token");
    EXPECT_EQ(a, b) << "equal strings must intern to the same Symbol";
    EXPECT_STREQ(a.Str(), "convergence_test_token");
}
