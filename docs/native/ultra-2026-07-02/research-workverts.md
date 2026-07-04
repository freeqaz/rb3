# BandPatchMesh::WorkVerts trio — asm-derived semantics & reconstruction (research handoff)

Unit: `main/system/bandobj/BandPatchMesh` · Target asm: `build/SZBE69_B8/asm/system/bandobj/BandPatchMesh.s`
(AddEdge @ line 2986, ExtendTwin @ 3178, TryAddFace @ 3463).

**All findings below were derived directly from the Bank-8 target asm and then
VERIFIED BY COMPILING** in worktree `~/code/milohax/rb3/.claude/worktrees/workverts-research`
(left in place, builds green, zero sibling regressions). Full source delta:
[`workverts-asm-derived.patch`](workverts-asm-derived.patch) in this directory.

## Measured results (HEAD → worktree)

| Function | HEAD | Worktree | Residual class |
|---|---|---|---|
| AddEdge     | 76.3% | **99.76%** | 3× `cmpw r0,r24` vs `cmpw r24,r0` operand order — permuter-class (both source spellings tried; neither flips it) |
| TryAddFace  | 89.2% | **92.92%** | monotonicity-block (b!=3) FPR/GPR scheduling cascades; `diff_op: none`; maybe +1-2pp from decl-order permutes |
| ExtendTwin  | 72.6% | **94.47%** | pure register-number cascades: `diff_op: none`, `insert/delete: none` — every remaining mismatch is arg naming on an IDENTICAL opcode sequence |
| (siblings)  | — | unchanged | SetMeshVerts/AddFace/SpreadEdges/AddUvs/Set+AddMeshVertAndTwins/SetVertsAndFaces/Project byte-identical % vs main baseline |

ExtendTwin at 94.5 with zero insert/delete/diff_op is the strongest possible
sub-100 semantic proof: the compiled operation sequence equals the binary's
instruction-for-instruction; only register NUMBERS differ.

## Per-function target-asm semantics

### AddEdge (target = wave-3's structure; wave-3 was RIGHT here)

Target asm facts (read them at `.s` line 2986ff):
- `lwz r24, 0x28(r5)` at ENTRY: `mv1->unk28` IS hoisted once into a callee-saved
  reg and never reloaded. The revert message's claim ("old code deliberately
  re-read it fresh; TryAddFace mutates the table mid-loop") is wrong on both
  counts: the binary hoists, and nothing in TryAddFace/AddMeshVertAndTwins/
  SetVert/AddUV/Normalize ever writes `unk28`/`unk2c` (checked each).
- There is NO null check on `mv1` (the `mv1 &&` in HEAD is decomp cruft). Both
  callers (SpreadEdges, SetSameVerts line ~888 `AddEdge(partner, mv)`) always
  pass non-null.
- The three per-corner tests FALL THROUGH: if `idx == face[0]` but the unk28
  test fails, the binary still tests face[1], face[2] (`bne .L_805275E4` goes to
  the NEXT corner, not loop end). HEAD's else-if chain stops. => the true source
  is a `for (b=0;b<3;b++) if (idx==face[b] && ...) { TryAddFace(faceidx,b); break; }`
  loop, unrolled by MWCC.
- The `(b+1)%3` is computed at RUNTIME per unrolled arm with the 0x55555556
  magic-divide on a CONSTANT (li r0,1 / mulhw / mulli / subfic) — signature of a
  `%3` inside an unrolled constant-trip loop that MWCC does not fold. This is
  unreachable from the else-if-chain form; it is why the loop form jumps to ~99.3.
- Face-list access: `lhz 0x32(rN)` with the pointer stepping by 2 and the 0x32
  kept as load displacement => a MEMBER-array access (`mv->faceList[i]`), not
  `(u16*)((char*)mv + 0x32)` pointer arithmetic (which emits `addi rN, mv, 0x32`
  and `lhz 0x0`). Fixed via a trailing `unsigned short faceList[1];` member —
  see "header change" below. Worth +0.5pp here and it is the same final blocker
  pattern in ExtendTwin.

