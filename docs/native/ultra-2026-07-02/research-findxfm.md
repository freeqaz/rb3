# FindXfm research — BandPatchMesh::FindXfm (58.7% AT_LIMIT)

Symbol: `FindXfm__13BandPatchMeshFP7RndMeshRC7Vector2R9Transform`
Unit: `main/system/bandobj/BandPatchMesh` (`src/system/bandobj/BandPatchMesh.cpp:1088`)
Target asm extracted to study: `build/SZBE69_B8/asm/system/bandobj/BandPatchMesh.s` (fn is 903 lines / 963 instrs, addr 8052CBxx–8052D8xx).
Bank divergence: **NO_DWARF** (Bank 8 only — Bank5 `--bank5` is useless here; m2c + raw asm are the ground truth).
DC3: **no FindXfm anywhere in DC3** (`lookup_dc3` empty) — this is RB3-specific code, no reference port.

Current objdiff: 58.6% — 483 diff_arg (mostly frame-offset noise: target frame **0xa60** vs base **0xa20**, Δ-0x40 = missing memory-backed locals), 104 insert / 201 delete (structural), REGISTER_SWAP 537 (f2↔f3 dominant), 2 branch-polarity inversions, stack: 20 SWAPPED / 41 DIFFER / **15 TGT-only slots**.

## Headline discovery #1 — every accessor call is visible in the asm

`RndMesh::Verts()/Faces()/Verts(i)` all go through `mGeomOwner->…` where `mGeomOwner` is
`ObjOwnerPtr<RndMesh>` (Mesh.h:357) whose `operator->` is `MILO_ASSERT(mPtr, 0xAB)`
(ObjPtr_p.h:141). The target contains **exactly 13 `Debug::Fail` sites, ALL at line 0xAB**
(`grep -c 0xab` = 13; **zero** at 0x6C5). Consequences:

1. **HEAD's explicit `MILO_ASSERT(mesh->GeomOwner(), 0x6C5)` is NOT in the binary — delete it** (wave-3 correctly removed it).
2. **The target NEVER caches `&Faces()[0]` / end into locals across the loops.** Every loop
   condition re-evaluates `mesh->Faces().end()` (1 accessor call = 1 assert site), every vert
   fetch is a fresh `mesh->Verts(idx)`. HEAD's cached `faceBase/endFace` removes ~200 target
   instructions → the 201 "delete" count. The 13 assert sites map 1:1 to accessor calls:
   Verts().size, Faces().size, found=Faces().end(), iter=Faces().begin(), loop1-cond end(),
   loop1 Verts(f[2]), loop1-inner Verts(*it), found==end() re-check, loop2 begin(),
   loop2-cond end(), loop2 Verts(p[2]), loop2-inner Verts(*it), gather Verts(*found++).
   Wave-3's `(unsigned short*)&mesh->Faces()[0] + mesh->Faces().size()*3` is TWO calls
   (2 asserts) per site — target has ONE. Use `(unsigned short *)mesh->Faces().end()`
   (stlport iterator is a plain `Face*`).

## Headline discovery #2 — HEAD's tail is semantically WRONG vs the binary (4 proven bugs)

All verified directly in raw asm (offsets given so the implementer can re-check):

1. **Tautology confirmed a transcription bug, and the binary does NOT have the OOB.**
   Target: `found` is initialized to `Faces().end()`, the containing-triangle loop sets
   `found = f; break;` on `matched == 3`, then `if (found == (ushort*)mesh->Faces().end())`
   (re-computed, 1 assert) runs a **full begin→end nearest-edge scan** updating `found` on
   every closer edge. Wave-3's found/fallback restructure was asm-CORRECT; no
   `#ifdef HX_NATIVE` safety bound is needed — the faithful version is safe by construction.
   (In HEAD the fallback loop can never execute — `facePtr` starts equal to `endFace` in both
   branches — and the not-found case dereferences `Faces().end()[0..2]` = real OOB.)

