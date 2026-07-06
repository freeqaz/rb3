# W3.2b — Point-light escalate-or-drop DECISION MEMO

**Stage:** D.S1 (checkpoint `D-S1`). **Author:** Opus (plan-only). **Date:** 2026-07-06.
**Lane:** D (Wave 7). **Scope (WAVE7_REVIEW A9):** decide from **existing evidence only** — the
repo's static ground-truth shots, W3.2's own venue-probe + A/B captures, and the Wii
`BoxMap.cpp`/`Env.cpp` source semantics. **No Dolphin harness built** (none exists; A9).

---

## RECOMMENDATION — **(iii) DROP** the box-ambient cube; keep per-pixel Lambert as the native model

Do **not** land the `RB3_BOX_AMBIENT` prototype (option i), and do **not** redesign around a
per-object point-light cube (option ii), **this wave or on current evidence**. Retain the
`wave6-boxmap-proto` branch as documented plumbing reference, leave `RB3_BOX_AMBIENT` registered
(`class=feature`, `not-live`) but never flipped, and **do not grow `SceneUniforms` 656→752**
(keeps DC3 blast at zero). Re-open only against a *measured* venue gap (trigger + capture request
in §5). The SYS-4 "flat-grey ambient" framing is **refuted by measurement**; the residual
point-light question is **real in mechanism but low visual impact, and per-pixel Lambert is
arguably the higher-fidelity model** — so there is nothing to escalate today.

---

## 1. The evidence chain (all pre-existing; nothing newly captured)

### E1 — Measured venue lighting is all point-spots, zero directionals (W3.2.S2 `RB3_VENUE_PROBE`)
Across **23 boot-reachable environs**, `mLightsApprox` = **27 point (type 0) + 1 fakespot (type 2)
+ ZERO directional (type 1)**. The Wave-6 prototype builds its cube from the **directional/fakespot**
queue only (`ApplyQueuedLights(cube, viewPos=null)`), so on real RB3 data the cube **collects
essentially no energy** and degenerates to the base ambient. The premise SYS-4 was framed on
("approx-as-Lambert routing inversion floods a flat grey ambient") **barely bites**: the current
code already routes those **point** lights to the **per-pixel Lambert** path.

### E2 — The A/B montage shows ON = a warm *over-brightening deviation*, not a fidelity gain
`captures/boxambient_off_vs_on.png` (`coop_dir_crowd` wide shot, songMs≈21.4k, 2×OFF + 2×ON):
- **Numerically** ON sits **inside the OFF A/A variance band** (mean luma OFF 20.7–24.7, ON 22.1–22.6;
  no blow-out, no pink wash — `captures/scores.json`).
- **Visually** the ON arm warms/brightens the band members (gold/orange cast, direction-shaped) vs
  the flatter reddish OFF arm. This is **not** point-light fidelity — with zero directionals the cube
  is empty, so the ON change is the **base-ambient over-count**: the prototype fills the base ambient
  into every face, `boxAmbientSample ≈ base·(|Nx|+|Ny|+|Nz|)` = **1.0–1.73× flat ambient** (W3.2.S2
  "Known deviation"). It moves the picture, but in the **wrong** direction (see E3).

### E3 — RB3's venue art direction is deliberately dim / high-contrast silhouette (Dolphin ground truth)
The repo's Dolphin shots (`c8-ground-truth-2026-07-01/dolphin-shots/`):
- `gp_00.png`: club venue — the crowd is rendered in **near-total silhouette**, backdrop dim
  purple/warm. The band/crowd are *meant* to be dark.
- `gp_b07.png`: same venue family — the foreground guitarist is a **near-black silhouette** (only a
  warm rim on mouth/glasses) against a bright hazy backdrop. Intentional backlit high-contrast look.

The current **per-pixel Lambert path (montage OFF) already reproduces this dim/moody envelope.**
The box-ambient base-fill (E2) would **flatten the silhouette and wash the characters warmer** —
*away* from the ground truth, not toward it. No existing shot shows the native venue as
"flat-grey/under-lit" in a way the cube would correct.

### E4 — The user-visible band members are lit by their OWN char rig, not the venue approx lights
The band closeups (`face_guitarist_ambient.png`, `face_singer_rimlit.png`) and the C8 lighting work
(`5587ce0`/`7f603e17` real-key lighting) establish that foreground characters are lit by
`mLightsReal` real key lights, not venue `mLightsApprox`. The approx set primarily shades the
**backdrop + crowd** — which the art keeps dim (E3). So even a *perfect* box-ambient cube changes
little of what the user actually looks at.

