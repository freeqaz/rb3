# Session 2026-06-02 (cont.) — Track-A gameplay-lighting POLISH pass

Follows `SESSION_2026-06-02_track-A-glow-landed.md` (dark highway + emissive + lit
lanes + venue-environ fix). This pass took the four deferred polish follow-ups —
gem bloom-halo, SP blue overlay, lane blue-tint, venue lighting — through a
parallel design workflow → probes → implementation → adversarial verification.
ultracode (multi-agent orchestration), Opus-led, main-loop image adjudication.

## Outcomes

| Item | Outcome | Where |
|---|---|---|
| **P3 — Lane blue-tint** | ✅ **SHIPPED (default-on).** | engine `71f21d0` |
| **P2 — Star-power blue track overlay** | ✅ **SHIPPED (default-on).** Was a capture artifact (works at 4× streak) + brightened. | engine `71f21d0` |
| **P4 — Venue point-light lighting** | 🟡 **SHIPPED gated default-OFF (opt-in `RB3_VENUE_LIGHT=1`).** Works, but washed/desaturated by default. | engine `71f21d0` |
| **P1 — Gem bloom-halo** | ⛔ **DEFERRED** — the implemented approach washes the highway. | engine branch `trackA-bloom` `332dfba5` (not merged) |

rb3 pin bumped to `71f21d0` (rb3 `a8e0d222`).

## P3 — Lane blue-tint (shipped)
The lane dividers (`rails.mat`) were forced-prelit + flat ×0.7 white. Measured retail
dividers (read the retail PNGs) ≈ normalized `(0.58, 0.70, 1.00)` cool blue-white →
replaced the flat ×0.7 with a per-channel tint **×(0.53, 0.64, 0.92)** (mean ~0.7, so
same brightness, cooler hue). Scoped `game.cam` + `rails.mat`. Low-risk, verified.

## P2 — SP blue track overlay (shipped)
`peakstate_plane` (the SP "track lights up blue with filigree" overlay,
`spotlight_*_track.tex`) read `alpha=0` in the earlier short captures → looked
"missing." **It was a capture artifact:** `GemTrackDir::PeakState(true)` fires + the
PropAnim fades the plane's alpha in **once the streak hits 4×** (probe-confirmed:
`PeakState b1=1`, alpha climbing 0.00→0.36+). It just never triggered in <10 s
autohit captures. The overlay rendered faint (gray base × blue diffuse, alpha-blend),
so a **×2.0 color boost** (scoped `game.cam`, `strstr "peakstate"`) makes the blue
read as a vivid glow — pixel-confirmed the strongest of the three wins, no blowout.
**Lesson (again):** verify streak/SP-gated HUD at a HIGH game state (≥4× streak / SP
deployed), not a short capture — same class as the A3 multiplier/5-star artifact.

## P4 — Venue point-light lighting (shipped opt-in, default-off)
Under `world.cam` only, read `RndEnviron::sCurrent`'s real lights into `SceneUniforms`
(the shader already implements `computePointLight` range falloff). The venue's real
lighting is **point-light-dominated**: a `rim.lit` directional (0.49) + four
`*_silhouette.lit` **point** lights (intensity 1.45, range ~38, the stage spots) +
colored theater accents — spread across a multi-environ rig.
- **Correcting the verification's headline finding:** a code reviewer claimed the
  point branch was *dead* (kPoint routed to `mLightsReal`, P4 reads only
  `mLightsApprox`). The **runtime probe directly refutes that** — the type-0 silhouette
  points ARE in `mLightsApprox` at gameplay (`numApprox=6`, `numReal=0`). Static
  analysis of `AddLight` ≠ the loaded/deserialized state. The points DO engage.
- **Why it's still default-off:** the parity reviewer pixel-measured the wall-dominated
  backdrop as washed/desaturated-grey (sat ~0.02) — the WHITE stage lights + a
  knocked-down ambient light the *band* locally but leave the far wall flat-grey, and
  the ambient pull-down dims the band/crowd venue-wide. It's a stylistic change, not a
  clear default win. Shipped as an **opt-in foundation**; tuning to match retail needs
  light-colour preservation + lower exposure (+ optionally also read `mLightsReal`).
- Safe: no crash/UB, value-init uniforms, byte-identical default else-branch, `game.cam`
  (the track look) + menu cams untouched.

## P1 — Gem bloom-halo (DEFERRED — washes the highway)
Approach (agent, engine branch `trackA-bloom` `332dfba5`): redirect the post-grade
highway/gems/HUD draw into a dedicated **sampleable** target (the framebuffer isn't
sampleable), run a 2nd `BloomPass`, composite back over the graded venue
(premultiplied-OVER for the base + ADDITIVE for the halo). Builds clean, gem/now-bar
halo visibly works, gated `RB3_HIGHWAY_BLOOM`. **But it washes the dark highway** —
confirmed *definitively* by a `RB3_HIGHWAY_BLOOM_BLEND=0` capture (over-blit only, no
halo): the track blows out bright, gems lost. The highway is **semi-transparent**
(`surface.mat` SrcAlpha, mixed additive `gem_smasher_glow`), so re-compositing the
whole highway layer over the bright graded venue lets the venue bleed through →
destroys the E1 dark-track contrast. Not a tuning issue (tight thresh/blend washes
too). **Correct redesign:** additive-halo-ONLY — re-render *just* the bright
bloom-source meshes (gems/now-bar) into the buffer and composite ONLY the additive
halo, leaving the base highway untouched on the framebuffer. Infra preserved on the
branch. (Also a real catch in that work: a 2nd `BloomPass` needs its own `Init()` —
`Run()` doesn't create `mDefaultSampler` → otherwise the whole frame is discarded.)

## Method (ultracode)
Parallel design workflow (4 Opus agents → specs + probe-specs) → main-loop runtime
probes (the two go/no-go's: how to trigger SP-deploy = just play to 4× streak; is
`RndEnviron::sCurrent` usable = yes, has real lights) → linear implementation (shared
engine region) with P1 dispatched to a parallel isolated-worktree agent → combined
adversarial verification workflow (regression / parity / code-scoping + synth) →
land. **Probe-first beat static analysis twice this pass** (the SP "missing" artifact;
the "dead point-light branch" claim). Pixel-measuring reviewers > eyeballing for the
wash/saturation calls.
