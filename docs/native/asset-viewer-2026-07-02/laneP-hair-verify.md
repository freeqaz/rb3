# Lane P handoff — H2 collide-hookup probe + skull-clip check + H4 color evidence

Date: 2026-07-02. Implements PLAN2.md "Lane P" after Lane G's CharHair CFG fix
(`81f38f3a`) and Lane S's viewer v2 (`793e718d`) landed. Adds an env-gated
collide-hookup coverage probe, runs it in-game on native, and answers H2/H4 with
evidence. **No fix beyond the probe.**

## TL;DR

- **H2 (collide hookup on native) is NOT a real gap.** Every flagged strand
  (nonzero `hookupFlags`) hooks its collides fully in-game on native — the
  on-stage `crazyhawk` gets **14/14 points hooked, 0 strands hooking none,
  mCollide=4, dir-collides=16**. The only zero-coverage hairs
  (`mohawk`/`bedhead`/`blownback`) have **`hookupFlags=0x0` by authoring** — a
  0 mask can never satisfy `mHookupFlags & col->mFlags`, so they hook nothing by
  design (short skull-tight styles). This is Wii-identical behavior, not a
  native reachability bug. The scout's H2 hypothesis ("flagged strands lose
  collision on native → hair hangs through skull") is **refuted**.
- **No skull-clip.** Post-fix the guitarist's `crazyhawk` stands up as a
  coherent dark hawk fan; the face is clear and no strands pass through the
  skull/face (collision push-out is working, consistent with 14/14 coverage).
- **H4 (color):** the "pale white wig" was the *collapsed-pose* artifact (flat
  strand ribbon faces catching light), now gone. Under matched dark venue
  lighting native hair reads dark, consistent with the dark/backlit Dolphin GT
  (`gp_b07`). The pale look survives only in the flat-unlit standalone viewer —
  expected (no material/venue lighting). No demonstrable color gap; a clean A/B
  needs a well-lit ambient shot of the same char, unavailable this run.
- **Probe committed** `ede6911f` (env-gated, silent by default, `#ifdef
  HX_NATIVE`, Wii-neutral proven). `src/system/char/CharHair.cpp` only.

## 1. The probe (deliverable 1)

`src/system/char/CharHair.cpp`, end of `CharHair::Hookup(ObjPtrList<CharCollide>&)`
(after the strand loop). Entirely `#ifdef HX_NATIVE`, gated on `RB3_HAIR_DBG=1`
(cached static), writes to stderr:

```
[HAIR_DBG] hair='<name>' strand=<i> pts-with-collides=<h>/<n> hookupFlags=0x<f>
[HAIR_DBG] SUMMARY hair='<name>' strands=<S> strands-hooking-none=<z>
           pts-hooked=<H>/<N> dir-collides=<D> passed-collides=<P> mCollide=<M>
```

`dir-collides` = independent `ObjDirItr<CharCollide>(Dir())` scan (reachable
collides at hookup time); `passed-collides` = the pre-filtered list actually
handed to `Hookup`; `mCollide` = distinct collides that hooked ≥1 point.

## 2. Wii neutrality (deliverable 2 — PROVEN)

- `mcp run_objdiff SimulateInternal__8CharHairFf` = **99.6%** (2 diff_arg + 2
  delete — the pre-existing `[259-262]` `lfs 0x50/0x54, r26` regalloc residual).
  Unchanged from Lane G's post-fix state.
- `mcp run_objdiff Hookup__8CharHairFR36ObjPtrList<11CharCollide,9ObjectDir>` =
  **99.4%** — all 43 residual mismatches are pre-existing paired-single
  REGISTER_SWAP / OFFSET_SWAP / relocation noise (permuter-class). (The right
  mangled name uses length prefix **`R36`**, not `R10`; the task's `R10` was
  wrong.)
- The entire probe is inside `#ifdef HX_NATIVE`; mwcc does not define it, so the
  Wii `.o` is byte-identical. The 99.4% is the long-standing state of Hookup, not
  a regression.

## 3. In-game native run (deliverable 3)

`RB3_HAIR_DBG=1 RB3_HEADMAT_DBG=1 python3 scripts/native/band-closeup-capture.py
--member all --frames 2 --frame-dt 600 --out /tmp/hair-h2/run1 --tag h2r1`.
**Run 1 rolled `crazyhawk`** on the guitarist (verdict=PASS, pinned=34/34,
drops_total=0). Log `/tmp/rb3-bandcloseup-h2r1-34459.log`.

### (a) Do flagged strands get nonzero collides in-game on native? — YES

Aggregated `[HAIR_DBG] SUMMARY` (distinct tuples), on-stage members in **bold**:

| hair | strands | hooking-none | pts-hooked | dir-collides | passed | mCollide | hookupFlags |
|---|---|---|---|---|---|---|---|
| **crazyhawk** (drawn) | 10 | 0 | **14/14** | 16 | 6 | 4 | 0x48 |
| **messyshort** (drawn) | 12 | 0 | 12/12 | 13 | 5 | 5 | — |
| **ramones** (drawn) | 8 | 0 | 11/11 | 15 | 5 | 5 | — |
| hair_youngozzy | 7 | 0 | 16/16 | 16 | 5 | 5 | 0x44/0xb9/0xd6/0xe6 |
| female_extra02 | 3 | 0 | 11/11 | 5 | 5 | 5 | — |
| mohawk | 4 | 4 | **0/4** | 15 | 5 | 0 | **0x0** |
| bedhead | 8 | 8 | 0/10 | 11 | 5 | 0 | **0x0** |
| blownback | 16 | 16 | 0/16 | 17 | 5 | 0 | **0x0** |