### E5 — Mechanism: per-pixel Lambert is *higher* spatial fidelity than the Wii point→cube path
From `BoxMap.cpp` (ground truth): the Wii point-light path (`ApplyLight(LightParams_Point)`) reduces
each point light to a **per-object single-sample directional** (dir = normalize(pos − meshCenter),
distance-attenuated) and accumulates it into a **6-axis ambient cube** evaluated **once per mesh**
(`Mesh.cpp:1463-1480` caches `mBoxLightColorsCached[6]` at the mesh's sphere center). That is a
**low-frequency, single-sample-per-mesh, ambient-only** approximation with no per-pixel normal
variation and a soft (no-terminator) response.

Our native path evaluates the same point light **per pixel** with a real `N·L` + distance
attenuation. Per-pixel Lambert is **strictly more spatial detail** than a per-object ambient cube.
The only faithfulness deltas are: (a) Wii's is softer/lower-contrast (ambient `max(0,dot)²`
accumulation over 6 discretized axes) with no shadow terminator; (b) Wii's is constant across a
mesh. Neither is a *quality* deficit — (a) is a stylistic softness, (b) is a resolution *loss*. On
RB3's dim, silhouette-styled venues (E3) both differences are **sub-perceptual**. W3.2.S2 reached
the same conclusion: per-pixel Lambert for RB3's point-spots "is arguably *more* accurate," and
option (iii)/(b) is "defensible on current data."

