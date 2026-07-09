# W26-PROP — STATUS

**Headline:** Discriminator = **CLIP-BINDING gap (c), PROVEN** (not attach/proxy). The
instrument-prop IK targets are *tip* bones whose correctly-posed `bone_target_*` parent
is at/near the hand, but the tip carries a large STATIC LocalXfm the clip never animates
natively. **Fix = a scoped, default-OFF IK-target redirect that DOES correctly relocate the
mis-posed targets to their at-hand parent frames — but it does NOT make the clamp dormant
(the charter's headline goal) and produces NO visible in-song change.** This is a
**narrowed-but-unfixed / honest-partial** outcome (the W25-CROWD model). Code kept
default-OFF as correct scaffolding + the pinned discriminator; RB3_IK_REACH_CLAMP stays
the (still-load-bearing) safety net. **No default flip. Nothing regressed.**

---

## STEP 0 — DISCRIMINATOR (A4): CLIP-BINDING gap (c)

Extended the `IK_ROOTCMP` probe with `IK_PROP_DBG` (dumps the far target's LocalXfm.v + its
parent's world + `d(parent,hand)`), ran in-song via the band-closeup harness
(`_w24_forearm_capture.py`, `coop_g_cg`/free-burst). Evidence: `evidence/step0-ikprop.log`.

The decisive per-ikhand signature (`d_par` = parent-to-hand distance; reach ≈ 20.3u):

| ikhand | target (tip) | parent | reach | d_hand(tip) | \|tip local\| | d_par | class |
|---|---|---|---|---|---|---|---|
| `strum.ikhand` | `bone_pick_strum` | `bone_target_strum` | 20.3 | 50.7 | **51.3** | **18.9** | CLIP-BIND (c) |
| `right/left_hand` (drum) | `bone_[RL]-tip_<piece>` | `bone_target_<piece>` | 20.3 | 51-53 | **48.8** | 26-30 | CLIP-BIND (c) |
| `fret.ikhand` | `bone_tip_fret` | `bone_target_fret` | 20.2 | ~28-35 | — | 7-15 | CLIP-BIND (c) |
| `mic*.ikhand` / `mic_stand` | `bone_mic*` / `bone_mic_stand_mouth` | `bone_mic` / `bone_neck` | 0-60 | 60-77 | — | 45-63 | parent-FAR (whole-chain displaced) |

**Verdict:** `IK_ROOTCMP` confirms `same=1` (chain resolves to the correct member root) → the
parent-chain/proxy gap (a) is REFUTED for guitar/drum. The `bone_target_*` parent is the
correctly-posed at-hand frame (e.g. `bone_target_strum` at d_par=18.9, WITHIN reach); the tip
bone's own static LocalXfm (`bone_pick_strum` local = (2.28,−48.90,−15.29), |local|=51) is what
flings the target out of reach. **The prop-bone clip track is never bound natively** (the hands
parse-time-binding class) ⇒ CLIP-BINDING gap (c). The vocalist mic is a *different*, harder
class (the whole mic-stand prop chain is displaced; `mic_stand.ikhand` reach=0 → A6 direct-set,
out of this simple-fix scope).

Checkpointed to `/tmp/wave26-checkpoints/PROP.json` BEFORE any fix code.

## THE FIX (implemented, flag-first, default-OFF, HX_NATIVE — G3 case-1)

`src/system/char/CharIKHand.cpp`, new `static sPropPoseRedirect()` + flag `RB3_PROP_POSE`
(default-OFF). At each IK-target world read (single-target + multi-target paths), when the
target's TransParent is a `bone_target_*` frame that is (i) closer to the hand than the tip
AND (ii) the tip is out of reach, redirect the IK destination to that parent frame. Strictly
scoped, byte-inert by default (whole thing `#ifdef HX_NATIVE` + env-gated).

**What it achieves:** the redirect fires correctly on exactly the mis-posed prop tips
(`bone_pick_strum`, `bone_[RL]-tip_*`, `bone_tip_fret`, drum pedals — `evidence/fixon-redirect-counts.txt`)
and lowers the raw target distance for `fret`/`left_hand` by 44-64% (`evidence/mworlddst-ON-vs-OFF.txt`).

**What it does NOT achieve (the honest gap):** the clamp does NOT go dormant for `strum.ikhand`
and drum `right_hand.ikhand`. Post-redirect `mWorldDst` stays large because the engine's
`mFinger` finger-compensation (`Poll` :319-330: `mHand · mFinger⁻¹ · target`) re-projects the
destination through the finger/pick bone, which is itself part of the clip-posed chain — a
feedback the target redirect alone cannot break. And because the clamp (default-ON) ALREADY
produces the correct clip-posed arm in-song, redirecting the target changes nothing visible.

## GATE TABLE

| gate | requirement | result |
|---|---|---|
| G3 case-1 batch_objdiff (flag-OFF) | == baseline exactly | **PASS** — Poll 96.13% (base 96.127), MeasureLengths 81.34% (base 81.355); all HX_NATIVE-gated |
| drawlog-golden `--fixed-clock --canonical-order` (flag-OFF) | 792 byte-identical | **PASS 792** (293 known-residual within bound) |
| rb3-tests (flags OFF) | 116/0 | **PASS 116** (7 skipped real-path fixtures; teardown SIGSEGV pre-existing) |
| A5-i: no in-reach target moved | strict | **PASS** — guard refuses redirect when `dTip ≤ reach²` or `dPar ≥ dTip`; boundary log entries are display-precision on bass/pedal targets, not the playing hands |
| A5-ii: clamp-fire relative drop | large drop | **NOT ACHIEVED** — strum/drum-R still `mode=skip` (mFinger feedback); fret/left_hand raw distance −44…−64% only |
| A5-iii: E1 per-instrument (in-reach IK, no fan) | judged | **NO CHANGE** — guitar_body ANATBEAT p50/max IDENTICAL ON vs OFF (~1.07/1.15, 0 events >1.5); clamp already handles in-song; vignette ≤4.2 residual unchanged |

Bounded: the clamp stays default-ON, so worst post-fix case == today's clip-pose (never the
spike-fan). ON is never worse than OFF in the controlled ANATBEAT measure.

## DISPOSITION — narrow + report (do NOT ship as a default flip)

Per the charter's A5 / honesty mandate: the fix does not deliver the headline goal (dormant
clamp, restored genuine IK) and shows no visible in-song benefit, so **no default flip**. The
code is retained **default-OFF** as (a) the pinned discriminator instrumentation (`IK_PROP_DBG`,
`RB3_PROP_DST_DBG`) that proves the mechanism, and (b) the correct first half of the eventual
fix (target relocation) — analogous to W25-CROWD keeping its byte-inert scaffolding. The
remaining work (the deeper W27 item): the prop-bone CLIP TRACKS are never bound natively AND the
`mFinger` finger-compensation feedback — both must be addressed for the clamp to go dormant.
Neither is a target-frame problem, so the redirect alone cannot close it. RB3_IK_REACH_CLAMP
stays the load-bearing safety net (NOT dormant, NOT removed).

