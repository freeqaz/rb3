# Why CrowdAudio produces ZERO intro/crowd audio on native — DIAGNOSIS (2026-06-21)

**Worktree:** rb3 `wt-crowdaud-diag179` (branch, from `179fcabb`), engine worktree
`milo-native-engine-worktrees/crowdaud-diag179` (clean, untouched, pin `b8f3cfa`).
Diagnostic instrumentation committed at `ecc18786` (native-only, `#ifdef HX_NATIVE`
+ `RB3_CROWD_DBG`, **remove before landing** — see "Instrumentation" below).
No Wii bytes touched, no engine change, no `MILO_ENGINE_PIN` bump.

## TL;DR — the gap is an ASSET gap, not a code/wiring gap

`CrowdAudio` is **fully alive on native**: compiled, factory-registered,
constructed from `world.milo`, Poll'd, and its `play_intro` handler (`OnIntro`)
**fires**. `OnIntro` calls `PlayLoop("crowd_intro.mogg")`, which does
`mBank->Find<BinkClip>(clipname)` — and that returns **nil**, so `PlayLoop`
returns `false` and no audio plays. The crowd audio is silent because:

> **The Xbox-360 venue milo the native port loads (`small_club_01.milo_xbox`)
> contains ZERO `BinkClip` objects.** The crowd/venue-intro audio loops live in
> `BinkClip` objects that exist only in the **Wii** venue milo (6 of them,
> referencing `.bik` Bink-audio streams). The 360 ARK the native build extracts
> from stores the crowd loops differently (mogg names appear only as strings in a
> `crowd_audio_config` Tex/config object, not as constructable `BinkClip`s).

So `CrowdAudio::PlayLoop` never finds a clip → never reaches `BinkClip::Play` →
never reaches the native `Synth::NewStreamFile` mogg path. **The native AudioDevice
route is fine and unused.** The fix is an *asset/data* problem, not a wiring one.

## The exact gap (file:line)

`src/system/bandobj/CrowdAudio.cpp:170` (canonical, un-instrumented):
```cpp
clip = mBank->Find<BinkClip>(clipname, false);   // returns nil on native
```
`clipname` = `"crowd_intro.mogg"` (then the excitement loops `crowd_norm/good/peak`),
`mBank` = the `small_club_01` venue WorldDir (set by `BandDirector.cpp:726`
`TheCrowdAudio->SetBank(mCurWorld)`). The `Find<BinkClip>` misses because no
`BinkClip` object by that name exists in `mBank`.

## Empirical proof (native harness, captured to /tmp/crowd_dbg.txt)

Drove `rb3-native` to the `antibodies` gameplay intro (`scripts/native/keyboard-to-gameplay.py`
flow, `RB3_CROWD_DBG=1`). The guaranteed-capture file showed, in order:

```
BandInit() entered; INIT_BAND macro=0x...          ← BandInit runs (App.cpp:379)
CrowdAudio::Init() Register() called               ← factory registered (Band.cpp:128)
NewObject('CrowdAudio') registered=1               ← world.milo constructs it
CrowdAudio::CrowdAudio() constructed this=0x...     ← object IS built, TheCrowdAudio set
NewObject('BandDirector') registered=1
OnIntro mRestarting=1 shouldVenueIntro=0 mIntro=0x.. mVenueIntro=0x.. mLevels=0x.. mBank=small_club_01
PlayLoop clipname='crowd_intro.mogg' bank=small_club_01 clip=(nil) force=0   ← THE BUG
PlayLoop clipname='crowd_good.mogg'  bank=small_club_01 clip=(nil) force=0
PlayLoop clipname='crowd_peak.mogg'  bank=small_club_01 clip=(nil) force=0
NewStreamFile OK cc='songs/antibodies/antibodies' sym=mogg   ← only the SONG mogg streams
```

Live engine query at songMs≈26s (`{crowd_audio}` DataVariable) → also reflects the
non-functional state. The **only** stream that ever opens is the song mogg; no
crowd/venue mogg ever reaches `Synth::NewStreamFile`.