### E6 — Landing the cube is NOT zero-cost: it crosses the DC3 shared-uniform contract
`static_assert(sizeof(SceneUniforms) == 656)` lives in the **shared** engine
`milo-native-engine/src/gfx/UniformStructs.h:56`, consumed by DC3 (its `Rnd_Wgpu.cpp:1685` uploads
`sizeof(SceneUniforms)`). Growing to **752** (`boxAmbient[6][4]`) forces DC3's `UniformStructs.h` +
WGSL struct to mirror the +96 field **in lockstep or DC3's build breaks** — a coordinator +
DC3-owner sequenced change (unlike W3.1a's zero-blast fog fill). So option (i)'s "harmless plumbing"
claim is **false**: it is a non-zero-blast shared-contract change *plus* the E2 over-count behavior
change, bought for a near-no-op on current venues.

---

## 2. Why NOT option (i) — "land the box-ambient prototype anyway"
- **Not harmless:** grows the shared `SceneUniforms` contract (E6, DC3 lockstep) **and** ships the
  E2 base-fill over-count (1.0–1.73×), a visible warming that pushes *away* from ground truth (E3).
- **Near-no-op on the only data we have:** zero directionals (E1) → the cube's whole reason to exist
  is idle. We'd pay the contract cost and a fidelity *regression* for no upside.
- Even after normalizing the base-fill (W3.2.S2 follow-up), the residual is a struct-growth for a
  cube that stays empty — i.e. cost with no benefit. **Reject.**

## 3. Why NOT option (ii) — "redesign around a per-object point-light cube"
This is the only path that would actually move RB3's point-spot venues, but it is the **wrong trade**
right now:
- **Expected value neutral-to-negative:** it would *replace* the higher-fidelity per-pixel Lambert
  (E5) with a lower-resolution per-object ambient cube, to be "faithful" to a soft approximation —
  on venues the art keeps dim/silhouette anyway (E3, E4). No existing shot shows per-pixel Lambert
  looking *worse* than ground truth, so there is no measured defect to buy down.
- **Blast radius (estimated, for the record):**
  - `Rnd_Wgpu_RB3.cpp`: per-**draw** box eval — the directional part is per-environ, but the point/spot
    part is position-dependent (`BoxMap.cpp:133-199` use `viewPos`), so it must run in `DrawMesh` with
    the object world position (available post-W1.6 via `RB3DrawContext`/`obj.world`). New per-frame
    CPU: `BoxMapLighting::ApplyLight` × 27 point lights × N draws.
  - `gfx/UniformStructs.h` + `gfx/standard_wgsl.inc`: struct growth (656→752 on `SceneUniforms`, **or**
    a per-object field on `ObjectUniforms` 128→224) + WGSL struct mirror + a `boxAmbientSample(N)`
    fragment path replacing the scalar ambient.
  - **DC3 lockstep** (E6): coordinator + DC3-owner sequenced; DC3 need not populate the field
    (zero-init + `enabled=0` keeps its scalar path) but the assert + WGSL must move.
  - **Unvalidated correctness (R1):** the per-vertex cube-sample weighting
    (`Σ max(0,N·axis)·cube[i]`) is the standard ambient-cube form but is **not** Dolphin-validated —
    and no Dolphin-driving harness exists (A9). Landing (ii) without that validation risks shipping a
    subtly-wrong shader.
- A large, multi-repo, coordinator-sequenced, per-frame-CPU change, gated on ground truth we don't
  have, to "fix" a gap per-pixel Lambert already covers. **Reject now.**

## 4. Why (iii) is the disciplined call
Per-pixel Lambert for RB3's point-spot venues is a **defensible native model** (E5), the current
output already matches the dim/silhouette ground truth (E3), the user-visible characters are lit by
their own rig anyway (E4), and the cube's premise (directional approx lighting) is **empirically
absent** (E1). Dropping costs nothing (the plumbing/branch/flag are preserved) and avoids a
shared-contract change + a fidelity regression for no measured benefit. This mirrors the campaign's
"honest-refutation" outcomes (W2.3): the hypothesis was tested against real data and did not hold.

**What Wave 8 implements: nothing for W3.2 unless §5's trigger fires.** Close W3.2 as
*refuted/dropped*. The reusable artifacts (uniform-plumbing pattern, `_w32-boxambient-ab.py`
scorer, the `wave6-boxmap-proto` branch) stay on the shelf. Fold the one genuinely-faithful
sub-behavior the prototype demonstrated — dropping the synthetic grey-key for no-light envs — into
the **W3.1b / venue-light** lane if/when it's revisited, *not* via the cube.

---

## 5. Re-open trigger + explicit human-capture request (if anyone insists on escalating)

**Re-open trigger:** a *measured* venue where the band members (wide, band-visible framing) are lit
**dominantly by venue `mLightsApprox` point-spots** (not their own char rig) and where native
per-pixel Lambert visibly diverges from Dolphin ground truth (over-contrasty terminator, missing
soft fill, or wrong color spread). Absent that, W3.2 stays dropped.

**Existing ground truth is insufficient to produce that measurement.** The shot inventory
(`gp_00`, `gp_b07`, `gp_contact`, `oye_01`, `oye_contact`, `reroll_a`) is **one venue family at
char-contact framing** + char closeups + menus — not a spread of point-spot-dominant venues at
band-visible framing with matched native probes. So escalation is **human-capture-gated**, and the
request is:

> **Human capture request (only if escalating to option ii):** 3 matched-frame Dolphin (retail Wii)
> venue captures, each paired with the native RB3 frame at the **same camera + songMs** and its
> `RB3_VENUE_PROBE` light dump, on venues chosen to differ in approx point-spot **color/placement**:
> (1) a warm-spot theater/bar, (2) a cool/blue-spot arena, (3) a mixed-color club — each framed
> **wide enough to show the band members lit by the venue** (not the existing char-contact
> closeups). Deliverables per venue: `dolphin_<venue>.png`, `native_lambert_<venue>.png`,
> `probe_<venue>.txt`. This lets per-pixel-Lambert-vs-cube be A/B'd on the *same* point-light set —
> the only way to decide whether the Wii per-object cube is visibly better, worse, or equal.

If that capture set comes back showing Lambert **equal or better** (the E5 prediction), W3.2 is
closed permanently. If it shows a real deficit on a specific venue, *then* option (ii) is scoped with
the §3 blast radius, R1 validated against those exact frames, and the DC3 lockstep coordinated.

---

## 6. One-line answer
**DROP.** SYS-4's "flat-grey ambient" is refuted (zero directionals; per-pixel Lambert already
matches the dim/silhouette ground truth); the box cube grows the DC3-shared uniform contract to ship
a near-no-op that warms characters the wrong way; per-object point-light cubes are *lower* fidelity
than what we render today. Keep per-pixel Lambert; preserve the prototype on the shelf; re-open only
on a human-captured, point-spot-dominant venue that measurably beats Lambert.
