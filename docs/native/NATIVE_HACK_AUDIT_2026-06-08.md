# RB3 Native Port — Hack Audit & Convergence Plan (2026-06-08)

> **Update (2026-06-08, same day):** a validation sweep re-checked every "blocked"
> item and **corrected two conclusions in this doc** (theme A and theme B below).
> See [`BLOCKER_VALIDATION_2026-06-08.md`](BLOCKER_VALIDATION_2026-06-08.md) for the
> verified verdicts and what landed.

A systematic audit of the native/web port for **"hacks": places where, on
`HX_NATIVE`, we disable / skip / stub something the original game does** — the
`gDeforms=0` class of "refuse to run faithful code over a (often stale) fear,"
as opposed to legitimate platform glue that gets *replaced* by host APIs.

Produced by a multi-agent audit (6-modality discovery → DC3 cross-reference →
scope classification → adversarial vet → test design). **79 raw findings → 73
unique → classified:**

| Scope | Count | Meaning |
|---|---|---|
| **IN_SCOPE_FIXABLE** | 10 | Faithful behavior disabled; can be brought online (some already done) |
| **IN_SCOPE_BLOCKED** | 7–8 | Faithful behavior disabled; needs a real new native impl first |
| **OUT_OF_SCOPE_GLUE** | 9 | Correct platform stubs (net/OS/render/movie) — replaced by host APIs |
| **BENIGN** | 47 | Env opt-outs that already **default to faithful** (the `RB3_NO_*` family), or working reimplementations mis-flagged as stubs |

## State of the port

