// rb3_stub_census.cpp — loud-by-default weak-stub census (W0.2).
//
// Pairs with the per-symbol trampolines in the generated band3_link_stubs.s
// (loud mode). Each weak FUNCTION stub is a trampoline that, on its FIRST call,
// jumps here via the extern "C" hook __hmx_stub_first_hit(name); the asm-side
// latch guarantees at-most-once per symbol, so this file just logs + records.
//
// Turns the old silent "none of these stubs is ever reached" belief into an
// enforced, loud-by-default invariant: a swallowed call (à la
// DrawParticlesBillboard / EndGame invisible-failure bugs) prints on frame 1
// and shows up in the startup+atexit census, and — via
// __hmx_stub_census_assert_unreachable_hits — fails the stub-census gate red
// if an `assert-unreachable`-classified stub is hit.
//
// The census table (name/kind/class for every weak symbol) is generated
// alongside the .s as band3_stub_table.inc — do not hand-edit either.
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
        fprintf(stderr, "[STUB CENSUS] exit: %zu of %d weak stubs hit this run\n",
                hits.size(), kHmxStubTotal);
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
    if (!quiet())
        fprintf(stderr, "[STUB CENSUS] linked=%d (func=%d data=%d)\n",
                kHmxStubTotal, kHmxStubFunc, kHmxStubData);
    atexit(census_atexit);
}

// Gate query (consumed by the stub-census gtest in W0.2.S3): collects every
// recorded hit whose registry class is `assert-unreachable` (cls == 'A').
// Returns the count; `out` receives the offending symbol names.
int __hmx_stub_census_assert_unreachable_hits(std::vector<std::string> &out) {
    std::lock_guard<std::mutex> lock(census_mutex());
    int count = 0;
    for (const char *hit : hit_list()) {
        for (int i = 0; i < kHmxStubTotal; ++i) {
            if (strcmp(hit, kHmxStubTable[i].name) == 0) {
                if (kHmxStubTable[i].cls == 'A') {
                    out.push_back(hit);
                    ++count;
                }
                break;
            }
        }
    }
    return count;
}
