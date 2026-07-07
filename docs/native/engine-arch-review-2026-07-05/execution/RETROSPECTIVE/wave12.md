# Wave 12 — Retrospective (hindsight review)

**Reviewer:** retrospective subagent (read-only). **Run:** `wf_c561d974-479`, 7 agents; C34
stalled → side-agent re-run `c7f101f7`. **Engine pin:** `146fd19` → `44716f4`. **Sources:**
`WAVE12_KICKOFF.md`, `WAVE12_REVIEW.md`, README Wave 11/12/13/14/15 sections, `W2.8g/STATUS.md`,
`SKEL/STATUS.md`, git log.

## What Wave 12 set out to do

Three lanes, all measurement/diagnosis-heavy:
- **Lane A (W0.3d-b):** name and (flag-first) fix the boot-varying `gRand` stream-position spread.
- **Lane B (W2.8g):** with palette+GPU exonerated in Wave 11, close the hands "SHELL axis"
  (authored-vertex→offset composition).
- **Lane C (W4.3):** four user-reported UI parity defects (U1 focused-text contrast, U2 sidebar
  devil-ratings/overlap/missing-panel, U3 flipped hold-labels, U4 ticker overlap).

## What it actually delivered

- **A.S1 ✅ good, held:** H-TIMING **refuted** (511 completions byte-identical, landed by frame 2;
  gdraw diverges from frame 4 with zero completions on the diverging frames). H-ORDER named:
  consumer-ORDER × variable-count **rejection samplers** (`mAnims` walk, `Rand::Gaussian` do-while,
  `CameraShot`/`Crowd` conditional draws), order sourced from the main↔ThreadCall glibc-arena race.
  This mechanism has **held through Wave 15** — never overturned. `--tol` 150ms lever + probe landed.
- **A.S2 ⚠️ honest partial:** reseed+serialize+mAnims-sort behind opt-in `RB3_LOAD_DETERMINISM`
  reduced spread ~62% but PRIMARY (10/10 identical stream position) FAILED because reseed fixes
  values not order. Correctly NOT default-flipped; staged design in STATUS. Still on the backlog
  Waves 13-15 — correctly, it's a large determinism seam.
