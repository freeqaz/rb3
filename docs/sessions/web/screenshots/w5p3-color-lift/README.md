# W5 Phase 3 — Color-Lift Attempt (Tier 2)

**Status:** Attempted, did NOT brighten dim text. Engine fix code executes
but visible pixel delta is ~0. Color path must be upstream of `mu.color`.

**RB3 branch:** `wt-web-w5p3-impl` (this worktree, unpushed)
**Engine branch:** `w5-phase3-color-lift` (local, unpushed)
**Engine commit:** `08b3932` — `std::max(0.6f, mu.color[0..2])` gated on
`isTextMeshHeur` in `BandRnd::DrawMesh` after the W5 useAlphaAsRGB block.
**Engine pin in RB3 native/CMakeLists.txt:** unchanged (`8397fa6`).
**Captured:** 2026-05-30
**Server:** `python3 native/web/server.py --port 8508 --assets-dir <repo>/orig-assets/extracted`
**Script:** `scripts/web/w5p3-capture.mjs` (clone of `w6-v1-capture.mjs`,
`OUT_DIR` redirected here).

**Side-by-side baseline:** [`../w6-v1-fix/`](../w6-v1-fix/) (post-W6-V1/V3
binder fix, last known reference for dim-text visibility).

---

## Frame-by-frame summary

| File                       | Screen                     | Frame | Painted% | avgRGB     | Notes                                                                                                                       |
| -------------------------- | -------------------------- | ----: | -------: | ---------- | --------------------------------------------------------------------------------------------------------------------------- |
| `01_splash.png`            | `splash_screen`            |    54 |    0.92% | 2,2,1      | Unchanged from baseline                                                                                                     |
| `02_main_hub.png`          | `main_hub_screen`          |   334 |   59.73% | 31,21,23   | News-ticker visible. Bright-pixel%(>180) unchanged from baseline in every horizontal stripe.                                |
| `03_song_select.png`       | `song_select_screen`       |  1304 |   91.15% | 31,34,36   | Song titles, group headers, song-count digits all present — but bright-pixel% **byte-identical** to baseline in every stripe. **Lift NOT visible.** |
| `04_part_difficulty.png`   | `part_difficulty_screen`   |  1584 |   93.85% | 69,71,70   | "20TH CENTURY BOY" title already crisp white in baseline; no change.                                                        |
| `05_game_screen_entry.png` | `tv3_c_screen` (transition)| 1612  |   23%    | 17,21,18   | Transition frame; not a reliable comparison.                                                                                |
| `06_gameplay_t5s.png`      | `tv3_c_screen` (fade)      |  1921 |   94.16% | 56,40,48   | Transition frame; large pixel delta from baseline is timing-driven, not a fix signal.                                       |
| `07_gameplay_t15s.png`     | `game_screen`              |  2048 |   67.99% | 25,14,18   | HUD overlay band similar to baseline; multiplier panel + score-area unchanged in brightness.                                |

---

## Why this didn't work

Per-stripe brightness analysis on the song-select screen (where dim song
titles live):

```
y= 80-120  bright>180% 1.23 -> 1.23    diff%=46.0   (titles row 1)
y=120-160  bright>180% 2.08 -> 2.08    diff%=49.9   (titles row 2)
y=160-200  bright>180% 2.31 -> 2.32    diff%=34.1   (titles row 3)
y=200-240  bright>180% 1.19 -> 1.19    diff%=31.3
y=280-320  bright>180% 3.80 -> 3.80    diff%=17.2   (count digits)
y=320-360  bright>180% 1.76 -> 1.76    diff%=47.2
y=360-400  bright>180% 1.85 -> 1.85    diff%=23.1
y=400-440  bright>180% 0.56 -> 0.56    diff%=43.4
```

Bright-pixel counts are bit-identical. The non-zero `diff%` is from
positional jitter (subpixel scroll between two independent capture runs),
not brightness change.

Verified compiled-code presence: `llvm-objdump` on the rebuilt
`Rnd_Wgpu_RB3.cpp.o` shows three `f32.const 0x1.333334p-1` (= 0.6f)
constants in the DrawMesh body, each followed by `call 534` to the
out-of-line `std::max<float>`. The lift IS being executed.

Most likely cause: `isTextMeshHeur = (mesh->Name() && mesh->Name()[0] == '\0')`
does not fire for the song-row / HUD-digit widget meshes — i.e. those
meshes have a non-empty name. This is consistent with the earlier
empirical-probe failure noted in `W5_TEXT_RENDERING.md` Phase 3 (printf
gated on the same predicate also failed to appear despite EndFrame
printfs appearing 1700+ times). The predicate is the suspect, not
`mu.color`.

See updated `W5_TEXT_RENDERING.md` Phase 3 STATUS section for the
Tier 1 hunt that needs to follow.
