# PLAN — Crowd + drum kit congregate at origin (native/web gameplay)

Synthesis of four scout docs (`scout-crowd.md`, `scout-bandgear.md`, `scout-xfm.md`,
`scout-repro-tooling.md`) plus my own code verification. Decision-ready: ranked
root-cause hypotheses, a concrete position-dump tool spec, and an order of attack.

Date: 2026-06-20. Engine pin: `MILO_ENGINE_PIN = 884ab17d…` (`native/CMakeLists.txt:74`).
Symptom (Scout 4, live): gem highway renders correctly; a bright JUMBLED PILE of
character + instrument geometry (crowd + band gear incl. drum kit) is collapsed to
one spot on the LEFT of the gameplay frame. A static venue prop (dartboard) renders
in the CORRECT place. Venue-dark is a SEPARATE lighting issue — do not conflate.

---

## 0. Cross-checking the scouts — resolved contradictions (read this first)

The scouts disagree on three load-bearing points. I read the actual code to settle
each; the resolutions reshape the hypothesis ranking.

### 0.1 RESOLVED — `RndTransProxy::Sync()` parents the PROXY to the character, NOT the character to the proxy. Scout 2 is RIGHT; Scouts 1 & 3 are WRONG on this.

`TransProxy.cpp:28-45` (verified): `Sync()` calls `this->SetTransParent(trans)` where
`trans = mProxy` (or `mProxy->Find(mPart)`). `this` is the **proxy**. In
`SyncTransProxies` (`BandWardrobe.cpp:326-339`) the call is
`it->SetProxy(mTargets[i])` → `mProxy = bandChar` → `Sync()` → **the proxy becomes a
child of the band character**. So the `player_<inst>0_*.tp` proxies are camera/closeup
TARGETS that FOLLOW the member; they do NOT drag the member onto the stage.

