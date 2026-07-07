# R1-DOLPHIN D4 — frame-matched Wii-vs-native inter-bone delta table

**Supersedes D3's unsynced-shell table** for the R5 hands-endgame decision.

- Join: pose-anchored nearest-frame (native dense sweep vs D2 frozen pose); PRIMARY metric = convention-invariant |Δmag|.
- Wii: `shell:ui/overshell (D2 frozen, no active vignette driver)` — bank8_debug_dol_on_retail_disc(patched_apploader). **No active vignette driver at the frozen instant** (stacks empty; the Wii pose is the frame label, matched against the native sweep).
- Native: `shell:main_hub playerN vignette sweep` — dense sweep of `playerN` vignette over frames [0.26..6.59], 32 samples.
- Convention (calibrated to reproduce D2 stored D to <0.5°, worst 0.1333): layout=col transpose=False order=ipc.
- Red-team (re-run on this capture): Wii[l-forearm->l-hand] vs native[l-hand->l-thumb01] (mid-sweep) → 157.543° **RED (separates)**.
- Convention pin (pelvis->spine1, pelvis-identity ref): cross-side delta min=2.536° max=7.809° (small ⇒ local-frame convention aligned).

## Headline (frame-matched, convention-invariant |Δmag|)

- **anchor** |Δmag| @ matched frame (mean): **0.465°** → frame-matchable, structurally shared.
- **thumb** |Δmag| @ matched (mean): **4.005°**; thumb FLOOR (min over sweep, mean): **3.438°** → frame-matchable, exonerated.
- **middle/ring** |Δmag| @ matched (mean): 18.851°; **middle/ring FLOOR (min over sweep, mean): 13.406° (max 41.215°)** ← no vignette frame closes this.

Interpretation: magdiff_min_over_sweep (the FLOOR) = the smallest CONVENTION-INVARIANT finger magnitude difference achievable by ANY frame of the shared vignette clip. thumb FLOOR ~ anchor FLOOR ~ 0 => those bones ARE frame-matchable (structurally shared, exonerated). If the middle/ring FLOOR stays well above the thumb FLOOR, no vignette frame reproduces the Wii finger magnitude => the middle/ring divergence SURVIVES frame-matching = a REAL skeleton/pose delta candidate. (R5 issues the verdict; D4 reports the surviving-vs-collapsing deltas.)

_(Secondary, convention-CONTAMINATED angle-delta means: anchor 96.578°, thumb 22.451°, middle/ring 29.306° — NOT the headline; the per-bone local-frame conjugation rotates the delta axis while preserving the angle, so only |Δmag| is a clean cross-engine claim.)_

## Per member / hand

PRIMARY columns = convention-invariant magnitude: `Wii |relRot|`, `native |relRot| @f*`, `|Δmag|@f*`, and the **FLOOR** = smallest `|Δmag|` over the WHOLE native vignette sweep (native range shown). A finger pair whose **FLOOR** stays large ⇒ NO shared-clip frame reproduces the Wii magnitude ⇒ divergence survives frame-matching. `Δang@f*` is the secondary convention-contaminated angle-delta.

### slot0 (male) — L-hand (matched native frame ≈ 4.281, ref |Δmag| 0.319°)

| pair | kind | Wii \|relRot\| | native \|relRot\| @f* | native range | **\|Δmag\|@f*** | **FLOOR (min\|Δmag\|)** | Δang@f* |
|---|---|---:|---:|---:|---:|---:|---:|
| l-forearm->l-hand | anchor | 91.31 | 91.297 | 86.189–101.611 | **0.013** | **0.013** | 112.959 |
| l-hand->l-middlefinger01 | finger_base | 5.404 | 6.955 | 6.832–19.876 | **1.551** | **1.429** | 7.955 |
| l-middlefinger01->l-middlefinger02 | finger | 24.366 | 31.953 | 31.826–45.043 | **7.587** | **7.46** | 29.861 |
| l-middlefinger02->l-middlefinger03 | finger | 17.732 | 30.152 | 30.008–43.262 | **12.42** | **12.276** | 27.013 |
| l-hand->l-ringfinger01 | finger_base | 7.428 | 12.094 | 11.967–25.191 | **4.666** | **4.539** | 13.855 |
| l-ringfinger01->l-ringfinger02 | finger | 26.126 | 33.629 | 33.307–66.063 | **7.502** | **7.181** | 32.555 |
| l-ringfinger02->l-ringfinger03 | finger | 5.739 | 18.071 | 17.752–50.502 | **12.332** | **12.013** | 16.301 |
| l-hand->l-thumb01 | finger_base | 123.644 | 124.859 | 124.859–124.859 | **1.214** | **1.214** | 68.15 |
| l-thumb01->l-thumb02 | finger | 23.562 | 23.577 | 23.577–23.577 | **0.015** | **0.015** | 11.254 |
| l-thumb02->l-thumb03 | finger | 11.948 | 11.98 | 11.98–11.98 | **0.033** | **0.033** | 5.772 |