Recommended source: exactly the worktree version (see patch). Keep `int mv1Idx`
as an explicit local (inlining `mv1->unk28` in the condition does NOT
compiler-hoist — measured 96.0, worse).

### TryAddFace (one REAL structural bug at HEAD + small forms)

Target asm facts:
- **THE find: the temp vert is built with the TWO-ARG copy SetVert.**
  `bl SetVert__...FPCQ213BandPatchMesh8MeshVertPCQ27RndMesh4Vert` with
  args `(&temp, verts[0], verts[0]->mVert)` — i.e.
  `temp.SetVert(verts[0], verts[0]->mVert);` which copies verts[0]'s
  unk4/unk10/unk1c/unk26. HEAD calls the ONE-ARG zeroing overload
  (`temp.SetVert(verts[0]->mVert)`), so HEAD's `Vector2 v(temp.unk1c)` is (0,0)
  instead of verts[0]'s accumulated patch UV. That changes AddUV's 0.25-radius
  monotonicity gate (centered at origin instead of at verts[0]) — a REAL
  semantic divergence from the running game at HEAD.
- Corner loop: `int fi = face[i]; MeshVert *mv = mMeshVerts[fi]; verts[i] = mv;`
  — `fi` is cached in callee-saved r21 and reused as the AddMeshVertAndTwins
  first arg (target `mr r4, r21`; HEAD reloads `face[i]`), `verts[i]->unk26`
  IS re-read from the stack slot after the call (HEAD already right).
- `verts[3]` is a stack array indexed by runtime b/next/prev (HEAD right);
  `next = (b==2)?0:b+1` lowers branchless nor/srawi/andc (HEAD right).
- t-clamp compares against FLOAT 1.0 (`lfs @F_0000803f` once, reused). HEAD's
  `t > 1.0` (double literal) emits an `lfd` double constant + a second float
  1.0 load — use `1.0f`.
- projy is computed BEFORE projx (both re-subtract vn-vb from still-live regs).
- The reject unwind is `while (added-- != 0) { unk10.back()->mVert = 0; unk10.pop_back(); }`
  — `back()` shows as data+size*4 then `lwz -0x4`; the counter decrements at the
  loop TEST (cmpwi then subi then bne), i.e. post-decrement in the condition.
- Everything else at HEAD (assert layout, allOut branchless `(allOut!=0)`,
  flags writeback, kUnAdded restore when allOut==0) already matches.

Expected: 92.9 now; residual is FPR scheduling inside the b!=3 block (target
keeps vb/vn uv fields live in f6-f9 across the t computation; ours reloads).
Permuter or decl-order dice; do not restructure semantics chasing it.

### ExtendTwin (HEAD's tail math is NOT the binary's; wave-3's was)

Target asm facts (all verified by the 94.5/no-insert-delete compile):
- Loop: for each incident face with flags==4, walk the 3 corners with the
  rotation `v0=next; next=curr` seeded from face[1]/face[2] (HEAD right).
  In the qualifying corner branch the source assigns
  `prevTwin = next; prevOther = v0; anchor = curr|next;` BEFORE the float math
  (branch 2 sets `anchor = next` AFTER the outDir update), and assigns the edge
  delta DIRECTLY into the persistent accumulators:
  `accumY = curr...uv.y - next...uv.y; accumX = ...x...; accumY *= unk44.y; accumX *= unk44.x;`
  (no dx/dy temps → the fsubs targets callee-saved f30/f31 in place).
- Declaration order that matches allocation: `accumX, accumY, prevTwin,
  prevOther, anchor` (r18/r17/r16 in that init order).
