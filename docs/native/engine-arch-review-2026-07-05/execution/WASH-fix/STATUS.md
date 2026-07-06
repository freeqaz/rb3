# WASH-fix — Stage A.S1 STATUS (Wave 8 two-hypothesis venue-wash instrumentation)

KEY=WASH-fix. Checkpoint id `A-S1`. Opus root-causer. Engine pin `a94762f`
(engine HEAD at run time; probe committed `71469af`). rb3 master.
Binary: `native/build-agent-WASH-fix/rb3-native` (clang/Debug, engine `71469af`,
`RB3_PP_LUMA_CEILING` UNSET in every arm per WAVE8_REVIEW A7). `RB3_FIXED_CLOCK=1`.

## 2026-07-06 — S1 instrumentation + A7 reproduction — DONE (probe-only, NO fix)

### Headline verdicts
- **H1 (PINK @ ms21000 = engagement-miss) — REFUTED.** 0/8 default boots had a P4
  engagement miss; the PINK boots are **fully engaged with byte-identical lighting
  inputs to the NEARBLACK boots**. The ms21000 PINK wash is generated *downstream*
  of the venue-light engagement machine, which is deterministic and exonerated.
- **H2 (GREY = composite desaturation of hot venue input) — CONFIRMED (mechanism),
  REFINED (timing).** default (composite ON) collapses mid-band saturation to
  grey (0.026–0.079) where `RB3_PP_OFF` (composite OFF) stays colored (0.46–0.75) —
  the composite is the desaturating agent, on a UNORM intermediate. But the grey is
  **director-shot-dependent, NOT pinned to ms3000**: on this pin the ms3000 wide shot
  is authored-pink colored; the grey manifested at ms6000–9000 on the character
  close-up cuts.
- **Net for S2:** BOTH symptoms live **downstream of the engagement path, in the
  composite / scene-render stage** — not in the `:1435`/`:1560`/`:2453` env-state
  decision points. The coordinator's matrix mechanism ("engagement stochastically
  masks a pink base") is *mechanically wrong* for the default build (see A7 below).

---

### A7 — matrix baseline reproduced on pin `a94762f`
Reduced arms, `RB3_PP_LUMA_CEILING` UNSET, ms21000±250, `wash_probe_run.py`:

| arm | N | wash (PINK/WHITE) | classes | engagement |
|---|---|---|---|---|
| default | 8 | **2/8** | PINK 2, NEARBLACK 6 | engaged 8/8 |
| venue_light_off (control) | 4 | **4/4** | PINK 4 | miss=venue_off 4/4 (deterministic) |

The 8/8-vs-low matrix signal **holds** (venue_light_off deterministically washes;
default stochastically ~25%). `pp` fact: `RGBA8Unorm, unorm=1` on every boot.

**Reinterpretation of the matrix (important).** The Wave-7 matrix named the mechanism
as "the P4 venue-light rewrite stochastically masking a pink base." The per-boot
instrumentation shows this is *not the mechanism in the default build*: the venue-light
path **engages on 8/8 default boots** and its lighting output is **byte-identical** on
PINK and NEARBLACK boots (below). `venue_light_off` washes 4/4 not because it "reveals
a masked pink base" but because it *replaces* the (mostly dark, retail-faithful) engaged
venue lighting with the flat-default flood (**1 white directional + 0.45 grey ambient**,
`Rnd_Wgpu_RB3.cpp:1576-1579`), which is uniformly bright and over-exposes the whole venue
through the composite **every** boot. The engaged path is dark → NEARBLACK; the flat
default is bright → always washes. The stochastic default PINK is a *separate* downstream
effect (see H1).

---

### H1 evidence — engagement-miss vs PINK, ms21000, 8 default boots + 4 control

`measure/probe_ms21000_default.json`, `measure/probe_ms21000_vlo.json`. `engaged`
and `greykey` parsed from the tail (last 40 world.cam `SCENE` digests = capture frame).

| boot | arm | class | mean_luma | engaged | miss | greykey(tail) |
|---|---|---|---|---|---|---|
| d1 | default | NEARBLACK | 0.090 | 1 | engaged | 6 |
| d2 | default | NEARBLACK | 0.089 | 1 | engaged | 6 |
| **d3** | default | **PINK** | 0.590 | **1** | engaged | 6 |
| d4 | default | NEARBLACK | 0.096 | 1 | engaged | 0 |
| **d5** | default | **PINK** | 0.216 | **1** | engaged | 0 |
| d6 | default | NEARBLACK | 0.095 | 1 | engaged | 0 |
| d7 | default | NEARBLACK | 0.112 | 1 | engaged | 0 |
| d8 | default | NEARBLACK | 0.105 | 1 | engaged | 0 |
| v1–v4 | venue_light_off | PINK ×4 | 0.51–0.60 | **0** | venue_off | 0 |