Every hair with a **nonzero** `hookupFlags` hooks 100% of its points. The three
zero-coverage hairs all have `hookupFlags=0x0` (per-strand confirmed), so
`mHookupFlags & col->mFlags == 0` always — they hook nothing *by authoring*, not
because collides are unreachable (dir-collides is 11–17 for all of them). Those
three are off-stage constant-preload short styles; they're not drawn on stage and
their short skull-tight geometry wouldn't clip even without collision. **H2 is
not a native gap.**

### (b) Does post-fix long hair clip through skull/shoulders? — NO

A/B on the guitarist (Duke: goggles + chops), brightened crops:
- Broken baseline `/tmp/hair-h2/bright_BROKEN_g_b_0.png` (from
  `/tmp/wig-bug/run1/r1_coop_g_b_0.png`): pale/white stringy strands **draped
  down the left side of the face**, bare crown — the "white wig".
- Post-fix `/tmp/hair-h2/bright_h2r1_coop_g_b_0.png`: an **upright dark
  crazyhawk fan** on the crown/back, **face clear**, no strands through the
  face/skull. The collapse is gone. Front angle
  (`/tmp/hair-h2/bright_h2r1_coop_g_n01_0.png`) confirms no face intrusion.

### Viewer numeric evidence (`/tmp/hair-h2/viewer/`)

`rb3-native --viewer male_hair_crazyhawk_resource.milo_xbox`:
- static = **64205** non-clear (20.9%); `--sim 30` = **84716** (27.6%) — the sim
  reaches the pixels and converges (matches Lane S's numbers).
- `--pose-dump crazyhawk_pose_sim30.json` → **19 `bone_hair_*` transforms** in
  distinct settled world positions (y range −2.1 … +9.4), i.e. a settled
  non-rest pose. Standalone (no head frame / no CharCollide volumes) the free
  solver droops one strand — the documented Lane S tradeoff; the in-game gate
  above is authoritative.

## 4. H4 color / brightness evidence (deliverable 4 — evidence only)

- **Best post-fix native long-hair closeup:**
  `/tmp/hair-h2/bright_h2r1_coop_g_b_0.png` (crazyhawk guitarist, brightened).
- **Closest Dolphin GT:** `docs/native/c8-ground-truth-2026-07-01/dolphin-shots/
  gp_b07.png` — a goggled guitarist in a dark backlit venue: hair reads
  **near-black** on the crown. (Also `face_guitarist_ambient.png`: a *different*,
  short-hair char, well-lit, **medium sandy-brown** — not the same style, so no
  direct color A/B.)
- **Delta:** under matched dark venue lighting native crazyhawk reads **dark
  red/near-black**, ≈ the GT `gp_b07` dark silhouette — no obvious
  brightness/color gap in matched conditions. The pale-white look persists ONLY
  in the flat-unlit standalone viewer render (`crazyhawk_sim30.png` = pale
  lavender), which confirms scout H4: the ribbons are inherently light under flat
  shading and the in-game venue lighting darkens them. The original "white wig"
  was the collapsed pose (broad flat ribbon faces catching light), not a texture
  bug — resolved by the pose fix. **No color fix warranted from this evidence;**
  a definitive A/B needs a well-lit ambient shot of the same crazyhawk char.

## 5. Commit (deliverable 5)

`ede6911f` — `probe(char): RB3_HAIR_DBG collide-hookup coverage in
CharHair::Hookup`. Staged **only** `src/system/char/CharHair.cpp`. Env-gated,
silent by default, `#ifdef HX_NATIVE`, Wii-neutral proven (§2).

## Captures

- Probe log: `/tmp/rb3-bandcloseup-h2r1-34459.log`
- In-game run: `/tmp/hair-h2/run1/` (34 PNGs; crazyhawk lineup)
- Brightened A/B: `/tmp/hair-h2/bright_h2r1_coop_g_b_0.png`,
  `bright_h2r1_coop_g_n01_0.png`, `bright_BROKEN_g_b_0.png`
- Viewer: `/tmp/hair-h2/viewer/crazyhawk_{static,sim30}.png`,
  `crazyhawk_pose_sim30.json`
- GT: `docs/native/c8-ground-truth-2026-07-01/dolphin-shots/gp_b07.png`,
  `face_guitarist_ambient.png`

## Verdict

- **H2: real gap? NO.** Collide hookup on native is correct — flagged strands
  (nonzero flags) hook 100% (crazyhawk 14/14, youngozzy 16/16, messyshort 12/12,
  ramones 11/11); zero-coverage cases are `hookupFlags=0x0`-authored short
  styles, Wii-identical. No skull-clip. The CFG fix (`81f38f3a`) is sufficient;
  no follow-up collision work needed.
- **H4: cosmetic, no fix.** Post-fix hair reads dark under matched dark venue
  lighting ≈ GT; the pale look was the (now-fixed) collapsed pose, not a
  texture/lighting bug that survives the fix.
