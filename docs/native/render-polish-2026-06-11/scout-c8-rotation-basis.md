# scout-c8-rotation-basis — C8 deep dive (recovered from the paused wave-3 agent)

> **Provenance.** The wave-3 C8 deep-dive agent (Fable) completed its investigation,
> implemented + measured a fix, and verified Wii byte-identity, but the workflow was
> paused before it wrote this doc or returned. This file is **reconstructed by the
> orchestrator** from the agent's transcript and its two committed branches:
> - rb3 fix: `wt-c8-deep-dive` @ `41ff9e97` — `fix(native char): capture rest pose in CHARACTER space, not world space (C8 root cause)` (1 file, +48/−2, `src/system/bandobj/BandCharacter.cpp`)
> - engine probe: `wt-c8-deep-dive` @ `6a324be` (milo-native-engine) — `diag(rnd-rb3): C8_PROBE per-slot bone attribution + vert bone-locality audit + smear attribution` (+183, `src/platform/Rnd_Wgpu_RB3.cpp`)
> Neither is landed. The fix is HX_NATIVE-gated and was measured to help; it still needs a composed-build rebuild + visual sign-off before landing (see VERIFICATION).

## 1. SYMPTOM

After the wave-2 reload-re-entrancy fix, band heads/hair/hands/torsos render and
persist, but a residual class of skinned garment meshes whose bind bone resolves to
the member's **own** skeleton (`own == bound`, e.g. `fingernails_resource`,
`eightholedocs_resource`, `bondagepants`, `thighboots`, `suitpants`, `gloves`,
`miniskirt`, `femrockboots`…) **smears across the screen** (200–460u extents) and is
then dropped by the engine V24 shard guard. A player sees partially-dressed band
members, strobing clothing, and occasional full-screen pale smears. Wave-3
`verify-char-render` (composed build, adversarial) measured **20–24 band garment
meshes guard-hidden or smeared per run** — the headline `char-render` residual.

The prior trail (`CHAR_SKINNING_DEFORM_INVESTIGATION.md`) called this a "C8
rotation-basis divergence" — the animated bone's rotation basis supposedly
sign-flips vs the basis the authored skin offsets were baked against.

## 2. ROOT CAUSE — it is a rest-bake **SPACE** error, not a rotation-basis sign flip

The deep dive **refuted** the rotation-basis-sign-flip framing. The native skinning
math runs the same decomp code as the Wii and the bone bases are fine. The defect is
that the native rebind captures the per-bone **rest pose in WORLD space**:

- `NativeCaptureRestPoseAfterDeform()` and `RebindHeadHandsAtRest()` stored
  `rest = own->WorldXfm()`, which **includes the band member's stage/venue
  PLACEMENT** (measured member root world ≈ x=−18.5, y=24; and `player1..player3`
  roots sit metres apart on the small_club stage).
- The skinned meshes' verts are authored in **MODEL space at the origin**. Raw
  locality audit (engine `C8_VERT` probe): a vert sits **5–9u** from its authored
  bind bone, but **27–60u (== |placement|)** from the world-space-baked rest.
- Baking `offset = inv(worldRest)` then makes every vert ride a **|placement|-length
  lever arm** as the bone rotates → displacement ≈ `R·sin(θ)` with `R ≈ |placement|`
  → the **200–460u smear** the V24 guard correctly refuses to draw.

Second, independent poison: after the mid-song reload **re-arm** (wave-2's
reload-re-entrancy), the Poll-time *first-resolve* capture path could snapshot a
bone **while a clip was playing** — measured: a **guitar-fret-hand pose** baked as
the "rest" for the fingernail bones. That is a *pose* error the space fix alone
cannot repair (the rest is simply wrong), so those bones must stay pending until a
clip-free capture is available.

### Evidence (engine `C8_PROBE`, gameplay, multiple members/outfits)
- **Vert→bind-bone locality** (the smoking gun): world-space bake = 27–60u from the
  authored bind bone; **character-space bake = 5–12u, far(>30u)=0/N** — i.e. exactly
  the raw authored locality. (`C8_VERT … locality: avg=5.0–11.5 max≤17.8 far(>30u)=0/…`)