**Engagement is 8/8 for default; 0/8 misses.** No `fogowner_null`, no `no_env`, no
staleness rewrite absence. The two PINK boots (d3, d5) are engaged.

**Lighting inputs are identical in PINK vs NEARBLACK.** Per-env `dl/pl/greykey` (tail):
the boots split into two *lighting clusters* (a boot-nondeterminism orthogonal to wash):
- cluster A (d1,d2,**d3**): `chars.env dl=1/pl=0`, `crowd.env dl=1/pl=0/greykey=6` → **1 PINK / 2 total**
- cluster B (d4,**d5**,d6,d7,d8): `chars.env dl=0/pl=1`, `crowd.env dl=0/pl=1/greykey=0` → **1 PINK / 5 total**

PINK boot d3's lighting == NEARBLACK boots d1,d2. PINK boot d5's lighting == NEARBLACK
boots d4,d6,d8. The grey-key fallback (`:1560`) fires in cluster A regardless of class,
so it does **not** correlate with the wash either.

**Visual (identical geometry + lighting, different output):**
- NEARBLACK d1 (`measure`/raws `/tmp/washfix-h1/…_01_t1.png`) = correct warm stage-lit
  retail venue (dark backdrop, warm band, highway pops).
- PINK d3 (`…_03_t3.png`) = same scene/pose flooded with a **screen-space magenta tint
  that preserves luminance structure** (CORK sign, wood grain, band shading visible
  *through* the pink; note-grid overlay also tinted; the game.cam highway/gems/HUD are
  unaffected).

**H1 conclusion:** the ms21000 PINK wash is **not** an engagement/staleness miss and
**not** the flat-default pink base. It is a boot-nondeterministic, screen-space,
structure-preserving magenta flood over the world.cam render. Its structure-preserving,
whole-frame, world.cam-only reach is most consistent with a **boot-nondeterministic
composite grade/tint** (`RndPostProc::Current()` selection at the venue-cam shot →
`vignetteColor`/`bloomColor`/saturation term — Lane-A composite, `RB3PostProc`/WGSL),
and secondarily with an **async-residency bloom-source** feeding the composite; it is
**inconsistent** with per-surface missing textures (those would be flat unlit magenta on
specific meshes, not a structure-preserving screen tint). **S2 discriminator (cheap):**
capture default vs `RB3_PP_OFF` on PINK-prone boots — if PINK survives PP_OFF it is in
the scene render (residency), if it vanishes it is the composite grade. This axis was
NOT settled here (PINK is only ~25% stochastic; a targeted many-boot PP_OFF A/B is an
S2 task).

---

### H2 evidence — composite desaturation of hot input (ms-sweep, director ON)

`W3.3/grayscale-sweep.py` (default / pp_off / venue_light_off, `RB3_WASH_PROBE=1`),
per-tonal-band venue-crop saturation (`tonal_band_sat.py`, `measure/probe_ms3000_*`):

| songMs | default mid_sat | default mean_val | pp_off mid_sat | pp_off mean_val |
|---|---|---|---|---|
| 2000 | 0.287 | 0.383 | 0.320 | 0.438 |
| 3000 | 0.330 | 0.259 | 0.393 | 0.275 |
| 4000 | 0.352 | 0.266 | 0.336 | 0.322 |
| 6000 | **0.079** | 0.426 | 0.459 | 0.322 |
| 9000 | **0.026** | 0.103 | **0.746** | 0.144 |

- **ms3000 is COLORED on this pin** (default mid_sat 0.330; the wide shot is authored
  pink/purple stage-lit — `default_ms03000.png`). The "grey at ms3000" of W3.3/A.S2 did
  **not** reproduce at ms3000 here — the director cut differs (RNG per boot).
- The **composite desaturation IS present** where the grey occurs (ms6000/9000): default
  collapses to mid_sat 0.026–0.079 while `pp_off` (composite OFF) stays 0.46–0.75 — a
  0.4–0.7 gap, far beyond shot-to-shot noise. `pp_off_ms09000.png` renders the same
  close-up warmly colored; `default_ms09000.png` renders it grey.
- Intermediate is UNORM (`RB3PostProc.cpp:155`, `[WASHPROBE] PP … RGBA8Unorm unorm=1`),
  so hot >1.0 venue/char lighting is clamped at the intermediate write before the grade.

**H2 conclusion:** the mechanism (composite grade over a UNORM intermediate desaturates
hot input → grey; removing the composite restores color) is **confirmed** and corroborates
W3.3 D.S2 + Wave-7 A.S2's matched-frame result. The **refinement** is that the grey is
**director-shot-dependent, not a deterministic ms3000 property** — it tracks whichever cut
feeds the composite a hot input (here the char close-ups at ms6000–9000). A song-start gate
pinned strictly to ms3000 (kickoff gate b) is therefore fragile on this pin; S2's grey gate
should target the *composite-desat delta* (default vs pp_off mid_sat) on a **director-pinned
shot**, not a fixed songMs.