2. **Target WRITES `xfm.v` and `xfm.m.z`; HEAD never does.** At 8052D648–8052D6E8:
   `Vector3 p(uv.x, uv.y, 1.0f)` (stack 0x80) is multiplied through **posOut** (sp198) and
   **normOut** (sp174) — `psq_st f4, 0x24(r26)` = xfm.v, `psq_st f8, 0x18(r26)` = xfm.m.z —
   then `Normalize(xfm.m.z, xfm.m.z)` inline (zero-check + frsqrte at 8052D6DC–D758).
   This is the **vector-first** overload `Multiply(const Vector3&, const Hmx::Matrix3&, Vector3&)`
   (Mtx.h:646 — row-combination v^T·M; the ps_muls1/ps_madds0 row-z/y/x pattern matches
   exactly). NOT the matrix-first Mtx.h:397 overload (that computes M·v = transposed result —
   classic wave-3-style transposition trap).
   Downstream impact: `ProjectPatches(const Transform&,…)` (line 981) builds its collide
   segment from `xfm.v` and `xfm.m.z` — with HEAD, `PreRender`'s `Transform tf60` is passed
   uninitialized and FindXfm only fills m.x/m.y, so the mTexture!=-1 patch path (face paint /
   tattoos) collides along a **garbage segment** on every platform. HEAD is NOT "observably
   identical to the Wii binary" here; it's UB that happens not to be visually gated.

3. **Negation is on unk10 (posOut.y row), not unk4.** 8052D804–D84C: `centerMV.unk4 =
   posOut.x` (stored un-negated from sp198), `centerMV.unk10 = posOut.y * -1.0f` (lfs
   sp1A4/1A8/1AC, `fmuls` with `@F_000080bf`, store 0xE0–E8). HEAD negates unk4 instead.
   Also: the multiply-by-−1 is computed from posOut then stored once → write it as
   `Scale(posOut.y, -1.0f, centerMV.unk10);` (Vec.h:223) or per-component
   `unk10.x = posOut.y.x * -1.0f;` — NOT HEAD's store-then-`*= -1.0f` (that reloads from 0xE0).