Consequence: Scout 1's #1 and Scout 3's §5 ("unresolved proxy → `SetTransParent(0,0)`
→ drum kit at origin") describe the proxy collapsing to origin, **not the band
character / drum kit**. The drum geometry rides the *character*, not the proxy (see
0.2). So an unresolved proxy mis-places a CLOSEUP CAMERA TARGET, which would break
closeup framing — it does not by itself put the visible drum kit at origin. The V23
fix is still about closeup targets ("the venue's player_<inst>0_*.tp **closeup-target
proxies** all collapse onto a shared stand-in dir", `BandDirector.cpp:702-711`,
verified), which is consistent with this corrected reading.

### 0.2 RESOLVED — the drum kit is the drummer character's instrument geometry, bound to the character's bones. Scout 2 is RIGHT; the bug is "drummer character at origin," not a standalone prop.

`BandCharacter::SyncObjects` (`BandCharacter.cpp:1347-1366`, verified) parents
`bone_prop0..3.mesh` + `bone_mic_stand_bottom.mesh` etc. **to the character**
(`t->SetTransParent(this)`). Instruments merge into `mInstDir` (`:147-149`) and attach
to those bones. So drum-kit world pos = bone-local × character bone chain × character
ROOT WorldXfm. If the character root is at origin, the whole kit is at origin.

### 0.3 RESOLVED (and this is the crux) — the band character ROOT WorldXfm is IDENTITY BY DESIGN on native, and NOTHING in the band path re-places it into the venue. Scout 3 surfaced the design fact; Scout 2 surfaced the missing-placement gap. Together they are the strongest band-side hypothesis.

`world/Dir.cpp:452-454` (verified, verbatim native comment):
> "the band-member BandCharacter objects keep mShowing=false and an **identity
> WorldXfm** — their visible geometry lives in mOutfitDir / mInstDir, positioned by
> the bone proxies."

But 0.1 just proved the bone PROXIES follow the character — they do not position it.
And I grepped `BandWardrobe.cpp` for any `SetWorldXfm`/`SetLocalXfm`/`SetTransParent`
that would place the band character root or `mTargets[i]` into the venue stage spot:
**none exists** (only `GetCharacter` accessors). So on native there is a real GAP:
the character root is identity, the proxies don't move it, and no code re-parents it
under a `player_<inst>0_base` venue stage dir. With an identity root, the bone chain
composes `mLocalXfm × identity` → the whole member + kit lands at/near origin. This is
the single cleanest explanation for the band-pile-at-origin half and it matches Scout
4's screenshot (a band pile, not a correctly-staged band).

NOTE: the world/Dir.cpp comment asserts the proxies DO position the geometry — that
assertion is contradicted by the TransProxy.cpp code (0.1). The comment is aspirational/
loosely worded; trust the code. This mismatch is itself evidence the placement was
never actually wired on native.

### 0.4 RESOLVED — V24 shard-guard is NOT a cause. All three code-reading scouts agree and I concur. It is skinned-mesh-only (`Rnd_Wgpu_RB3.cpp` `if (skinned && …)`), so it cannot touch a static drum kit, and for the crowd it only AMPLIFIES an upstream origin bug (some bones at origin → ~25× AABB span → drop → bunched survivors). Fixing placement removes the trigger; do not relax the guard.

### 0.5 PARTIALLY RESOLVED — is the crowd at TRUE origin, or "spread but bunched/dropped"? Scouts conflict and NEITHER has live position data.

- Scout 1/3 cite a PRIOR scout that probe-verified small_club crowd `unk0` positions
  are spread + sane `(±150, -30..-300, 3.6/4.5)`, with the bunching being a skinning
  artifact since FIXED (`adb5240e` inverse-bind rebake).
- Scout 4's CURRENT screenshot shows a pile, but cannot distinguish "all at origin"
  from "spread but ~25× shard-dropped → bunched survivors" from a 2D image.

Verified mechanism (`Crowd.cpp:216-237` `Set3DCharAll`): per-member translation =
authored `instIt->mXfm` copied to `unk0`, applied ABSOLUTE via `SetWorldXfm` (no parent
compose). Rotation only comes from `mPlacementMesh->WorldXfm().m`. So crowd-at-true-
origin requires `unk0` to be zero (authored-data/endian load failure) OR a regression
since the prior scout. The position-dump tool (§2) is REQUIRED to settle 0.5 — it is
the highest-value first action.

---

## 1. Ranked root-cause hypotheses

Ranked by (a) explanatory power for BOTH halves via one seam (Occam) and (b) evidence
strength. H1 is the strongest SINGLE-SEAM candidate; H2 is the strongest BAND-side
candidate; the rest are alternates the tool will adjudicate.

### H1 — Venue world-dir reparent collapse: `WorldInstance::SyncDir` re-parents band stage spots AND crowd placement mesh onto a stand-in at origin (ONE shared seam) — RANK 1

- **Hypothesis.** The band character root and the crowd `mPlacementMesh` are BOTH
  positioned by being parented under venue stage/placement dirs that `WorldInstance::
  SyncDir` reparents via `ObjRef::Replace`. If a `Find`/ObjPair in that reparent loop
  resolves to a stand-in dir at origin on native, every object that should hang off the
  venue stage (band roots) AND the crowd placement basis collapses to origin at once —
  while pure static props (dartboard), which load their own saved `mLocalor`+parent
  directly, stay correct. This is the only hypothesis that explains band+crowd via ONE
  cause AND leaves the dartboard alone.
- **Evidence FOR.** `WorldInstance::SyncDir` is a PROVEN-fragile seam: a prior bug here
  was a transposed `ObjPair` (MEMORY: venue-env fix, rb3 `d988a301`) in the SAME
  function. Static props loading their own xfm directly (`Trans.cpp` `BEGIN_LOADS`,
  Scout 2 §3) explains why the dartboard survives. Crowd translation being absolute
  (`Crowd.cpp:408`) means a bad `mPlacementMesh` xfm alone would only mis-ORIENT the
  crowd, NOT move it — so for H1 to hit the crowd, the crowd's `unk0` source data must
  also route through the collapsed dir (verify the venue milo wiring).
- **Evidence AGAINST.** Scout 2 §4 notes the crowd translation does NOT parent-compose,
  so a single dir collapse hits band but not crowd-translation unless crowd `unk0` is
  ALSO sourced from the collapsed subtree. `WorldInstance::SyncDir` is byte-matched Wii
  code with no HX_NATIVE seam, so a native-specific collapse would have to come from
  upstream data (deferred venue load) feeding it, not the function itself.
- **Tool confirms/refutes.** Position dump (§2): if band roots AND crowd `unk0`/draw
  positions are at origin while static props are not → H1 strongly supported. If crowd
  `unk0` is SPREAD but band roots are at origin → H1 refuted, fall to H2 (band-only).
- **Fix sketch.** rb3 `src/system/world/Instance.cpp` (HX_NATIVE-gated): in `SyncDir`,
  ensure the reparent `Find` resolves the real venue stage dir before the deferred
  load completes; or defer `SyncDir` until the venue dir is fully resolved. Gate behind
  `#ifdef HX_NATIVE` with a byte-identical `#else`.
- **Repo.** rb3 (`src/system/world/Instance.cpp`), HX_NATIVE-gated.

### H2 — Band character root never placed into the venue stage spot (identity by design + missing placement wiring) → drummer + kit + whole band at origin — RANK 2 (strongest BAND-side; explains the kit decisively)

- **Hypothesis.** Per 0.2/0.3: the band character root WorldXfm is identity by design
  (`world/Dir.cpp:452-454`), the bone proxies FOLLOW the character (do not place it,
  0.1), and NO code in the native band path sets the character root WorldXfm or
  re-parents it under a `player_<inst>0_base` venue stage dir. So the band draws via the
  `world/Dir.cpp:448-461` bridge at identity → whole member + kit at origin.
- **Evidence FOR.** Verified: `SyncObjects` parents kit bones to the character
  (`BandCharacter.cpp:1347-1366`); `Sync()` parents proxies to the character
  (`TransProxy.cpp:28-45`); grep of `BandWardrobe.cpp` finds NO root-placement call.
  The `world/Dir.cpp` comment CLAIMS proxies position the geometry but the code
  contradicts that (0.3) — a textbook "the wiring was assumed, never implemented"
  signature. Scout 4's screenshot shows a literal band pile.
- **Evidence AGAINST.** Does not explain a TRUE crowd-at-origin (crowd is a separate
  system) — so if the tool shows the crowd ALSO genuinely at origin, H2 is only half
  the story (pair with H1 or H4). If retail actually relies on the proxies to place the
  member (i.e. the comment is right and `mPart`/`mProxy` semantics differ from my read),
  H2 weakens — but the code in 0.1 is unambiguous about parenting direction.
- **Tool confirms/refutes.** Position dump (§2): `bandChar->WorldXfm().v ≈ (0,0,0)` AND
  `bandChar->TransParent()==NULL` (or a non-venue parent) for all 4 members → H2
  confirmed. If band roots are at the stage spot but the KIT floats → it's H5 (merge),
  not H2.
- **Fix sketch.** rb3 (HX_NATIVE): after `SyncTransProxies`, explicitly place each
  `mTargets[i]` root — either `SetTransParent` it under the venue's
  `player_<inst>0_base` dir (find by name, mirror the `SyncTransProxies` name match) or
  copy that dir's WorldXfm into the character root via `SetWorldXfm`. Likely in
  `BandWardrobe::SetVenueDir`/`SyncTransProxies` or `BandDirector::EnterVenue`.
- **Repo.** rb3 (`src/system/bandobj/BandWardrobe.cpp` or `BandDirector.cpp`), HX_NATIVE.

### H3 — `mVenueNames` empty / proxies unmatched (V23 guard `!mVenue.Name().Null()` fails) — RANK 3

- **Hypothesis.** The V23 native fix (`BandDirector.cpp:701-718`) only runs
  `LoadCharacters` if `!mVenue.Name().Null()`. If the deferred venue name is still null
  at that point, `mVenueNames` stays empty → `SyncTransProxies` matches 0 proxies → the
  closeup/camera targets collapse onto a stand-in (the symptom V23 was written to fix).
- **Evidence FOR.** Verified guard + comment (`BandDirector.cpp:712-717`): "was 0"
  before the fix; explicitly fragile (depends on `LoadCharacters` running before
  `SetVenueDir` and on the `mic`→`vocals` remap at `BandWardrobe.cpp:695-705`).
- **Evidence AGAINST.** Per 0.1, unmatched proxies collapse CLOSEUP TARGETS, not the
  visible drum geometry — so H3 alone explains broken closeups, not the band pile. H3
  is more likely a CONTRIBUTING/upstream factor to H2 (if `mVenueNames` is empty, any
  H2-style root-placement that keys off `player_<inst>0_base` also can't run).
- **Tool confirms/refutes.** Add a counter in `SyncTransProxies` (matched-proxy count)
  and log `mVenueNames[0..3]` after `LoadMainCharacters:704`. Count==0 or empty names →
  H3 live. Existing `VENUE_DBG`/`CHAR_DBG` env already logs the `LoadCharacters` call.
- **Fix sketch.** rb3 (HX_NATIVE): make the V23 `LoadCharacters` call robust to a still-
  deferred venue name (resolve the venue name earlier, or retry once the name resolves).
- **Repo.** rb3 (`BandDirector.cpp` / `BandWardrobe.cpp`), HX_NATIVE.

### H4 — Crowd multimesh `Instance.mXfm` decodes to identity/zero on native load (endian / Xbox-compressed venue) — RANK 4 (crowd-only; would unify with band ONLY if band spots share the decode)

- **Hypothesis.** The crowd per-member `unk0` comes from `RndMultiMesh::Instance.mXfm`
  read by `operator>>(BinStream&, Transform&)` (`Mtx.h:227` = `bs >> m >> v`). If the
  native venue milo decodes wrong-endian/compressed, every instance `mXfm` → zero →
  whole crowd at true origin. If band stage-spot transforms use the SAME decode from the
  SAME file, this could also hit the band (unifying), but Scout 2 §4 notes the dartboard
  (same decode) is FINE → this is WEAKENED.
- **Evidence FOR.** Same `operator>>` shared across all venue transforms; native loads
  a different-endian/compressed asset than Wii.
- **Evidence AGAINST.** The dartboard (static prop, same decode path) renders correctly
  (Scout 4) → a blanket transform-decode failure is unlikely. A prior scout probe found
  small_club crowd `unk0` spread + sane.
- **Tool confirms/refutes.** Position dump (§2): if `m3DChars[i].unk0.v` are all zero/
  equal → authored-data/decode failure (H4). If spread but the DRAWN position is origin
  → composition/`mPlacementMesh` (not H4).
- **Fix sketch.** rb3 (HX_NATIVE) endian handling in the multimesh/Transform load for the
  native venue path; or use the already-extracted/converted asset.
- **Repo.** rb3 (`MultiMesh.cpp` / `Mtx.h` load path), HX_NATIVE — only if the tool shows
  zeroed `unk0`.

### H5 — Instrument merge LocalXfm wrong (`BandCharacter::FilterMerge`, `:2738-2748`) — RANK 5 (distinguisher, low)

- **Hypothesis.** The kit attaches to the character but with a bad merged `LocalXfm` →
  kit mis-placed RELATIVE to the drummer (drummer body staged, kit floating), not the
  whole-member-at-origin symptom.
- **Evidence FOR/AGAINST.** Scout 2 §6.4. AGAINST: Scout 4 shows a whole pile, not a
  staged drummer with a detached kit — so this is a distinguisher, not the lead.
- **Tool confirms/refutes.** If body is at the stage spot but `mInstDir` world is far →
  H5; if body AND kit both at origin → H2/H1.
- **Repo.** rb3 (`BandCharacter.cpp`), HX_NATIVE.

### Occam summary
- ONE-seam-explains-both: **H1** (venue reparent collapse) is the only clean single
  cause; it is the priority to test FIRST with the tool because confirming it solves
  both halves at once.
- If H1 refuted, the reality is **H2 (band) + (H4 or skinning-amplified-spread) (crowd)**
  — two coincident causes, exactly as Scouts 1 & 2 independently concluded. H2 is the
  decisive driver of the visible drum-kit-at-origin regardless.

---

## 2. TOOL SPEC — position-dump debug tool (`rb3_pos_dump`)

Mirror the existing `RB3DtaCharProbe` pattern (Scout 4's recommendation — verified at
`native/src/rb3_http_handlers.cpp:462`, registered `:502`, env-gated `CHAR_PROBE_DUMP`).
Simplest thing that yields per-object world positions for crowd members + band gear +
static props, over the existing `/api/dta/eval` route (no new HTTP wiring, has a crash
guard `DtaEvalCrashHandler`).

### Hook point
- **File.** `native/src/rb3_http_handlers.cpp` — add `static DataNode RB3DtaPosDump(
  DataArray*)` next to `RB3DtaCharProbe`, register
  `DataRegisterFunc(Symbol("rb3_pos_dump"), RB3DtaPosDump)` alongside line 502.
- **Native-only.** This is `native/src/` code — NO `src/system/*` edit, so no Wii-codegen
  risk. (If a helper ever needs to read `WorldCrowd` internals from `Crowd.cpp`, that
  helper MUST be `#ifdef HX_NATIVE` with a byte-identical `#else`.)

### What it does
1. Walk the live root recursively: `for (ObjDirItr<Hmx::Object> it(ObjectDir::sMainDir,
   true); it; ++it)` (pattern from `rb3_render_mesh.cpp:112` `CountAndBound`, verified).
   For each object, in this cast order:
   - `dynamic_cast<WorldCrowd*>` FIRST — crowd members are NOT separate objects; one
     archetype draws N instances. For each `mCharacters` `CharData`, for each
     `m3DChars[i]`, emit the authored per-member position `m3DChars[i].unk0.v` (verified
     `Crowd.h:39` / `Crowd.cpp:216-237`). This is the crowd-distribution ground truth
     that settles 0.5.
   - else `dynamic_cast<BandCharacter*>` — emit the band ROOT: `WorldXfm().v`,
     `TransParent()` name (NULL → H2/H3 confirmed), and `mInstDir` world sphere center
     (kit position; far from root → H5). This is the band ground truth.
   - else `dynamic_cast<RndTransformable*>` LAST (covers static venue props incl. the
     dartboard control case) — emit `WorldXfm().v`, Name, ClassName.
2. Verbose per-object stderr lines are gated behind env `POS_DUMP_VERBOSE` (mirror
   `CHAR_PROBE_DUMP`), so `{rb3_pos_dump}` stays cheap to poll.
3. Return a short SUMMARY string from the HTTP call (so the harness gets a verdict
   without log parsing): counts + the smoking-gun metric "objects within 1u of origin".

### Env var
- `POS_DUMP_VERBOSE` (gates the detailed `[POSDUMP]` stderr dump). The HTTP summary is
  always returned.

### Output format (parseable)
One line per object on stderr (the harness greps `[POSDUMP]`):
```
[POSDUMP] kind=crowd   name=<crowdname> i=<idx>  pos=<x>,<y>,<z>          # m3DChars[i].unk0.v
[POSDUMP] kind=band    name=player<N> slot=<inst> root=<x>,<y>,<z> parent=<name|NULL> inst=<x>,<y>,<z>
[POSDUMP] kind=prop    name=<objname> class=<ClassName> world=<x>,<y>,<z>
```
HTTP summary string (returned to `dta_eval`):
```
posdump crowd=<Ncrowd> band=<Nband> props=<Nprops> at_origin=<K> band_at_origin=<B>/4 crowd_at_origin=<C>/<Ncrowd>
```
where `at_origin` counts `|pos| < 1.0`. This single line is enough for a python harness
to assert the distribution and adjudicate H1/H2/H4 immediately.

### Python harness sketch
Copy `scripts/native/song-end-test.py` boot+wait (pure-stdlib HTTP, `free_port()`,
`wait_for(/api/health)`, `dta_eval()` that parses
`{"ok":true,"data":{"type":..,"value":..}}`). New script
`scripts/native/crowd-origin-posdump.py`:
```python
# 1. launch rb3-native with env: RB3_GAME=1 RB3_HTTP=1 RB3_HTTP_PORT=<p>
#    MILO_HEADLESS=1 RB3_DATA=orig-assets/extracted POS_DUMP_VERBOSE=1
#    RB3_GAME_INPUT=<NAV from song-end-test.py> ; redirect stderr -> /tmp/rb3-posdump-<p>.log
# 2. wait_for /api/health until songMs > 2000  (gameplay underway)
# 3. summary = dta_eval(port, "{rb3_pos_dump}")        # the verdict line
# 4. parse [POSDUMP] lines from the log; compute:
#       crowd: are unk0 all ~equal/zero (H4) vs spread (refutes H4)?
#       band:  all 4 roots |pos|<1 and parent NULL? (H2/H3)
#       props: dartboard-like static props NOT at origin? (control)
# 5. assert + print a verdict table mapping to H1/H2/H4 per §1.
```
Validation the harness should print explicitly (the §1 forks):
- crowd `unk0` spread + band roots origin + props fine → **H2** (band-only) — drum kit
  fix is band-side; crowd "pile" is shard-amplified spread, not origin.
- crowd `unk0` origin + band roots origin + props fine → **H1** (shared reparent) or
  H1+H4.
- crowd `unk0` origin + props ALSO origin → blanket decode (H4 broadened) — but Scout 4
  says props are fine, so expect this to be refuted.

---

## 3. Recommendation — order of attack + confidence

### Order of attack (next batch)
1. **BUILD THE TOOL FIRST (ground truth before any fix).** Add `rb3_pos_dump`
   (~15-line DTA func) + `crowd-origin-posdump.py`. Run it to settle 0.5 (crowd true-
   origin vs spread) and to read all 4 band roots + parents. This single run adjudicates
   H1 vs H2 vs H4 and prevents fixing the wrong half. Cheap, no risk, native-only.
2. **Then fix per the verdict:**
   - If band roots are at origin/parent-NULL (expected) → implement **H2** (place the
     band character root into the venue stage spot, HX_NATIVE). This deterministically
     fixes the drum-kit-at-origin half — the half we are MOST confident about (0.2/0.3
     are code-verified, not speculative).
   - In parallel, if the dump shows crowd `unk0` at true origin → pursue **H1** (shared
     `WorldInstance::SyncDir` reparent) since it would fix both; else the crowd "pile"
     is the already-known shard-amplified spread and needs no new placement fix (verify
     with `SHARD_DBG`/`SHARD_RATIO_DBG`, already in the engine).
3. Re-run the tool + a gameplay screenshot to confirm: 4 band roots at distinct stage
   spots, drum kit at the drum spot (not 0,0,0), crowd spread, dartboard unchanged,
   `SHARD_GUARD` band/crowd drop count ≈ 0.

### Confidence statement
- **Top hypothesis for the DRUM-KIT half: H2 — HIGH confidence.** The chain is code-
  verified end-to-end: kit bones parent to the character (`BandCharacter.cpp:1347-1366`),
  the character root is identity by design (`world/Dir.cpp:452-454`), the proxies follow
  the character rather than placing it (`TransProxy.cpp:28-45`), and no root-placement
  call exists in the band path (grep-confirmed). The only thing that could lower this is
  if retail genuinely places the root via a mechanism I haven't located (the loosely-
  worded `world/Dir.cpp` comment hints proxies do it — but the code refutes that).
- **Top hypothesis for ONE-SEAM-EXPLAINS-BOTH: H1 — MEDIUM confidence**, contingent on
  the position dump showing the crowd at TRUE origin (not shard-amplified spread). The
  dartboard-is-fine evidence both supports H1 (props bypass the collapsed dir) and
  bounds it (the collapse must be selective).
- **What would raise confidence to HIGH on the unified story:** the §2 tool showing all
  4 band roots AND crowd `unk0` at origin while static props are correct → confirms a
  selective shared reparent collapse (H1); a single HX_NATIVE fix in
  `world/Instance.cpp` then covers both. If instead crowd `unk0` is spread, accept the
  two-cause reality (H2 for band, shard-spread for crowd) and ship H2 first.

### Repo split (where fixes land)
- Placement logic (H1/H2/H3/H4/H5): **rb3 `src/system/*`**, HX_NATIVE-gated (byte-
  identical `#else`) — these TUs are compiled into `rb3-native` from rb3's own `src/`
  (`native/CMakeLists.txt:246-275`), NOT the engine.
- Draw-time world read / shard guard: **milo-native-engine** (`Rnd_Wgpu_RB3.cpp`) — but
  per 0.4 the guard is not the cause; no engine fix is expected unless the tool shows a
  draw-time identity stub (Scout 3 §4 already verified there is none for static meshes).
- The debug tool (`rb3_pos_dump`): **rb3 `native/src/`** (native-only, no gating needed).