---

### Decision-path writeup (file:line, `Rnd_Wgpu_RB3.cpp` @ `71469af` / `RB3PostProc.cpp`)

1. **Engagement — `:1435`** `sVenueLightEnabled() && camNm=="world.cam" && venv && venv->mAmbientFogOwner`.
   Measured **TRUE on 8/8 default boots** at the ms21000 shot; the silent
   `mAmbientFogOwner==null` clause never fired. **EXONERATED for the PINK wash.**
2. **Flat-default else — `:1575-1580`** (1 white dir + 0.45 grey ambient). Reached only by
   `venue_light_off` (miss=venue_off, 4/4). This IS the bright base that always washes —
   but it is **not** entered by the default build, so it is not the default PINK's cause.
3. **Grey-key no-lights fallback — `:1560`** (`dl==0 && pl==0`). Fires in cluster-A boots
   (crowd.env) **regardless of PINK/NEARBLACK** → does not correlate with the wash. (Also
   independently refuted for the grey by Wave-6 under `RB3_PP_OFF`.)
4. **DrawMesh env-staleness — `:2453`** pointer-equality rewrite. `STALE rewrite=env` lines
   present on the venue frames; no pointer-equality staleness miss observed (no env-name
   change without a rewrite). **EXONERATED.**
5. **Composite intermediate — `RB3PostProc.cpp:155`** `td.format = mTargetFmt` = RGBA8/BGRA8
   **UNORM**. Confirmed per boot. This is the H2 clamp point (hot >1.0 clamped at write) and
   the leading candidate stage for BOTH symptoms (grey = desaturating grade on hot input;
   pink = a boot-nondeterministic tint/grade — S2 to localize the sub-term via
   `RndPostProc::Current()` grade params + an intermediate readback).

### Instrumentation (committed engine `71469af`, probe-only, default-OFF)
`RB3_WASH_PROBE=1` → `[WASHPROBE] SCENE …` (engagement/miss/dl/pl/greykey per world.cam
write), `[WASHPROBE] STALE rewrite=env …` (DrawMesh staleness), `[WASHPROBE] PP intermediate
… (unorm=..)`. Registered probe flag in `NativeCompatFlags.classification.json`.

### Gates
- **flag-OFF byte-identical:** `drawlog-golden.py --fixed-clock --canonical-order` (probe
  UNSET) → **PASS, 792 draws** (268 known bounded eye-jitter residuals, non-blocking). The
  probe is behavior-neutral.
- **DC3 zero-blast:** satisfied by construction — no edit to `UniformStructs.h` /
  `standard_wgsl.inc` (probe touches only `Rnd_Wgpu_RB3.cpp` + `RB3PostProc.cpp` +
  `classification.json`).
- `milo-engine-tests` full suite not re-run for a probe-only change; flag-OFF byte-identity
  is the behavioral proof.

### Reproduce
```
# H1 + A7 (ms21000):
RB3_WASH_PROBE=1 python3 docs/native/engine-arch-review-2026-07-05/execution/WASH-fix/wash_probe_run.py \
  --bin native/build-agent-WASH-fix/rb3-native --songms 21000 --arms default --n 8 \
  --out .../WASH-fix/measure --tag ms21000_default
# H1 control / fail-red:            --arms venue_light_off --n 4 --tag ms21000_vlo
# H2 (ms-sweep, director ON):
RB3_WASH_PROBE=1 python3 .../W3.3/grayscale-sweep.py --config default  --bin <bin> --out /tmp/washfix-h2/default
RB3_WASH_PROBE=1 python3 .../W3.3/grayscale-sweep.py --config pp_off   --bin <bin> --out /tmp/washfix-h2/pp_off
python3 .../WASH-fix/tonal_band_sat.py /tmp/washfix-h2/{default,pp_off}/*_ms0{3,6,9}000.png
```

### Handoff to S2
- Fix the **composite**, not the engagement machine (both symptoms are downstream).
  H2/grey: luminance/chroma-preserving grade on the UNORM-clamped hot input (the W3.3-fix /
  `RB3_PP_LUMA_CEILING` direction, extended below the highlight knee into the mid-tones —
  A.S2 already showed the ceiling only touches L>0.82).
- H1/pink: first run the **PP_OFF-on-PINK-boots discriminator** to split composite-grade vs
  async-residency. If composite: localize the boot-nondeterministic term
  (`RndPostProc::Current()` `vignetteColor`/`bloomColor`/saturation at the venue-cam shot).
  If residency: it is **out of Lane A's fence** (loader/ThreadCall) — file with attribution.
- Gate the grey fix on the **default-vs-pp_off mid_sat delta on a director-PINNED shot**,
  not a fixed ms3000 (grey is shot-dependent on this pin). Keep the deterministic
  `venue_light_off` PINK 4/4 as the fail-red control.