4. **Scale factor is `0.5f * Length(...)`, NOT `0.5f / Length(...)`.** The whole tail has
   exactly ONE `fdivs` (8052D00C — the fallback loop's `t` division). f31/f30 are the plain
   sqrt Newton refinements of Length(posOut.x/y) (frsqrte→ x·rsqrt(x)=sqrt), computed at
   8052D5C8–D644 BEFORE the xfm.v/m.z multiplies, and the final rows are
   `xfm.m.x = centerMV.unk4 * (0.5f*lenX)`, `xfm.m.y = centerMV.unk10 * (0.5f*lenY)`
   (8052D854–D8AC). HEAD's `invRowX = 1.0f/Length(...)` is inverted.

Additional tail facts:
- `RndMesh::Vert centerVert;` is default-constructed (pos stays **(0,0,0)** — HEAD's
  `pos.Set(uv.x,uv.y,1)` is wrong; the (u,v,1) vector is the separate local `p`), then
  `centerVert.norm = xfm.m.z;` (3 scalar copies reloading 0x18(r26) at 8052D7E8), then
  `SetVert`, unk4/unk10 fill, `centerMV.Normalize(1)` (bl), final row stores.
- Matrix fill after gather is **interleaved per-vertex** (asm 8052D154+: for triVerts[0]:
  uv.y, uv.x, 1.0f → uvMat.x; pos → posMat.x; norm → normMat.x; then triVerts[1]…), keeping
  HEAD's y-before-x quirk within the uv row. HEAD groups by matrix instead.
- Gather loop walks the pointer: `triVerts[i] = &mesh->Verts(*found++);` (var_r30 += 2 each
  iter), do-while i<3, single accessor call inside.
- `Invert(uvMat, uvMat)` is a real call (matches HEAD); the two matrix Multiplies inline with
  the alias check (`&dest != &b`) — HEAD's existing `Multiply(uvMat, posMat, posOut)` form is fine.

## Loop-body micro-structure (asm-derived)

Loop 1 (containing triangle), per m2c:
- inner loop counter IS `matched` — `do { … if (cross*firstSign < zero) break; matched++;
  v0 = v1; iter++; } while (matched < 3);` then `if (matched == 3) { found = f; break; } f += 3;`
  (wave-3 got this right; HEAD's separate `j` counter is wrong).
- `float zero = 0.0f` lives in f29 across the loop; `if (zero == firstSign) firstSign = cross;`
  `cross = d.x*e.y - d.y*e.x` — identical to HEAD.
- **d and e are memory-backed Vector2 locals** (unconditional stfs to sp30/sp34 and sp28/sp2C
  every iteration = dead aggregate stores). Write them as
  `Vector2 d(uv.x - v0->uv.x, uv.y - v0->uv.y); Vector2 e(v1->uv.x - v0->uv.x, …);`
  (Vector2(float,float) ctor exists, Vec.h:12). HEAD's plain floats never store → part of the
  15 TGT-only stack slots and the Δ-0x40 frame.

Loop 2 (nearest edge fallback):
- `float minDistSq = 1e30f` (`@F_caf24971` = 0x4971F2CA); `t = (d.x*e.x + d.y*e.y) /
  (e.x*e.x + e.y*e.y)`; clamp with literals 0.0f/1.0f (pool statics @26143/@26144);
  `Vector2 closest(v0->uv.x + e.x*t, v0->uv.y + e.y*t); Vector2 dd(closest.x - uv.x, …);`
  — d/e/closest/dd are all Vector2 stack locals (sp20/sp18/sp10/sp8, declared in that order).
- **Bool materialization confirmed**: `if (distSq < minDistSq) { minDistSq = distSq; better=1; }
  else better=0; if (better) found = p;` (m2c var_r0) — exactly wave-3's `int better` shape.
- inner loop is `do { … j++; } while (j < 3);` (no break), outer `p += 3` with
  end() recomputed in the condition.

## Wave-3 hunk verdict (git show 4a49b1a4)

RIGHT (keep): remove 0x6C5 assert; recompute Faces() bound in loop conditions; found-vs-end
real comparison + full-scan fallback; `do/while(matched<3)` in loop 1; `int better`
materialization; do-while j in loop 2. Its FindXfm semantics were correct — the visual
breakage that forced revert 82f390b1 came from ExtendTwin/AddEdge, not this function.

WRONG/INCOMPLETE: two-accessor-call bound recompute (use one `Faces().end()` cast); tail left
untouched (bugs 2–4 above = the remaining ~37%); no Vector2 locals; kept `for` in loop-2
converted only halfway. It reached 63.4%; the tail rewrite is the big remaining lever.

## Recommended implementation sketch

```cpp
bool BandPatchMesh::FindXfm(RndMesh *mesh, const Vector2 &uv, Transform &xfm) {
    if (mesh->Verts().size() == 0 || mesh->Faces().size() == 0) {
        TheDebug.Notify(FormatString("Patches can't project onto %s, has no verts or faces!").Str());
        return false;
    }
    unsigned short *found = (unsigned short *)mesh->Faces().end();
    unsigned short *f = (unsigned short *)mesh->Faces().begin();
    float zero = 0.0f;
    while (f != (unsigned short *)mesh->Faces().end()) {
        RndMesh::Vert *v0 = &mesh->Verts(f[2]);
        int matched = 0;
        float firstSign = 0.0f;
        unsigned short *iter = f;
        do {
            RndMesh::Vert *v1 = &mesh->Verts(*iter);
            Vector2 d(uv.x - v0->uv.x, uv.y - v0->uv.y);
            Vector2 e(v1->uv.x - v0->uv.x, v1->uv.y - v0->uv.y);
            float cross = d.x * e.y - d.y * e.x;
            if (zero == firstSign) firstSign = cross;
            if (cross * firstSign < zero) break;
            matched++; v0 = v1; iter++;
        } while (matched < 3);
        if (matched == 3) { found = f; break; }
        f += 3;
    }
    if (found == (unsigned short *)mesh->Faces().end()) {
        float minDistSq = 1e30f;
        for (unsigned short *p = (unsigned short *)mesh->Faces().begin();
             p != (unsigned short *)mesh->Faces().end(); p += 3) {
            RndMesh::Vert *v0 = &mesh->Verts(p[2]);
            unsigned short *iter = p;
            int j = 0;
            do {
                RndMesh::Vert *v1 = &mesh->Verts(*iter);
                Vector2 d(uv.x - v0->uv.x, uv.y - v0->uv.y);
                Vector2 e(v1->uv.x - v0->uv.x, v1->uv.y - v0->uv.y);
                float t = (d.x * e.x + d.y * e.y) / (e.x * e.x + e.y * e.y);
                if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
                Vector2 closest(v0->uv.x + e.x * t, v0->uv.y + e.y * t);
                Vector2 dd(closest.x - uv.x, closest.y - uv.y);
                float distSq = dd.x * dd.x + dd.y * dd.y;
                int better = 0;
                if (distSq < minDistSq) { minDistSq = distSq; better = 1; }
                if (better) found = p;
                v0 = v1; iter++; j++;
            } while (j < 3);
        }
    }
    RndMesh::Vert *triVerts[3];
    for (int i = 0; i < 3; i++) triVerts[i] = &mesh->Verts(*found++); // pointer-walk form
    Hmx::Matrix3 uvMat, posMat, normMat;                // fill PER-VERTEX, uv.y before uv.x:
    uvMat.x.y = triVerts[0]->uv.y; uvMat.x.x = triVerts[0]->uv.x; uvMat.x.z = 1.0f;
    posMat.x = triVerts[0]->pos;  normMat.x = triVerts[0]->norm;
    uvMat.y.y = triVerts[1]->uv.y; uvMat.y.x = triVerts[1]->uv.x; uvMat.y.z = 1.0f;
    posMat.y = triVerts[1]->pos;  normMat.y = triVerts[1]->norm;
    uvMat.z.y = triVerts[2]->uv.y; uvMat.z.x = triVerts[2]->uv.x; uvMat.z.z = 1.0f;
    posMat.z = triVerts[2]->pos;  normMat.z = triVerts[2]->norm;
    Invert(uvMat, uvMat);
    Hmx::Matrix3 posOut;  Multiply(uvMat, posMat, posOut);
    Hmx::Matrix3 normOut; Multiply(uvMat, normMat, normOut);
    float lenX = Length(posOut.x);              // NO reciprocal
    float lenY = Length(posOut.y);
    Vector3 p(uv.x, uv.y, 1.0f);
    Multiply(p, posOut, xfm.v);                 // vector-FIRST overload (Mtx.h:646)
    Multiply(p, normOut, xfm.m.z);
    Normalize(xfm.m.z, xfm.m.z);
    RndMesh::Vert centerVert;                   // pos stays (0,0,0)
    centerVert.norm = xfm.m.z;
    MeshVert centerMV;
    centerMV.SetVert(&centerVert);
    centerMV.unk4 = posOut.x;                   // NOT negated
    Scale(posOut.y, -1.0f, centerMV.unk10);     // negate the Y row (or per-component * -1.0f)
    centerMV.Normalize(1);
    float scaleX = 0.5f * lenX;
    float scaleY = 0.5f * lenY;
    xfm.m.x.x = centerMV.unk4.x * scaleX;  xfm.m.x.y = centerMV.unk4.y * scaleX;  xfm.m.x.z = centerMV.unk4.z * scaleX;
    xfm.m.y.x = centerMV.unk10.x * scaleY; xfm.m.y.y = centerMV.unk10.y * scaleY; xfm.m.y.z = centerMV.unk10.z * scaleY;
    return true;
}
```

Iterate the tail details (exact order of lenX/lenY vs p construction, Vert-copy forms, decl
order for the SWAPPED stack pairs) against `run_objdiff` / `/compare-asm`; the 13-assert-site
count is a quick structural self-check (`grep -c "0xab"` on the built fn's disasm must be 13).

## Expected achievable %

Structure is now fully determined from the asm; the wave-3 partials (63.4% with only the loop
half done) plus the tail being long, call-pinned (SetVert/Normalize/Invert bls and 13 assert
branches act as scheduling anchors) suggest **≥90% is near-certain, 100% is a realistic goal**.
Residual risk is FPR allocation in the two ps-heavy Multiply regions (f2↔f3 family —
historically permuter-class) — if it stalls at 97–99, that's the cause.

## Verification requirements (do not skip)

- This rewrite CHANGES native runtime behavior vs HEAD (fixes the uninit `xfm.v`/`m.z` and
  the OOB not-found path; flips negation row and scale formula). It is asm-proven, but per the
  wave-3 rule any sub-100 shared geom/anim change needs the **native visual gate**:
  `scripts/native/band-closeup-capture.py` on a character with face-paint/tattoo patches
  (the `patch.mTexture != -1` path in `PreRender` is the ONLY FindXfm caller), judged for
  coherence across a few prefab rotations. At exactly 100% the change is automatically safe.
- Full-rebuild regression gate over the unit (FindXfm shares headers/inlines with the other
  three BandPatchMesh residuals — watch AddEdge/ExtendTwin/TryAddFace don't regress).

## Key asm evidence quick-index

- `/tmp/findxfm.s` (extracted; regenerate with awk over the unit asm)
- 13× `li r5, 0xab` = accessor asserts, no 0x6c5
- 8052D6D0/8052D6E0: `psq_st f8, 0x18(r26)` / `psq_st f4, 0x24(r26)` → m.z / v writes
- 8052D804–D84C: unk4 from 0x198 un-negated; unk10 = 0x1A4 row × `@F_000080bf` (−1.0f)
- 8052D868/D870: `fmuls f4, f3(0.5f), f31` / `f3 = 0.5f * f30` → scale = 0.5·Length
- one `fdivs` total (8052D00C, the `t` division) → no 1/Length anywhere
