# char-Load 5b un-gate crash — CONFIRMED root cause + fix plan

**Date:** 2026-06-11 (Fable investigation lane). **Status: root cause CONFIRMED
empirically (ASan + targeted probes), fix validated end-to-end, ready for Opus
to implement.** Supersedes the "viseme double-free" hypothesis in
`ungate-blocked-2026-06-11.md` — that hypothesis is **REFUTED** (see below).

Worktree handoff state: `.claude/worktrees/ungate-5b` (branch `wt-ungate-5b`)
contains ONLY the un-gate edit (`RB3_NO_HEAD_SHAPER` env opt-out in
`src/system/bandobj/BandHeadShaper.cpp`), with a warm `-O0` Debug build at
`native/build-ungate` that **reproduces the SIGSEGV** (verified post-cleanup:
`song-end-test.py` exits 139 un-gated, PASSES with `RB3_NO_HEAD_SHAPER=1`).

## TL;DR

The crash is NOT a char-Load bug, NOT a viseme/gVisemes ownership bug, and NOT
an original-engine bug. It is an **ABA false positive in the pre-existing
HX_NATIVE freed-address guard** (`HxAddrWasFreed`, `src/system/obj/Object.cpp:50-69`)
consumed by `ObjPtr::~ObjPtr` / `ObjPtr::Replace`
(`src/system/obj/ObjPtr_p.h:37-39, 55-59`). The guard's freed-address set is
only ever **un-marked by `CharBonesObject`'s constructor**
(`src/system/char/CharBones.cpp:1338`), so when a freed `CharBonesObject`'s
address is recycled by the allocator into any OTHER live `Hmx::Object` (e.g. an
`AnimTask`), the stale "freed" mark makes `~ObjPtr` **skip
`mPtr->Release(this)` on a LIVE pointee**. That strands a dangling `ObjRef*` in
the live pointee's `mRefs`; when that object later dies,
`Hmx::Object::~Object`'s ref loop (`src/system/obj/Object.cpp:121`,
`(*rit)->Replace(this, nullptr)`) walks corrupt/dangling refs → heap UAF →
memory corruption → the observed downstream `~Object @ nil` SIGSEGV in the
panel-unload chain.

Un-gating the head shaper triggers it because head shaping **mass-produces
freed `CharBonesObject`s**: `head_male.milo`/`head_female.milo`/viseme dirs are
full of `CharClip`→`CharBonesSamples` (a `CharBonesObject`); `SetMeshAnim`
loads-and-`delete`s an entire secondary head dir per gender
(`BandHeadShaper.cpp:74`); `BandHeadShaper::Start/End` `new`/`RELEASE` a
`CharBonesMeshes` per character. Each death marks its address "freed forever"
(until 8192 more CharBones frees wrap the ring). During the splash→next-screen
transition's anim storm (UITrigger→`RndAnimatable::Animate`→`TaskMgr::Start`
creating `AnimTask`s at high rate), recycled char-heap addresses land under
live tasks and the guard false-fires. The gated baseline barely churns
CharBones objects → no aliasing → no crash.

## The confirmed object + the two sites

- **Object:** an `AnimTask` (a `Task`/`Hmx::Object` on `TheTaskMgr`'s
  `TaskTimeline`) — and transitively the timeline's `TaskInfo.unk0`
  `ObjPtr<Task>` entries (owner=`nil`) referencing it. Confirmed by probe lines
  `[HXGUARD-FP ~ObjPtr] mPtr=<addr> owner=(nil) SKIPPED Release on LIVE pointee`
  (4 hits, frame 28-29, the exact splash transition) followed within one frame
  by ASan `heap-use-after-free` in `Hmx::Object::~Object` (Object.cpp:121,
  the `(*rit)->Replace` loop) destroying an `AnimTask` from
  `AnimTask::Poll → delete this` (`TaskTimeline::Poll` →
  `TaskMgr::Poll` → `App::RunOneFrame`). The freed block read was a dead
  task's `mRefs` `vector<ObjRef*>` buffer (alloc stack:
  `Hmx::Object::AddRef ← ObjPtr<Task> copy ← TaskInfo copy ←
  TaskTimeline::AddTask ← TaskMgr::Start ← RndAnimatable::Animate ←
  EventTrigger/UITrigger` — i.e. timeline/task ref bookkeeping).