### Why earlier passes saw "zero CrowdAudio activity"
The harness's Popen stdout/stderr redirect drops the engine's early-boot lines
(everything before `RB3 AV-cal:`), and `CrowdAudio`'s `fprintf(stderr,...)` is in
that dropped window. Routing the probe to a **file** (`/tmp/crowd_dbg.txt`) instead
of stderr revealed the full, correct picture above. The prior measurement doc's
"ZERO CrowdAudio activity in the log" was a logging artifact, not the truth —
CrowdAudio *is* active; it just finds no clips.

## Asset evidence — Wii vs Xbox venue milo (`small_club_01`)

`strings | grep -c '^BinkClip$'` on each variant:

| asset variant | path | BinkClip objects | crowd stream refs |
|---|---|---|---|
| **native (`extracted`)** | `orig-assets/extracted/world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox` | **0** | `crowd_*.mogg` strings in `crowd_audio_config` only |
| xbox-full | `orig-assets/extracted-xbox-full/.../small_club_01.milo_xbox` | **0** | same |
| **Wii** | `orig-assets/wii-extracted/.../small_club_01.milo_wii` | **6** | `crowd_{intro,danger,poor,norm,good,peak}.bik` BinkClip objects |

The Xbox venue milo header classes are only `Tex/Mat/WorldInstance/WorldDir/
BandCamShot/AmbientOcclusion` (+ 22 `Sfx` for the up/down WAV transitions) — **no
`BinkClip`, no `Sequence`, no `StandardStream`**. The Wii milo additionally has the
6 crowd-loop `BinkClip`s.

Separately, the **stream payloads themselves are also absent** from the native
`extracted` tree: `world/venue/small_club/streams/` does not exist there (only the
2 song moggs + SFX are under `extracted`). The Wii tree has
`world/venue/small_club/streams/crowd_small_intro.bik` (intro only). So even if the
BinkClip objects were present, the mogg/bik files they point at
(`../streams/crowd_small_*.mogg`) are not extracted.

## moggsAvailable: NO (two stacked asset gaps)
1. **No `BinkClip` objects** in the Xbox venue milo → `Find<BinkClip>` returns nil
   (the proximate cause, blocks before any file I/O).
2. **No crowd stream files** (`world/venue/small_club/streams/*.mogg|.bik`) in the
   native `extracted` tree → would hit the `Synth::NewStreamFile` `"fake"` BufFile
   fallback (`Synth.cpp:576-580`, silent) even if (1) were fixed.

## Fix scope (native-only; pick ONE primary path)

The diagnosis flips the task from "wire CrowdAudio" to "supply CrowdAudio's data".
CrowdAudio's code path is already correct and native-wired (`BinkClip::Play` →
`TheSynth->NewStream` → `StandardStream`+`VorbisReader`, all live under HX_NATIVE).

**Option A — Use the Wii crowd assets (recommended, smallest surface).**
The Wii build has the 6 `BinkClip` objects + at least `crowd_small_intro.bik`.
Extract the full Wii `world/venue/<class>/streams/*.bik` set and either (a) merge
the Wii venue milo's 6 BinkClip sub-objects into the loaded bank, or (b) on native,
when `Find<BinkClip>` misses, synthesize the BinkClips from `crowd_audio_config`
strings and point them at the Wii `.bik` streams. The native `Synth::NewStreamDecoder`
currently decodes `"mogg"`/`"main"` only — `.bik`/Bink audio decode would need a
native decoder (engine `src/platform`), OR transcode the `.bik` crowd loops to
`.mogg` offline (mirrors `scripts/web/transcode_videos.py` for the intro cinematic)
and feed the existing Vorbis path.

**Option B — Re-extract/repack the crowd BinkClips + mogg streams into the Xbox
venue milo** so `Find<BinkClip>` resolves and the `.mogg` streams exist under
`world/venue/small_club/streams/`. Heaviest (ARK repack), but keeps the engine path
byte-identical to retail 360.

