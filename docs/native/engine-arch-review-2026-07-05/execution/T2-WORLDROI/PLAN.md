# T2-WORLDROI — World-cam ROI provenance (skinned-aware) — PLAN

**Lane:** Wave-19 Lane P (`WAVE19_KICKOFF.md` d93fa894). **Role:** plan author (Opus).
**Charter:** `RETROSPECTIVE/OPTIONS.md` §6 T2 (§6.3) + `WAVE19_REVIEW.md` **A1 (HIGH, BINDING)**.
**Engine pin:** `beb89e5` (= engine HEAD, verified; dirty tree = `M src/platform/FxSendNative.cpp` only, disjoint from this lane's region). **rb3:** work on current `master`.
**Naming box (review F9, BINDING):** T2 is the SPATIAL/ROI instrument (which mesh/bone/owner
drew a pixel). It touches NOTHING on the FRAME-ASSIGNMENT TIMING axis (T1) or R4's ledger
`order` axis. No doc/comment in this lane may conflate them.

A Fable reviewer and then an implementer consume this file. Every code anchor is re-derived
BY SYMBOL at current HEAD with the symbol named next to the line number. Assumptions the
reviewer should attack are tagged **ASSUMPTION**.

---

## 0. What T2 delivers (killer query)

> Given a pixel ROI on a gameplay/band frame (the top-center FOREARM-FLOAT structure), which
> mesh — and which BONE and owning character member — drew it, with what material/pass state?

Today the R3 prov sidecar answers this for UI-cam draws but is **blind for world-cam skinned
draws** in exactly two ways (`R3-UIDUMP/STATUS.md`, §6.3):
- **(a) skinned rects are useless** — `rectKind=1` sphere fallback → near-full-viewport;
- **(b) no owner scope** for world draws (M2 hooks are Text/UILabel/PanelDir only).

T2 closes both, behind the existing default-OFF `RB3_DRAWLOG_PROV`, with one new default-OFF
A/B control knob. Flag-off byte-identical; the 792-draw drawlog golden is untouched.

---

## 1. Design decision — R-C / A1 resolution (BINDING)

The kickoff's declared engine region (`Rnd_Wgpu_RB3.cpp:3752-4009`, "mitten pre-pass composed
palette") is the **wrong block** and the mitten is hand-gated (`IsBandHandMesh`, :3752-3753).
Review **A1** re-scopes it: the reusable data is the **general skinned-palette compose loop
inside `BandRnd::DrawMesh`**, which runs for *every* skinned draw (band, crowd, extras, world),
and A1's cheapest sufficient form is the **bone WORLD translation `bt->WorldXfm().v`** — no new
palette tap.

**Decision (verified, not assumed):** compute the skinned bbox + per-bone sub-rects by
**RE-DERIVING bone worlds inside `RecordDrawProv`** (the existing sidecar sink) — *not* by
editing the 1500-line `DrawMesh` compose body. Justification, each point verified in-tree:

1. `RecordDrawProv(mesh, mat, mu.color, skinned, ctx.world)` is called at
   **`Rnd_Wgpu_RB3.cpp:5629-5630`** (`if (ProvOn())`), at the END of `DrawMesh`, so the compose
   loop has already run for this draw.
2. The skinned palette (`BoneUniforms bones{}`, **:3374**) is a **per-draw local recomputed
   every skinned draw** — there is NO cache-hit skip of the compose block between
   `skinned = owner->IsSkinned()` (**:2675**) and the loop at **:3776** (the only early
   `return`s in that window are the `RB3_SKIP_SKINNED`/`RB3_SKIP_STATIC`/name-iso probe exits
   and the "no geometry" exits that fire *before* `RecordDrawLog`, so they never reach the
   sidecar). Therefore every skinned draw in the prov ring had its bone worlds freshly forced
   by the `RB3_NO_SKEL_WORLDFIX` block (**:3656-3681**, `WorldXfm_Force` root→leaf).
3. `RecordDrawProv` re-derives the identical bones via `owner = mesh->GeomOwner()` (mirrors
   **:2546**), `owner->NumBones()` (**:3377**) clamped to `kMaxBones` (**:3378**),
   `owner->BoneTransAt(b)->WorldXfm().v` (mirrors **:3777/:3790**). Same objects, same fresh
   worlds. Zero intrusion into the compose loop → zero risk to the mitten / clamp / worldfix
   defaults.

**Per-bone sub-rect form:** for bone `b`, project both the bone world point AND its parent
(`bt->TransParent()->WorldXfm().v`) to screen; the sub-rect is the bbox of those two projected
points — i.e. the on-screen limb SEGMENT (elbow→wrist localizes the forearm). The overall mesh
rect (`rectKind=3`) is the union of all finite bone points. This gives a real screen extent per
bone (a single point would be a degenerate sub-rect).

**Skip rule (A1, "skip `sBonesIdentity`/fallback bones"):** apply the same finite guard the loop
uses at **:3827** (`fabs(v.{x,y,z}) < 1e5`); a non-finite/runaway bone world is skipped (it
never contributes to the bbox). Null `bt` skipped. A clamped bone (SKIN_CLAMP, :3948) still has
a real skeleton world → it IS included (the clamp only substitutes the bone's *vertices* to
bind; its world position is still where the structure is — correct for localization).

**Cache-bypass disclosure (A1 lane step 0):** if a skinned draw ever reaches `RecordDrawProv`
with `numBones==0` or all-non-finite bones, it CANNOT form a skinned bbox → it falls back to the
legacy sphere (`rectKind=1`) and the prov row SAYS so (rectKind=1 + `skinned:true` from the
existing drawlog `flags&8`). No silent sphere. Lane step 0 measures how many draws that is.

**ASSUMPTION (reviewer, attack):** `mActiveViewProjCpu` (written per-cam at **:1545**, ProvOn-
gated) holds the world.cam viewProj for world skinned draws at `RecordDrawProv` time. This is
the SAME matrix the existing `rectKind=0/1` paths already use (**:5783/:5803**), so world draws
are no worse off than today; if a world draw's last scene-uniform write was a different cam, its
rect is mis-projected — but that is a pre-existing property of the sidecar, not new to T2. The
known-answer gate (M4-G3) is what proves world-cam projection is actually correct in practice.

---

## 2. Anchors re-derived by symbol at HEAD

### Engine — `milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp` (@ `beb89e5`)
| Line | Symbol / role |
|---|---|
| 2530 | `void BandRnd::DrawMesh(RndMesh* mesh)` — the draw entry |
| 2546 | `RndMesh* owner = mesh->GeomOwner(); if(!owner) owner = mesh;` |
| 2675 | `bool skinned = owner->IsSkinned();` |
| 3374 | `BoneUniforms bones{};` — per-draw local palette (recomputed every draw) |
| 3377 | `int numBones = owner->NumBones();` (clamp `kMaxBones` :3378) |
| 3656-3681 | `RB3_NO_SKEL_WORLDFIX` force block — `WorldXfm_Force` root→leaf (fresh worlds) |
| 3776 | per-bone loop `for (int b=0; b<numBones; b++)` |
| 3777 | `RndTransformable* bt = owner->BoneTransAt(b);` |
| 3785-3789 | identity/fallback fill for null/`sBonesIdentity` bones (the "skip" set) |
| 3790 | `const Transform& wt = bt->WorldXfm();` |
| 3827-3831 | finite guard `fabs(wt.v.*) < 1e5f` else identity fallback |
| 3833 | `Multiply(owner->BoneOffsetAt(b), wt, skin);` |
| 4046-4048 | `MiloXfmToColMajor(skin, dst)` palette write |
| 1545 | `if (ProvOn()) memcpy(mActiveViewProjCpu, viewProj, ...)` — per-cam viewProj CPU copy |
| 5629-5630 | `if (ProvOn()) RecordDrawProv(mesh, mat, mu.color, skinned, ctx.world);` |
| 5670-5690 | `BandRnd::ProvOn()` / `RB3DrawProvEnabled()` cached gates |
| 5704-5713 | `RB3DrawScopePush/Pop(int kind, ...)` — scope stacks (0=panel,1=owner) |
| 5718-5722 | `BandRnd::ProvNotePassOpen` |
| 5727-5745 | `static bool RB3ProvProjectToScreen(world[16], vp[16], lx,ly,lz, vpW,vpH, &px,&py)` |
| 5747-5825 | `void BandRnd::RecordDrawProv(...)` — kind=0 verts (:5776), kind=1 sphere (:5789), kind=2 degenerate (:5819) |

### Engine — `milo-native-engine/src/platform/Rnd_Wgpu_RB3.h`
| Line | Member |
|---|---|
| 498 | `std::vector<RB3DrawProv> mDrawProv;` |
| 504-505 | `void RecordDrawProv(RndMesh*, RndMat*, const float boundColor[4], bool skinned, const float world[16]);` |
| 508 | `float mActiveViewProjCpu[16];` |
| 512-514 | `mProvPassCounter / mProvCurPassIdx / mProvCurPassDepthOp` |

### Engine — `milo-native-engine/src/platform/RB3DrawLogDebug.h`
| Line | Symbol |
|---|---|
| 61-74 | `struct RB3DrawProv { ... float rect[4]; uint8_t rectKind; ... }` |
| 89-100 | `RB3DrawScopePush/Pop(int,const char*)` + `struct RB3DrawScopeGuard` |

### rb3 game-side
| File:Line | Symbol / role |
|---|---|
| `native/src/rb3_http_handlers.cpp:218` | `RB3HttpServer::HandleDrawLog(Command&)` |
| `…:250-300` | `emitDraw` lambda — prov object emit at :280-298 |
| `…:302-322` | `wantRoi` rect-intersect filter + `lastWriter` |
| `native/src/rb3_uidump.cpp:97-130` | `EmitDraws` (authored↔prov join; byMesh/byOwner) |
| `src/system/char/Character.cpp:291` | `void Character::DrawShowing()` — owner-scope site (`Name()` :340 in-fn) |
| `src/system/rndobj/Text.cpp:26-49` | the `#ifdef HX_NATIVE` fwd-decl + `RB3ProvScope` RAII pattern to copy |
| `scripts/native/uidump_query.py:146` | `print_roi`; `:170` `mode_roi_query`; `:127` `fetch_roi` |
| `scripts/native/char-burst-capture.py` | boot→gameplay burst harness (production-smoke base) |
| `native/tests/goldens/drawlog/splash_screen.json` | the **792-draw** flag-off golden (captured `RB3_DRAWLOG=1`, prov OFF) |
| `milo-native-engine/src/platform/NativeCompatFlags.classification.json:518` | `RB3_DRAWLOG_PROV` row (mirror for new knob) |

---

## 3. Edits (exact sites, new files, flags, classjson)

All engine edits are the SOLE engine writes this wave (Lane P). rb3-side edits are the drawlog
handler + Character scope hook + query script. Every edit is inert when `RB3_DRAWLOG_PROV` is
unset.

### E1 — `RB3DrawLogDebug.h`: extend `RB3DrawProv` (parallel non-golden struct)
Add, after the existing `rectKind` field (:71):
```cpp
// T2-WORLDROI (Wave 19): skinned-pose provenance. rectKind==3 => bbox over the
// draw's composed bone WORLD points (world-cam skinned draws). boneRects localizes
// which BONE(s) a pixel ROI hit (bone->parent screen segment per bone). Empty for
// non-skinned draws and for skinned draws that fell back to sphere (rectKind==1).
struct RB3ProvBoneRect { std::string bone; float rect[4]; };
std::vector<RB3ProvBoneRect> boneRects;   // per-bone screen sub-rects (rectKind==3 only)
```
`rectKind` doc comment extended: `0 exact-verts, 1 sphere-fallback, 2 unavailable, 3 skinned-pose bbox`.
No change to `RB3DrawRecord` (the golden contract). No new default behavior.

### E2 — `Rnd_Wgpu_RB3.cpp`: skinned-pose branch in `RecordDrawProv` (:5747-5825)
1. Add a cached knob near the other prov gates:
   ```cpp
   static int RB3ProvSkinSphere() {           // A/B RED-baseline control (default OFF)
       static int s = -1;
       if (s < 0) { const char* e = getenv("RB3_PROV_SKIN_SPHERE"); s = (e && e[0] && e[0]!='0') ? 1 : 0; }
       return s;
   }
   ```
2. In `RecordDrawProv`, BEFORE the existing `vv`/sphere block (:5776), add a skinned branch:
   ```cpp
   if (skinned && !RB3ProvSkinSphere()) {
       RndMesh* owner = mesh ? mesh->GeomOwner() : nullptr;
       if (!owner) owner = mesh;
       int nb = owner ? owner->NumBones() : 0;
       if (nb > kMaxBones) nb = kMaxBones;
       for (int b = 0; b < nb; ++b) {
           RndTransformable* bt = owner->BoneTransAt(b);
           if (!bt) continue;
           const Transform& wt = bt->WorldXfm();
           if (!(std::fabs(wt.v.x)<1e5f && std::fabs(wt.v.y)<1e5f && std::fabs(wt.v.z)<1e5f)) continue;
           float bx, by;                        // bone world -> screen
           if (!RB3ProvProjectToScreen(world/*=identity for skinned; see note*/, mActiveViewProjCpu,
                                       wt.v.x, wt.v.y, wt.v.z, vpW, vpH, bx, by)) { anyBehind=true; continue; }
           // overall bbox
           if (bx<minx)minx=bx; if(by<miny)miny=by; if(bx>maxx)maxx=bx; if(by>maxy)maxy=by; gotAny=true;
           // per-bone sub-rect = bone->parent screen segment
           float sminbx=bx, minby=by, maxbx=bx, maxby=by;
           RndTransformable* par = bt->TransParent();
           if (par) { const Transform& pw = par->WorldXfm(); float px2,py2;
               if (std::fabs(pw.v.x)<1e5f && RB3ProvProjectToScreen(world, mActiveViewProjCpu,
                       pw.v.x,pw.v.y,pw.v.z, vpW,vpH, px2,py2)) {
                   if(px2<minbx)minbx=px2; if(py2<minby)minby=py2; if(px2>maxbx)maxbx=px2; if(py2>maxby)maxby=py2; } }
           RB3ProvBoneRect br; br.bone = bt->Name()?bt->Name():"";
           br.rect[0]=std::max(0.f,minbx); br.rect[1]=std::max(0.f,minby);
           br.rect[2]=std::min(vpW,maxbx)-br.rect[0]; br.rect[3]=std::min(vpH,maxby)-br.rect[1];
           p.boneRects.push_back(std::move(br));
       }
       if (gotAny) kind = 3;                     // else falls through to sphere below
   }
   ```
   **Projection-matrix note (BINDING for the implementer):** for a skinned draw the bone
   `WorldXfm().v` is already a WORLD-space point (the palette composes world-space skin — see
   the `vs_skinned` comment at **:3373**), and `ctx.world` for skinned meshes is identity (the
   skinned path uses the mesh world only via the palette). So the skinned branch must project
   the bone world DIRECTLY through `mActiveViewProjCpu` — i.e. pass an IDENTITY `world` to
   `RB3ProvProjectToScreen`, not `ctx.world`. Implementer: pass a local `float I16[16]` identity
   (or add a projection helper that skips the world stage). **ASSUMPTION (reviewer, attack):**
   skinned `ctx.world`==identity — verify once with a `[RENDER_DBG]`-style print on a band draw
   at lane step 0; if some skinned world draws carry a non-identity `ctx.world`, apply it (the
   bone point is then mesh-local and needs `world`). This is the single riskiest math point.
3. Guard the existing `vv` exact-vert block so it only runs when `kind != 3` (skinned already
   handled): change `if (vv && !vv->empty() ...)` gating so a skinned draw that produced a bbox
   does not also run the sphere/vert path. If the skinned branch produced NO points
   (`!gotAny`), control falls through to the existing sphere fallback (`kind=1`) — the disclosed
   bypass path.

No other engine file changes. `MittenBlendXfm`, the compose loop, the clamp, worldfix: untouched.

### E3 — `src/system/char/Character.cpp`: world-owner scope hook (`DrawShowing`, :291)
Copy the `#ifdef HX_NATIVE` forward-decl + `RB3ProvScope` RAII struct verbatim from
`Text.cpp:26-49` (it is file-local; duplicate it in an anon namespace at the top of
`Character.cpp` under `#ifdef HX_NATIVE`). Then at the very top of `Character::DrawShowing()`
(:292, before `START_AUTO_TIMER`):
```cpp
#ifdef HX_NATIVE
    RB3ProvScope _provOwner(1 /*owner*/, Name());   // pushes only when name non-empty + prov ON
#endif
```
RAII covers all return paths (incl. the crowd early-return at :329). Pushes the character's
`Name()` as the OWNER scope so every mesh drawn under this character (band member OR crowd)
snapshots `scopeOwner = <member name>` in `RecordDrawProv` (:5758, already reads
`sProvScopeOwner.back()`). Wii/matching build never compiles it → **byte-identical** (same
inertness proof as the shipped Text/PanelDir/UILabel hooks). No new field needed — `scopeOwner`
already exists and is serialized.

**ASSUMPTION (reviewer, attack):** `Character::Name()` is the useful owner label (the kickoff
referenced a "`rb3_char_probe` per-slot machinery" that does NOT exist in the tree — grep:
`native/src/*char*` = none). `Character::Name()` is the self-contained substitute; if the band
member identity is carried elsewhere (e.g. a `WorldObject` slot name), lane step 0 confirms
`Name()` is non-empty and distinct per member on a band frame, else switch the pushed string.

### E4 — `native/src/rb3_http_handlers.cpp`: serialize `boneRects` (:280-298)
In the `emitDraw` prov object, after the `rectKind` field, append a `boneRects` array when
`rectKind==3` (else omit — keeps every other row byte-for-byte as today):
```cpp
if (p.rectKind == 3 && !p.boneRects.empty()) {
    json += ", \"boneRects\": [";
    for (size_t k = 0; k < p.boneRects.size(); ++k) {
        const RB3ProvBoneRect& br = p.boneRects[k];
        snprintf(buf, sizeof(buf), "%s{\"bone\":\"%s\",\"rect\":[%.1f,%.1f,%.1f,%.1f]}",
                 k ? "," : "", RB3JsonEscape(br.bone).c_str(),
                 br.rect[0], br.rect[1], br.rect[2], br.rect[3]);
        json += buf;
    }
    json += "]";
}
```
The ROI intersect filter (:302-315) is UNCHANGED — a `rectKind==3` bbox intersects the ROI via
the same `p.rect` test (skinned bbox is a real positioned rect, no longer near-full-viewport).
Bone-level narrowing is done client-side in the query script against `boneRects` (E5).

### E5 — `scripts/native/uidump_query.py`: report bones + owner in `print_roi` (:146)
Extend `print_roi` to, for each matched draw, additionally print the `boneRects` whose rect
intersects the ROI (naming the bone(s)) and the `owner` (already in `prov`). Add a
`--roi` sub-report line per draw: `bones=[<names intersecting ROI>]`. No new mode; the existing
`mode_roi_query` (:170) already fetches `/api/drawlog?roi=` and `/api/uidump`. Add a small
`bones_in_roi(prov, roi)` helper.

### E6 — classjson append (under `/tmp/milo-engine-classjson.lock`, append-only)
Add ONE row (the only new env flag) mirroring the `RB3_DRAWLOG_PROV` row at :518:
```json
 "RB3_PROV_SKIN_SPHERE": {
  "class": "probe",
  "owner": "render/ui-forensics",
  "faithfulStatus": "n/a: Wave-19 T2-WORLDROI A/B control — forces skinned draws back to the legacy rectKind=1 sphere fallback (the R3 v1 world-cam blindness) so the skinned-pose bbox can be diffed against it. Test-only; default preserves rectKind=3 skinned bbox. Implies/requires RB3_DRAWLOG_PROV.",
  "default": "off",
  "read": "presence"
 }
```
NO regen (coordinator regenerates once at close-out). NO default flips, NO pin bump.
`RB3_DRAWLOG_PROV` itself already has a row — the `boneRects`/`rectKind=3` extension needs no
new flag.

---

## 4. Milestones (M1..M5) with per-milestone exit criteria

Build in an **own build dir** (`tools/setup-worktree.sh` is not required; use
`cmake --build native/build-agent-W19-T2` or the shared dir under `/tmp/rb3-native-build.lock`).

- **M0 (lane step 0 — flavor + coverage audit; NO edits).**
  - Confirm the four T2-touched TUs compile into rb3-native: `Character.cpp` (already engine-
    consuming), `rb3_http_handlers.cpp`, `rb3_uidump.cpp`, `Rnd_Wgpu_RB3.cpp` — all present.
  - Boot to gameplay with `RB3_DRAWLOG_PROV=1` and confirm skinned world draws reach the compose
    loop (existing `SKIN_CLAMP_PROBE`/`RB3_CROWD_BONE_PROBE` fire on world draws) → validates the
    re-derivation freshness assumption (§1.2).
  - One-line print of a band skinned draw's `ctx.world` to confirm identity (§E2 projection
    assumption). **Exit:** the two assumptions (skinned `ctx.world`≈I; compose runs for world
    skinned draws) are confirmed or the plan's projection path is corrected before M1.