- **Guard drops, full song, band (non-crowd) meshes:**
  - world-space (baseline): **25.2 drops/frame** (top: fingernails 24897, eightholedocs 21006, bondagepants 20298…)
  - character-space fix: **20.4 drops/frame** (−19%)
  - character-space fix **+ `RB3_NO_IK=1`**: **4.9 drops/frame** (−80%)
- The `RB3_NO_IK` A/B proves the **remaining** residual after the space fix is a
  **separate left-limb IK mispose class** (the top residual drops are boots/shoes/
  pants on `bone_L-*` chains: `thighboots`, `maleslipons2`, `wrestlingboots`,
  `suitpants`, `timberlandboots`), not the rest-bake bug.

## 3. FIX DESIGN (implemented on `wt-c8-deep-dive` @ `41ff9e97`, NOT landed)

`src/system/bandobj/BandCharacter.cpp`, all `#ifdef HX_NATIVE`:

1. **Capture rest in CHARACTER space.** New `NativeCharSpaceRestXfm(own)`: walk
   `TransParent()` to the trans-chain root (the member instance), and return
   `L_rest = world_rest · inv(rootWorld)`. Then `offset = meshWorld(=I) · inv(L_rest)`
   composes to `inv(authoredBindLocal) · L(t) · M(t)` — **placement-independent** and
   correct through animation. Bones rooted at the static magnet (root world ==
   identity) are unaffected (`rel == world` there). Applied at both capture sites
   (`NativeCaptureRestPoseAfterDeform` post-`SetDeformation`, and the
   `RebindHeadHandsAtRest` first-resolve path).
2. **Never capture while a clip plays.** In the first-resolve path, if
   `mDriver && mDriver->FirstPlaying()`, skip (mark the bone `pending`, reason
   `clipPlaying`) rather than snapshot a mid-clip/IK pose. Those meshes stay on the
   guard (status quo) until a clip-free capture happens — strictly no worse than today.

Risk: medium (the rebind machinery has now had three hardening rounds). The change
is additive + HX_NATIVE-gated; the Wii arm is untouched.

## 4. VERIFICATION (done by the agent) + what still gates landing

- **Wii match-neutral: CONFIRMED.** Rebuilt `BandCharacter.o` in-worktree;
  `.text / .rodata / .data / .sdata` SHA-256 all **IDENTICAL** to master
  (`.text f3c8da8b62aaf053` both sides).
- **Locality + drop-rate: measured** as in §2 (the fix's own `C8_VERT`/`SHARD_GUARD`
  probes).
- **Ran clean** through gameplay bursts (`~/tmp/rp-c8/r4`, `r5`) and song-select
  depth captures (`~/tmp/rp-c8/songsel`, 5 frames PASS) — no crash.
- **STILL NEEDED before landing (the agent was paused here):**
  1. Rebuild the **composed** master tree with the fix and do a side-by-side
     before/after **visual** burst (the agent verified metrics + byte-identity but
     never wrote its final visual judgment; confirm garments are actually visible,
     not just less-dropped).
  2. Decide the IK residual: the space fix gets band drops 25.2→20.4/frame; the
     remaining ~20/frame is the **left-limb IK mispose** class (`RB3_NO_IK` → 4.9).
     That is a **separate follow-up** (see below) — the C8 fix should land on its own
     merit (it strictly reduces drops and is byte-identical), with the IK class
     tracked as the next char task.

## 5. FOLLOW-UP UNCOVERED: left-limb IK mispose (new, separate from C8)

`RB3_NO_IK=1` drops the post-fix band guard-drop rate from 20.4 to **4.9/frame**, so
**most of the remaining smear is IK**, concentrated on `bone_L-*` (left thigh/knee/
ankle) chains for footwear/legwear. Candidate causes to chase next: the native
foot-plant/IK effector applying a bad transform to the left limb (cf. the DC3
"feet-in-floor" IK trail — possibly the same engine IK-apply class), or an
L/R-handedness error in the IK solver's target on native. Probes are in place
(engine `C8_PROBE` per-slot `bone_L-*` attribution, `RB3_NO_IK` A/B).

## 6. REFERENCE SCREENSHOTS NEEDED

- CR-1/CR-2 (already on the campaign list): retail band-member closeup + venue
  walk-in — to set the visual acceptance bar for "fully dressed + correct legs."
- A retail close-up of a band member's **lower legs/footwear during animation** would
  directly calibrate the IK follow-up (§5).
