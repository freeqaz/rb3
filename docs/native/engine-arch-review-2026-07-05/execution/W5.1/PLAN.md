# W5.1 (Lane C, Wave 8) — Venue black poster quads — PLAN

**Item:** SYS-5 family, from the Wave-6 W4.1 backlog handoff. Solid-black poster/decal
quads in the venue/backdrop set-dressing (reference: `/tmp/wave6-current-state/partdiff_default.png`,
the "Silvio's RESTAURANT BAR & GRILLE" cork-board with a black central "SHOW … ALL AGES" poster).

**Fence (WAVE8_KICKOFF Lane C + coordinator brief):** rb3 game/asset code only.
FORBIDDEN: `BandCharacter.cpp`, `RB3MaterialBinder.cpp`, `Rnd_Wgpu_RB3.cpp`, `RB3PostProc.*`,
all `../milo-native-engine/**` render/gfx files.

## Subtasks

- **C.S1 (probe census FIRST, Opus)** — per WAVE8_REVIEW A6, run the ZERO-NEW-CODE
  discriminator before any new diagnosis: boot to the affected scene(s) with
  `RB3_HEADMAT_DBG=1` (the W2.7 probe, `RB3MaterialBinder.cpp:230`) and census the black
  quads: null-diffuse (W2.7 family) vs authored-black vs alpha/bind issue.
  Deliver census table → family verdict → either (a) C.S2 fix design inside the fence, or
  (b) staged-patch/backlog with diagnosis. **This document + STATUS.md = the C.S1 deliverable.**

## Build / harness

- Build: `native/build-agent-CS1/rb3-native` (engine pin `a94762f`, incremental rebuild verified).
- Harness: `census/census.py` (part_difficulty settle census), `census/vignette.py`
  (catches the `tv3_a` transition vignette live during gameplay-load), both headless
  `RB3_HTTP=1 RB3_FIXED_CLOCK=1 RB3_HEADMAT_DBG=1`, `RB3_PP_LUMA_CEILING` unset (A7).

## Exit criteria (A6)

Probe census table (quad → material → diffuse state) + family verdict + fail-red-free
handoff or backlog. No source changed in C.S1 (pure probe).