- **Site 1 (the bad skip / "first free" of the back-reference):**
  `ObjPtr<T>::~ObjPtr`, `src/system/obj/ObjPtr_p.h:37-39` — skips
  `mPtr->Release(this)` because `HxAddrWasFreed(mPtr)` is stale-TRUE for a
  LIVE pointee. The pointee's `mRefs` keeps a pointer to the now-destroyed
  ObjPtr (e.g. a destroyed `TaskInfo` list node).
- **Site 2 (the use / second touch):** `Hmx::Object::~Object`,
  `src/system/obj/Object.cpp:121` — the dying object's `mRefs` walk
  dereferences the dangling/corrupted entries → ASan heap-use-after-free /
  stack-use-after-return at exactly this line; in the non-ASan build the
  corruption cascades into the splash→next panel-unload dtor chain
  (`ObjectDir::DeleteObjects` → `~ParallelGroupSeq` → `~Object @ nil`).

### Why LP64/native-only

The guard itself is `#ifdef HX_NATIVE` (does not exist on Wii). Wii never
skips `Release`, so the back-ref bookkeeping is airtight there. This is a bug
in our native band-aid, not in the original engine. (The guard was originally
added for a REAL dangling `CharDriver::mBones` → freed `CharBonesObject` UAF
in venue teardown; the fix below keeps that protection.)

### Refuted hypotheses (do not re-investigate)

- **gVisemes borrowing / SetName reparent (BandHeadShaper.cpp:150-176):** not
  involved. The probe run shows the corruption begins in task/anim refs, not
  dir ownership; `FindSubdir`'s borrowed `mSubDirs[0]` + `ObjDirPtr gVisemes[]`
  is a *strong* AddRef'd ref (ObjDirPtr has no HxAddrWasFreed guard at all).
- **`ObjDirPtr` LP64 dtor/op= (Dir.h:42/143):** not on the failing path.
- **`AnimTask::mBlendTask` double-delete:** probes (`[AT-DANGLING-DEL]`,
  `[AT-ADOPT-DEAD]`) never fired.
- The backtrace's `ParallelGroupSeq` frame is downstream **collateral** of the
  heap corruption, not the double-freed object.

## Proof method (reproducible)

1. ASan build of `rb3-native` in the worktree (`-fsanitize=address
   -fsanitize-recover=address -fno-omit-frame-pointer`, run with
   `ASAN_OPTIONS=halt_on_error=0:alloc_dealloc_mismatch=0:detect_leaks=0`).
   GPU must avoid the NVIDIA stack under ASan (its `time()` interceptor
   SIGSEGVs in driver init): run with `VK_LOADER_DRIVERS_DISABLE='*'
   __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json
   EGL_PLATFORM=surfaceless LIBGL_ALWAYS_SOFTWARE=1` (Dawn falls back to a
   Null adapter; the crash is GPU-independent). One temp tweak needed:
   `__attribute__((no_sanitize_address))` on `InvokeSysV` in
   `native/src/rb3_replay_api.cpp` (inline asm runs out of registers under
   instrumentation).
2. Un-gated → `heap-use-after-free` at `Object.cpp:121` in the `~Object`
   mRefs loop of a dying `AnimTask` (plus a stack-use-after-return at the same
   line). Gated (`RB3_NO_HEAD_SHAPER=1`) → **zero** UAF, clean exit.
3. A live-object registry probe (insert in `Hmx::Object` ctor / erase in dtor)
   added to the two `ObjPtr_p.h` guards printed `SKIPPED Release on LIVE
   pointee` ×4 immediately before the UAF — the ABA false positive caught in
   the act.
4. **Fix validation:** adding `HxNoteReusedAddr(this)` to `Hmx::Object::Object()`
   → ASan un-gated run: 0 guard-FPs, 0 UAF, exit 0; non-ASan un-gated
   `song-end-test.py`: full PASS (boot → splash → menus → song select →
   gameplay → `{game is_game_over}==1`); gated run also PASS.

## THE FIX (exact)

**File:** `src/system/obj/Object.cpp:97`

