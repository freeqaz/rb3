# C8 + Festival-Bink — synthesis & go/no-go PLAN (2026-06-30)

**Agent:** synthesis (Opus). Inputs: `c8-prior-synthesis.md`, `c8-measurement.md`,
`bink-scope.md` (all in this dir). Engine pin `998b8734`. No native runs (per brief);
verified the two load-bearing facts by file/asset inspection only.

---

## Bottom line

| Frontier | Verdict | One-line reason |
|---|---|---|
| **C8 / feet-in-floor / rotation-basis** | **DEFER** | Fresh deterministic measurement on master shows the bug **does not reproduce** — RB3 band feet are faithful (toe ~0, ankle ~4.3 = Xbox). Nothing to implement; porting the DC3 plant would "fix" a non-bug and risk regression. |
| **Festival Bink crowd backdrop** | **BLOCKED** | Render plumbing is bounded + DC3-proven (~1 day, RB3-only Option B1), but the festival crowd `.bik` source files are **absent from every `orig-assets/` root** (verified 0 hits). Nothing to decode until the assets are obtained. |

**Neither frontier is a "drop everything and write code" item this session.** The
prompt's hypothetical was "C8 blocked, only Bink implementable" — reality is the
inverse-ish: **C8 is effectively resolved/non-reproducing (not blocked), and Bink's
code is ready but blocked on an out-of-repo asset.** See the ordered recommendation.

---

## FRONTIER 1 — C8 / feet-in-floor: DEFER (the bug does not reproduce on master)

### Why DEFER and not IMPLEMENT
The prior-synthesis doc recommends porting DC3's `Dc3RunPostPollFootPlant`
(`dc3 CharIKFoot.cpp:325`, called from `dc3 App.cpp:1109`) into RB3's
`App::RunOneFrame` after `TheTaskMgr.Poll()` (RB3 seam confirmed at `src/App.cpp:561`;
the `CharIKFoot::DoFSM` hook exists at `src/system/char/CharIKFoot.cpp:26` with an
`HX_NATIVE` branch at :87). That recommendation was written **read-heavy, no native
runs**, extrapolating from DC3's still-open gate and pre-resolution RB3 docs.

The **measurement agent ran the actual deterministic harness on current master** and
got the decisive, direct number — RB3 player-LOCAL foot, floor ≈ 0, n=45 LIVE samples:

| quantity | Xbox 360 ground truth | DC3 native (BUG) | **RB3 native (master)** |
|---|---|---|---|
| toe_z | 0.006 … 0.53 | **−4.3** | **−0.0** (worst −0.2) |
| ankle_z | 4.1 … 5.4 | +1.6 … +2.0 | **4.3** |
| pelvis_z | 36 … 39 | ~36 | 37.6 |

RB3's ankle sits ~4.3 above the floor and the toe sits **at** the floor — it matches
Xbox and does **not** show the DC3 signature (ankle collapse → toe −4.3). The full
ankle→root local chain composes perfectly: `Ldet=1.000`, `Lrow=(1,1,1)` on every bone
(no length/scale corruption), `CHAIN_COMPOSE dMag=0.00` on every link (no
stale-cache / severed-dirty / wrong-parent). IK is inert (`RB3_NO_IK=1` A/B identical,
zero band drops both). The prior "186u leg fling / 15× boot ratio" **does not
reproduce** in the deterministic harness for any member.

**Direct measurement on the shipped build beats inference from pre-resolution docs.**
Porting the DC3 plant would assert a result the RB3 pose already satisfies — at best a
no-op, at worst it lifts an already-planted toe and *introduces* a divergence. So:
**do not port the plant. There is no actionable C8 feet bug on RB3 master.**

### Are (A) feet-sink and (B) thin-geo smear one root? — Moot on master
The prior-synthesis argues "same family, different layers" (A = pose-pipeline output
error, B = bind-basis error, B downstream of A). That model was for a world where A
was a live bug. On master **A is not firing**, and the measurement shows the leg pose
basis is geometrically correct (rotation-only ankle/toe articulation that lands the toe
at the floor). The residual **(B) thin-geo `_skin.2` drops** are the known ~1-in-4
wardrobe-roll *transient* small-bind-ratio false-positives — **off-frame in 100% of
closeups** and **already correctly handled by the V24 guard**. They are a separate,
much smaller residual, not the "C8 blocks everything" story the older
`char-rebake-scope.md` tells.

