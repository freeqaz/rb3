# R3-UIDUMP — Lane U execution PLAN (Wave 17)

**Executor:** Lane U (Opus). **Source plan:** `RETROSPECTIVE/plans/PLAN-R3-uidump.md`
(executed as written, with INDEX.md COORDINATOR RESOLUTIONS + WAVE17_KICKOFF acceptance
A1/A5 applied). Engine pin `51640ff`.

## Objective (from the plan)
Build the standing UI-forensics instrument: `GET /api/uidump` (authored scene-graph dump
joined to the just-drawn frame), `/api/drawlog?prov=1` provenance sidecar, and a pixel-ROI
killer query (`scripts/native/uidump_query.py`). Acceptance is retrodictive: reproduce from
the shipped build (G3) the ROWFIX main-vs-alt font-material split and (G4) the W14 red-band
LoadOp diagnosis.

## Sequencing (A1)
- Engine sidecar developed in ISOLATED ENGINE WORKTREE `/home/free/code/milohax/milo-engine-wt-uidump`
  (branch `wave17-uidump-lane-U`) from wave start. Own rb3 build dir `native/build-agent-W17-UIDUMP`
  points `-DMILO_ENGINE_PATH` at that worktree.
- Poll `git log` in shared engine for Lane S's `:4736`-region probe commit (`RB3_PALETTE_DUMP`).
  When it lands: rebase the worktree onto it, then land my engine commit to engine master.
- If not landed by mid-wave checkpoint: note ESCALATE in STATUS, continue game-side.
- Coordinator does the ONE pin bump at wave close-out. NO lane pin bump.

## FULL engine edit-site list (A5 — re-derived by symbol at HEAD==pin, tree shifted from plan's cited lines)
All in the rb3-flavor TUs (Wii object files untouched by construction):

| Site | Symbol / anchor | Edit |
|---|---|---|
| `Rnd_Wgpu_RB3.cpp:2461` | `BandRnd::DrawMesh(RndMesh*)` | (context: capture scope) |
| `Rnd_Wgpu_RB3.cpp:5282-5283` | `if (DrawLogOn()) RecordDrawLog(...)` capture call | pass mesh/mat/matRes.mu.color/RndCam::sCurrent to prov sidecar (extend private RecordDrawLog signature or a prov note call) |
| `Rnd_Wgpu_RB3.cpp:5324` | `void BandRnd::RecordDrawLog(...)` def | fill parallel `RB3DrawProv` when prov on |
| `Rnd_Wgpu_RB3.cpp:5313` | `bool BandRnd::DrawLogOn()` | (template for `ProvOn()` cached gate) |
| `Rnd_Wgpu_RB3.cpp:1380` | `RB3SceneBinding BandRnd::WriteSceneUniforms(RndCam*)` | CPU copy of viewProj + cam name into `mActiveViewProjCpu[16]` (gated) |
| `Rnd_Wgpu_RB3.cpp:1935,2246,2298,2348` | four `mPass = mEncoder.BeginRenderPass(&rp)` | `ProvNotePassOpen(depthLoadOp)` — pass seq + depth LoadOp |
| `RB3PostProc.cpp:88-93` | `menuDepthOp` (Wave-14 U-CLEAN) | `RB3_MENU_DEPTH_CLEAR` test knob (force `LoadOp::Clear` at boundary) + `ProvNotePassOpen` at `:112` BeginRenderPass |
| `RB3PostProc.cpp:112` | `mPass = mEncoder.BeginRenderPass(&rp)` (post-grade re-open) | `ProvNotePassOpen` |
| `RB3MaterialBinder.cpp` | `RB3BuildMaterialUniforms` | NOTE: boundColor captured at RecordDrawLog callsite from live `matRes.mu.color` — no binder scratch-handoff needed (plan's `RB3ProvNoteBoundColor` avoided). Binder unchanged. |
| `RB3DrawLogDebug.h` | record-contract header | add `RB3DrawProv`, `RB3DebugGetDrawProv()`, `RB3DrawScopePush/Pop/Guard` |
| `NativeCompatFlags.classification.json` | classjson (ENGINE repo) | append `RB3_DRAWLOG_PROV`, `RB3_MENU_DEPTH_CLEAR` (default-OFF) — rides the engine commit |

Verified disjoint from Lane S's insertion region (`:4736`-`~:4990` INSTR_B / shard-guard
`:5055` / HEADMAT_DBG `:5130`). My `:5283`/`:5324` sites are >290 lines below S's block.

## Game-side edit-site list (rb3 repo, all `#ifdef HX_NATIVE`)
| Site | Edit |
|---|---|
| `native/src/rb3_http_server.{h,cpp}` | new `kCmdUIDump`; route `GET /api/uidump`; extend `/api/drawlog` to accept `?prov=`/`?roi=` query (pass through param1) |
| `native/src/rb3_http_handlers.cpp` | `HandleDrawLog` prov+roi branch; wire `HandleUIDump` |
| `native/src/rb3_uidump.cpp` (new TU) | `HandleUIDump`: walk TheUI screens/panels/dirs, emit authored objects, join drawlog+prov |
| `src/system/rndobj/Text.cpp:1721` | `RndText::DrawShowing` — scope push `(owner, Name())` around mMeshMap loop |
| `src/system/ui/PanelDir.cpp` or `UIPanel::Draw` | scope push `(panel, dir Name())` |
| `src/system/ui/UILabel.cpp` `DrawShowing` | scope push `(owner, Name())` |
| `scripts/native/uidump_query.py` (new) | ROI killer query + `--assert-rowfix` / `--assert-redband` gate replays |

## Milestones
- **M1** — engine sidecar spike: `RB3DrawProv` + names + rect + passIdx/LoadOp + `?prov=1`
  serialization. Gate: `drawlog-golden.py` PASS (792) with prov compiled-but-off. Go/no-go:
  song_select `?prov=1` shows sane rect for `ml_highlight_glasstopp.mesh`, `highlight_yellow.mesh`
  absent; degenerate-rect fraction <20%.
- **M2** — scope hooks + `/api/uidump` walk + join.
- **M3** — ROI query + G3 (ROWFIX split) + G4 (W14 red band) retrodictions + evidence.
- **M4** — subsume/document + classjson + STATUS kill-list annotation.

## Gates (fail-red each — see plan §5)
G1 golden compat; G2 zero-cost-off + Wii-safe; G3 ROWFIX font split; G4 W14 LoadOp truth;
G5 no-regression net.

## Process (OPTIONS §4, binding)
Evidence under `execution/R3-UIDUMP/evidence/` (never bare /tmp); no unvalidated oracles
(G3/G4 each ship with a fail-red control); flavor-membership already checked (rb3-flavor TUs);
flag hit-counts on negatives. Checkpoints `/tmp/wave17-checkpoints/U.json`.
