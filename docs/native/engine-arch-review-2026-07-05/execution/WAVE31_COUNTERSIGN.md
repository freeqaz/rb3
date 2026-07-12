# WAVE 31 — PRE-E1 COUNTERSIGN

**Role:** independent read-only re-derivation of each lane's load-bearing numbers
FROM THE COMMITTED RAW ARTIFACTS (gunzip gz, `grep -c` per probe tag — W27 lesson:
grep raw logs, not curated excerpts). No source edits, no builds. Base SHA rb3
`fd119705`; engine pin `b36bcfc` (Lane C fix in engine `0083bad`, pin bump is
coordinator-owned).

**Bottom line:** Lanes A / C(backtrace) / D fully re-derive to their claims.
Lane B mechanism + census re-derive; two cited crop filenames are naming-drift
absent (functional equivalents present) and "overshell" has no standalone crop.
**One real evidence gap: Lane C's post-fix `rc=0 10/10` (and rb3-tests /
drawlog-golden / lineup-gate) acceptance has NO committed artifact — only the
PRE-fix `rc=139 5/5` baseline is on disk.** No pkill/killall/bare-ninja violations
in any lane.

---

## Lane A — W31-SET-PLAY-DISPATCH

Raw: `evidence/raw/{base,fix}_census.log.gz` (songMs-matched --fixed-clock A/B).

| metric | claim (base→fix) | re-derived (grep raw gz) | verdict |
|---|---|---|---|
| stand_rhythm/solo `CHARDRV_PLAY` | 3 → 80 | 3 → 80 | **MATCH** |
| perf-play songMs quartile spread | [25,16,20,19] | [25,16,20,19] (equal-width bins over beat 6.767–83.003; Σ=80; all 4 ≥16 ⇒ ≥3-of-4 A3) | **MATCH** |
| distinct dispatched intensities | play/idle/intense (3) | `SETPLAY_SEND` s2 moods = idle(8)/intense(2)/play(7) = 3 distinct, not constant (A3) | **MATCH** |
| drummer `idle_play_*` (CHARDRV_PLAY dir=player3) | 0 → 14 | 0 → 14 | **MATCH** |
| sit-group `BANDPERF_STATE` grp='sit' (E3) | 24 → 26 | 24 → 26 (≤10×OFF=160; vs W30 lever 16→4373) | **MATCH** |
| total `BANDPERF_STATE` (E3) | 119 → 131 | 119 → 131 (~1.1×, ≤2×) | **MATCH** |
| A7 probe table BANDPERF_CLIP / CHARDRV_PLAY / SETPLAY_KEYS / SETPLAY_SEND | 171→156 / 349→327 / 5→5 / 26→26 | 171→156 / 349→327 / 5→5 / 26→26 | **MATCH (all)** |

Fix commit `a3916764` present; touches ONLY `src/system/bandobj/BandDirector.cpp`,
5 intensity `SendMessage(_val.Sym(),"<inst>")` → `SendMessage("<inst>",_val.Sym())`
swaps (bass/drum/guitar/mic/keyboard) exactly as claimed. Docs `fa8b5d55`.
**Evidence-honesty notes (non-blocking):** (a) a naive `zgrep -c idle_play` = 75
(multiple tag lines per event — BANDPERF_CLIP + CHARDRV_PLAY + …); the load-bearing
"14" is correctly the per-driver `CHARDRV_PLAY dir='player3'` count — E1 should use
the per-driver key, not the aggregate. (b) A/B "base" sit=24 is the buggy-arg
baseline, not the A4 `OFF=16` W29-idle baseline; STATUS discloses this and 26 ≪ 160
either way. **SyncProperty 99.96→100.0% not rerun** (objdiff build not in the
authorized rerun list); source diff + the "REGISTER_SWAP at 5 SendMessage sites"
rationale corroborate the mechanism.

## Lane B — W31-HUD-GLYPHS

Raw: `evidence/uifloordbg_material_census.txt`, `uifloordbg_songselect.log.gz`, PNGs.

| item | claim | re-derived | verdict |
|---|---|---|---|
| buttons.mat draws/frame | 7150 | 7150 (census) | **MATCH** |
| icon-path mats | icons_tour 5203, instrument_icons_small 5120, instrument_icons 1236 | 5203 / 5120 / 1236 (census) | **MATCH** |
| crop pair — song_select footer (F3) | white blob vs retail | `b_footer_whiteblobs_F3.png` + `b_retail_footer.png` present | **MATCH** |
| crop pair — pill/star (F2/F4) | native vs retail | `b_native_pill_star_F2F4.png` + `b_retail_pill_star.png` present | **MATCH** |
| crop — hub | MENU dot white | `b_main_hub.png` (1280×720) present | **MATCH (overlay)** |
| crop — difficulty sidebar (late-add PRESENT) | icons present on focused song row | `b_diff_sidebar_PRESENT.png` present | **MATCH** |
| crop — **overshell** screen | "verified white across … overshell" | **no standalone crop**; asserted as an overlay within hub/song_select full-frames | **GAP (soft)** |

No engine write landed: `8d46802c` is docs-only (PLAN+STATUS); working tree
`native/src/rb3_render_hook.cpp` clean → consistent with `engineAckNeeded=true`.
**Evidence-honesty gap:** STATUS/checkpoint cite `crop_footer_overshell.png` and
`crop_diffsidebar.png` — **both absent** from `evidence/` (naming drift; functional
equivalents `b_footer_whiteblobs_F3.png` / `b_diff_sidebar_PRESENT.png` are present).
"Overshell" is claimed as a distinct verified screen but has no dedicated crop — it
rides inside the hub/song_select frames. Split-memo (F2/F3/F4 = 3 mechanisms) and
"difficulty icons PRESENT" are argued, not number-bearing — not independently
re-derivable here beyond the census above.