### slot0 (male) — R-hand (matched native frame ≈ 0.91, ref |Δmag| 0.595°)

| pair | kind | Wii \|relRot\| | native \|relRot\| @f* | native range | **\|Δmag\|@f*** | **FLOOR (min\|Δmag\|)** | Δang@f* |
|---|---|---:|---:|---:|---:|---:|---:|
| r-forearm->r-hand | anchor | 91.31 | 90.111 | 78.317–93.377 | **1.2** | **1.2** | 92.607 |
| r-hand->r-middlefinger01 | finger_base | 5.363 | 9.449 | 6.994–20.069 | **4.086** | **1.631** | 10.723 |
| r-middlefinger01->r-middlefinger02 | finger | 24.366 | 34.517 | 32.003–45.247 | **10.151** | **7.637** | 31.066 |
| r-middlefinger02->r-middlefinger03 | finger | 17.732 | 32.723 | 30.187–43.428 | **14.99** | **12.455** | 28.677 |
| r-hand->r-ringfinger01 | finger_base | 7.385 | 14.656 | 12.13–25.379 | **7.271** | **4.746** | 16.01 |
| r-ringfinger01->r-ringfinger02 | finger | 26.127 | 40.001 | 33.716–66.563 | **13.875** | **7.589** | 36.307 |
| r-ringfinger02->r-ringfinger03 | finger | 5.741 | 24.443 | 18.157–51.002 | **18.703** | **12.417** | 22.257 |
| r-hand->r-thumb01 | finger_base | 123.723 | 124.855 | 124.855–124.855 | **1.132** | **1.132** | 56.699 |
| r-thumb01->r-thumb02 | finger | 23.562 | 23.577 | 23.577–23.577 | **0.015** | **0.015** | 4.392 |
| r-thumb02->r-thumb03 | finger | 11.948 | 11.98 | 11.98–11.98 | **0.032** | **0.032** | 2.327 |

### slot1 (female) — L-hand (matched native frame ≈ 2.283, ref |Δmag| 4.236°)

| pair | kind | Wii \|relRot\| | native \|relRot\| @f* | native range | **\|Δmag\|@f*** | **FLOOR (min\|Δmag\|)** | Δang@f* |
|---|---|---:|---:|---:|---:|---:|---:|
| l-forearm->l-hand | anchor | 91.31 | 91.144 | 84.561–96.654 | **0.166** | **0.166** | 98.187 |
| l-hand->l-middlefinger01 | finger_base | 5.404 | 14.822 | 7.818–17.074 | **9.418** | **2.414** | 14.662 |
| l-middlefinger01->l-middlefinger02 | finger | 24.366 | 22.214 | 11.84–24.733 | **2.153** | **0.346** | 27.252 |
| l-middlefinger02->l-middlefinger03 | finger | 17.732 | 32.124 | 21.762–34.647 | **14.392** | **4.03** | 31.359 |
| l-hand->l-ringfinger01 | finger_base | 7.428 | 19.606 | 11.197–21.922 | **12.177** | **3.769** | 18.12 |
| l-ringfinger01->l-ringfinger02 | finger | 26.126 | 44.068 | 18.12–50.365 | **17.941** | **0.639** | 44.624 |
| l-ringfinger02->l-ringfinger03 | finger | 5.739 | 37.11 | 11.17–43.406 | **31.37** | **5.43** | 35.872 |
| l-hand->l-thumb01 | finger_base | 123.644 | 125.669 | 125.669–125.669 | **2.025** | **2.025** | 75.073 |
| l-thumb01->l-thumb02 | finger | 23.562 | 14.492 | 14.492–14.492 | **9.07** | **9.07** | 10.852 |
| l-thumb02->l-thumb03 | finger | 11.948 | 6.263 | 6.263–6.263 | **5.684** | **5.684** | 6.394 |

### slot1 (female) — R-hand (matched native frame ≈ 1.239, ref |Δmag| 4.558°)

