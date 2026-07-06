# W3.1a — STATUS

Append-only log. Update under `flock /tmp/rb3-docs.lock`. One `## <subtask-id> — done|partial|blocked`
section per subtask with commit SHAs + blockers. Re-runs read this + `git log --grep=W3.1a` and skip
done work.

## Plan — done (Opus planner, 2026-07-06)
PLAN.md written with `## Subtasks` (S1 fog / S2 projLight / S3 gate+A/B). Investigation verified against
LIVE source (engine HEAD `dbf2758`, post-W2.1-flip): fog/projLight fields + WGSL consumption already
exist (zero DC3 blast); the zeros to replace are `Rnd_Wgpu_RB3.cpp:1429` (fog) / `:1431` (projLight).
**Two findings beyond the brief:** (1) fog also needs `RB3MaterialBinder.cpp` to set
`materialFogEnabled` from `RndMat::mFog` (Mat.h:329) — WGSL ANDs scene∧material; (2) projLight is
engine-only-feasible (RndLight `mTexture`/`mTextureXfm` public, `GetRB3TexView` exists) but needs a
local port of DC3 `RndLight::Projection()` + a slot-3 gobo bind → marked SECONDARY/cut-first.

## W3.1a.S1 — done
## W3.1a.S2 — pending
## W3.1a.S3 — pending

## W3.1a.S1 — done (sonnet implementer, 2026-07-06)
Engine commit `a4bde9f` (`../milo-native-engine`, on top of `dbf2758` W2.1-flip; pin unchanged `609efb7`).

**Resume note:** started with pre-existing uncommitted work already in the engine working tree
(matching this exact subtask, with TEMP verification hacks `RB3_FOG_PROBE_TMP`/`RB3_FOG_FORCE_TMP`
still in the diff, un-committed) -- picked it up rather than redo, per the resume contract. Cleaned
the TMP probe/force code out before committing; final diff matches PLAN.md's S1 steps exactly
(flag accessor `RB3EnvFogEnabled()` in `Rnd_Wgpu_RB3.cpp` + declared in `RB3MaterialBinder.h`,
scene fog fill at the old `s.fogEnabled = 0;` site, material-side `mu.materialFogEnabled` in
`RB3MaterialBinder.cpp`). Left the sibling's uncommitted `FxSendNative.cpp` edit untouched.

**Gates (flag-OFF, hard):**
- `drawlog-golden.py --canonical-order --fixed-clock` -> PASS, 888/888 draws (matches the
  post-W0.3d-fix/W2.1-flip committed golden; no change under flag-OFF).
- `lineup-gate.py` -> PASS all layers (img/segA/ratioB/countC/pin).
- `milo-engine-tests` (DC3-context, `build-tests`) -> **200 total / 0 failed / 2 skipped** (the
  198/0/2 hard gate). RB3's platform files (`Rnd_Wgpu_RB3.cpp`/`RB3MaterialBinder.cpp`) are not
  even compiled into this DC3-context test binary, so this is zero-blast-radius by construction,
  not just by proof.
- DC3 contract diff: `git diff 609efb7..HEAD -- src/gfx/UniformStructs.h src/gfx/standard_wgsl.inc`
  -> empty (0 lines). No struct/WGSL/`static_assert` change.

**flag-ON pipeline proof (S1-scoped, not the S3 real-asset venue A/B):** a full boot sweep with a
temporary (uncommitted, reverted before commit) probe found **no in-repo test venue currently
authors real fog** -- every boot-reachable `RndEnviron` swept (34 distinct environs) reports
`FogEnable()==false`. So the plan's literal "screenshot a fog-authoring venue" check has no asset
to exercise. Substituted an instrumented (also uncommitted, reverted) force-override to prove the
wired pipeline end-to-end instead:
  - Matched-frame A/B (camera-pinned via patch-lineup-capture.py conventions): baseline vs
    forced-scene-fog+forced-material-gate -> **mean per-pixel diff 75.7** (visible depth-fade tint,
    stronger on distant geometry -- correct fog falloff shape).
  - Fail-red: same forced scene-side fog but material-side gate held OFF -> **mean diff ~9.3**,
    back down near baseline noise -- proves the WGSL `scene.fogEnabled && material.materialFogEnabled`
    AND-gate is the live path (not a coincidental tint), exactly as the plan's fail-red asked.
  - All instrumentation was reverted; the committed diff has zero temp/debug code.