- **M1 — struct + engine skinned bbox + serialize (E1, E2, E4).**
  **Exit:** rb3-native builds (own dir). With `RB3_DRAWLOG_PROV=1` on a band gameplay frame, at
  least one skinned draw reports `rectKind:3` with a non-empty `boneRects[]` and a positioned
  `rect` (NOT near-full-viewport). Flag-OFF `drawlog-golden.py --fixed-clock` = **792 draws,
  byte-identical** (prov off ⇒ `mDrawProv` empty ⇒ `RecordDrawProv` never called).

- **M2 — owner scope hook (E3).**
  **Exit:** with prov ON, band/crowd skinned draws carry `prov.owner == <Character::Name()>`
  (non-empty, distinct across members on a band-wide frame). `drawlog-golden.py --fixed-clock`
  still 792 byte-identical (hook is `#ifdef HX_NATIVE` + prov-gated; verify the golden build,
  which is HX_NATIVE, is byte-identical because the push is a no-op when prov is off — the
  cached-branch inertness of `RB3DrawScopePush`, :5704).

- **M3 — query script + bone reporting (E5) + coverage number.**
  **Exit:** `uidump_query.py --port P --roi X,Y,W,H` against a running band frame prints, per
  matched draw, `mesh / owner / bones=[…] / mat / pass`. Report the coverage count: of all
  skinned draws in the frame, N have `rectKind:3`, M fell back to `rectKind:1` (disclosed
  bypass). Commit the count as evidence.

