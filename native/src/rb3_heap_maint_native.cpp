// rb3_heap_maint_native.cpp — periodic glibc heap trim for the native/web port.
//
// Frame-degradation fix (2026-06-21). In-song RSS climbed monotonically (~115
// KB/s, collapsing the web build to a few fps after ~60 s). Attribution (smaps
// per-mapping RSS diff over 100 s of gameplay):
//
//     +8.94 MB  [heap]   262.5 -> 271.4 MB   <- the leak
//     +4.14 MB  [anon]    26.3 ->  30.4 MB
//
// The growth is in the glibc main arena ([heap], brk-served), NOT anonymous mmap
// (an mmap interposer showed mmapLive FLAT at 1.38 MB the whole run) and NOT net
// live malloc bytes (a per-call-site malloc interposer showed tracked live bytes
// PLATEAU at +2.75 MB while [heap] RSS grew +8.94 MB). That gap is classic glibc
// arena FRAGMENTATION / retention: the heavy per-frame churn of small short-lived
// allocations (SFX SfxInst/SampleInst objects + their decode buffers, DataNode /
// message / vector temporaries, etc.) bumps the arena's brk high-water mark, and
// glibc does not return those pages to the OS on free — so RSS ratchets up even
// though the live set is bounded.
//
// glibc only auto-trims the very top of the arena and only past M_TRIM_THRESHOLD;
// a fragmented top block keeps the pages resident indefinitely. Calling
// malloc_trim(0) explicitly walks the arena and madvise(MADV_DONTNEED)s the free
// pages back to the OS, capping the ratchet. We do it on a cadence (every
// RB3_HEAP_TRIM_FRAMES frames, default 240 ~= a few seconds) so the per-call cost
// (a heap walk) is amortised and never lands in a latency-critical frame burst.
//
// This is the second half of the fix; the first (rb3_xma_sidecar.h decode cache)
// removes the single largest churning allocation (the per-trigger PCM decode).
// Together they bound in-song RSS. Native + web only (this TU is native-side);
// the shared Wii decomp is untouched. Opt-out: RB3_HEAP_TRIM_OFF=1.
#ifdef HX_NATIVE

#include <cstdlib>

#if defined(__GLIBC__) || (defined(__linux__) && !defined(__EMSCRIPTEN__))
#include <malloc.h> // malloc_trim (glibc); harmless no-op elsewhere via the guard
#define RB3_HAVE_MALLOC_TRIM 1
#endif

namespace {
int HeapTrimPeriodFrames() {
    static int sPeriod = -1;
    if (sPeriod < 0) {
        sPeriod = 240; // default cadence
        if (const char *off = getenv("RB3_HEAP_TRIM_OFF"))
            if (off[0] && off[0] != '0')
                sPeriod = 0; // disabled
        if (sPeriod != 0)
            if (const char *p = getenv("RB3_HEAP_TRIM_FRAMES")) {
                int v = atoi(p);
                if (v > 0)
                    sPeriod = v;
            }
    }
    return sPeriod;
}
} // namespace

// Called once per native frame from App::Run()'s HX_NATIVE loop. Trims the glibc
// heap on a cadence to return fragmentation-retained free pages to the OS.
void RB3NativeHeapMaintenance(int frame) {
    const int period = HeapTrimPeriodFrames();
    if (period <= 0)
        return;
    if ((frame % period) != 0)
        return;
#ifdef RB3_HAVE_MALLOC_TRIM
    // pad=0: release as much of the arena top + any other trimmable free space as
    // possible. Cheap relative to a frame (a single arena walk) at this cadence.
    malloc_trim(0);
#endif
}

#endif // HX_NATIVE