### What's left for C8 (both cheap, both optional, neither is "fix the bug")
1. **Confidence-closing capture (small).** The measurement is bone-telemetry-only
   because this club venue frames upper-body (feet off-frame in every deterministic
   shot). To close it visually + across venues/moves: run `band-closeup-capture.py`
   against a **feet-framing venue** (arena/festival `coop_*` shot, or a temporary
   down-tilt on `{rb3_force_shot}`) and read `C8_SLOT` toe_z in player-local space
   across a full song. **Predicted: toe_z ≥ −1 (planted).** If — and only if — some
   venue+move reads ≤ −3, *that* pair is the first real RB3 repro and the DC3 poll-order
   re-dirty-after-IK fix (`dc3 HamDriver.cpp:95-101`, `dc3 19-feet-ik-wave3.md`) becomes
   the candidate. No frame measured so far shows this.
2. **Doc/memory correction (small).** `docs/native/converge-2026-06-20/deferred/char-rebake-scope.md`,
   `docs/native/render-polish-2026-06-11/task-ik-mispose-impl.md`, and the
   `[[project_dc3_feet_in_floor_anim]]` memory note still say RB3 is "C8-blocked / feet
   sink ~4u / 186u fling / STILL UNFIXED." Correct them to the measured state so future
   agents stop re-chasing a non-reproducing bug. This is the **highest-leverage C8
   action** — it has already cost multiple multi-day sessions.

### Is the historical "blocker" still a blocker? No.
The old P0 blocker was "needs Xbox/Xenia ankle-Z ground truth." That ground truth
**now exists** (DC3 captured it via Xenia GDB-RSP; `dc3 2026-06-09-xenia-xbox-foot-truth.md`),
**RB3 matches it**, and `rb3-xenon` exists (`/home/free/code/milohax/rb3-xenon`,
confirmed) as RB3's own re-runnable capture path if symptom B ever needs a thin-geo
bone-basis ground truth. So C8 is **not blocked** — it's measured-faithful.

### Do NOT re-chase (refuted, with citations in c8-measurement.md / c8-prior-synthesis.md)
leg/pelvis local-length change rest→live (det=1, lengths constant) · 186u fling / 15×
ratio (no reproduce) · stale-cache / severed-dirty / wrong-parent (dMag=0) · IK
handedness / IK mispose (IK inert) · toe-channel LP64 decode · femur shrink ·
poll-order as a clean lever (sorter already deterministic).

---

## FRONTIER 2 — Festival Bink crowd backdrop: BLOCKED on assets (code is ready)

### The render plumbing is IMPLEMENT-sized and DC3-proven — Option B1
DC3 already ships the painted-RT path (`dc3 TexMovie.cpp::DrawToTexture`, lines
201-232): decode a frame with `FFmpegMovieImpl` (native) / `WebMovieImpl` (web) →
`UploadRGBAToRndTex(mTex, rgba, w, h)` (shared `Tex_Wgpu.cpp:258`, already RB3-linked
and RT-aware). RB3's `Movie` is the byte-matched Wii Bink class (no `GetImpl()` /
`MovieImpl*`), so the clean route is **B1 — an RB3-only side-helper** that never
touches the matched class:

- **NEW `native/src/rb3_texmovie_native.cpp`** (~120–180 LOC): a `map<TexMovie*,decoder>`
  owning an FFmpeg/Web decoder per TexMovie, keyed off `bink_movie_file`; forwards
  `Begin/Poll/End`; exposes the latest RGBA frame.
- **`src/system/movie/TexMovie.cpp:86` `DrawToTexture()`** — add an `#ifdef HX_NATIVE`
  branch: if the helper has a decoded frame, `UploadRGBAToRndTex(mTex, rgba, w, h)`;
  drive `BeginFromFile`/`Poll` from `:72 BeginMovie` / `:116 Poll`.
- **CMake (`native/CMakeLists.txt:178`)** — compile the FFmpeg decode core into the new
  TU (the upstream `FFmpegMovieImpl.h` `#include "movie/MovieImpl.h"` is DC3-only, so
  lift/shim the decode core rather than un-excluding the DC3-shaped TU wholesale); link
  `libavformat/avcodec/swscale/avutil` (system-present, `libavcodec 62.28`).