| pair | kind | Wii \|relRot\| | native \|relRot\| @f* | native range | **\|Δmag\|@f*** | **FLOOR (min\|Δmag\|)** | Δang@f* |
|---|---|---:|---:|---:|---:|---:|---:|
| r-forearm->r-hand | anchor | 91.31 | 91.331 | 89.607–98.123 | **0.021** | **0.021** | 75.842 |
| r-hand->r-middlefinger01 | finger_base | 5.363 | 8.094 | 7.99–15.669 | **2.731** | **2.627** | 11.481 |
| r-middlefinger01->r-middlefinger02 | finger | 24.366 | 11.268 | 10.342–22.875 | **13.098** | **1.491** | 19.67 |
| r-middlefinger02->r-middlefinger03 | finger | 17.732 | 21.124 | 20.193–32.593 | **3.392** | **2.461** | 17.456 |
| r-hand->r-ringfinger01 | finger_base | 7.385 | 10.73 | 10.193–20.367 | **3.345** | **2.809** | 13.915 |
| r-ringfinger01->r-ringfinger02 | finger | 26.127 | 16.924 | 14.608–46.331 | **9.202** | **0.909** | 21.844 |
| r-ringfinger02->r-ringfinger03 | finger | 5.741 | 9.897 | 7.594–39.198 | **4.156** | **1.853** | 8.116 |
| r-hand->r-thumb01 | finger_base | 123.723 | 127.213 | 127.213–127.213 | **3.49** | **3.49** | 27.679 |
| r-thumb01->r-thumb02 | finger | 23.562 | 14.527 | 14.527–14.527 | **9.035** | **9.035** | 12.119 |
| r-thumb02->r-thumb03 | finger | 11.948 | 6.263 | 6.263–6.263 | **5.684** | **5.684** | 6.882 |

### slot2 (male) — L-hand (matched native frame ≈ 3.196, ref |Δmag| 0.392°)

| pair | kind | Wii \|relRot\| | native \|relRot\| @f* | native range | **\|Δmag\|@f*** | **FLOOR (min\|Δmag\|)** | Δang@f* |
|---|---|---:|---:|---:|---:|---:|---:|
| l-forearm->l-hand | anchor | 91.31 | 91.623 | 81.963–98.037 | **0.313** | **0.018** | 104.043 |
| l-hand->l-middlefinger01 | finger_base | 5.404 | 19.876 | 19.876–33.214 | **14.472** | **14.472** | 20.459 |
| l-middlefinger01->l-middlefinger02 | finger | 24.366 | 45.043 | 45.043–76.094 | **20.677** | **20.677** | 40.178 |
| l-middlefinger02->l-middlefinger03 | finger | 17.732 | 43.262 | 43.262–70.891 | **25.53** | **25.53** | 38.555 |
| l-hand->l-ringfinger01 | finger_base | 7.428 | 25.191 | 25.191–42.746 | **17.763** | **17.763** | 24.606 |
| l-ringfinger01->l-ringfinger02 | finger | 26.126 | 66.06 | 66.06–78.078 | **39.933** | **39.933** | 59.112 |
| l-ringfinger02->l-ringfinger03 | finger | 5.739 | 50.499 | 46.765–50.499 | **44.759** | **41.025** | 48.152 |
| l-hand->l-thumb01 | finger_base | 123.644 | 124.851 | 119.016–124.858 | **1.207** | **0.056** | 53.796 |
| l-thumb01->l-thumb02 | finger | 23.562 | 23.577 | 23.577–27.355 | **0.015** | **0.015** | 11.93 |
| l-thumb02->l-thumb03 | finger | 11.948 | 11.98 | 5.385–11.98 | **0.033** | **0.033** | 6.114 |

### slot2 (male) — R-hand (matched native frame ≈ 4.281, ref |Δmag| 0.699°)

| pair | kind | Wii \|relRot\| | native \|relRot\| @f* | native range | **\|Δmag\|@f*** | **FLOOR (min\|Δmag\|)** | Δang@f* |
|---|---|---:|---:|---:|---:|---:|---:|
| r-forearm->r-hand | anchor | 91.31 | 92.188 | 77.197–116.292 | **0.878** | **0.715** | 108.635 |
| r-hand->r-middlefinger01 | finger_base | 5.363 | 22.236 | 19.784–32.542 | **16.874** | **14.421** | 24.403 |
| r-middlefinger01->r-middlefinger02 | finger | 24.366 | 50.618 | 44.828–74.536 | **26.251** | **20.462** | 36.626 |
| r-middlefinger02->r-middlefinger03 | finger | 17.732 | 48.203 | 43.047–69.513 | **30.471** | **25.315** | 37.209 |
| r-hand->r-ringfinger01 | finger_base | 7.385 | 28.225 | 25.069–41.843 | **20.84** | **17.684** | 30.537 |
| r-ringfinger01->r-ringfinger02 | finger | 26.127 | 68.232 | 65.997–77.501 | **42.106** | **39.871** | 52.64 |
| r-ringfinger02->r-ringfinger03 | finger | 5.741 | 49.847 | 46.956–50.533 | **44.107** | **41.215** | 45.803 |
| r-hand->r-thumb01 | finger_base | 123.723 | 123.807 | 119.305–124.896 | **0.084** | **0.084** | 34.398 |
| r-thumb01->r-thumb02 | finger | 23.562 | 24.246 | 23.543–27.158 | **0.684** | **0.01** | 12.56 |
| r-thumb02->r-thumb03 | finger | 11.948 | 10.797 | 5.679–12.008 | **1.151** | **0.003** | 6.129 |

