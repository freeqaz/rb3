# Festival `*_screenmask` white-blank fix — INDEPENDENT VERIFY (Opus, adversarial gate)

Verifies `screenmask-impl.md` (Option A). Re-derived from scratch on the paired worktree
build; did NOT trust the impl doc's numbers.

- **Engine commit:** `998b87340438dfa8c993ef8b46fffb0c725f5da1`
  (branch `wt-converge-screenmask`, parent engine `20dba55`).
- **Worktree:** `/home/free/code/milohax/rb3/.claude/worktrees/converge-screenmask`,
  engine at `/home/free/code/milohax/milo-native-engine-worktrees/converge-screenmask`.
- **Binary verified:** `native/build-native/rb3-native` contains the fix (the
  `skip unpainted-RT diffuse` debug string is present; no-op rebuild = up to date).

## VERDICT: **LAND_WITH_NOTES** — confidence HIGH.

The fix is correct, the discriminator is robust (trace-proven: skip fires ONLY on the
dead movie RTs, never on a painted RT / sky-dome / backdrop / UI rect), the gate flips
cleanly, and it is DC3-safe by construction. The note: the result is a **black
background**, not the intended crowd backdrop — Option A's honest tradeoff — and the
impl doc overstates the after-luma.

---

## 1. White blank GONE — PASS (re-measured)

Same-binary A/B (`/tmp/bch_override.py --bin <worktree> --override festival_01`):

| shot | FIX ON (default) | FIX OFF (`RB3_SCREENMASK_FALLBACK_OFF=1`) |
|---|---|---|
| `coop_crowd_mass01_screenmask` | luma **11–15**, white% **0.4** | luma **206–208**, white% **82** |
| `coop_crowd_mass_screenmask`   | luma **11**, white% **0.4**    | luma **213**, white% **84** |

The full-screen white is gone; highway / gems / star-power / HUD / band character all
render through the now-skipped quad. The opt-out **exactly restores** the white blit
(luma 208, white% 82) — proves the gate and that this fallback WAS the white blank.
8/8 deterministic pins both runs.

## 2. MUST-NOT-BREAK — PASS (the key risk)

**Discriminator trace (the decisive evidence).** `RB3_SCREENMASK_DBG=1` over the
festival run: the skip fired on EXACTLY two textures and **zero** sky/cloud/backdrop/
poster/env:
```
  508x 'crowd_mass.tex'   <- the target bug (dead movie RT)
  645x 'movie.tex'        <- the separate intro/transition movie, already alpha=0 invisible
```
Across small_club + big_club runs the skip fired ONLY on `movie.tex` (447–706x), again
zero sky/backdrop. The predicate never touches a painted RT.

**Code audit of the predicate** (`Rnd_Wgpu_RB3.cpp:3346` `if (!hasTex && diffuse &&
diffuse->IsRenderTarget())`):
- A **painted RT** (sky-dome) is created by `BeginDrawTarget` (L1906) which sets
  `e.uploaded=true` + a valid `e.view`. `GetRB3TexView` (L814) returns the view iff
  `uploaded` → `hasTex==true` → the `!hasTex` guard FAILS → **NOT skipped**. Robust:
  any RT that is ever a camera `TargetTex()` gets a view before its material samples it.
- An **unpainted movie RT** (`crowd_mass.tex`) never goes through `BeginDrawTarget`, has
  no `sTexGpu` entry and no CPU pixels → `UploadRndTexIfNeeded` returns `{}` →
  `hasTex==false`; `IsRenderTarget()` true (`mType & kRendered`) → **skipped**.
- A **null / non-RT diffuse** (solid-color UI rects) → `IsRenderTarget()` false (or null)
  → falls to `mWhiteView` exactly as before. **Unchanged.**
- Note: `IsRenderTarget()` = `mType & kRendered(2)` also matches `kRenderedNoZ(0x22)` /
  `kDepthVolumeMap(0xA2)`, but that only widens the RT-typed set; the real protection is
  the `!hasTex` term, so any painted variant is still safe.

**`movie.tex` skip is a true no-op.** Pre-fix small_club `coop_all_n00` rendered luma 27
white% 0.4 (NOT white) — so `movie.tex`, though it hit the white fallback texture, was
drawn with alpha=0 modulation and was already invisible everywhere. The festival
screenmask was white because its `mColor` is opaque-white (alpha=1); `movie.tex` is
alpha=0. Skipping the alpha-0 one changes nothing. Club before/after both render the
full venue (interior, band, props) — no missing sky/backdrop.

**Direct-crowd shots** (`coop_dir_crowd00`/`crowdb`, no screenmask): they have NO RT
diffuse, so the skip provably cannot touch them (trace-confirmed: never skipped). Their
luma differs between boots (66→117 in the impl doc's cross-boot A/B, 70 vs 98 in mine) —
this is per-boot band-pose + comic-poster dynamic-contrast variance, NOT the fix. The
impl doc correctly attributes it to per-boot flicker; the code path confirms it.

**Wide venue matrix** (small_club / big_club club before/after, arena_02): no new black,
no missing sky, venue geometry intact. (arena_01 CRASHES per task; not tested.)

## 3. Gate proof — PASS

`RB3_SCREENMASK_FALLBACK_OFF=1` restores the white blit (208 / white% 82 on the SAME
binary) → the fix is real and reversible.

## 4. DC3-safety — PASS (by construction)

`CMakeLists.txt:363-366`: DC3 backend compiles `Rnd_Wgpu.cpp`; only the RB3 backend
compiles the edited `Rnd_Wgpu_RB3.cpp`. No shared header touched (`IsRenderTarget` is an
existing inline on the shared `RndTex`). No `#ifdef` needed.

---

## NOTES (why LAND_WITH_NOTES, not a clean LAND)

1. **Result is a BLACK background, not the crowd.** With the fix ON the screenmask shots
   are luma ~11–15 with ~80% **black** — the gameplay (highway/HUD/band) renders, but the
   background behind that camera framing is a black void (the crowd backdrop WAS the movie,
   which still does not play on native = Option B, explicitly deferred). This is Option A's
   stated tradeoff: "jarring full-white → acceptable band-through-world." It is a clear net
   improvement (a black background is far less jarring than a full-screen white flash) and
   all must-not-break gates pass — but it is NOT the intended animated crowd backdrop, so
   the PASS gate's "crowd VISIBLE" is only partially met (gameplay visible; crowd absent).

2. **Impl-doc luma claim is overstated.** `screenmask-impl.md` claims the after-luma "drops
   into the venue's normal 60–80 range." My independent re-measure shows luma **~11–15**
   (black-dominated, black% ~80), NOT 60–80. The festival screenmask camera framing simply
   has little world geometry behind it. Recommend correcting that one line in the impl doc;
   it does not change the verdict.

3. **Do NOT close GAP 4.** Option B (native in-world `TexMovie`/Bink decoder) remains the
   faithful path. Keep GAP 4 open, downgraded to "Option-A landed; Option-B deferred."

## LANDING

- **Engine SHA to push:** `998b87340438dfa8c993ef8b46fffb0c725f5da1` → engine `master`.
- **Pin bump:** set `MILO_ENGINE_PIN` in `rb3/native/CMakeLists.txt` to that SHA in the
  matching rb3 commit.
- Coordinator consolidates; this agent did NOT push / bump master / touch master.
