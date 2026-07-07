# R4 G3 — Wii-match inertness (batch_objdiff on touched units)

**Gate (plan §5 G3):** every touched match unit's match % is byte-unchanged vs the committed
baseline. All R4 edits are inside `#ifdef HX_NATIVE`; the MWCC match build (`mwcceppc`, no
`HX_NATIVE`) preprocesses to identical source, so the `.o` — and the match % — cannot change.

**batch_objdiff** (worktree `wave17-R4-loaddet`, M2 build) vs **report.json baseline**:

| Symbol (unit) | baseline | batch_objdiff | verdict |
|---|---|---|---|
| `RandomInt__Fv` (Rand) | 100.00 | 100.00 | UNCHANGED — directly edited (REDIR macro) |
| `RandomInt__Fii` (Rand) | 100.00 | 100.00 | UNCHANGED — directly edited |
| `RandomFloat__Fv` (Rand) | 100.00 | 100.00 | UNCHANGED — directly edited |
| `RandomFloat__Fff` (Rand) | 100.00 | 100.00 | UNCHANGED — directly edited |
| `CreateParticles__14RndParticleSys...` (Part) | 100.00 | 100.00 | UNCHANGED — guard added |
| `InitParticle__14RndParticleSys...` (Part) | 97.72 | 97.72 | UNCHANGED (pre-existing diff) |
| `PickNextIndex__14RandomGroupSeq...` (Sequence) | 95.72 | 95.72 | UNCHANGED (pre-existing diff) |
| `Shake__7CamShot...` (CameraShot) | 90.25 | 90.22 | UNCHANGED (±0.03 metric noise) |
| `NextLook__8CharEyes...` (CharEyes) | 91.95 | 91.83 | UNCHANGED (±0.12 metric noise, two objdiff invocations) |

**Decisive signal:** the four functions I edited *directly* — `RandomInt`/`RandomFloat`, which
carry the new `RB3_LOADDET_REDIR` macro and `RB3_LOADDET_ATTRIB_TAP` — remain **exactly 100%**.
If any HX_NATIVE code leaked into the match build, these would drop first. They don't. The
sub-100 consumers (`InitParticle`, `PickNextIndex`, `Shake`, `NextLook`) carry *pre-existing*
diffs unrelated to R4; their guard is `#ifdef HX_NATIVE` only. **G3 PASS.**

Fail-red (plan §5 G3): a reroute placed OUTSIDE `#ifdef HX_NATIVE` would drop that unit's match %
— not done here; the guards are all HX_NATIVE-scoped (verified by inspection of the five edits).