- **M4 — gates (see §5). Exit:** all three gates GREEN with their fail-reds shown; classjson row
  appended (E6).

- **M5 — production smoke (triage only; see §6). Exit:** one query on a burst_08-class frame
  NAMES the FOREARM-FLOAT structure's mesh / bone / owner; the answer committed under
  `execution/T2-WORLDROI/evidence/`.

---

## 5. Gates — each with its fail-red demonstration (BINDING)

All gates run headless (`RB3_HTTP=1 RB3_FIXED_CLOCK=1`), free port, frame-count settling,
pgid-only cleanup. Evidence committed under `execution/T2-WORLDROI/evidence/` or it doesn't
exist (§4 lint 7).

- **G1 — RED baseline: v1 sphere-rect blindness mislocates; skinned bbox localizes (fail-red 1,
  the charter's RED baseline).**
  Two arms on the SAME band gameplay ROI (a top-center band-structure ROI):
  - RED arm `RB3_PROV_SKIN_SPHERE=1` → skinned draws report `rectKind:1`, near-full-viewport
    rects → the ROI intersect returns an UNDISCRIMINATED set (many skinned draws; the rects do
    not separate the structure from the rest of the band).
  - GREEN arm (default, `RB3_PROV_SKIN_SPHERE` unset) → `rectKind:3` positioned bbox + bones →
    the ROI narrows to a BOUNDED set naming a specific mesh + bone(s).
  **GREEN iff** `count(rectKind3 draws intersecting ROI) < count(sphere draws intersecting ROI)`
  by a measurable margin AND the GREEN arm names ≥1 mesh+bone the RED arm could not. The knob is
  its own fail-red control (RED = the documented old behavior). Because the band render is
  non-deterministic across runs (`R5-MITTEN/frame_pairing_note.txt`), G1 is measured **within a
  single boot** (flip the knob is a rebuild → instead capture BOTH the sphere-forced and bbox
  numbers from ONE boot by querying twice is impossible with a compile-time-cached knob; so run
  two boots and compare the ROI-set CARDINALITY, which is robust to camera-cut nondeterminism —
  the sphere arm returns ~all skinned draws regardless of frame, the bbox arm returns few). This
  cardinality contrast is the fail-red, not a pixel diff.

- **G2 — disjoint-ROI negative control (fail-red 2, the charter's negative control).**
  On the SAME boot as the GREEN arm, query a corner/background ROI (e.g. a top-left 80×80 patch
  known to be venue/backdrop, no band). **GREEN iff** the owner set returned is DISJOINT from the
  band-structure ROI's owner set (ideally empty / venue-only) — guards against "everything
  matches everywhere," the exact v1 blindness. Fail-red: if the disjoint ROI returns the SAME
  owner/mesh set as the structure ROI, the bbox is still full-viewport → gate RED.

- **G3 — known-answer check (positive control).**
  Query an ROI over a visible FRETTING/instrument HAND on a band frame. **GREEN iff** the named
  mesh is a hand mesh (matches `IsBandHandMesh` classification) and the `boneRects` intersecting
  the ROI name finger/wrist bones consistent with `HandBoneRole`/`HandBoneSide` (the engine's
  own hand-bone classifier). This proves the world-cam projection + bone naming is CORRECT, not
  just bounded. (Reuses the mitten's hand-bone taxonomy as the oracle — an independently-shipped
  classifier, so this is not a self-referential gate.)

- **G4 — flag-off golden invariance (regression net).**
  `drawlog-golden.py --fixed-clock --scene splash_screen` (prov OFF) = **792 draws,
  byte-identical** to `native/tests/goldens/drawlog/splash_screen.json`, on the T2 build. Plus
  `--fail-red-audit` confirms the comparator still reads RED on a perturbation. This is the
  BINDING "flag-off drawlog-golden 792 canonical byte-identical" contract.

---

## 6. Production smoke — FOREARM-FLOAT triage (M5, triage ONLY, no fix)

Per charter + `R5-HANDS-ENDGAME/CLOSURE.md` post-flip addendum, the **FOREARM-FLOAT** backlog
key is: *a persistent top-center floating flesh-colored structure in the burst_08/12 frames,
unchanged OFF→ON (forearm/prop-level, NOT finger-level; Wave-9 "disconnected floating forearm"
lineage).* T2's job is to NAME it, not fix it.

**Protocol:** adapt `scripts/native/char-burst-capture.py` (boot→gameplay burst) with
`RB3_DRAWLOG_PROV=1`. At each burst screenshot, detect the top-center flesh-colored structure
(a coarse skin-tone mask over the upper-center band, mirroring `char-burst`'s framing; the
burst_08 reference is `R5-MITTEN/evidence/ON_burst_08.png`, male guitarist, right). On a frame
where it is present, take its screen ROI and run `/api/drawlog?roi=` + `uidump_query.py`.
**Deliverable (committed evidence):** the ROI report naming the structure's `mesh`, the
`boneRects` bone(s) it intersects, and the `owner` (which band member). Because the band frame is
non-deterministic, this is triage output on ONE captured frame — NOT an A/B gate, NOT a fix. Any
fix is a future charter.

**Fail-safe:** if the structure is not skin-masked reliably, fall back to a manual ROI over the
top-center region of the captured `ON_burst_08`-class screenshot and report what the query names
there. The bar is "the instrument produces a mesh/bone/owner answer for that pixel region," which
is exactly the one-query replacement for the eyeball-lineage triage the charter promises.

---

## 7. What is verified vs assumed

**Verified in-tree (@ beb89e5 / rb3 master):**
- The compose loop + worldfix recompute every skinned draw that reaches `RecordDrawProv` (no
  cache-skip gate); bone worlds are fresh (§1.2).
- `RecordDrawProv` is the single sidecar sink, called at :5630 under `ProvOn()`; `RB3DrawProv`
  is a parallel non-golden struct; the 792 golden is captured `RB3_DRAWLOG=1` (prov OFF) so
  `boneRects`/`rectKind=3` cannot touch it.
- `Character::DrawShowing` (:291) is a clean single owner-scope site covering band + crowd, and
  the `#ifdef HX_NATIVE` RAII hook pattern (Text.cpp:26-49) is byte-identical on Wii.
- `mActiveViewProjCpu` is written per-cam (ProvOn-gated, :1545) incl. world.cam.
- The ROI intersect (:302-315) works unchanged on a `rectKind=3` bbox.

**ASSUMPTIONS (reviewer attacks; each has a lane-step-0 or gate check):**
- A-1: skinned `ctx.world` ≈ identity so bone world points project directly through
  `mActiveViewProjCpu` (§E2; checked at M0; the single riskiest math point).
- A-2: `Character::Name()` is the useful owner label (no `rb3_char_probe` exists; §E3; checked
  at M0).
- A-3: `mActiveViewProjCpu` holds the world.cam viewProj for world skinned draws at record time
  (§1; G3 known-answer proves it end-to-end).

---

## 8. Collision statement vs the other two Wave-19 lanes

- **T1-FRAMETRACE** owns `scripts/native/loaddet_gate.py` + `execution/R4-M4/wash_cosample.py` +
  `native/src/rb3_loaddet_probe.cpp` + Loader/ThreadCall + `Rand.{h,cpp}` (per review A5).
  **T2 touches NONE of these.**
- **W-ISO** owns `Rand.h/.cpp` tags + the four consumer TUs (`CharClipDriver.cpp`, `Crowd.cpp`,
  `CharInterest.cpp`, `LightPresetManager.cpp`) + `scripts/native/capture_lints.py`.
  **T2 touches NONE of these.** T2's owner-scope hook lives in `Character.cpp` (NOT in any
  W-ISO consumer TU) and `Crowd.cpp` is untouched by T2.
- **T2 is the SOLE ENGINE WRITER** this wave; its engine edits are confined to
  `Rnd_Wgpu_RB3.cpp` (RecordDrawProv only), `Rnd_Wgpu_RB3.h` (none required — no new member;
  `boneRects` lives on `RB3DrawProv`), and `RB3DrawLogDebug.h` (the struct). This is **disjoint**
  from the only dirty engine file `FxSendNative.cpp` (audio) — never stage it. Never stage rb3's
  uncommitted `native/src/rb3_session_trace.cpp`.
- **classjson:** one appended row (`RB3_PROV_SKIN_SPHERE`) under
  `/tmp/milo-engine-classjson.lock`; no regen.

---

## 9. Disposition / flag summary

| Flag | Default | Role |
|---|---|---|
| `RB3_DRAWLOG_PROV` | OFF (existing) | master gate; T2 rides it (rectKind=3 + boneRects + owner scope) |
| `RB3_PROV_SKIN_SPHERE` | OFF (NEW) | A/B control: force legacy sphere for skinned (G1 RED baseline) |

No default-ON flip. No pin bump. Flag-off byte-identical (792 golden). Any fix to FOREARM-FLOAT
is a FUTURE charter — this lane is instrument + triage only.