```cpp
Hmx::Object::Object() : mTypeDef(0), mName(gNullStr), mDir(0) {
#ifdef HX_NATIVE
    // A new live object constructed at this address proves any stale entry in
    // the HxAddrWasFreed set is an ABA alias (the freed CharBonesObject's
    // memory was recycled). Clear it so the ObjPtr_p.h guards cannot skip
    // Release on a LIVE pointee (which strands a dangling ObjRef in our mRefs
    // and corrupts the ~Object ref walk). Root cause of the 5b un-gate crash;
    // see docs/native/char-load-5b/viseme-uaf-plan.md.
    HxNoteReusedAddr((const void *)this);
#endif
}
```

`HxNoteReusedAddr` already exists (`Object.cpp:67`, inside the same-file
`#ifdef HX_NATIVE` block above it — no new declaration needed; it's defined
before first use). This exactly cancels the stale mark for the proven crash
class (offset-0 `Hmx::Object` bases: `Task`, `AnimTask`, and the vast majority
of engine objects), while preserving the guard's true-positive protection for
the original venue-teardown dangling-`mBones` case.

**Optional hardening (recommended, but not required to un-block 5b):** for
pointees whose `Hmx::Object` subobject is NOT at offset 0 (some
multiply-inherited Rnd types), the ctor's `this` differs from the recycled
block base, so a stale mark could in principle survive. Closing that hole
needs a range-erase at allocation: in `_MemAlloc`
(`src/system/utl/MemMgr.cpp:1030`), under `#ifdef HX_NATIVE`, erase all
freed-set entries in `[p, p+size)` (expose a
`void HxNoteFreedRangeReused(const void *p, size_t n)` from Object.cpp doing
`set.lower_bound(p)`…erase while `< p+n`; the set is ≤ 8192 entries, the wrap
logic in `HxNoteFreedAddr` already tolerates values erased early). Ship the
one-liner first; add this only if a residual `[HXGUARD-FP]`-class crash is
ever seen again.

**Do NOT instead:** (a) delete the guards (re-opens the original
venue-teardown UAF the guard was added for), (b) null-guard the `~Object` loop
(masks corruption after the fact), or (c) touch `BandHeadShaper`
ownership (it is correct).

### HX_NATIVE gating / Wii neutrality

The entire fix sits inside `#ifdef HX_NATIVE` in `Hmx::Object::Object()`
(plus optionally `_MemAlloc`). Wii/MWCC codegen is untouched — the ctor body
compiles identically when `HX_NATIVE` is undefined. The two guard sites in
`ObjPtr_p.h` are not modified at all.

## Verification Opus must run (in the worktree)

```bash
cd /home/free/code/milohax/rb3/.claude/worktrees/ungate-5b
cmake --build native/build-ungate --target rb3-native -j16

# 1) The previously-crashing repro must now PASS end-to-end:
python3 scripts/native/song-end-test.py --bin native/build-ungate/rb3-native \
    --data orig-assets/extracted --verbose
#    PASS criteria: reaches game_screen AND "song ended -> game-over reached".

# 2) Gated A/B must still pass (opt-out path unchanged):
RB3_NO_HEAD_SHAPER=1 python3 scripts/native/song-end-test.py \
    --bin native/build-ungate/rb3-native --data orig-assets/extracted

# 3) CharLoad5b gtest still 4/4 (byte-correctness untouched):
cmake --build native/build-ungate --target rb3-tests -j16 && \
    native/build-ungate/rb3-tests --gtest_filter='CharLoad5b*'

# 4) Wii match neutrality: tools/ninja-locked (from repo root of the worktree)
#    — BandHeadShaper.o / Object.o match% unchanged (all edits HX_NATIVE-gated).
```

Then land: the un-gate edit (BandHeadShaper.cpp, already in the worktree) +
the Object.cpp ctor one-liner, one commit.

## Pre-existing latent issues observed (NOT part of this fix — file separately)

- `CharCollide::Deform` (`src/system/char/CharCollide.cpp:196-206`):
  stack-use-after-scope — `upX`/`upY` are scoped inside an if/else but used
  via `upPtr` after the scope ends. Fires in gated AND un-gated runs; benign
  at -O0, real UB. Trivial fix: hoist both `Vector3`s above the `if`.
- `BandRetargetVignette::EnterDir` (`BandRetargetVignette.cpp:56`):
  global-buffer-overflow READ (8 bytes), fires in gated baseline too.
- Exit-order noise: at `exit()`, the static `HxFreedAddrs` set is destroyed
  before some global `ObjPtr` dtors consult it (harmless, process-exit only).
