# R1-DOLPHIN D3 — Wii-vs-native inter-bone delta table

- Wii side: `shell:ui/overshell (D2 Bank-8-on-retail)` — bank8_debug_dol_on_retail_disc(patched_apploader) (vtable 0x80bfeaa8, D2 ground truth).
- Native side: `shell:main_hub (charcache preview band)` — rb3-native; members: slot0 male/charcache, slot1 female/charcache, slot2 male/charcache, slot3 female/charcache.
- Convention (calibrated to reproduce D2 stored D to <0.5deg, 20 checks, worst 0.1333): layout=col transpose=False order=ipc.
- Red-team (known-bad pair): Wii[L-forearm->L-hand] vs native[L-hand->L-thumb01] -> delta=168.822deg **RED (machinery separates)**.
- Summary: anchor delta mean=90.611deg, finger delta mean=29.732deg (max=89.603deg).

D_side = inv(W_parent).W_child. delta = angle(D_wii . inv(D_native)) (matrix-relative). Per-member rows (lint 2: gender/member split).

## slot0 (male, charcache) — L-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| l-forearm->l-hand | anchor | 91.31 | 92.727 | **114.759** | 12.343 | 0x556e46482200 | ok |
| l-hand->l-middlefinger01 | finger_base | 5.404 | 19.831 | **20.996** | 6.55 | 0x556e46485140 | ok |
| l-middlefinger01->l-middlefinger02 | finger | 24.366 | 45.008 | **36.874** | 3.125 | 0x556e464854a0 | ok |
| l-middlefinger02->l-middlefinger03 | finger | 17.732 | 43.194 | **36.121** | 1.692 | 0x556e46485800 | ok |
| l-hand->l-ringfinger01 | finger_base | 7.428 | 25.145 | **27.034** | 5.931 | 0x556e46486fa0 | ok |
| l-ringfinger01->l-ringfinger02 | finger | 26.126 | 65.957 | **56.205** | 2.733 | 0x556e46487300 | ok |
| l-ringfinger02->l-ringfinger03 | finger | 5.74 | 50.397 | **47.418** | 1.696 | 0x556e46487660 | ok |
| l-hand->l-thumb01 | finger_base | 123.644 | 124.859 | **63.196** | 4.433 | 0x556e46489160 | ok |
| l-thumb01->l-thumb02 | finger | 23.562 | 23.577 | **15.659** | 4.21 | 0x556e464894c0 | ok |
| l-thumb02->l-thumb03 | finger | 11.948 | 11.98 | **8.005** | 2.301 | 0x556e46489820 | ok |

## slot0 (male, charcache) — R-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| r-forearm->r-hand | anchor | 91.31 | 81.368 | **82.45** | 17.15 | 0x556e46492940 | ok |
| r-hand->r-middlefinger01 | finger_base | 5.363 | 10.93 | **13.239** | 9.294 | 0x556e46495880 | ok |
| r-middlefinger01->r-middlefinger02 | finger | 24.366 | 36.02 | **26.351** | 3.886 | 0x556e46495be0 | ok |
| r-middlefinger02->r-middlefinger03 | finger | 17.732 | 34.236 | **25.71** | 1.872 | 0x556e46495f40 | ok |
| r-hand->r-ringfinger01 | finger_base | 7.385 | 16.166 | **19.232** | 8.63 | 0x556e464976e0 | ok |
| r-ringfinger01->r-ringfinger02 | finger | 26.126 | 43.735 | **33.065** | 3.371 | 0x556e46497a40 | ok |
| r-ringfinger02->r-ringfinger03 | finger | 5.74 | 28.173 | **24.654** | 1.561 | 0x556e46497da0 | ok |
| r-hand->r-thumb01 | finger_base | 123.723 | 124.855 | **31.512** | 4.993 | 0x556e464998a0 | ok |
| r-thumb01->r-thumb02 | finger | 23.562 | 23.577 | **7.095** | 4.254 | 0x556e46499c00 | ok |
| r-thumb02->r-thumb03 | finger | 11.948 | 11.98 | **3.672** | 1.987 | 0x556e46499f60 | ok |

