// rb3_stub_census.cpp — loud-by-default weak-stub census (W0.2).
//
// Pairs with the per-symbol trampolines in the generated band3_link_stubs.s,
// dta_link_stubs.s, and rndobj_synth_link_stubs.s (loud mode, W0.2.S2/S4).
// Each weak FUNCTION stub is a trampoline that, on its FIRST call, jumps here
// via the extern "C" hook __hmx_stub_first_hit(name); the asm-side latch
// guarantees at-most-once per symbol, so this file just logs + records.
//
// Turns the old silent "none of these stubs is ever reached" belief into an
// enforced, loud-by-default invariant: a swallowed call (à la
// DrawParticlesBillboard / EndGame invisible-failure bugs) prints on frame 1
// and shows up in the startup+atexit census, and — via
// __hmx_stub_census_assert_unreachable_hits — fails the stub-census gate red
// if an `assert-unreachable`-classified stub is hit.
//
// Three independent census tables (name/kind/class for every weak symbol in
// their respective stub set) are generated alongside the three .s files as
// band3_stub_table.inc / dta_stub_table.inc / rndobj_synth_stub_table.inc —
// do not hand-edit any of them. Each carries its own struct/array/constant
// names (HmxStubInfo/kHmxStubTable, HmxDtaStubInfo/kHmxDtaStubTable,
// HmxRndSynthStubInfo/kHmxRndSynthStubTable) specifically so all three can be
// #included in this one TU without an ODR clash. Hit matching is by
// std::strcmp against each table's `name` column (NOT positional index), so
// summing/iterating independent tables here needs no shared ordering — this
// is what lets W0.2.S4 add two more tables with zero changes to the band3
// table or the S1-S3 trampolines/gtest.
//
// All output is loud by default; set RB3_STUB_QUIET to silence the per-hit
// line and the census dumps (the recorded hit list and the gate query are
// unaffected).

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "band3_stub_table.inc"
#include "dta_stub_table.inc"
#include "rndobj_synth_stub_table.inc"

namespace {

std::mutex &census_mutex() {
    static std::mutex m;
    return m;
}

// Names are static string literals from band3_stub_table.inc's sibling
// __hmx_name_<i> .rodata entries, so storing the raw pointer is safe/cheap.
std::vector<const char *> &hit_list() {
    static std::vector<const char *> v;
    return v;
}

bool quiet() { return getenv("RB3_STUB_QUIET") != nullptr; }

void census_atexit() {
    std::lock_guard<std::mutex> lock(census_mutex());
    const std::vector<const char *> &hits = hit_list();
    if (!quiet()) {
        int total = kHmxStubTotal + kHmxDtaStubTotal + kHmxRndSynthStubTotal;
        fprintf(stderr, "[STUB CENSUS] exit: %zu of %d weak stubs hit this run\n",
                hits.size(), total);
        for (const char *n : hits)
            fprintf(stderr, "[STUB CENSUS]   hit: %s\n", n);
    }
}

} // namespace

// Called from the generated __hmx_tramp_<i> trampolines on a symbol's first
// call (asm latch => at most once per symbol). Must stay extern "C" so the
// asm `call __hmx_stub_first_hit@PLT` resolves without name mangling.
extern "C" void __hmx_stub_first_hit(const char *name) {
    if (!quiet())
        fprintf(stderr, "[STUB] first call to %s\n", name);
    std::lock_guard<std::mutex> lock(census_mutex());
    hit_list().push_back(name);
}

// Prints the linked-stub census at boot and arms the atexit hit dump. Safe to
// call more than once (only the first arms atexit / prints the banner).
extern "C" void __hmx_stub_census_startup() {
    static bool armed = false;
    {
        std::lock_guard<std::mutex> lock(census_mutex());
        if (armed)
            return;
        armed = true;
    }
    if (!quiet()) {
        int total = kHmxStubTotal + kHmxDtaStubTotal + kHmxRndSynthStubTotal;
        int func = kHmxStubFunc + kHmxDtaStubFunc + kHmxRndSynthStubFunc;
        int data = kHmxStubData + kHmxDtaStubData + kHmxRndSynthStubData;
        fprintf(stderr, "[STUB CENSUS] linked=%d (func=%d data=%d)\n", total, func, data);
        fprintf(stderr, "[STUB CENSUS]   band3=%d dta=%d rndobj_synth=%d\n",
                kHmxStubTotal, kHmxDtaStubTotal, kHmxRndSynthStubTotal);
    }
    atexit(census_atexit);
}

// Gate query (consumed by the stub-census gtest in W0.2.S3): collects every
// recorded hit whose registry class is `assert-unreachable` (cls == 'A'),
// across all three independent stub-set tables (band3, dta, rndobj_synth).
// Returns the count; `out` receives the offending symbol names.
int __hmx_stub_census_assert_unreachable_hits(std::vector<std::string> &out) {
    std::lock_guard<std::mutex> lock(census_mutex());
    int count = 0;
    for (const char *hit : hit_list()) {
        bool matched = false;
        for (int i = 0; i < kHmxStubTotal && !matched; ++i) {
            if (strcmp(hit, kHmxStubTable[i].name) == 0) {
                matched = true;
                if (kHmxStubTable[i].cls == 'A') {
                    out.push_back(hit);
                    ++count;
                }
            }
        }
        for (int i = 0; i < kHmxDtaStubTotal && !matched; ++i) {
            if (strcmp(hit, kHmxDtaStubTable[i].name) == 0) {
                matched = true;
                if (kHmxDtaStubTable[i].cls == 'A') {
                    out.push_back(hit);
                    ++count;
                }
            }
        }
        for (int i = 0; i < kHmxRndSynthStubTotal && !matched; ++i) {
            if (strcmp(hit, kHmxRndSynthStubTable[i].name) == 0) {
                matched = true;
                if (kHmxRndSynthStubTable[i].cls == 'A') {
                    out.push_back(hit);
                    ++count;
                }
            }
        }
    }
    return count;
}