- **Web** reuses `WebMovieImpl`'s in-world readback path (distinct from the intro
  `<video>` overlay) + a one-line glob add to `scripts/web/transcode_videos.py`.
- **DC3-safety:** `UploadRGBAToRndTex` is shared but additive (DC3 already uses it); the
  new TU + the `HX_NATIVE` `TexMovie` branch are RB3-only. Safe. Reject Option B2
  (reshaping the matched `Movie` onto DC3's `GetImpl()` — risks the match for no extra
  visual gain).

Effort if assets existed: **~1 day** (helper ~0.5–1d, CMake/FFmpeg ~1–2h, web ~2–3h,
RT upload ~1h).

### The blocker — verified this session
The festival crowd movies are **external `.bik` references that are absent from every
`orig-assets/` root**:
- `find orig-assets -iname 'fest*mass*.bik' -o -iname 'mass_crowd*.bik'` → **0 hits**.
- The festival `gen/` dirs contain **only** `festival_0{1,2}.milo_{xbox,wii}` archives —
  no `texture/` or `streams/` sibling with the movie files, and the biks are not inline
  (the milo "BIK" string hits are compressed-texture noise, not `BIKi/BIKb/BIKg`).
- The `.tmov` objects reference `texture/fest1_mass0[1-6].bik` (festival_01) /
  `../textures/mass_crowd[1-6].bik` (festival_02) — all missing.

**There is nothing to decode.** Neither native FFmpeg nor the web `.webm` transcode has
a source. (`transcode_videos.py` is also hardcoded to the 2 cinematics, but that's a
trivial glob extension — the gate is the missing source, not the script.)

### The cheapest unblock
**Other venues' crowd biks DO exist** in the repo —
`orig-assets/wii-extracted/world/venue/{arena,big_club,small_club}/streams/crowd_*_intro.bik`
(verified) — proving these movie streams *are* extractable from a complete disc; they
were simply not pulled for festival. So the unblock is an **asset hunt**: locate/extract
`fest1_mass0[1-6].bik` + `mass_crowd[1-6].bik` from a fuller Xbox/PS3 (or Wii) disc dump
into `orig-assets/.../festival/.../texture/` (or `streams/`). This is out-of-repo,
**unbounded research/acquisition, not code.** Until then, the just-landed **Option A
(skip the white quad → black backdrop)** is the correct shipped state.

---

## Ordered recommendation — what to do next

Honest framing: C8 has **no code to write** (non-reproducing), Bink has **no asset to
feed** (missing source). So the next actions are deliberately small/research, not a code
sprint.

1. **C8 — doc/memory correction (small, do first).** Highest leverage per minute:
   update `char-rebake-scope.md`, `task-ik-mispose-impl.md`, and the
   `[[project_dc3_feet_in_floor_anim]]` memory note to the measured state ("RB3 band feet
   faithful on master — toe ~0/ankle ~4.3 = Xbox; 186u fling does not reproduce; thin-geo
   `_skin.2` drops are off-frame V24-handled false-positives"). Prevents the next agent
   from burning another multi-day session on a solved/non-bug.
2. **C8 — optional confidence capture (small).** One feet-framing-venue `C8_SLOT` toe_z
   run (arena/festival down-tilt) to convert DEFER → confidently-resolved. Predicted
   toe_z ≥ −1. Only escalates to the DC3 poll-order fix if a venue/move actually reads
   ≤ −3 (none measured so far).
3. **Bink — asset hunt (research, unbounded, gating).** Locate/extract the festival
   crowd `.bik` files. **Only if/when they land** does Option B1 become a clean ~1-day
   RB3-only DC3-safe implement.
4. **Do NOT** port `Dc3RunPostPollFootPlant` into RB3, and **do NOT** start the B1 movie
   helper before the assets exist. Both would be work against a non-existent problem.

If forced to pick "the one frontier to invest engineering in next": **neither is a code
win right now.** The only *implementable code* lead is Bink-B1, and it is hard-gated on
the asset hunt (#3). The only *zero-asset, ships-value-today* action is the C8 doc
correction (#1).
