# R1-DOLPHIN M1 — boot + discovery findings (2026-07-07)

All reads via `/proc/<pid>/fd/<shm>` where `<shm>` is the fd whose readlink contains
`dolphin-emu.<pid>` (the deleted memfd backing guest RAM). `/proc/<pid>/mem` is BLOCKED
(yama `ptrace_scope=1`), so the memfd-fd route is the working bulk-read channel — cleaner
and faster than RSP `m` packets. Guest→shm mapping: MEM1 (guest 0x80000000) at shm file
offset 0 (size 0x2000000, 24MB usable 0x1800000); MEM2 (guest 0x90000000) at shm file
offset 0x02040000 (size 0x4000000).

## Route A — debug Bank-8 DOL + `-C Dolphin.Core.DefaultISO=<wbfs>`, boot `-e main.dol`
DECISIVE FAILURE (this is the A4 "first decisive route-A failure" → switch to B).
- Banner prints (Dolphin run-loop starts), process stays alive 150s+.
- BUT the game never initializes: MEM2 heap 3/16384 nonzero pages (empty); disc ID
  @0x80000000 == 0 (no "SZBE69"); `TheTaskMgr`@0x80cacb98 == 0; `TheBandDirector`
  @0x80d16c9c == 0; 0 CharBone-vtable hits. MEM1 3169/6144 nz pages = just the loaded
  13MB DOL image, no runtime growth.
- Cause: bare-DOL (homebrew) boot skips the disc apploader, so the game DOL lacks the
  disc/IOS/BI2 environment it needs and stalls immediately (matches PLAN §6.1 "likeliest
  M1-A failure"). Route A gives the valid Bank-8 map but does NOT boot the game.

## Route B — retail `Rock Band 3 (USA).wbfs` booted directly (`-e <wbfs>`)
BOOT GO.
- Banner: `Dolphin aabea5b | JIT64 SC | Null | HLE | SZBE69` (disc ID recognized via
  apploader). RSS ~600MB. disc ID @0x80000000 == "SZBE69".
- Reaches an interactive scene: MEM2 6423/16384 nz pages (heap populated); heap interns
  411 distinct `bone_*` names + `skeleton`, `char/` (29), `main_hub` (20), `gtr_` (703),
  `venue` (72) — characters/skeletons loaded to the hub without any input.
- BUT the Bank-8 map is INVALID here (retail ≠ Bank-8): `__vt__8CharBone` 0x80bfeaa8 and
  `__vt__13CharServoBone` 0x80c05d60 → 0 hits in both regions. No retail symbol map exists
  in-repo → discovery must be name-scan / structural.

## Retail layout diverges from BOTH references (the M1 blocker)
- Bank-5 DWARF (band_r_wii.elf) puts `RndTransformable::mName` @+260 (virtual base) and
  `mWorldXfm` @+96. On retail, the named posed objects carry mName @**+24** (Object as a
  NON-virtual primary base), and there is NO orthonormal matrix at +96 or +32.
- The renderable hub objects are `bone_*.mesh` pieces (RndMesh, retail vtable
  **0x80876cb0**, 368 instances) using MULTIPLE INHERITANCE — several sub-object vtables
  at +0/+48/+64/+80; the actual world transform is at an unknown per-class offset and did
  not surface as a clean (even scale-tolerant) rotation at any consistent offset.
- Pure skeleton "bone_R-hand" (no `.mesh`) Trans nodes are barely present at this scene
  (6 interned, 0 with a readable posed rotation) — the hub renderables are the `.mesh`
  objects, whose transform layout requires bespoke RE.

## Verdict: M1 = BOOT-GO (route B) + clean-named-bone-matrix NO-GO within the ~1-day box.
The plan's M1 GO wanted "one MAP-NAMED memory read → sane bone matrix". That is
unreachable: the only bootable route (B, retail) has no valid map, and the name-scan
fallback hits a harder-than-budgeted mapless per-class layout-RE wall that also lowers the
confidence of any resulting ground truth (retail build, symbol-free layout). Priced paths
forward are in STATUS.md §Priced options.

## Update — richer discovery after the game auto-advanced (still route B)
Left running, the game reached an animated scene (1087 interned bone names). The
object graph IS discoverable by name-suffix clusters (name @+24, vtable @+0):
- `bone_*.mesh`  vtable 0x80876cb0  (~2545)  = RndMesh renderables (multiple-inheritance)
- `bone_*.cb`    vtable 0x8089a57c  (~3542)  = CharBone driver objects
- `bone_*.ikf`   vtable 0x80876bd8  (~48)    = IK channels
- `bone_*.quat`  vtable 0x80858c90           = quat anim channels
(`TheTaskMgr`/`TheBandDirector` read non-zero at the Bank-8 map VAs, but those are
COINCIDENTAL on retail — the 0 vtable hits prove the layout differs; do not trust them.)

## Why a clean matrix still NO-GO: empirical offset derivation does not converge
Attempted to derive retail offsets empirically: for CharBone(.cb) objects, scan every
member offset for a MEM2 pointer whose target holds an orthonormal 3x3 at some offset.
Result = only FALSE POSITIVES: the "rotations" that pass an orthogonality test after
row-normalization have raw row scales of 0.5 / 3.6 / 68.2 (NOT rigid unit-scale bone
matrices), and the winning offset is inconsistent (104 / 136 / 200). A real bone world
matrix is rigid (unit rows); nothing rigid + named surfaced at a consistent offset.
CONCLUSION: without a valid symbol map (route A, dead) or a retail-mapped Ghidra program
(not available), reading a CORRECT posed bone world matrix from the retail heap is not
reliably achievable — it needs symbol-anchored per-class RE, larger than the M1 box.