- **Residual gap for S3:** since no in-repo asset authors fog, S3's "venue A/B package" step will
  need either (a) an asset that authors `fog_enable`/authored fog params (search wider than the
  boot-reachable set this sweep covered), or (b) reuse this S1 finding plus an explicitly-disclosed
  instrumented-override methodology for the coordinator sign-off package, since a byte-real
  "fog visibly renders on an authored venue" demo isn't available with current in-repo assets.

**classification.json:** appended `RB3_ENV_FOG` row (feature, default off) under
`flock /tmp/milo-engine-classjson.lock`, append-only, no `gen.inc` regen.

S2 (projLight) and S3 (gate consolidation + A/B package) not started -- separate subtasks, own
model (opus) per PLAN.md.


## VERIFY — partial (S1 fog solid & gates green; S2 projLight + S3 sign-off package undone; exit #2 asset-blocked)
Adversarial verifier, own dirs `build-agent-W3.1a-verify` (rb3-native) + `build-agent-W3.1a-verify-tests`
(milo-engine-tests, dc3 backend, decomp-context reconstructed from build-tests cache vars). Engine HEAD
`a4bde9f`, pin `609efb7` unchanged. Re-derived every claim; did NOT trust STATUS.

**Independently GREEN (re-ran for real):**
- **Exit #1 flag-OFF byte-identical [hard]:** `drawlog-golden.py --canonical-order --fixed-clock`
  (unset RB3_ENV_FOG) → PASS 888/888. `lineup-gate.py` → PASS all layers (img/segA/ratioB/countC/pin).
- **Exit #3 milo-engine-tests [hard]:** fresh configure+build of `milo-engine-tests` in my own dir,
  `DC3_DATA/MILO_LIB ctest -j1` → **100% pass, 0 failed / 200, 2 skipped (ExtractBik, SkinGolden.CaptureGolden)
  = 198/0/2**. Engine-booting tests (AssetLoading/CharClip/MoggDecode) compile `standard_wgsl.inc` through
  the real Dawn PipelineManager and pass → shared WGSL still valid. (No test literally named `WgslValidation`
  in this suite; shader validation is exercised transitively.)
- **Exit #4 DC3 zero-blast [hard]:** `git diff 609efb7..HEAD -- src/gfx/UniformStructs.h src/gfx/standard_wgsl.inc`
  → **empty**. static_assert(656) untouched.
- **Code review:** flag accessor `RB3EnvFogEnabled()` presence-truthy opt-in; `venv` (decl :1302) in scope at
  the fill (:1450); flag-OFF path is literally `else { s.fogEnabled = 0; }` (byte-identical); material side
  `mu.materialFogEnabled = mat->mFog ? 1 : 0` gated by the SAME flag → WGSL scene∧material AND-gate coherent;
  `s.numProjLights = 0` untouched (S2 correctly not started). classification.json valid JSON, RB3_ENV_FOG row
  = feature/off; `gen.inc` untouched (coordinator regens). Working tree clean except sibling `FxSendNative.cpp`
  (correctly left untouched, hard rule 8).
- Flag-ON boot on splash → no crash, 888 draws (fill inert where no fog authored).

**NOT MET / open (why this is PARTIAL, not pass):**
- **Exit #2 fog visibly renders flag-ON [hard] — NOT met with a real asset.** S1 STATUS honestly reports no
  boot-reachable in-repo venue authors fog (`FogEnable()==false` on all 34 environs swept); the flag-ON render
  was demonstrated only via an instrumented force-override (now reverted → I could not independently re-run it
  without re-instrumenting, which is out of verifier scope). The scene∧material AND-gate fail-red (mean diff
  75.7 → ~9.3) is reported-by-implementer + code-verified, NOT independently reproduced.
- **Exit #5 projLight (S2) — neither landed nor formally cut.** `RB3_ENV_PROJLIGHT` absent from classification;
  STATUS says "pending". The plan's CUT RULE (file to Wave 6 W3.1b with reason in STATUS) has not been executed.
- **S3 (gate consolidation + venue A/B package) — not done.** No `venue-ab/` artifact dir; no coordinator
  sign-off package produced.

**Verdict:** the S1 fog fill (scene + material) is correctly implemented, provably byte-identical flag-OFF,
DC3-safe (empty contract diff), and 198/0/2 green — that slice is acceptance-ready. The full W3.1a item is
incomplete: real fog-render proof (exit #2) is asset-blocked, and S2/S3 are undone. Recommend coordinator
either (a) accept fog-only + explicitly cut projLight to W3.1b + commission the S3 real-asset A/B, or
(b) reopen S2+S3. No behavior ships (default-OFF); safe to leave as-is until then.
