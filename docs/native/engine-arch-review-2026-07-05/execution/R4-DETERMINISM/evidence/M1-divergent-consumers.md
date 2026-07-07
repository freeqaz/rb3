# R4 M1 — ranked divergent gRand consumers (attribution)

**Run:** `scripts/native/loaddet_gate.py --attrib --arm off --n 4 --k 300 --boot-window 120 --jitter 200`
**Binary:** `native/build-agent-R4/rb3-native` (build 2026-07-07)
**Method:** `RB3_LOADDET_ATTRIB=1` tags every gRand draw with `__builtin_return_address(0)`
in the four `Rand.cpp` free-function wrappers (verified-complete tap — `grep 'gRand\.'`
shows no member-call bypasses outside `Rand.cpp`; `Gaussian` is Wind-local only). Per-frame
flush → module offset (PIE-invariant) → offline `addr2line`. OFF arm (seam off) under
worker-latency jitter reproduces the divergence (fail-red control). N=4 boots.

Raw: `attrib-off-gate.json`, `attrib-off-boot.json`, `attrib-off.json` (arm summary),
`attrib-off-run.log`.

## Key correction: rank by CONSUMER FUNCTION, not by PC line

The raw table shows 33 "divergent sites" in the gate window — but 26 of them are distinct
source lines *inside one function* (`RndParticleSys::InitParticle`), whose per-boot draw
vectors are **identical** (they scale together with the particle spawn count). Grouped by the
owning function (the real isolation unit — one stream tag per consumer), the divergent set is
small and named. This is plan §6 risk #4 (dladdr/addr2line line-granularity) resolved by
function grouping.

## Gate window (anchor→anchor+300) — PRIMARY, N=4

OFF-arm postAnchorDelta = **[33847, 34010, 32482, 37747]**, spread **5265** (fail-red repro).

| Consumer function | draw sites | per-boot total | spread | verdict |
|---|---|---|---|---|
| `RndParticleSys::InitParticle` (Part.cpp) | 26 | [32980,33111,31552,37353] | **5801** | DIVERGENT — dominant |
| `CamShot::Shake` (CameraShot.cpp) | 5 | [559,585,622,597] | 63 | DIVERGENT |
| `RndParticleSys::CreateParticles` (Part.cpp) | 1 | [287,294,293,303] | 16 | DIVERGENT |
| `CharEyes::NextLook` (CharEyes.cpp) | 1 | [19,19,17,23] | 6 | DIVERGENT |
| `CharInterest::ComputeScore` | 1 | [4,4,4,4] | 0 | fixed (not a leak) |

→ **4 divergent gate-window consumers.** `InitParticle`'s spread (5801) alone accounts for the
entire postAnchorDelta spread — particle spawn-count variation is the root leak; `CreateParticles`
is the count decider that feeds it (same `RndParticleSys` subsystem, one stream tag `part`).

## Boot window (frames 0–120) — informative, NON-gating

| Consumer function | per-boot total | spread | verdict |
|---|---|---|---|
| `RndParticleSys::InitParticle` | [13845,13793,14022,13871] | 229 | DIVERGENT |
| `RndParticleSys::CreateParticles` | [633,651,647,647] | 18 | DIVERGENT |
| `CamShot::Shake` | [212,227,227,211] | 16 | DIVERGENT |
| `RandomGroupSeq::PickNextIndex` | [10,4,4,4] | 6 | DIVERGENT (menu-only) |
| `CharEyes::NextLook` | [19,21,21,21] | 2 | DIVERGENT |
| CharClipSet::SyncObjects / RandomVal / ComputeScore / DataRandomElem / CheckBursts / FileMergerOrganizer::StartLoad / EventTrigger::Trigger | — | 0 | fixed |

## M1 verdict: **GO**

Divergent variable-count consumers form a small named set (4 in the gate window, +
`RandomGroupSeq::PickNextIndex` in the boot window). ≤ 8 sites → proceed to M2 with exactly:

- `part` stream → `RndParticleSys::InitParticle` + `RndParticleSys::CreateParticles`
- `camshot` stream → `CamShot::Shake`
- `chareyes` stream → `CharEyes::NextLook`
- `randgroupseq` stream → `RandomGroupSeq::PickNextIndex` (boot-window; harmless, closes menu axis)

`Wind::` formally exonerated (Part §2.2, re-confirmed: absent from the table). `mAnims` walk
already sorted (landed). Isolating these off gRand makes the global per-frame gRand count a sum
of fixed-count consumers → postAnchorDelta boot-invariant by construction (PRIMARY collapses).
