# W8 — MWCC Argument-Swap Roadmap

**Branch:** `wt-web-w8-argswap`
**Base:** `3daf7100` (master 2026-05-30)
**Engine pin:** `e6c8f86`

## Background — the W6-V2 pattern

The W6-V2 fix at `src/system/rndobj/Anim.cpp:381` corrected an **argument swap** in
the decomp source:

```cpp
// retail asm:                          mAnim->SetFrame(frame, blend)
// decomp source (pre-V2):              mAnim->SetFrame(blend, frame)   // <-- arg-swap
// post-V2 (HX_NATIVE-gated):
#ifdef HX_NATIVE
    mAnim->SetFrame(frame, blend);   // corrected order for clang/emcc
#else
    mAnim->SetFrame(blend, frame);   // preserves the Wii-MWCC near-match
#endif
```

`AnimTask::Poll` is the centralized driver for *every* `RndAnimatable` in the
engine (PropAnim, TransAnim, MatAnim, MeshAnim, EnvAnim, LitAnim, PartAnim,
CamAnim, Group, Dir, …). Because all anim subclasses receive their per-frame
`(frame, blend)` through this single Poll, fixing the swap here propagated to
**every UI reveal anim, transform anim, light anim, etc.** With the V2 fix:

- PLAY NOW / CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS reveal anims fire
  → main hub menu list becomes visible (W6-V2 root symptom).
- Every PropAnim-driven visibility toggle starts ticking past frame 1.
- Cascade: cinematic camera anims, lighting presets, character pose blends.

The reactive workaround at `src/system/rndobj/TransAnim.cpp:290-296` (which
detected `blend < 0 || blend > 1` and swapped back) is now **dead code** after
V2 (blend is always in [0,1] when the Poll path is correct). Left in place as a
belt-and-suspenders sanity check — it has zero cost when blend is valid.

### Mechanism — how MWCC compiled past the bug

The retail asm at `0x8087CC04` loads `f1=frame, f2=blend` for the `bctrl`
SetFrame vcall (see `build/SZBE69_B8/asm/system/rndobj/Anim.s:3050-3056`).
Our decomp source compiles to `f1=blend, f2=frame` — literally what the source
says. The two diverge, and `Poll__8AnimTaskFf` therefore matches at **96.7%**
(not 100%): the arg-load mismatch *is* the 3.3% gap. MWCC's optimizer did not
"compile the swap away" — it faithfully compiled the source-as-written, and
the 3.3% mismatch was simply small enough to slip past the matching threshold
when this function was first declared "matches in retail, debug sucks"
(`src/system/rndobj/Anim.cpp:350`).

On the Wii original, the LIVE runtime asm called `SetFrame(frame, blend)`. On
the decomp+native+web port, our compiled .o calls `SetFrame(blend, frame)` —
which means every anim freezes at `frame=blend≈1.0` on the first Poll, never
advancing.

> Important takeaway: a function at 95–99% can still encode a real semantic
> bug. "Matches in retail with the right inline settings" is not a guarantee
> of correctness; it is a guarantee that the .o file's bytes are close to
> retail's. Source-vs-asm divergence in the gap is the latent-bug carrier.

## Methodology — how candidates were identified

This sweep used four complementary discovery passes within the agent's
allowed file scope (`src/system/rndobj/`, `src/system/world/`, `src/system/char/`,
`src/system/bandobj/`, `src/system/ui/`, `src/system/math/`, `src/band3/meta_band/`,
`src/band3/game/`). Excluded: `src/band3/bandtrack/` (HUD-meters agent), engine
internals (engine-RndCam agent), `native/web/index.html` (W4c agent).

### Pass 1 — explicit (blend, frame) order grep
```bash
grep -rEn '(SetFrame|Animate|Interp)\([^,]*\bblend\b[^,]*,[^)]*\bframe\b' src/ --include='*.cpp'
```
Result: only the V2 fix itself (Anim.cpp:383) and the TransAnim comment. No
other source explicitly writes `(blend, frame)` order. **Conclusion:** the
SetFrame-family arg-swap is unique to AnimTask::Poll; downstream callers all
use the conventional `(frame, value)` order.

### Pass 2 — adjacent-same-type-args inventory
Scanned `Set*(float, float)` virtual signatures across engine + game UI:
```
SetFrame(frame, blend)        — Anim/PropAnim/MatAnim/etc (all anim subclasses)
SetPreFrame(float, float)     — BandCamShot
SetFrameEx(float, float)      — BandCamShot
SetDeployTiming(float, float) — NoteTube (bandtrack — out of scope)
SetFade(float, float)         — GemTrackDir
SetFraction(float, float)     — CharIKSliderMidi
SetCellSize(float, float)     — Font
SetZRange(float, float)       — Cam
SetScale(float, float)        — PatchDir
SetRateVar/SetScaleVar        — Gen (named lo, hi)
SetBaseSize(float, float)     — PatchPanel
SetPitchDeviationInfo         — Stats
```
Audited every call site for these. All callers pass arguments in the
declared parameter order with no apparent semantic mis-naming. Most calls
flow through `HANDLE_ACTION(set_X, SetX(_msg->Float(2), _msg->Float(3)))`
DTA dispatchers, where order is locked to the DTA script and verified at
runtime by visible behavior.

