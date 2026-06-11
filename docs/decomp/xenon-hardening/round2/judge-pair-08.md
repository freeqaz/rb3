# Judge verdict — Pair 08

**Verdict: CORRECT — confidence HIGH**

## Identity under test
- Wii (ground truth, CW map): `SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>`
  = `SetlistMergePanel::SendSongsToMetaPerformer(const stlpmtx_std::vector<int>&)`
  @ `0x80365020` (Bank 8), TU `meta_band/SetlistMergePanel.o`, size `0x168` (360 B)
- Xenon: `Function_82617830` @ `0x82617830`, 328 B (stripped XEX)
- Nominated by **BSIM**, sim×conf = **23.666** (sim 0.518 / conf 45.687)

## Decisive evidence — call graph agreement (3 named callees match exactly)
Verified against `orig/SZBE69_B8/files/band_r_wii.map`:

| role | Wii callee (from asm + map) | Xenon callee | resolved? |
|---|---|---|---|
| get singleton | `0x802ecad0 Current__13MetaPerformerFv` | `FUN_82563f98` | ✓ Current |
| battle path | `0x802ed970 SetBattle__13MetaPerformerFPC18BattleSavedSetlist` | `Function_825691D0` | ✓ SetBattle |
| setlist path | `0x802ed2e0 SetSetlist__13MetaPerformerFPC12SavedSetlist` | `Function_8256AF10` | ✓ SetSetlist |
| RTTI cast | `0x80a38f54 __dynamic_cast` (2 typeinfo args) | `Function_82804DA8(p,0,0x82c42080,0x82c4209c,0)` | unresolved but signature = dynamic_cast |
| songs path | `0x802edb60 SetSongs__13MetaPerformerFRC...vector<i,...>` | `Function_825692D0(Current(),param_2)` | unresolved but position+args = SetSongs |

`MetaPerformer::SetSongs(vector<int>)` (the vector<int> overload at 0x802edb60) is
*literally the function name being verified* ("SendSongsToMetaPerformer") and is the
callee on the not-equal branch on both sides.

## Control-flow skeleton — precise match
Both bodies, in order:
1. Compute the param vector's int-count (`(end-begin)>>2`).
2. Load a global object's member pointer (Wii `316(global+1208)`; Xenon `*(DAT_82dcbf70+0x160)`), null/size-guard it.
3. **vector-equality loop**: element-by-element `int` compare of param vector vs the
   member's vector (`lwzx`/`cmpw` on Wii; `*(iVar7+*p2) != *(iVar7+p1[4])` on Xenon),
   break on first mismatch; bool result.
4. equal → virtual call `(*vtbl+0x14)(obj)` (Wii `lwz12,28(12);bctrl`; Xenon `(**(*piVar1+0x14))(piVar1)`),
   then `__dynamic_cast` with two RTTI pointers, then field check **`==6 || ==7`**
   (Wii `44(r30)`/`cmpwi 6`,`cmpwi 7`; Xenon `*(iVar3+0x30)==6||==7`) → Current()+SetBattle OR Current()+SetSetlist.
5. not-equal → Current()+SetSongs(param vector).

The distinctive `==6 || ==7` constant pair, the two-RTTI-pointer dynamic_cast, the
int-vector equality loop, and the three-way `Current()+Set{Battle,Setlist,Songs}`
dispatch are all present identically on both sides. This is far beyond coincidence.

## The one difference — fully explained (not a divergence)
Wii has an extra path after the dynamic_cast null result (asm `0x803650f0`–`0x80365118`):
`MakeString<...>` + `Fail__5DebugFPCc` — a `MILO_ASSERT`. The Xenon (Xbox 360 retail)
build strips asserts, so this call is absent. This is the expected
MWCC-debug-vs-MSVC-release delta and explains why Xenon (328 B) is slightly *smaller*
than Wii (360 B) — size ratio 0.91, well inside the normal band. Field-offset deltas
(0x2c/44 vs 0x30; 316/1208 vs 0x160) are the expected MWCC-vs-MSVC struct-layout
differences flagged in the substrate notes.

## Caveats
- No m2c (objdump-only pack); judged from raw Bank-8 asm + Ghidra Xenon pseudo-C. Both
  views are self-contained and the callee names resolve cleanly, so confidence stays HIGH.
- 2 of 5 Xenon callees are unresolved (`Function_` placeholders), but both fit their
  expected identity by call position + argument shape (dynamic_cast; SetSongs).

## For the next agent
Pair 08 is a textbook **CORRECT** BSIM match in the 20–30 stratum: 3/5 callees name-agree,
the other 2 fit by position, and the control flow (vector-equality + 6/7 check + 3-way
MetaPerformer dispatch) is an exact semantic match. The only body difference is a
stripped assert. This corroborates the substrate's "callee-graph leans correct" read for
band3 BSIM and argues the dc3-BinDiff pessimism (0.193 band3) is an oracle artifact, not
a real precision dip — at least for this pair.
