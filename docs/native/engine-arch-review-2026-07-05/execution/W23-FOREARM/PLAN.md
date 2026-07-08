# W23-FOREARM — PLAN (DISCOVERY-ONLY)

**Lane:** FOREARM (Wave 23, re-dispatched standalone). **Mode:** DISCOVERY-ONLY —
HARD STOP before any fix code. Deliverable = a NAMED driver + a Wave-24 fix charter.

## Charter (A7/A8, binding — override the kickoff where they differ)

- Binding is CLOSED (exonerated Wave 22, own==bound at draw). NO skinning/rebind/
  mitten/binding edits. Hands family CLOSED.
- Find WHAT drives `bone_R-foreArm` (+ bilateral L twin) to world y≈+182 while
  body/upperArm stay near y≈0. Is it a CLIP / IK / camera-cut reset / walk-on
  count-in freeze class (`67e87ae1`)?
- A7 method: `BAND_ANIM_PROBE='*'` (wildcard, prints real member Name() + clip),
  `BAND_ANIM_BONE=bone_R-foreArm.mesh`. Amend the `%30` time-throttle
  (`BandCharacter.cpp:700`) to EVENT-triggered emission (bone world-y > threshold
  or large `moved`) — a probe-only edit, HX_NATIVE + BAND_ANIM_PROBE env-gated,
  inert by default.
- A8: do NOT gate on exact-frame reproducibility (camera cuts ±6 noise). Long
  fixed-clock burst + event-triggered probe captures the driver identity AT the
  event.

## Steps

0. **[done]** Confirm `BandCharacter.cpp` compiles into rb3-native
   (`build.ninja:5772`). Build under `/tmp/rb3-native-build.lock`.
1. **[done]** Probe-only edit: `%30` → event-triggered (`bonePost.y > BAND_ANIM_YTHRESH`
   default 50, or `moved > thresh`) + `%120` heartbeat so quiet frames stay legible;
   emit-line prefix `evt=HI|beat`. HX_NATIVE + `BAND_ANIM_PROBE` gated.
2. **[done]** Long fixed-clock burst on R-foreArm (`keyboard-to-gameplay.py
   --song-downs 3 --game-burst 12 --burst-interval 0.3`), `BAND_ANIM_PROBE='*'`,
   `BAND_ANIM_BONE=bone_R-foreArm.mesh`. Capture the driver name AT the event.
3. **[done]** Bilateral confirmation: repeat with `bone_L-foreArm.mesh`.
4. **[done]** Contrast: repeat with `bone_R-upperArm.mesh` (parent bone — is it high too?).
5. **[done]** Name the driver. Distinguish "bone WORLD is high" (pose) from "SKIN
   explodes downstream of a fine bone" (skinning-compose). W22 said bone-world y≈182;
   confirm it holds under the event probe.
6. **[done]** drawlog-golden 792 PASS with probe env UNSET (prove inertness).
7. **[done]** Commit the probe edit (own files only), write STATUS + Wave-24 charter.

## Gates

- drawlog-golden 792 byte-identical / canonical-PASS with `BAND_ANIM_PROBE` unset — DONE (exit 0).
- No skinning/rebind/mitten/binding edits — RESPECTED (only the probe block touched).
- No default flips, no pin bumps, no new shipped flags.
- Stage only own files; never `rb3_session_trace.cpp` / engine `FxSendNative.cpp`.