### Pass 3 — "matches in retail" sub-100% functions
Cross-referenced files containing the `// matches in retail` comment family
against `build/SZBE69_B8/report.json`. These are functions where the decomp
author flagged the function as "looks-matching-enough" but didn't reach
100%. The V2 site was in this set. Reviewed top 40 candidates manually for
adjacent-same-type-arg call sites where source variable naming hints at a
possible swap. **Result: see Pass 4 finding below.**

### Pass 4 — confirmed candidate via asm diff
`RGTrainerPanel::HandleLegendLefty(bool)` at `src/band3/game/RGTrainerPanel.cpp:507`
shows arg-load mismatches at the `Animate` and `SetFrame` call sites:

| Index | Target asm                  | Base asm (our source)        | Source line  |
|------:|-----------------------------|------------------------------|--------------|
| 121   | `fmr f2, f29 (=f2_var)`     | `fmr f2, f30 (=f12_var)`     | Animate arg2 |
| 128   | `fmr f1, f29 (=f2_var)`     | `fmr f1, f30 (=f12_var)`     | SetFrame arg1|

The source has:
```cpp
leftyAnim->Animate(f12, f12, kTaskUISeconds, 0, 0);   // line 505
leftyAnim->SetFrame(f12, f2);                          // line 507
```
But the retail asm implies:
```cpp
leftyAnim->Animate(f12, f2,  kTaskUISeconds, 0, 0);   // start=f12, end=f2
leftyAnim->SetFrame(f2, 1.0f);                         // frame=f2, blend=full
```
Match% is 97.69%. **Status: latent arg-mistranscription, NOT fixed this pass.**
Reason: function is only reached from the *Pro/Real Guitar trainer panel*
(`RGTrainerPanel`); the W3c boot path goes splash → main_hub → song_select →
part_difficulty → gameplay and never reaches the RG trainer. No visible
symptom on the canonical web-port boot path. **Documented for future work.**

## Per-candidate table

| File:Line                                       | Function                                        | Match %  | Swap kind            | Confirmed | Fixed | Visible symptom                                  |
|------------------------------------------------:|:------------------------------------------------|---------:|:---------------------|:----------|:------|:-------------------------------------------------|
| `src/system/rndobj/Anim.cpp:381`                | `AnimTask::Poll → mAnim->SetFrame`              | 96.7%    | (blend, frame)       | YES       | YES (W6-V2, `ca671682`) | Every reveal anim freezes at frame 1; main hub menu missing |
| `src/system/rndobj/TransAnim.cpp:290-296`       | `RndTransAnim::SetFrame` reactive workaround    | n/a      | defensive detect+swap| YES       | LEFT-IN-PLACE | Camera-NaN protection; now harmless redundant safety net |
| `src/band3/game/RGTrainerPanel.cpp:505,507`     | `RGTrainerPanel::HandleLegendLefty` Animate + SetFrame | 97.7%    | (f12, f12) / (f12, f2) likely mistranscription | YES (asm-diff) | NO | Pro/Real-Guitar trainer lefty-flip mis-animates; not on W3c critical path |
| `src/system/world/CameraShot.cpp:225,233`       | `CamShot::SetFrame → frame->Interp`             | 93.4%    | (none — verified)    | NO        | n/a   | n/a |
| `src/system/world/LightPreset.cpp:894+`         | `AnimateLightFromPreset`                        | 96.85%   | (none — Interp calls correct) | NO | n/a | n/a |
| `src/system/bandobj/PatchDir.cpp:282`           | `sGrpAnim->SetFrame(deform, blend)`             | 100%     | (none — non-1.0 blend by design) | NO | n/a | n/a |

## Cascade observations

