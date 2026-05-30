# W7 — Cursor + HUD-bars investigation (no fix shipped)

**Branch:** `wt-web-w7-cursor-hud` (rb3 master `cb0bb31e`, engine main `8397fa6`).
**Capture script:** the standard `scripts/web/post-v2-capture.mjs` rebuilt the same 7 frames against the worktree (port 8581).

PNGs in this directory are gitignored (per repo policy). They reproduce the post-V2 baseline (visual state unchanged — no W7 fix shipped). To re-capture:

```bash
cd /home/free/code/milohax/rb3
tools/setup-worktree.sh <name>
cd .claude/worktrees/<name>
source /home/free/emsdk/emsdk_env.sh  # or export PATH=…emsdk:…emsdk/upstream/emscripten:$PATH
bash scripts/web/build.sh
python3 native/web/server.py --port $(cat .worktree-port) --assets-dir orig-assets/extracted &
node scripts/web/post-v2-capture.mjs --port $(cat .worktree-port)
```

## What the captures confirm (vs post-V2 baseline)

| Frame | Painted% | avgRGB | Verdict vs post-V2 |
| --- | --- | --- | --- |
| `01_splash.png` | 62.27% | 32,22,32 | unchanged |
| `02_main_hub.png` | 62.56% | 40,31,29 | unchanged — PLAY NOW visible w/ yellow highlight box; other 4 items overlap |
| `03_song_select.png` | 93.44% | 31,35,38 | unchanged |
| `04_part_difficulty.png` | 92.15% | 67,68,68 | unchanged |
| `05_game_screen_entry.png` | 5.6% | 2,1,3 | early cinematic frame (tv3_b) |
| `06_gameplay_t5s.png` | 0.61% | 0,1,1 | mid-transition (capture race) |
| `07_gameplay_t15s.png` | 65.73% | 34,17,23 | unchanged — empty meter geometry, no fills, "0" score digit |

## Investigation outcome

See [`docs/plans/web-port/W7_CURSOR_HUDBARS.md`](../../../../plans/web-port/W7_CURSOR_HUDBARS.md) for the full root-cause analysis. TL;DR: the user-supplied W6-V1 hypothesis (more silent `dynamic_cast<App*>` HX_NATIVE early-return sites) is FALSE — comprehensive grep found 0 new sites. The real cursor + HUD-bar issues are different mechanisms (sibling label positioning for the hub menu; mesh-fill PropAnim / SetShowing gates for the meters) and require targeted probes.