## Lane C — W31-EXIT-TRAP

Raw: `evidence/step0_backtrace_symbolized.txt(.gz)`; engine fix `0083bad`.

| item | claim | re-derived | verdict |
|---|---|---|---|
| backtrace symbolized | yes | YES — full Dawn/wgpu/BandRnd frames with source lines (#14 `TextureView::~TextureView`, #16 `gBandRnd+1432`, #18 `BandRnd::~BandRnd Rnd_Wgpu_RB3.h:78`, #22 `App::~App App.cpp:523`); `gBandRnd+1432 == mComposeDiffView` named | **MATCH** |
| PRE-fix rc | 139 ×5/5 | committed file states "rc: 139 (SIGSEGV), 5/5 consistent" | **MATCH** |
| **POST-fix rc distribution** | rc=0 **10/10** | **NO committed artifact** — only the pre-fix 139 5/5 is on disk; the 10/10 run exists only in `native/build-agent-W31-EXIT-TRAP` | **GAP — not re-derivable** |
| rb3-tests 116/7/0, drawlog-golden PASS, lineup PASS | asserted | no committed logs for any of the three | **GAP — not re-derivable** |
| fix scope | engine-only, Rnd_Wgpu_RB3.cpp | `0083bad` touches ONLY `src/platform/Rnd_Wgpu_RB3.cpp` (+33): adds `mPart*` + `mCompose*` (incl. `mComposeDiffView=nullptr`) releases before `mGpu.Shutdown()` — matches root cause; FxSendNative.cpp untouched | **MATCH (mechanism corroborated)** |

**This is the one material evidence gap of the wave.** The root-cause diagnosis is
committed and symbolized, and the fix diff exactly implements the two-cluster
release it names — so the fix is *plausible* — but the headline acceptance
(`rc=0 10/10` + three green gates) cannot be checked against committed artifacts.
E1/close-out should re-run the bounded non-HTTP boot loop on the merged tree
(this is also the A7 gate for tolerance-line removal) rather than accept the
number on report. Rider (web-yellow) is capture-only, evidence present, no fix —
consistent.

## Lane D — W31-HUBWALKER-SHARDS

Raw: `evidence/shard_mesh_table.tsv`, `boneprobe_extra_head.log.gz`,
`drawprobe_skinclamp.log.gz`, `charcache_player0-3_probe.json`.

| item | claim | re-derived | verdict |
|---|---|---|---|
| per-mesh per-bone table completeness | full table | 34 rows (mesh → bone → mesh-local u); 18u–780u range | **MATCH** |
| (i) forehead-cone headline meshes | eyebrows11→bone_forehead 650, head03→L-brow1 780, goatee→L-lipcorner 650, hair02→hair_R-front03 648 | table: 650.3 / 779.6 / 650.3 / 648.1 — all four present, exact | **MATCH** |
| (iii) probe pointer-keyed? | yes (matrix-relative + pointer, lints 1/2, E7) | YES — every bone row carries `ptr=0x…`, `meshPtr=0x…`, `dir=`/`meshDir=`; charcache carries `char_addr`/`driver_addr` | **MATCH** |
| (iii) apex / basis | ~42° coherent basis, all face bones → shared apex ~290u, skinDet=1.0 | skinDet=1.0000, skinRot≈[0.745…] ⇒ acos(0.745)≈41.8°, all 33 face bones skinPos≈(-287,56,123) vs offPos≈(0,-4,-64) ⇒ ~290u point-radial collapse | **MATCH** |
| census-trap E7 (player0-3 name-key) | player0-3 = 0 skinned meshes; shards on crowd/extras+outfit fringe | json: all 4 slots `skinned_meshes:0`, `total_skinned_meshes:0`; walkers driven (`main.drv` playerN_{m,f}, 70 tracks; `expression.drv` empty) | **MATCH** |
| verdict | SKEL_FAMILY_STOP, no fix | probe TU `42d4a59a` (read-only, distinct TU per A2 + CMakeLists wiring); no fix code | **MATCH** |

Note: `drawprobe_skinclamp.log.gz` carries 477 `SKIN_CLAMP` census lines — the (i)
mesh naming is grounded in the draw-path clamp census, not asserted.

---

## Process-rule scan (all lanes)

`grep`/`zgrep` over every lane's gz logs, txt/json evidence, PLAN/STATUS, and
capture `.py` for `pkill` / `killall` / bare `ninja`: **0 violations.** Both harness
scripts (`W31-HUD-GLYPHS/evidence/capture.py`, `W31-HUBWALKER-SHARDS/evidence/
shard-capture.py`) use pgid-only cleanup (`os.killpg(os.getpgid(proc.pid), …)`).

## Countersign verdict

- **Lane A:** COUNTERSIGNED — every load-bearing number re-derives exactly from the
  committed A/B gz. Fix commit real and minimal.
- **Lane B:** COUNTERSIGNED with soft gap — census/mechanism re-derive; "overshell"
  lacks a standalone crop and two STATUS-cited crop filenames are absent (naming
  drift). No engine write, as claimed.
- **Lane C:** BACKTRACE COUNTERSIGNED; **ACCEPTANCE NOT RE-DERIVABLE** — post-fix
  `rc=0 10/10` + the three gates have no committed artifact (only pre-fix `139 5/5`
  is on disk). Fix diff corroborates the mechanism. E1 must re-run on the merged
  tree before crediting the number (this is also the A7 tolerance-removal gate).
- **Lane D:** COUNTERSIGNED — table complete, (iii) probe pointer-keyed, apex/basis
  and E7 census-trap all re-derive; verdict is diagnosis-only, no fix.

_Author: Wave-31 pre-E1 countersign agent. Read-only; no builds run._