### slot3 (female) — L-hand (matched native frame ≈ 0.583, ref |Δmag| 7.212°)

| pair | kind | Wii \|relRot\| | native \|relRot\| @f* | native range | **\|Δmag\|@f*** | **FLOOR (min\|Δmag\|)** | Δang@f* |
|---|---|---:|---:|---:|---:|---:|---:|
| l-forearm->l-hand | anchor | 91.31 | 90.314 | 85.504–97.252 | **0.996** | **0.231** | 75.893 |
| l-hand->l-middlefinger01 | finger_base | 5.404 | 24.376 | 17.531–30.925 | **18.972** | **12.127** | 27.407 |
| l-middlefinger01->l-middlefinger02 | finger | 24.366 | 40.955 | 25.763–55.516 | **16.589** | **1.397** | 23.988 |
| l-middlefinger02->l-middlefinger03 | finger | 17.732 | 49.084 | 35.543–62.124 | **31.352** | **17.811** | 35.293 |
| l-hand->l-ringfinger01 | finger_base | 7.428 | 32.775 | 23.46–41.746 | **25.347** | **16.032** | 35.497 |
| l-ringfinger01->l-ringfinger02 | finger | 26.126 | 56.708 | 50.743–62.39 | **30.582** | **24.617** | 39.881 |
| l-ringfinger02->l-ringfinger03 | finger | 5.739 | 41.427 | 39.66–43.284 | **35.688** | **33.921** | 37.062 |
| l-hand->l-thumb01 | finger_base | 123.644 | 124.301 | 121.601–127.111 | **0.657** | **0.07** | 16.093 |
| l-thumb01->l-thumb02 | finger | 23.562 | 5.331 | 3.614–6.998 | **18.231** | **16.564** | 18.547 |
| l-thumb02->l-thumb03 | finger | 11.948 | 2.985 | 0.222–6.066 | **8.963** | **5.882** | 9.16 |

### slot3 (female) — R-hand (matched native frame ≈ 5.274, ref |Δmag| 6.949°)

| pair | kind | Wii \|relRot\| | native \|relRot\| @f* | native range | **\|Δmag\|@f*** | **FLOOR (min\|Δmag\|)** | Δang@f* |
|---|---|---:|---:|---:|---:|---:|---:|
| r-forearm->r-hand | anchor | 91.31 | 91.446 | 87.882–97.961 | **0.135** | **0.135** | 104.459 |
| r-hand->r-middlefinger01 | finger_base | 5.363 | 25.565 | 16.706–30.345 | **20.202** | **11.343** | 26.826 |
| r-middlefinger01->r-middlefinger02 | finger | 24.366 | 42.865 | 23.731–53.123 | **18.498** | **0.124** | 30.435 |
| r-middlefinger02->r-middlefinger03 | finger | 17.732 | 50.332 | 33.419–59.772 | **32.599** | **15.686** | 39.539 |
| r-hand->r-ringfinger01 | finger_base | 7.385 | 34.317 | 22.362–40.837 | **26.932** | **14.977** | 34.187 |
| r-ringfinger01->r-ringfinger02 | finger | 26.127 | 57.245 | 49.667–61.35 | **31.119** | **23.54** | 44.538 |
| r-ringfinger02->r-ringfinger03 | finger | 5.741 | 40.408 | 39.139–42.768 | **34.667** | **33.399** | 36.802 |
| r-hand->r-thumb01 | finger_base | 123.723 | 123.483 | 121.519–127.124 | **0.24** | **0.102** | 54.301 |
| r-thumb01->r-thumb02 | finger | 23.562 | 5.656 | 3.432–6.894 | **17.907** | **16.668** | 18.394 |
| r-thumb02->r-thumb03 | finger | 11.948 | 2.434 | 0.52–6.363 | **9.514** | **5.585** | 9.819 |