- **Cross product (sign):** binary computes
  `cross = accumX*(prevTwin.uv.y - prevOther.uv.y) - accumY*(prevTwin.uv.x - prevOther.uv.x)`
  — HEAD has (prevOther − prevTwin), i.e. HEAD'S SIGN IS INVERTED vs the
  running game. Wave-3's operand order was the asm-correct one.
- Normalize/rotate: NO ox/oy locals — reload outDir.x/.y after the sqrt call
  (`invLen = sign * (1.0f / std::sqrt(outDir.x*outDir.x + outDir.y*outDir.y))`),
  newY line before newX, store .y then .x.
- **The 2x2 solve is a memory-homed matrix built from two Vector2 locals and
  inverted IN PLACE** (this is what unlocked 72.6→94.1):
  stack slots 0x18/0x1c and 0x20/0x24 first hold rowT=(mv.uv−prevOther.uv),
  rowA=(mv.uv−anchor.uv) (as Vector2 aggregates — real stfs home stores), then
  are OVERWRITTEN with the standard in-place 2x2 inverse
  `[[a,b],[c,d]] -> 1/det [[d,-b],[-c,a]]` where the matrix is
  `[[rowT.x, rowT.y],[rowA.x, rowA.y]]`, det = rowT.x*rowA.y − rowT.y*rowA.x
  computed through EXPLICIT product temps d1/d2 (kills the fmsubs fusion the
  plain expression produces; the cross above, by contrast, IS fused). The final
  block RELOADS the inverse from the stack slots:
  `p = outDir.x*rowT.y + outDir.y*rowA.y; q = outDir.x*rowT.x + outDir.y*rowA.x;`
  then with Vector2 locals `da = mv.unk1c − anchor.unk1c`, `dt = mv.unk1c − prevOther.unk1c`:
  `outUv.y = q*dt.y + p*da.y; outUv.x = q*dt.x + p*da.x;`
  (algebra check: this exactly solves outDir = p*(mv.uv−anchor.uv)+q*(mv.uv−prevOther.uv)
  and re-applies p,q in patch-UV space. HEAD's resX/resY/outUv formulas mix
  rows/columns and do NOT solve that system — HEAD's outUv is mathematically
  different from the running game.)
- Degenerate det: `float absDet = fabs(det);` compared against FLOAT 1e-15f
  (fabs → frsp → fcmpo vs `lfs @F_7d1d9026`; HEAD's bare `fabs(det) < 1e-15f`
  compares in double → wrong constant kind). An `int ok` flag is REAL (the
  binary materializes r0=0/1 + cmpwi). On the degenerate path the binary
  RETURNS WITHOUT WRITING outUv (HEAD writes zeros — observably identical
  because the only caller, SetVertsAndFaces, pre-zeroes v40/v48; keep the
  binary form).
- Declaration-order gotchas found by measurement: declare rowA BEFORE rowT
  (aggregates allocate downward → rowT lands at 0x18 like the target), da
  before dt; hoist `float negAx = -rowA.x; float negTy = -rowT.y;` before the
  in-place writes (+0.2pp).

## Header change (required for the last AddEdge/ExtendTwin form)

`BandPatchMesh::MeshVert` gains a trailing `unsigned short faceList[1];`
(flexible-array idiom for the per-vert face-incidence list that already lives
at header-end in the arena). Layout-neutral on BOTH ABIs: it occupies existing
tail padding (Wii sizeof stays 0x34, LP64 stays 0x38); offsetof(faceList) ==
kMVFaceList on both. Verified match-neutral for all sibling functions.
Follow-up option for the implementer: replace the `kMVFaceList` cast arithmetic
everywhere with `->faceList[i]` and derive kMVSlotBase from offsetof — cleaner
and removes the LP64 footgun the big comment at the top of the .cpp documents.

## The elephant: this contradicts the 82f390b1 revert narrative