**Option C — Native CrowdAudio synthesis bridge (no asset repack).** Behind
`#ifdef HX_NATIVE`, when `CrowdAudio::PlayLoop`'s `Find<BinkClip>` misses, build a
BinkClip on the fly from the `crowd_audio_config` mogg-name strings and play it via
the existing `NewStream` path — *contingent on the stream files being present*
(still needs Option A/B's stream payloads). This isolates the change to CrowdAudio
but does not remove the missing-file dependency.

**Recommended:** Option A — transcode the Wii crowd `.bik` loops → `.mogg`, drop them
under `orig-assets/extracted/world/venue/<class>/streams/`, and add a small
HX_NATIVE bridge in `CrowdAudio::PlayLoop` (or a native pre-pass) to construct the
6 BinkClips from the `crowd_audio_config` names when the bank lacks them. This is the
minimum that makes the intro window non-silent without an ARK repack, and reuses the
already-working Vorbis stream path.

## Effort
**MEDIUM.** The code wiring is trivial (CrowdAudio is alive; the change is a small
HX_NATIVE BinkClip-synthesis bridge ~30-60 lines, match-neutral). The real work is
the ASSET pipeline: locate/extract the Wii crowd `.bik` streams (and venue_intro,
which is missing even on Wii for small_club — only `crowd_small_intro.bik` is
present), transcode to `.mogg`, and place them in the native asset tree. Budget the
bulk of effort on asset sourcing + a transcode step, not on engine/native code.

## Match-neutrality
- `src/system/bandobj/CrowdAudio.cpp`, `BinkClip.cpp`, `Synth.cpp` = shared Wii
  decomp → any native hook must be `#ifdef HX_NATIVE` (the diagnostic edits here
  already are). Prefer putting the synthesis bridge in `native/src/` glue if
  possible, or a tightly-gated HX_NATIVE block in `CrowdAudio::PlayLoop`.
- The asset additions (`streams/*.mogg`, transcode script) are pure data + tooling,
  zero match impact.

## Instrumentation added (diagnostic; REMOVE before landing — commit `ecc18786`)
All `#ifdef HX_NATIVE` + `getenv("RB3_CROWD_DBG")`, writing to `/tmp/crowd_dbg.txt`:
- `src/system/bandobj/CrowdAudio.cpp` — ctor / `Init` / `OnIntro` / `PlayLoop` probes
- `src/system/bandobj/Band.cpp` — `BandInit()` entry + INIT_BAND macro state
- `src/system/synth/Synth.cpp` — `NewStreamFile` OK/FAKE (missing-mogg) probe
- `src/system/obj/Object.cpp` — `NewObject(Crowd*/WorldDir/BandDirector)` probe
- `native/src/rb3_game_object_factories.cpp` — added `CrowdAudio::Init()` to the
  render-tool factory list (this path = `RB3RegisterGameObjectFactories`, only used
  by `LoadMiloAndWalk`; the real RB3_GAME boot already registers CrowdAudio via
  `App.cpp` → `BandInit()` → `Band.cpp:128` → `CrowdAudio::Init()`, so this edit is
  redundant for the game and can be dropped).

## Reproduce
```bash
cd <worktree>; export RB3_CROWD_DBG=1; rm -f /tmp/crowd_dbg.txt
python3 /tmp/crowd_probe.py        # drives to gameplay, keeps proc alive, queries {crowd_audio}
cat /tmp/crowd_dbg.txt             # the CrowdAudio construct/OnIntro/PlayLoop(nil) chain
# asset check:
strings -n3 orig-assets/extracted/world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox | grep -c '^BinkClip$'  # → 0
strings -n3 orig-assets/wii-extracted/world/venue/small_club/small_club_01/gen/small_club_01.milo_wii   | grep -c '^BinkClip$'  # → 6
```