- **B.S1 ✅ SPACE_AXIS, held:** Instrument B + rest-free discriminator showed sub-shells transport
  isometrically (iso≈0) → per-vertex weight/index/**decode REFUTED**. This finding survived Wave
  13's premise inversion intact.
- **B.S2 ❌ the wave's central error (see below).**
- **C1 ✅ diagnosed→escalate:** hub focused-text = compositing (grade lifts dark glyphs + focus bar
  composites through AA text). Correct FOR THE HUB → Wave 13 UIGRADE machinery → Wave 14 flip.
- **C2c ✅ NOT A BUG, held:** all-devil ratings are faithful equal-count bucketing; retail ref was a
  587-song DLC library vs the 83-song stock lib (apples-to-oranges). Held into Wave 13.
- **C3 ✅ NOT A BUG, held:** flipped hold-labels = faithful `InlineHelp` flip-card animation caught
  mid-transition. Held into Wave 13.

## What later waves refuted — and what would have caught it in Wave 12

### 1. B.S2's own/bound labels were SWAPPED — the whole SKEL re-lane was built on it (CRITICAL)

Wave 12 `W2.8g/STATUS.md:136-146` stated as settled fact:
> `own = Find(boneName)` = the **SHARED MAGNET** instance (invOff IDENTICAL 106° across members);
> `bound = mesh->BoneTransAt(b)` = the **PER-MEMBER static** bone.

On this it concluded "the 87° basis gap is **IRREDUCIBLE with any single live bone**" and re-laned
to **skeleton instancing** ("make `Find(name)` resolve the per-member animating bone instead of the
shared magnet") → the Wave-13 **SKEL** lane.

Wave 13 SKEL S.S1 **inverted the premise with runtime pointers**: `own=Find(name)` is **per-member
and ANIMATES** (4 distinct ptrs across members, 116 distinct own vs 42 shared bound, moves
186-276u/Poll, gender-posed male 109.5°/female 120.1°). `bound` is the SHARED static male-bind. The
Wave-12 dual-skin probe **sampled pre-rebind state with own/bound labelled backwards.** The re-lane
recommendation ("make Find(name) return the per-member bone") was chasing a state that *already
existed* — `own` was per-member and animating all along.

**Missing tool / one-run question:** a **runtime pointer-identity probe** — "for `own` and `bound`,
print the actual pointer address, its per-member distinctness, and whether it moves frame-to-frame."
One run answers "which channel is the per-member animating one." Wave 13 ran exactly this
(4-distinct-ptrs dump) and the premise flipped instantly. Wave 12 instead **inherited the label
convention from the probe's naming** and built a whole cross-wave re-lane on it. The pre-dispatch
review (Fable A5/A6) hardened the *instruments against gate-gaming* and pre-registered
space-vs-decode branches, but never demanded a pointer-identity check on the probe's own labels —
so the swapped-label risk passed the gate.

### 2. "Offset-bake class exhausted / 6th cell measured dead" was a CONFOUNDED (non-gender-split) measurement

Wave 12 declared the 6th single-live-bone cell (`RB3_HANDS_SHELL_FIX`) "measured dead" from a
**global** wext/Tier-2 regression (wext mean 68.9→82.4u UP, Tier-2 0.33→0.81u worse, flesh-spike
starburst) and pronounced the offset-bake class exhausted.

Wave 15 HANDS-ADJUDICATION: the "6th dead cell death certificate was **CONFOUNDED**" — the shared-B
bake killed **female/gloves/nails**, not the composition. Male-only in isolation was fine (Tier-1
3.1°, 0/1038 blocks >5°; male authored offsets match `bound`'s basis to **0.1°** across all 38
bones). The genuinely-correct, **never-measured** cell (keep AUTHORED per-mesh offsets + repoint to
`own` with `SetBone calcOffset=false`) was named only in Wave 15 → Wave 16.

**Missing tool / one-run question:** **gender-split + per-mesh isolation in the wext/Tier-2 gate.**
A global aggregate reads "male-PASS + female/gloves/nails-FAIL" as one red number and mislabels it
"class dead." A per-mesh/per-gender split would have shown male passing → pointing straight at the
shared-bind mismatch (the actual defect) instead of declaring exhaustion. Wave 15 explicitly
re-anchored every gate to "gender-split everything" and declared `wext>60` **not** a hands-shard
oracle (legit two-hand extents reach 104u) — both instruments Wave 12 trusted were later
partially invalidated.

### 3. C1's "compositing" diagnosis was hub-only; the focused-TEXT-color family had a second mechanism

Wave 12 filed U1 (hub + song-select row + partdiff GUITAR) as one compositing family. The hub half
was right (→ shipped Wave 14). But Wave 15 W4.4-ROWFIX found song_select/partdiff focused text stays
white because **the native RndText glyph shader IGNORES font-material color** (UILabel sets the dark
color, luma unchanged) — a real engine gap, a *different* mechanism than the hub's grade
compositing. Wave 12 lumped a two-mechanism family under one diagnosis. Not wrong, but under-split;
cost surfaced two waves later. Cheaper Wave-12 probe: a per-glyph "does font-material color reach and
change the rendered luma?" readback on the song-select row (Wave 15's actual discriminator).

## What the wave got RIGHT (with hindsight)

- A.S1's H-ORDER rejection-sampler mechanism — **held through Wave 15**, never overturned.
- Refusing to force-land A.S2 (PRIMARY-FAIL kept as documented partial) — correct discipline.
- Two clean NOT-A-BUG closures (C2c devil-ratings, C3 hold-labels) — both held into Wave 13.
- B.S1's isometric-transport / decode-refuted finding — survived the Wave-13 inversion.
- The **pre-dispatch review gate earned its keep in Lane A**: Fable caught THREE false premises
  before dispatch — the "staged-since-Wave-4 loader patch exists" phantom (3rd resurrection; A1),
  "SortDraws shipped default-ON" (A3), and the "insertion-order→consumption-order" mechanism (A2,
  rewritten to timing-vs-order, which A.S1 then resolved). Zero cost this wave for those three.

## Tooling gaps (concrete)

1. **Runtime pointer-identity probe for skinning bone channels.** Question answered in one run:
   "which of `own`/`bound` is per-member, and does it animate?" Campaign did instead: trusted the
   probe's label convention → built the SKEL re-lane → spent Wave 13 S.S1 inverting it with the dump
   that should have run in Wave 12. **~1 full lane-wave of misdirection.**
2. **Gender-split + per-mesh isolation baked into the wext/Tier-2 gates.** Question: "does this cell
   regress male, or only female/gloves/nails?" Campaign did instead: a global aggregate → "class
   exhausted" → SKEL (W13) + RESKIN (W14), both eventually REFUTED, before Wave 15's adjudication
   found the never-measured cell. This is the single largest wasted-effort driver attributable to a
   Wave-12 conclusion.
3. **Concurrent-agent process-kill isolation.** C34 stalled 6× — a `pkill -f rb3-native` from one
   agent killed a sibling's running instance. Cost a side-agent re-run; fixed after the fact with a
   pgid-only cleanup rule. A shared harness rule (kill by pgid, never by process-name match) would
   have prevented the stall entirely.
4. **Two-mechanism split in the UI focused-text family (minor).** A "does font-material color change
   rendered glyph luma?" readback would have separated the hub compositing bug from the RndText
   shader-color gap in Wave 12 instead of Wave 15.

## Wasted effort estimate

**MODERATE (Wave-12-attributable).** A.S1/A.S2 and all four C-items were efficient and honest. The
waste is concentrated in Lane B: the **swapped own/bound labels** produced a skeleton-instancing
re-lane recommendation that Wave 13 had to fully invert, and the **non-gender-split "class
exhausted" declaration** steered the campaign into the SKEL→RESKIN detour (two engine-lane waves,
both refuted) before Wave 15 named the simple cell. Some hands exploration was genuinely necessary
(the defect is subtle), but the swapped-label framing and the premature exhaustion verdict were pure
avoidable misdirection — both preventable by two probes (pointer-identity, gender-split) that cost
one run each.
