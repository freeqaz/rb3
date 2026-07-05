// Stub-census gate (W0.2.S3) — the CI half of the loud-by-default weak-stub
// invariant introduced in W0.2.
//
// Boots the headless engine (EngineTestFixture), then asserts the process-wide
// weak-stub census recorded NO hit on a symbol classified `assert-unreachable`
// (cls == 'A'). This is exactly the gate that would have gone red on frame 1
// for the DrawParticlesBillboard (particles never rendered) and EndGame (song
// never ended) invisible-failure bugs: a swallowed call to a stub the port
// believes is unreachable now fails the build instead of silently returning 0.
//
// A companion boot-smoke script (scripts/native/stub_census_smoke.py) does the
// same check against the standalone rb3-native binary and adds a registry↔.s
// drift gate; this gtest is the in-process CI surface.
//
// Note: the engine is a process-global singleton and the suite is serialized
// (RESOURCE_LOCK rb3_engine_singleton), so the census hit set observed here is
// process-wide — a *stronger* condition than a single clean boot, which is fine
// for the "no assert-unreachable stub is ever hit" assertion.

#include "test_helpers.h"

#include <string>
#include <vector>

// Census table (name/kind/class for every weak symbol), generated from
// native/src/band3_stub_registry.tsv alongside band3_link_stubs.s. Pulled in
// here with internal linkage — this TU gets its own copy of kHmxStubTable and
// the count constants, independent of the one in rb3_stub_census.cpp.
#include "band3_stub_table.inc"

// Census query implemented in rb3_stub_census.cpp (ordinary C++ linkage — it
// is declared outside that file's extern "C" block). Collects every recorded
// hit whose registry class is `assert-unreachable`.
int __hmx_stub_census_assert_unreachable_hits(std::vector<std::string> &out);

class StubCensus : public EngineTestFixture {};

// The gate: after boot, no `assert-unreachable`-classified weak stub may have
// been hit. If one was, the port silently swallowed a call it asserted could
// never happen — surface it loudly and fail red with the offending names.
TEST_F(StubCensus, NoAssertUnreachableStubHitDuringBoot) {
    std::vector<std::string> hits;
    int n = __hmx_stub_census_assert_unreachable_hits(hits);
    if (n != 0) {
        std::string joined;
        for (const std::string &s : hits) {
            joined += "\n    ";
            joined += s;
        }
        ADD_FAILURE() << n << " weak stub(s) classified `assert-unreachable` were "
                         "hit during boot — a swallowed call the port believes is "
                         "unreachable. Either the call is genuinely reached and the "
                         "registry row should be promoted to `ok-noop` (with a dated "
                         "justification in native/src/band3_stub_registry.tsv, then "
                         "regenerate), or this is a real invisible-failure bug to fix:"
                      << joined;
    }
    EXPECT_EQ(n, 0);
    EXPECT_TRUE(hits.empty());
}

// Registry completeness / table sanity: the generated census table must cover
// every weak symbol (func + data), the counts must be self-consistent, and no
// row may carry an out-of-range kind/class code (which would mean the generator
// emitted an unclassified row). Guards against the .inc drifting out of the
// registry's invariants.
TEST_F(StubCensus, CensusTableComplete) {
    EXPECT_EQ(kHmxStubTotal, kHmxStubFunc + kHmxStubData)
        << "linked total must equal func + data";
    EXPECT_EQ(kHmxStubTotal, 582) << "band3 weak-stub count (see W0.2 MEASURED baseline)";
    EXPECT_EQ(kHmxStubFunc, 521);
    EXPECT_EQ(kHmxStubData, 61);

    int func = 0, data = 0;
    for (int i = 0; i < kHmxStubTotal; ++i) {
        const HmxStubInfo &row = kHmxStubTable[i];
        ASSERT_NE(row.name, nullptr) << "row " << i << " has a null name";
        ASSERT_NE(row.name[0], '\0') << "row " << i << " has an empty name";
        // kind: 'F' func / 'D' data.
        EXPECT_TRUE(row.kind == 'F' || row.kind == 'D')
            << "row " << i << " (" << row.name << ") has invalid kind '" << row.kind << "'";
        // class: 'A' assert-unreachable / 'N' ok-noop / 'D' data-blob — never unclassified.
        EXPECT_TRUE(row.cls == 'A' || row.cls == 'N' || row.cls == 'D')
            << "row " << i << " (" << row.name << ") is unclassified: cls '" << row.cls << "'";
        // Cross-invariant: data rows are data-blob; func rows are A or N.
        if (row.kind == 'D') {
            EXPECT_EQ(row.cls, 'D') << "data row " << row.name << " must be class data-blob";
            ++data;
        } else {
            EXPECT_TRUE(row.cls == 'A' || row.cls == 'N')
                << "func row " << row.name << " must be assert-unreachable or ok-noop";
            ++func;
        }
    }
    EXPECT_EQ(func, kHmxStubFunc) << "counted func rows must match kHmxStubFunc";
    EXPECT_EQ(data, kHmxStubData) << "counted data rows must match kHmxStubData";
}