## slot1 (female, charcache) — L-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| l-forearm->l-hand | anchor | 91.31 | 95.27 | **70.232** | 17.337 | 0x556e4645ffa0 | ok |
| l-hand->l-middlefinger01 | finger_base | 5.404 | 12.835 | **14.509** | 9.055 | 0x556e46462ee0 | ok |
| l-middlefinger01->l-middlefinger02 | finger | 24.366 | 19.876 | **24.355** | 3.681 | 0x556e46463240 | ok |
| l-middlefinger02->l-middlefinger03 | finger | 17.732 | 29.772 | **27.731** | 1.935 | 0x556e464635a0 | ok |
| l-hand->l-ringfinger01 | finger_base | 7.428 | 17.521 | **18.212** | 8.804 | 0x556e46464d40 | ok |
| l-ringfinger01->l-ringfinger02 | finger | 26.126 | 38.209 | **38.204** | 3.311 | 0x556e464650a0 | ok |
| l-ringfinger02->l-ringfinger03 | finger | 5.74 | 31.252 | **29.691** | 1.379 | 0x556e46465400 | ok |
| l-hand->l-thumb01 | finger_base | 123.644 | 125.669 | **52.152** | 4.788 | 0x556e46466f00 | ok |
| l-thumb01->l-thumb02 | finger | 23.562 | 14.492 | **12.408** | 3.695 | 0x556e46467260 | ok |
| l-thumb02->l-thumb03 | finger | 11.948 | 6.263 | **6.987** | 2.035 | 0x556e464675c0 | ok |

## slot1 (female, charcache) — R-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| r-forearm->r-hand | anchor | 91.31 | 90.565 | **84.753** | 15.277 | 0x556e46ecba60 | ok |
| r-hand->r-middlefinger01 | finger_base | 5.363 | 12.279 | **14.034** | 8.334 | 0x556e46ece9a0 | ok |
| r-middlefinger01->r-middlefinger02 | finger | 24.366 | 18.889 | **21.345** | 2.836 | 0x556e46eced00 | ok |
| r-middlefinger02->r-middlefinger03 | finger | 17.732 | 28.676 | **24.276** | 1.898 | 0x556e46ecf060 | ok |
| r-hand->r-ringfinger01 | finger_base | 7.385 | 16.697 | **17.373** | 7.173 | 0x556e46ed0800 | ok |
| r-ringfinger01->r-ringfinger02 | finger | 26.126 | 36.25 | **32.745** | 2.431 | 0x556e46ed0b60 | ok |
| r-ringfinger02->r-ringfinger03 | finger | 5.74 | 29.153 | **26.734** | 1.723 | 0x556e46ed0ec0 | ok |
| r-hand->r-thumb01 | finger_base | 123.723 | 127.213 | **42.684** | 5.528 | 0x556e46ed29c0 | ok |
| r-thumb01->r-thumb02 | finger | 23.562 | 14.527 | **11.367** | 3.987 | 0x556e46ed2d20 | ok |
| r-thumb02->r-thumb03 | finger | 11.948 | 6.263 | **6.595** | 2.204 | 0x556e46ed3080 | ok |

## slot2 (male, charcache) — L-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| l-forearm->l-hand | anchor | 91.31 | 85.579 | **127.557** | 11.247 | 0x556e47ff1440 | ok |
| l-hand->l-middlefinger01 | finger_base | 5.404 | 28.811 | **27.469** | 5.152 | 0x556e47ff4380 | ok |
| l-middlefinger01->l-middlefinger02 | finger | 24.366 | 65.904 | **65.518** | 2.946 | 0x556e47ff46e0 | ok |
| l-middlefinger02->l-middlefinger03 | finger | 17.732 | 61.814 | **60.906** | 1.235 | 0x556e47ff4a40 | ok |
| l-hand->l-ringfinger01 | finger_base | 7.428 | 36.888 | **34.145** | 4.458 | 0x556e47ff61e0 | ok |
| l-ringfinger01->l-ringfinger02 | finger | 26.126 | 74.132 | **73.784** | 2.848 | 0x556e47ff92a0 | ok |
| l-ringfinger02->l-ringfinger03 | finger | 5.74 | 47.983 | **47.184** | 1.11 | 0x556e47ff94c0 | ok |
| l-hand->l-thumb01 | finger_base | 123.644 | 120.931 | **89.603** | 4.374 | 0x556e47ffafc0 | ok |
| l-thumb01->l-thumb02 | finger | 23.562 | 26.129 | **6.828** | 4.524 | 0x556e47ffb320 | ok |
| l-thumb02->l-thumb03 | finger | 11.948 | 7.552 | **5.088** | 2.291 | 0x556e47ffb680 | ok |