The revert bisect blamed wave-3's ExtendTwin ("cross operands reversed, outUv
rows swapped, early-out dropped") and AddEdge's hoist. The target asm says the
OPPOSITE: wave-3's cross order, outUv mapping, no-write early-out, and the
hoist are what the shipping Wii binary does; it is CURRENT HEAD that diverges
from the running game (inverted fringe direction, non-solving outUv, origin-
centered monotonicity gate in TryAddFace). The binary renders correctly in
Dolphin, so the asm-true semantics cannot be "wrong" per se.

Both observations can be true only if one of these holds:
1. **The bisect attribution was confounded** — the band-closeup prefab rotates
   per launch (documented A/B trap), several hunks were toggled at once, and a
   stale-build failure mode exists in this repo's history. The revert DID fix
   the visuals, but possibly because it reverted FindXfm/TryAddFace/interaction
   effects rather than the ExtendTwin math per se. Note wave-3 also carried a
   FindXfm rewrite; at HEAD, FindXfm's `if (endFace == endFace)` is trivially
   true and on the no-exact-hit path the fallback loop runs ZERO iterations and
   `endFace[i]` reads 3 shorts PAST the face array (garbage triangle → garbage
   Transform). Wave-3 fixed that real bug; the revert restored it.
2. **Native-only data amplification** — native renders the Xbox-era assets, so
   these functions see different mesh/uv data than any Wii capture. The correct
   solve amplifies near-degenerate uv triangles (small det, |outDir|→0 gives
   inf via 1.0/sqrt(0)) in places HEAD's scrambled formula happens to damp.
   If the pinned-shot gate genuinely reproduces spikes with the asm-proven
   code, the fix is a native-side numeric guard (finite check / det floor
   under HX_NATIVE), NOT keeping the wrong math.

## Re-land protocol (mandatory)

1. Take the worktree patch (or the worktree itself — it is fully warm).
2. Wii side first: `tools/ninja-locked` + verify the three functions at
   99.76 / 92.92 / 94.47 and the sibling table unchanged (report.json diff).
3. Native visual gate BEFORE landing: `scripts/native/band-closeup-capture.py`
   with PINNED forced shots ({rb3_force_shot}), A/B against current HEAD on the
   SAME shot list; judge coherence (no spike/melt), not lineup differences.
   Toggle ONE function at a time if anything fails (the patch hunks are
   independent).
4. If (and only if) the gate fails on the asm-proven code: split with
   `#ifdef HX_NATIVE` keeping HEAD's runtime behavior on native, land the
   binary-true code for the Wii match build, and file the native amplification
   finding — do not discard the Wii-side gains again.
5. These are AT_LIMIT-listed functions: update decomp.db via report_result
   after landing.

## Traps checklist for the implementer

- Do NOT restore HEAD's `mv1 &&` guard or else-if chain in AddEdge (kills the
  %3-magic codegen and contradicts the binary's fall-through).
- Do NOT "clean up" `int ok` in ExtendTwin into an early return — the flag is
  real in the binary.
- Keep `d1`/`d2` det temps (fusion kill); keep the cross as ONE expression
  (fusion wanted).
- Keep `temp.SetVert(verts[0], verts[0]->mVert)` two-arg — the call target is
  unambiguous in the .s; this is the one place HEAD's behavior gate matters
  most (monotonicity radius recentering), so watch the gate for missing/extra
  patch faces, not just spikes.
- 1.0f / 1e-15f literals must stay FLOAT-typed; `float absDet` must exist.
- Declaration orders are load-bearing: accumX,accumY,prevTwin,prevOther,anchor;
  rowA before rowT; da before dt; negAx/negTy hoisted.
- The MCP objdiff JSON layer chokes on the long AddEdge/ExtendTwin mangled
  names — use `build/tools/objdiff-cli diff -u "main/system/bandobj/BandPatchMesh" 'SYM' --format json-pretty`
  from the worktree root after `tools/ninja-locked`.
- Bank-5 DWARF was not needed or used; all semantics above come from the
  Bank-8 dtk asm (target-accurate by construction).
