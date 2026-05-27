# Real-Stub Census — In-Scope Source (2026-05-26)

Read-only census for placeholder/trivial function bodies that mask a non-trivial
target ("real stubs" left behind to get the link working). Scope = `src/band3/**`
and `src/system/**`, **excluding** the native-port-replaced subtrees
(`rndwii/`, `os/`, `sdk/`, `network/`, `lib/`, `synthwii/`, `stlport/`, any
`*Wii*` file).

## Method

1. Scanned 859 in-scope `.cpp` files for trivial-bodied **definitions**
   (`{}`, `return;`, `return 0/false/true/NULL/nullptr/-1/0.0f/...;`, sole
   `MILO_FAIL`/`MILO_UNIMPLEMENTED`). 1,116 trivial-body defs found.
2. Mapped each to its mangled symbol in `build/SZBE69_B8/report.json` by
   **full demangled signature** (`Class::Method(argtypes)`, not just name — so
   overloads don't collide). 823 exact-signature matches + 240 name-fallback;
   53 unmatched (all inlined nested-struct ctors / template-context lines that
   have no standalone symbol).
3. Classified: **REAL STUB** = trivial source body AND target `size` ≥ 48 bytes
   AND `fuzzy_match_percent` < 60. **FALSE POSITIVE** = trivial source body but
   target also tiny (≤ 32 bytes) → a legitimately-trivial override.
4. Cross-checked from the opposite direction: every in-scope function with
   target ≥ 64 bytes and fuzzy < 30% was inspected to see whether its low score
   is caused by a trivial source stub.

## REAL STUBS

**None.** Zero functions in scope have a trivial source body masking a
non-trivial target.

| unit | function | source | target size | match% | intended behavior |
|------|----------|--------|-------------|--------|-------------------|
| — | _(none found)_ | — | — | — | — |

Thresholds were progressively relaxed to be sure nothing was hiding:

| filter | hits |
|--------|------|
| size ≥ 48 & fuzzy < 60 (exact-sig) | 0 |
| any-overload size ≥ 48 & fuzzy < 60 | 0 |
| size ≥ 32 & fuzzy < 90 | 0 |
| size ≥ 24 & fuzzy < 99 (exact-sig, no init-list) | 1 → `Splash::Splash()` 92.7% (real ctor w/ init list, in progress — **not** a stub) |

The team's belief that stubs are exhausted is **confirmed for the in-scope tree.**

## What the low-fuzzy functions actually are (reverse check)

54 in-scope functions are ≥ 64 bytes at < 30% match. After excluding deferred
networking units, 37 remain — and **every one is a non-stub** for a different
reason:

### Missing source (asm-backed, 0% — not stubs, no C++ body at all)
- **`src/band3/meta_band/JoinInvitePanel.cpp` — file does not exist.** 13 funcs
  (`Handle` 1732 B, `GetPresenceToken` 1056 B, `JoinInvite` 972 B,
  `SetPresenceInfo`, `OnMsg(JoinResultMsg)`, `SetType`, `Enter`/`Exit`, ctor/dtor,
  `ClassName`, …). Online-invite game-logic — **deferred networking** per scope.
- **`src/band3/meta_band/BandNetGameData.cpp` — file does not exist.** 12 funcs
  (`Handle` 1052 B, `DefaultRankedMatch` 808 B, `AuthenticateJoin` 752 B,
  `AuthenticationData` 584 B, `GetEndGameStats`, `Poll`, ctor/dtor,
  `LeaderboardID`, `WinningUser`, `NetGameData::~NetGameData`). Ranked-match /
  leaderboard game-logic — **deferred networking** per scope.
- **`TrainerGemTab::SetPattern(const TrainerSection*, const vector<GameGem>&)`**
  (148 B) — declared in `src/band3/game/TrainerGemTab.h:30`, **no definition in
  the .cpp**. The one genuinely interesting in-scope, non-networking gap: a
  *missing definition*, not a placeholder stub. (`TrainerGemTab.cpp` is in the
  current working-tree change set — may already be in progress.)

### Real full implementations that are simply hard to match (not stubs)
- **`BoxMapLighting::ApplyLight`** (760 B, `src/system/rndobj/BoxMap.cpp:114`) —
  complete source; known MWCC paired-singles (`psq_`/`ps_*`) SIMD blocker
  (see MEMORY `boxmap_psq_blocker`). Replace in native port.

### STL template instantiations (not source stubs)
- `vector<VocalNote>` / `vector<VocalPhrase>` internals — `band3/game/SongDB`
  (these are the working-tree structural-diff items already tracked).
- `_Vector_impl<OldColorOption>` ×2 — `system/bandobj/OutfitConfig`.
- `_Vector_impl<UserStat>` — `BandNetGameData` (networking).
- `MakeString<…>` template specializations — generated, not stubbable.

### Out-of-scope but caught by the unit filter
- `RockCentral` / `ArtFileConverter` / `SaveArtUpdater` / `Quazal::*` —
  `net_band` networking. `NetCacheMgr::UseSSL` — out of scope per MEMORY.

## False-positive clusters (trivial source body, trivial target — checked & correct)

414 trivial defs matched targets ≤ 32 bytes (correct lightweight overrides). The
largest single cluster, called out in the brief:

- **`StoreIndexFileReader` (`src/system/meta/StorePackedMetadata.cpp:139-149`)** —
  13 overrides of the `BinStream`/file-reader interface
  (`Read`, `ReadAsync`, `Write`, `Seek`, `Tell`, `Flush`, `Eof`, `Fail`, `Size`,
  `UncompressedSize`, `ReadDone`, `GetFileHandle`, dtor). Targets are all 4–8 B
  (`li r3,0; blr`) and all **100% matched**. These are correctly-trivial by
  design — the index reader genuinely no-ops most of the stream interface. The
  unit's overall 28% comes from its *other* large functions, **not** these.

Other large false-positive sources (all trivial-by-design overrides / inlined
nested-struct ctors, target ≤ 32 B): `CharLipSync`, `CharClip`, `CharEyes`,
`Character`, `OutfitConfig` (MatSwap/Piercing/Overlay ctors), `Stats`
(StreakInfo/MultiplierInfo/SectionInfo), `EventAnim`, `MatAnim`, `Sfx`, `Object`,
`Dir` (InlinedDir), `MidiParser` (PostProcess), `SongData`
(TrackInfo/FakeTrack).

## Summary

**Real stubs found: 0.** Total target bytes of real stubs: **0**. The in-scope
source has no placeholder bodies masking non-trivial targets — every trivial
body either matches a trivial target (414 correct overrides) or sits inside a
ctor/template that's legitimately small or in-progress.

The remaining large-but-0% in-scope functions are *not* stubs: they are
**missing source** (no `.cpp` body — chiefly the online-multiplayer units
`JoinInvitePanel` and `BandNetGameData`, which are deferred networking per the
port roadmap) or **fully-implemented-but-unmatched** code
(`BoxMapLighting::ApplyLight`, the SIMD blocker).

**Top 5 to fill first** (highest-value in-scope work, though strictly these are
*missing/unmatched*, not stubs):

1. `TrainerGemTab::SetPattern` (148 B) — the only plain single-player
   game-logic gap; missing definition, declared in the header. Likely already
   being worked (file is in the working-tree diff).
2. `BoxMapLighting::ApplyLight` (760 B) — real code; needs MWCC paired-singles
   intrinsics. High effort, known blocker; defer or native-port-replace.
3–5. Everything else of size is `JoinInvitePanel` / `BandNetGameData` online
   networking — **defer until the port has a single-player baseline** per the
   roadmap; not stub work and out of the current focus surface.