## HONESTY / caveats

- The mWorldDst / clamp-preDist numbers are confounded by the documented run-to-run IK explosion
  intermittency (W25). The controlled measure is the ANATBEAT ratio on `guitar_body` clips (the
  clamp-contained in-song regime), which is identical ON vs OFF — that is the load-bearing result.
- The vocalist mic (whole-chain displacement) and `mic_stand.ikhand` reach=0 are NOT addressed
  (A6 direct-set path; out of the simple-redirect scope) — reported, deferred.
- E1 PNGs (`evidence/e1-guitarist-fix{ON,OFF}.png`) are from separate runs (≈33ms songMs apart),
  not a frame-locked A/B; the ANATBEAT distribution is the rigorous comparison.

## Files changed (staged by path, this lane only)
- `src/system/char/CharIKHand.cpp` — `sPropPoseRedirect()` + `RB3_PROP_POSE` redirect (default-OFF)
  at both IK-target read sites; `IK_PROP_DBG` discriminator probe; `RB3_PROP_DST_DBG` mWorldDst
  probe. All `#ifdef HX_NATIVE` + env-gated → Wii object byte-identical.
- `docs/native/engine-arch-review-2026-07-05/execution/W26-PROP/{PLAN,STATUS}.md`,
  `run_prop_probe.py`, `evidence/`.

---
## ERRATA (Wave-26 close-out review `055992be`, E7)
- The mFinger-feedback explanation for why the clamp can't go dormant is INFERRED, not
  bypass-tested: strum median target distance actually 199.5→203.8 ON (marginally worse, not the
  predicted large drop). The W27 charter must BYPASS-TEST the mFinger re-projection
  (`Poll:319-330`) before concluding it's the blocker.
- Env-parse nit FIXED (E7): `RB3_PROP_POSE=""` previously enabled the flag (`e[0] != '0'` is true
  for `'\0'`); now requires a non-empty value (CharIKHand.cpp:55). Default-OFF unchanged.
- The weight-loop still uses the UN-redirected tip LocalXfm — a known incompleteness of the
  partial redirect (W27 item, alongside binding the prop clip tracks).
