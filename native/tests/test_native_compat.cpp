// NativeCompat flag-registry coverage + read-mode wiring guard (W0.6.S2).
//
// The registry is the single source of truth for a native-compat flag's read
// SEMANTICS. The #1 way to silently break a rewire is a presence/truthy read-mode
// mismatch (PLAN.md §Key-facts-4 "semantic-drift landmine"): the generated table
// bakes each flag's FlagRead mode, and NativeCompat::OptOutActive resolves each
// flag BY THAT MODE. These cases pin:
//   (a) the table is non-empty and every flag name is unique (cheap coverage
//       guard — a duplicated row would give two conflicting resolutions), and
//   (b) the five flags rewired in W0.6.S2 carry their EXACT expected read mode,
//       so a regen that flipped one (the drift landmine) is caught here.
//
// These assertions are order-independent: they read the committed table via
// Find()/Table(), never depending on WHEN the read-once singleton was first
// constructed. The exhaustive {unset,"","0","1","x"} input-parity of the rewrite
// is proven separately (a standalone harness, recorded in W0.6/STATUS.md) because
// the read-once env cache resolves at first Get() and cannot be re-varied per
// case within one process.

#include "test_helpers.h"

#include "platform/NativeCompatFlags.h"

#include <cstring>
#include <set>
#include <string>

TEST(NativeCompatRegistry, TableNonEmptyAndNamesUnique) {
    NativeCompatTable table = NativeCompat::Get().Table();
    ASSERT_GT(table.size(), 0u) << "generated registry table is empty";

    std::set<std::string> seen;
    for (std::size_t i = 0; i < table.size(); ++i) {
        const NativeCompatFlag &f = table[i];
        ASSERT_NE(f.name, nullptr);
        ASSERT_TRUE(seen.insert(f.name).second)
            << "duplicate flag row in registry: " << f.name;
        // docAnchor is the row anchor and today equals the name.
        EXPECT_STREQ(f.name, f.docAnchor);
    }
}

// Each of the five W0.6.S2-rewired flags must keep its EXACT read mode. This is
// the semantic-drift guard: presence must stay presence, truthy must stay truthy.
struct RewiredFlagCase {
    const char *name;
    FlagRead    expectedRead;
};

TEST(NativeCompatRegistry, RewiredFlagsHaveExpectedReadMode) {
    const RewiredFlagCase kCases[] = {
        {"RB3_GAMEWARM_OFF", FlagRead::Presence},
        {"RB3_TEX_PREWARM_OFF", FlagRead::Presence},
        {"RB3_HEAP_TRIM_OFF", FlagRead::Truthy},
        {"RB3_NO_SFX", FlagRead::Truthy},
        {"RB3_PREVIEW_PREFETCH_OFF", FlagRead::Truthy},
    };
    for (const RewiredFlagCase &c : kCases) {
        const NativeCompatFlag *f = NativeCompat::Get().Find(c.name);
        ASSERT_NE(f, nullptr) << c.name << " missing from registry";
        EXPECT_EQ(f->read, c.expectedRead)
            << c.name << " read-mode drifted (presence/truthy mismatch)";
        EXPECT_EQ(f->cls, FlagClass::Workaround) << c.name;
    }
}

TEST(NativeCompatRegistry, FindUnknownReturnsNull) {
    EXPECT_EQ(NativeCompat::Get().Find("RB3_DEFINITELY_NOT_A_REGISTERED_FLAG"), nullptr);
    // A null name must not crash the linear scan.
    EXPECT_EQ(NativeCompat::Get().Find(nullptr), nullptr);
}

// Unregistered names fail SAFE to feature-enabled (default-ON) — matching an unset
// opt-out env — rather than crashing or defaulting to disabled.
TEST(NativeCompatRegistry, OptOutActiveUnregisteredFailsSafeEnabled) {
    EXPECT_TRUE(NativeCompat::Get().OptOutActive("RB3_DEFINITELY_NOT_A_REGISTERED_FLAG"));
    EXPECT_TRUE(NativeCompat::Get().OptOutActive(nullptr));
}