**Mostly faithful with isolated disables — not broadly stubbed.** The large
majority of flagged items are either correct out-of-scope platform glue (all
matching DC3's posture) or *benign* env-gated opt-outs that are default-ON and
faithful (the entire `RB3_NO_IK` / `RB3_NO_CLIP` / `RB3_NO_FACE` / `RB3_NO_DEFORM`
/ `RB3_NO_POSEMESHES` family, plus VorbisReader / AudioDevice / Keyboard / Joypad
"false positives" that are actually working reimplementations). The real
convergence debt is a small, tightly-coupled **char-customize + vocals** cluster.

The single biggest gap was that the port had **zero automated tests** — every fix
was "verified once and forgotten." Standing up a gtest harness (done, below) is
the prerequisite that turns each fix into a permanent guard.

## Bring-online worklist (status)

| # | Item | File | Risk | Status |
|---|---|---|---|---|
| 0 | gDeforms gender-morph load + `IsExoBone` LP64 fix | `bandobj/BandCharDesc.cpp`, `rndobj/MeshDeform.cpp` | LOW | ✅ **done** (`a5999979`) |
| 1 | gtest harness + CMake test target | `native/CMakeLists.txt`, `native/tests/` | LOW | ✅ **done** (`f51ab466`) |
| 2 | `PostLoadVocals` — remove stale `HX_NATIVE` skip | `beatmatch/SongData.cpp` | MED | ✅ **done** (`082bcea4`) |
| 3 | `GameMicManager::Init` — un-gate (latent null-deref SIGSEGV) | `App.cpp` | MED | ✅ **done** (`8d300cd7`) |
| 4 | `VocalPlayer` RTTI-cast guard delete (RTTI now real) | `band3/game/Game.cpp:813` | LOW | ✅ **done** (`579e7416`) |
| 5a | BandHeadShaper female-branch `gHeadMale` typo (decomp bug, separable) | `bandobj/BandHeadShaper.cpp:161` | LOW | ✅ **done** (`4e49ef34`, match-positive) |
| 5b | BandHeadShaper head-milo load (`_tmp0/_tmp1` gates) | `bandobj/BandHeadShaper.cpp:137,156` | MED-HIGH | 🚧 **blocked** on `CharClip`/`CharBonesSamples` Load (NOT BandFaceDeform) |
| 6 | worldcenter backdrop-mesh skip | `rndobj/Draw.cpp:79` | MED | ✅ **KEEP** — RTT fixed but orthogonal (box is a depth-occluder) |
| 7 | chars.milo band-preview cache (`CharCache::InitMe`) | `meta_band/CharCache.cpp:50` | MED | 📋 **re-designed** — see validation doc theme B |

`Hmx::Object::PreLoad` (P1) was flagged but is **already correctly shimmed** (the
strong `void PreLoad(BinStream&){Load(bs);}` def displaces the weak stub — binary-
verified); no action needed. Optional: converge to DC3's inline-in-header form.

## Blocked themes (need new native code)

- **A. ~~Packed big-endian `BandFaceDeform::DeltaArray::Load` reader~~ — CORRECTED: NOT a blocker.** The validation sweep proved the current shared `DeltaArray::Load` is **already byte-correct on the LE host** (38/38 records of `head_male.milo_xbox` frame[1] decode identically). The two `>>` reads are **non-overlapping** (start-index `u16` @0-1, `num` @2-3 — `char unk0` is a misnomer for the start index's high byte), so `num`/`thisoffset()` never corrupt and the stream stays synced; the proposed "read `unk0` as 1 byte" fix would have *desynced* it. Locked by gtest `NativeSubsystems.BandFaceDeformDeltaArrayLoadBE` (`15e3c048`). The real BandHeadShaper head-milo blocker is `CharClip`/`CharBonesSamples`/`CharBones` `Load` (version-desync + string-len overflow), tracked as item 5b. See validation doc theme A.
- **B. Character-customize preview cache: chars.milo + `CharSync::UpdateCharCache` + per-frame `BandCharacter::Poll`** (`meta_band/CharCache.cpp:50`, `meta_band/CharSync.cpp`, effort HIGH). **Re-designed** — see [validation doc theme B](BLOCKER_VALIDATION_2026-06-08.md). Corrections vs the original framing: (1) the "same object types as the working `world_chars` path" premise is **false** — `world_chars.milo` has zero BandCharacters (TransProxies); the real working path is the venue milo via `BandWardrobe`. (2) Un-gating `UpdateCharCache` runs it **menu-wide on every screen transition**, not just customize. (3) The highest-risk site is a **hard crash** — `BandCharacter::StartLoad:1359` derefs `mFileMerger` unconditionally (runtime-gate: confirm `chars.milo` players carry a `FileMerger.fm` child before un-gating). Guard list widened to CharCache:65/120 + CharSync:179. gDeforms prereq is satisfied.
- **C. Native-desktop intro cinematic (FFmpeg-shaped Bink backend)** (`rb3_movie_native.cpp:198`, effort MED-HIGH). Web already plays the intro via a `<video>` overlay; native-desktop instant-skips. Least-valuable surface.
- **D. Whammy slip (guitar/bass pitch-bend) send-loop architecture** (`rb3_stream_receiver_native.cpp:145`, effort HIGH). Slip channels stall the cursor-gated refill ring after the ~1.1 s prime. Fix DC3-style: decouple the send loop from the cursor gate (raise `mWantToSend` off ring back-pressure), then implement the bend resampling. Land in the shared engine so DC3 inherits. Common case (no whammy) is already faithful.
- **E. Single-player scoreboard top-center milo** (`ui/TrackPanelDir.cpp:294`, effort MED). The native synthetic `right.grp`/`left.grp` X-neutralization is a stopgap; the prior "empty `[xfms]`" root cause was a misdiagnosis (real positions live in `mTypeProps`/`mConfigurableObjects`). Benign to shipping (correct visual) but divergent in mechanism — diagnostic-first.
- **F. Char head/hands/face skeleton-rebind animation** (`bandobj/BandCharacter.cpp:802`, the torso-only-scope residual from the landed band-char fix). Hair/face dynamic meshes still clamp to bind (rotation-basis mismatch). Tracked in the char-skinning investigation doc.

## Out-of-scope glue (correct — do not "fix")

9 items, all matching DC3's posture: RockCentral/`TheNet`/`EntityUploader`/USB-MIDI
online + Wii-peripheral inits (gated off, WFC was shut down 2014); `WebSvcMgr` /
`NetworkSocket` transport stubs (host-socket swap, deferred netcode); the DC3 Bink
stub (RB3 has its own movie backend); `ParseStack` symbolicator; `PlatformMgr`
Wii host-OS overlays (HomeMenu/DiscErrorMgr).

## Benign (47 — default-faithful)

Dominated by the **char** subsystem (21): the `RB3_NO_*` diagnostic opt-outs that
all default to running the faithful path (`RB3_NO_IK`, `RB3_NO_CLIP`, `RB3_NO_FACE`,
`RB3_NO_DEFORM`, `RB3_NO_POSEMESHES`, `RB3_NO_SKEL_REBIND`, `RB3_NO_SKIN_CLAMP`, …),
plus synth/net/bandobj/ui working reimplementations the discovery sweep mis-flagged
as stubs. **No action** — these are the *right* shape (faithful default + debug
escape hatch).

## Test suite (the durable-guard prerequisite)

Wired up in `f51ab466`, mirroring `dc3-decomp/native/tests/` (`milo-tests`):

- **Target:** `rb3-tests` (CMake `BUILD_TESTS=ON`). Reuses `rb3-native`'s exact
  source list via `get_target_property` (minus `main_native.cpp`; `gtest_main`
  provides `main()`) so static-init object-factory registration stays live.
  `gtest_discover_tests` with `DISCOVERY_MODE PRE_TEST` + `RESOURCE_LOCK`
  (the engine is a process-global singleton → serialize in-process tests).
- **Fixtures** (`native/tests/test_helpers.{h,cpp}`): `MemBinStream` + synthetic
  byte builders; `SymbolTestFixture` (pure — Symbol+MakeString only); and
  `EngineTestFixture` (`EnsureEngineInit` = RunBoot-style headless
  `SystemPreInit`/`SystemInit`; SKIPs if `RB3_DATA` is absent).
- **Landed tests (10/10 green):**
  - `test_binstream.cpp` — locks `BinStream` endian polarity (`mLittleEndian` =
    *file* endianness, not host). The audit flagged this invariant as reverted
    twice; these are millisecond, asset-free CI gates.
  - `test_subsystems.cpp` — headless engine boots + `SystemConfig` / object
    factories live (guards the "refuse-to-init a subsystem" hack class).

### Run

```bash
cmake --build native/build-native --target rb3-tests -j$(nproc)
cd native/build-native && RB3_DATA=/home/free/code/milohax/rb3/orig-assets/extracted ctest --output-on-failure
# or directly:  ./rb3-tests --gtest_filter='BinStreamEndian.*'
```

### Test rollout (next, ordered)

1. ~~harness + CMake~~ ✅ · ~~subsystems-online~~ ✅ (expand: gDeforms!=0 +
   `GetDeformClip("male"/"female")` resolve; `TheGameMicManager`!=null;
   `TheSynth`/`TheMusicLibrary` non-null; save round-trip).
2. **serialization-binstream** (expand): mogg AES-CTR seek counter, v0xE HMXA XOR,
   ChunkStream `0x810` header swap, DataFlex hold-char — and the **BandFaceDeform
   endian reader** once written (theme A).
3. **asset-loading-pipeline**: DirLoader stream coherence, inline-cache vs
   shared-subdir scope, merge-collision redirection, the chars.milo probe.
4. **char-deform-skeleton**: CharBonesSamples Save/Load round-trip (proves the
   "feared" version-assert path is byte-correct so nobody re-disables gDeforms),
   foot-bone invariants, `GetDeformClip` non-null.
5. **boot-to-gameplay-flow** (subprocess + HTTP-poll, GPU): one green flow guards
   the whole IN_SCOPE hack class at once (deform load, skel rebind, vocals post-
   load, mic mgr, library poll, song-end shims). **This is the automated guard
   for the PostLoadVocals + GameMicManager bring-onlines just landed.**
6. **audio**: convert the Python capture heuristics (music-vs-noise coherence,
   fs-pin railing, instant-attack limiter) into fast gtests.

## Method note (the gDeforms lesson)

The audit is *analysis*; each bring-online was **empirically re-verified before
landing** — the original gDeforms investigation's premise ("female shatters
without it") turned out wrong on the settled tree (the skeleton rebind owns the
fling; gDeforms is the gender vertex-morph). So: read the actual code, build,
drive the headless harness, confirm the specific risk didn't materialize, *then*
commit. PostLoadVocals's `mPlayerTrackConfigList` deref and GameMicManager's
`mic_fx.milo` load were both verified this way (boot reaches `game_screen`, no
crash) rather than assumed from the audit.
