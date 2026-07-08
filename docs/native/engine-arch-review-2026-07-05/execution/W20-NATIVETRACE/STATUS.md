# Lane N — STATUS (native load-path divergence trace)

**Verdict headline:** On today's build (engine pin `6e6387c`, 12 defaults ON), the hand
meshes' bone slots are bound to a SINGLE shared skeleton-root instance at the moment they
are parsed — **binding is decided at parse-time name resolution** (`RndMesh::Load`
`bs >> mBones`, Mesh.cpp:947 → FindObject descent). Mechanism **(b) CONFIRMED** and ranked
first. The three `BandCharacter::Filter` remap branches are **counted**: `:4202` sBoneMergeDir
and `:4182` sCharSharedDir **NEVER fire** on the shipped build (br2=br3=br0=0, all members);
only `:4188` instrument fires (~192/member). **The FilterSubdir shim is the direct cause of
those dead remaps** (kReplace suppresses the merge that would iterate them). This PROMOTES
mechanism (a) as a contributor and **supersedes the 2026-06-06 "shim-off did not change
binding" record for hand meshes** — but restoring the remap (shim OFF) does NOT fix the
hands: it produces per-member binding that still visually flings (per-member ≠ gender-posed).

## The causal chain (each link probe-evidenced, CONTROL = shipped default)

1. **Parse-time name resolution binds the shared root.** `LOADBIND_RESOLVE`: 405/405
   hand-mesh bone-name ObjPtr resolves at `bs >> mBones` land on ONE dir
   `0x55c62d5a3d00` (`dirIsRoot=1`, anon preloaded skeleton). (Mesh.cpp:947; doc ~:536-556)
2. **That shared root == sBoneMergeDir at the `hands` install.** `LOADBIND_INSTALL`:
   sBoneMergeDir = `0x55c62d5a3d00` (= the resolve root), set from
   `sOutfitDir->FindObject("bone_pelvis.mesh")->Dir()` (:4339).
3. **The `:4202` sBoneMergeDir remap never fires.** `LOADBIND_COUNTERS`: br2=br3=**0** all
   members. It cannot: the shim (`FilterSubdir` :4280) returns kReplace for every merged
   subdir (68/68 `LOADBIND_SUBDIR` = kReplace overrode=1), and a kReplace subdir's objects
   are never iterated through `Filter`, so `o1->Dir()==sBoneMergeDir` never matches.
4. **At rebind entry (pre-mutation, A2) all 205 hand-bone slots are SHARED_ROOT + distinct
   from the per-member Find.** `LOADBIND_SLOT`: 205/205 owningDir=`0x55c62d5a3d00`
   (one instance, all 4 members = `inst0`), `distinct=1` (bound ≠ member's Find). The
   per-member animated instance EXISTS (ownFindPtr distinct) but the hand meshes never
   bind it. → the female (player1) lands the authored female-bind offsets on the shared
   male-bind root → the shipped clamp/head-rebind then holds it. (matches doc items 2–4)

**Which load event binds hand meshes to the shared root TODAY:** the PARSE of each hand
mesh (`RndMesh::Load` `bs >> mBones`), via FindObject descent into the shared preloaded
`skeleton.milo` root — NOT the merge. The merge's sBoneMergeDir remap that WOULD re-point
them is dead (shim kReplace). Mechanism (b) is primary; the shim (a) is what keeps the
(b)-established binding from being corrected by the retail remap.

## Shim reconciliation arm (A1, pre-registered) — RESULT: **CHANGED ⇒ record superseded (for hands)**

| | CONTROL (shim ON) | NOSHIM (shim OFF, retail kMerge) |
|---|---|---|
| FilterSubdir action | 68× kReplace (override on) | 288× kMerge (override off) |
| br2/br3 sBoneMergeDir remap | **0** | **31,488 / member** |
| br0 sCharSharedDir remap | 0 | 320 / member |
| hand owningDir instances | 1 shared (`inst0`, all members) | **4 distinct** (`inst0..3`, per member) |
| distinctFromOwnFind | 205× True (bound ≠ own) | 205× **False** (bound == own) |
| reached gameplay | yes | yes (white textures, as recorded) |
| **visual (gameplay)** | hands coherent (clamp holds) | **hands FLUNG/shattered** (worse) |

Pre-registered outcomes were: UNCHANGED ⇒ shim exonerated; CHANGED ⇒ 2026-06-06 record
superseded. **Topology CHANGED** — the shim-off arm re-points every hand bone onto the
member's OWN instance (distinct=0). This contradicts the doc's "full shim-off … same
shared root" claim. Reconciliation note: the doc's arm measured OUTFIT/torso `upArmPtr`
via BAND_DRAW_PROBE at DRAW time; this arm measures HAND meshes at rebind ENTRY — the two
are not the same population, so the record is superseded specifically for the hand meshes'
rest-entry binding, not necessarily refuted for its own torso-draw measurement. The
mechanism note stands either way: the shim gates the remaps (counted br2 0→31488).

## Are the remap branches live? (the direct answer)
- `:4182` sCharSharedDir — **dead on shipped** (0), live under shim-off (320).
- `:4188` sInstrument — **live on shipped** (~192/member).
- `:4202` sBoneMergeDir — **dead on shipped** (0) — VERDICT §1 "never-firing" is now a
  COUNTED zero — live under shim-off (31,488/member).

## A11-relevant observation (for the coordinator's synthesis, NOT a lane claim)
The shim-off arm delivers exactly what the loader-fix hypothesis wanted — per-member hand
binding (distinct=0, one instance per member) — yet the hands still **visually fling**
(`noshim_shimOFF_gameplay.png`). This is direct native evidence that **per-member binding
alone is NOT sufficient**: it re-enters the VERDICT §2 problem (the per-member instance's
seed rest is 87° off the authored bind basis; the female's authored offsets are ~29° off
the shared male bind). The record's shelved faithful fix required TWO things — un-share AND
pose each per-member skeleton to its outfit's gender bind (doc ~:939-948). The A11
discharge (what the Wii object has that per-member+authored lacked) therefore lands on
**gender-posed rest basis**, corroborated here: topology-only per-member binding fails the
visual gate. Lane W's Wii basis capture (A7) is the piece needed to close it.

## Lints / process
- Lint 8 (counted negatives): br0/br2/br3 zeros are COUNTED (LOADBIND_COUNTERS), not absent.
- Lint 9 (flavor membership): rb3-native builds clean with both probe TUs (BandCharacter.cpp,
  Mesh.cpp) — step 0 PASS.
- Regression: `drawlog-golden --fixed-clock --canonical-order` PASS 792 with probes present
  but OFF (probes proven inert when unset).
- All probes `#ifdef HX_NATIVE` + env-gated default-OFF; Wii byte-identical. No default
  flips, no pin bump, no fixes, no banned cells implemented. rb3_session_trace.cpp /
  engine FxSendNative.cpp never staged.

## Evidence
- `evidence/loadbind_control_shimON.log`, `evidence/loadbind_noshim_shimOFF.log` (LOADBIND rows)
- `evidence/*_a10.json` (205-row per-slot A10 tables, both arms) + `build_a10_table.py`
- `evidence/a10_native_table.md`, `evidence/branch_hitcount_table.md`
- `evidence/control_shimON_gameplay.png` (coherent), `evidence/noshim_shimOFF_gameplay.png` (flung)
- `evidence/boot_commands.md` (regen)