## slot2 (male, charcache) — R-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| r-forearm->r-hand | anchor | 91.31 | 107.999 | **102.838** | 17.204 | 0x556e480047a0 | ok |
| r-hand->r-middlefinger01 | finger_base | 5.363 | 22.245 | **25.551** | 9.232 | 0x556e483a6580 | ok |
| r-middlefinger01->r-middlefinger02 | finger | 24.366 | 50.611 | **31.454** | 3.995 | 0x556e483a68e0 | ok |
| r-middlefinger02->r-middlefinger03 | finger | 17.732 | 48.2 | **33.744** | 1.792 | 0x556e483a6c40 | ok |
| r-hand->r-ringfinger01 | finger_base | 7.385 | 28.234 | **31.532** | 8.554 | 0x556e483a83e0 | ok |
| r-ringfinger01->r-ringfinger02 | finger | 26.126 | 68.233 | **47.38** | 3.484 | 0x556e483a8740 | ok |
| r-ringfinger02->r-ringfinger03 | finger | 5.74 | 49.836 | **44.893** | 1.432 | 0x556e483a8aa0 | ok |
| r-hand->r-thumb01 | finger_base | 123.723 | 123.804 | **17.83** | 4.965 | 0x556e483aa5a0 | ok |
| r-thumb01->r-thumb02 | finger | 23.562 | 24.243 | **15.159** | 4.289 | 0x556e483aa900 | ok |
| r-thumb02->r-thumb03 | finger | 11.948 | 10.79 | **7.346** | 2.037 | 0x556e483aac60 | ok |

## slot3 (female, charcache) — L-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| l-forearm->l-hand | anchor | 91.31 | 87.073 | **94.337** | 16.733 | 0x556e48869e80 | ok |
| l-hand->l-middlefinger01 | finger_base | 5.404 | 29.932 | **33.626** | 8.471 | 0x556e4886cdc0 | ok |
| l-middlefinger01->l-middlefinger02 | finger | 24.366 | 53.304 | **30.83** | 3.762 | 0x556e4886d120 | ok |
| l-middlefinger02->l-middlefinger03 | finger | 17.732 | 60.158 | **43.513** | 2.174 | 0x556e4886d480 | ok |
| l-hand->l-ringfinger01 | finger_base | 7.428 | 40.386 | **44.457** | 8.641 | 0x556e4886ec20 | ok |
| l-ringfinger01->l-ringfinger02 | finger | 26.126 | 61.518 | **37.755** | 3.358 | 0x556e4886ef80 | ok |
| l-ringfinger02->l-ringfinger03 | finger | 5.74 | 39.906 | **34.468** | 1.912 | 0x556e4886f2e0 | ok |
| l-hand->l-thumb01 | finger_base | 123.644 | 122.013 | **46.844** | 5.326 | 0x556e48894920 | ok |
| l-thumb01->l-thumb02 | finger | 23.562 | 6.773 | **19.184** | 3.211 | 0x556e48894c80 | ok |
| l-thumb02->l-thumb03 | finger | 11.948 | 0.799 | **11.638** | 2.117 | 0x556e48894fe0 | ok |

## slot3 (female, charcache) — R-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| r-forearm->r-hand | anchor | 91.31 | 91.882 | **47.961** | 14.464 | 0x556e488add00 | ok |
| r-hand->r-middlefinger01 | finger_base | 5.363 | 16.664 | **18.698** | 7.926 | 0x556e488b0c40 | ok |
| r-middlefinger01->r-middlefinger02 | finger | 24.366 | 23.646 | **24.762** | 2.786 | 0x556e488b0fa0 | ok |
| r-middlefinger02->r-middlefinger03 | finger | 17.732 | 33.332 | **29.523** | 1.841 | 0x556e488b1300 | ok |
| r-hand->r-ringfinger01 | finger_base | 7.385 | 22.307 | **23.036** | 6.727 | 0x556e488b2aa0 | ok |
| r-ringfinger01->r-ringfinger02 | finger | 26.126 | 49.626 | **46.307** | 2.39 | 0x556e488b2e00 | ok |
| r-ringfinger02->r-ringfinger03 | finger | 5.74 | 42.774 | **40.906** | 1.636 | 0x556e488b3160 | ok |
| r-hand->r-thumb01 | finger_base | 123.723 | 127.143 | **42.187** | 5.545 | 0x556e488b4c60 | ok |
| r-thumb01->r-thumb02 | finger | 23.562 | 3.426 | **21.35** | 3.535 | 0x556e488b4fc0 | ok |
| r-thumb02->r-thumb03 | finger | 11.948 | 6.368 | **8.782** | 2.026 | 0x556e488b5320 | ok |