- V2 fix (already landed at `ca671682`) restores **every** UI reveal anim
  (PLAY NOW / CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS confirmed visible
  in this pass's smoke test).
- TransAnim's reactive workaround at lines 290-296 was the FIRST attempt at
  fixing the same symptom (cameras → NaN because RndTransAnim::MakeTransform
  extrapolated by song-frame instead of weight). With V2 upstream of it, the
  workaround's swap-detection branch (`blend < 0 || blend > 1`) is now dead —
  blend always arrives in [0,1]. Left in place as a fail-safe; cost is one
  load + two compares per RndTransAnim::SetFrame call.

## Open candidates (suspected but not pursued)

The MWCC arg-swap mechanism requires three conditions to hide a swap behind a
high match%:

1. Two adjacent same-type arguments at a virtual-call site (so MWCC's bctrl
   setup is the only observable difference).
2. The callee doesn't observably care about one of the args (e.g.
   SetFrame's `blend` is sometimes clamped to 1.0 anyway).
3. The function is in the 95-99% match band where small diffs aren't
   investigated.

Functions in scope matching this fingerprint that were spot-checked but
not deeply asm-diffed:

| File                                      | Function                                | Match % | Notes |
|------------------------------------------:|:----------------------------------------|--------:|:------|
| `src/system/char/CharLipSyncDriver.cpp`   | `CharLipSyncDriver::Poll`               | 95.18%  | Audio-driven; not on visible W3c path. |
| `src/system/char/CharEyes.cpp`            | `CharEyes::Poll`                        | 95.70%  | Char-eye gaze; subtle but visible if wrong. |
| `src/system/char/CharIKSliderMidi.cpp`    | `CharIKSliderMidi::Poll`                | 95.77%  | IK slider; SetFraction(float, float) candidate. |
| `src/system/char/CharLookAt.cpp`          | `CharLookAt::Poll`                      | 95.96%  | Head-look-at; subtle. |
| `src/system/char/CharBoneTwist.cpp`       | `CharBoneTwist::Poll`                   | 96.17%  | Bone twist; subtle if wrong. |
| `src/system/rndobj/Line.cpp`              | `UpdateLine` overloads                  | 96.04–97.13% | RndLine point lerping; sub-frame precision. |
| `src/band3/game/CrowdRating.cpp`          | `CrowdRating::UpdatePhrase/Update`      | 96.97%  | (float, float) args — `(score, target?)` or similar. |
| `src/band3/game/GemPlayer.cpp`            | `UpdateGameCymbalLanes`                 | 97.50%  | Cymbal lane visibility; testable on drums. |
| `src/band3/game/Player.cpp`               | `UpdateSectionStats(float, float)`      | 96.86%  | Section-stat tracking; subtle. |

These functions each require ~30 minutes of asm-side analysis to confirm
or rule out a swap. None are blocking the W3c boot path; none manifest as
visible bugs in the current capture. **Recommended cadence:** investigate
on demand when a specific visible bug suggests this class of failure.

## Risk

- **V2 fix risk (already shipped):** the `#ifdef HX_NATIVE` gate preserves
  the Wii-MWCC near-match (96.7% unchanged). Zero risk to retail decomp.
  On native+web, behavior is now correct as long as `HX_NATIVE` is defined
  (it is — see `native/CMakeLists.txt`).
- **TransAnim reactive workaround risk:** if a future change removes the
  Anim.cpp V2 fix without also removing the TransAnim workaround, the
  workaround will silently mask the regression. Mitigation: the workaround
  comment at TransAnim.cpp:276 explicitly references AnimTask::Poll as the
  source of the swap; a reviewer reverting Anim.cpp would naturally see
  the cross-reference.
- **HandleLegendLefty (unfixed):** if a future agent enables the Pro/Real
  Guitar trainer in the web port, the lefty-flip legend will animate
  incorrectly. Add a `// TODO(W8-argswap):` note in
  `RGTrainerPanel::HandleLegendLefty` so the next eye spots it.

## Verification — smoke test

`scripts/web/w3c-gameplay-test.mjs --port 8893 --play-seconds 30`:
- splash → main_hub → song_select → part_difficulty → gameplay end-to-end
- PLAY NOW / CAREER / TRAINING / CUSTOMIZE / GET MORE SONGS visible in 01_main_hub
- gameplay highway + gems render through t=30s (`05_gameplay_t30s.png`)
- 34.3 fps average; no crash; AudioDevice pumpCount=2767 (~62s)
- **Status: PASS** (no W6/W7 regressions)

## Recommendation for next agent

If you suspect a specific visible bug might be arg-swap-class:

1. Identify the rendering function that *should* be driving the broken
   visual (e.g. UpdateXxx, Poll, Animate).
2. Check `build/SZBE69_B8/report.json` for that function's match%. If it's
   in the 95-99% band with adjacent same-type args at a virtual call,
   it's worth `objdiff-cli diff --include-instructions --analyze`.
3. Compare target asm's argument loads (`fmr fN, fM` immediately before
   `bctrl`) against the source's expected argument order. Mismatches in
   `diff_arg` `fmr` instructions immediately before `bctrl` are the
   arg-swap fingerprint.
4. Apply the HX_NATIVE gate pattern (see V2 fix at Anim.cpp:378-384).
5. Update this doc's per-candidate table with the finding.
