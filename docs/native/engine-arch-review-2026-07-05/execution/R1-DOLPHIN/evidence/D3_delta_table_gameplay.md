# R1-DOLPHIN D3 — Wii-vs-native inter-bone delta table

- Wii side: `shell:ui/overshell (D2 Bank-8-on-retail)` — bank8_debug_dol_on_retail_disc(patched_apploader) (vtable 0x80bfeaa8, D2 ground truth).
- Native side: `gameplay:game_screen (director band, guitar)` — rb3-native; members: slot0 male/director, slot1 female/director, slot2 male/director, slot3 female/director.
- Convention (calibrated to reproduce D2 stored D to <0.5deg, 20 checks, worst 0.1333): layout=col transpose=False order=ipc.
- Red-team (known-bad pair): Wii[L-forearm->L-hand] vs native[L-hand->L-thumb01] -> delta=105.478deg **RED (machinery separates)**.
- Summary: anchor delta mean=125.858deg, finger delta mean=56.582deg (max=173.356deg).

D_side = inv(W_parent).W_child. delta = angle(D_wii . inv(D_native)) (matrix-relative). Per-member rows (lint 2: gender/member split).

## slot0 (male, director) — L-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| l-forearm->l-hand | anchor | 91.31 | 168.959 | **119.918** | 5.175 | 0x559c72880ba0 | ok |
| l-hand->l-middlefinger01 | finger_base | 5.404 | 28.833 | **31.125** | 7.147 | 0x559c72883ae0 | ok |
| l-middlefinger01->l-middlefinger02 | finger | 24.366 | 99.421 | **81.459** | 3.096 | 0x559c72883e40 | ok |
| l-middlefinger02->l-middlefinger03 | finger | 17.732 | 70.431 | **57.522** | 0.272 | 0x559c728841a0 | ok |
| l-hand->l-ringfinger01 | finger_base | 7.428 | 29.879 | **32.041** | 6.103 | 0x559c72885940 | ok |
| l-ringfinger01->l-ringfinger02 | finger | 26.126 | 84.0 | **65.862** | 2.706 | 0x559c72885ca0 | ok |
| l-ringfinger02->l-ringfinger03 | finger | 5.74 | 71.83 | **67.399** | 0.237 | 0x559c72886000 | ok |
| l-hand->l-thumb01 | finger_base | 123.644 | 167.019 | **143.539** | 4.982 | 0x559c72887b00 | ok |
| l-thumb01->l-thumb02 | finger | 23.562 | 27.179 | **32.409** | 2.796 | 0x559c72887e60 | ok |
| l-thumb02->l-thumb03 | finger | 11.948 | 11.98 | **15.286** | 1.628 | 0x559c728881c0 | ok |

## slot0 (male, director) — R-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| r-forearm->r-hand | anchor | 91.31 | 84.481 | **118.591** | 15.034 | 0x559c728912e0 | ok |
| r-hand->r-middlefinger01 | finger_base | 5.363 | 71.322 | **71.038** | 6.837 | 0x559c72894220 | ok |
| r-middlefinger01->r-middlefinger02 | finger | 24.366 | 41.655 | **35.367** | 1.639 | 0x559c72894580 | ok |
| r-middlefinger02->r-middlefinger03 | finger | 17.732 | 15.289 | **16.25** | 1.249 | 0x559c728948e0 | ok |
| r-hand->r-ringfinger01 | finger_base | 7.385 | 57.692 | **56.076** | 5.543 | 0x559c72896080 | ok |
| r-ringfinger01->r-ringfinger02 | finger | 26.126 | 40.938 | **36.712** | 1.678 | 0x559c728963e0 | ok |
| r-ringfinger02->r-ringfinger03 | finger | 5.74 | 10.791 | **9.445** | 1.184 | 0x559c72896740 | ok |
| r-hand->r-thumb01 | finger_base | 123.723 | 158.396 | **126.204** | 4.882 | 0x559c72898240 | ok |
| r-thumb01->r-thumb02 | finger | 23.562 | 15.085 | **18.303** | 4.268 | 0x559c728985a0 | ok |
| r-thumb02->r-thumb03 | finger | 11.948 | 14.276 | **11.495** | 1.745 | 0x559c72898900 | ok |

## slot1 (female, director) — L-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| l-forearm->l-hand | anchor | 91.31 | 164.426 | **111.533** | 7.85 | 0x559c71f6c5c0 | ok |
| l-hand->l-middlefinger01 | finger_base | 5.404 | 45.563 | **41.168** | 8.791 | 0x559c71f6f500 | ok |
| l-middlefinger01->l-middlefinger02 | finger | 24.366 | 54.503 | **77.05** | 3.775 | 0x559c71f6f860 | ok |
| l-middlefinger02->l-middlefinger03 | finger | 17.732 | 31.917 | **48.439** | 2.442 | 0x559c71f6fbc0 | ok |
| l-hand->l-ringfinger01 | finger_base | 7.428 | 30.81 | **25.751** | 8.614 | 0x559c71f71360 | ok |
| l-ringfinger01->l-ringfinger02 | finger | 26.126 | 55.728 | **77.425** | 3.354 | 0x559c71f716c0 | ok |
| l-ringfinger02->l-ringfinger03 | finger | 5.74 | 43.839 | **48.481** | 2.176 | 0x559c71f71a20 | ok |
| l-hand->l-thumb01 | finger_base | 123.644 | 135.58 | **100.788** | 5.609 | 0x559c71f73520 | ok |
| l-thumb01->l-thumb02 | finger | 23.562 | 0.627 | **23.742** | 3.06 | 0x559c71f73880 | ok |
| l-thumb02->l-thumb03 | finger | 11.948 | 49.907 | **43.496** | 1.383 | 0x559c71f73be0 | ok |

## slot1 (female, director) — R-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| r-forearm->r-hand | anchor | 91.31 | 159.825 | **157.596** | 2.666 | 0x559c729f94a0 | ok |
| r-hand->r-middlefinger01 | finger_base | 5.363 | 114.382 | **111.005** | 7.936 | 0x559c729fc3e0 | ok |
| r-middlefinger01->r-middlefinger02 | finger | 24.366 | 64.195 | **84.116** | 1.963 | 0x559c729fc740 | ok |
| r-middlefinger02->r-middlefinger03 | finger | 17.732 | 65.342 | **79.612** | 2.527 | 0x559c729fcaa0 | ok |
| r-hand->r-ringfinger01 | finger_base | 7.385 | 107.292 | **101.536** | 7.963 | 0x559c729fe240 | ok |
| r-ringfinger01->r-ringfinger02 | finger | 26.126 | 80.126 | **99.733** | 2.114 | 0x559c729fe5a0 | ok |
| r-ringfinger02->r-ringfinger03 | finger | 5.74 | 66.267 | **70.457** | 2.291 | 0x559c729fe900 | ok |
| r-hand->r-thumb01 | finger_base | 123.723 | 150.575 | **114.656** | 5.158 | 0x559c72a00400 | ok |
| r-thumb01->r-thumb02 | finger | 23.562 | 7.404 | **22.42** | 0.808 | 0x559c72a00760 | ok |
| r-thumb02->r-thumb03 | finger | 11.948 | 44.277 | **42.1** | 0.403 | 0x559c72a00ac0 | ok |

## slot2 (male, director) — L-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| l-forearm->l-hand | anchor | 91.31 | 122.98 | **46.581** | 15.857 | 0x559c73aa11a0 | ok |
| l-hand->l-middlefinger01 | finger_base | 5.404 | 54.961 | **59.176** | 9.384 | 0x559c73aa6080 | ok |
| l-middlefinger01->l-middlefinger02 | finger | 24.366 | 74.663 | **51.259** | 3.272 | 0x559c73aa63e0 | ok |
| l-middlefinger02->l-middlefinger03 | finger | 17.732 | 61.794 | **44.737** | 2.242 | 0x559c73aa6740 | ok |
| l-hand->l-ringfinger01 | finger_base | 7.428 | 55.964 | **60.294** | 9.406 | 0x559c73aa7ee0 | ok |
| l-ringfinger01->l-ringfinger02 | finger | 26.126 | 76.412 | **51.588** | 2.857 | 0x559c73aa8240 | ok |
| l-ringfinger02->l-ringfinger03 | finger | 5.74 | 49.802 | **44.238** | 1.914 | 0x559c73aa85a0 | ok |
| l-hand->l-thumb01 | finger_base | 123.644 | 138.519 | **69.56** | 4.619 | 0x559c73aaa0a0 | ok |
| l-thumb01->l-thumb02 | finger | 23.562 | 22.453 | **10.377** | 4.402 | 0x559c73aaa400 | ok |
| l-thumb02->l-thumb03 | finger | 11.948 | 38.155 | **27.906** | 1.736 | 0x559c73aaa760 | ok |

## slot2 (male, director) — R-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| r-forearm->r-hand | anchor | 91.31 | 90.507 | **149.811** | 11.571 | 0x559c73ab3880 | ok |
| r-hand->r-middlefinger01 | finger_base | 5.363 | 32.428 | **31.357** | 6.799 | 0x559c73e520a0 | ok |
| r-middlefinger01->r-middlefinger02 | finger | 24.366 | 74.035 | **86.1** | 3.996 | 0x559c73e52400 | ok |
| r-middlefinger02->r-middlefinger03 | finger | 17.732 | 54.103 | **63.023** | 1.683 | 0x559c73e52760 | ok |
| r-hand->r-ringfinger01 | finger_base | 7.385 | 33.41 | **30.468** | 6.861 | 0x559c73e53f00 | ok |
| r-ringfinger01->r-ringfinger02 | finger | 26.126 | 63.299 | **75.984** | 3.466 | 0x559c73e54260 | ok |
| r-ringfinger02->r-ringfinger03 | finger | 5.74 | 42.106 | **44.532** | 1.399 | 0x559c73e545c0 | ok |
| r-hand->r-thumb01 | finger_base | 123.723 | 139.268 | **107.284** | 4.716 | 0x559c73e560c0 | ok |
| r-thumb01->r-thumb02 | finger | 23.562 | 23.577 | **17.126** | 0.394 | 0x559c73e56420 | ok |
| r-thumb02->r-thumb03 | finger | 11.948 | 11.98 | **8.75** | 0.457 | 0x559c73e56780 | ok |

## slot3 (female, director) — L-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| l-forearm->l-hand | anchor | 91.31 | 119.508 | **162.723** | 6.377 | 0x559c74315cc0 | ok |
| l-hand->l-middlefinger01 | finger_base | 5.404 | 75.044 | **79.787** | 3.633 | 0x559c74318c00 | ok |
| l-middlefinger01->l-middlefinger02 | finger | 24.366 | 69.208 | **46.57** | 3.506 | 0x559c74318f60 | ok |
| l-middlefinger02->l-middlefinger03 | finger | 17.732 | 60.395 | **43.84** | 1.343 | 0x559c743192c0 | ok |
| l-hand->l-ringfinger01 | finger_base | 7.428 | 87.169 | **92.79** | 4.473 | 0x559c7431aa60 | ok |
| l-ringfinger01->l-ringfinger02 | finger | 26.126 | 67.075 | **42.759** | 3.213 | 0x559c7431adc0 | ok |
| l-ringfinger02->l-ringfinger03 | finger | 5.74 | 42.633 | **37.254** | 1.589 | 0x559c7431b120 | ok |
| l-hand->l-thumb01 | finger_base | 123.644 | 103.174 | **173.356** | 4.245 | 0x559c74340760 | ok |
| l-thumb01->l-thumb02 | finger | 23.562 | 0.313 | **23.51** | 2.714 | 0x559c74340ac0 | ok |
| l-thumb02->l-thumb03 | finger | 11.948 | 9.258 | **7.686** | 0.947 | 0x559c74340e20 | ok |

## slot3 (female, director) — R-hand

| pair | kind | Wii relRot | native relRot | delta deg | dTrans | native trans_addr | status |
|---|---|---:|---:|---:|---:|---|---|
| r-forearm->r-hand | anchor | 91.31 | 89.557 | **140.112** | 5.536 | 0x559c74359b00 | ok |
| r-hand->r-middlefinger01 | finger_base | 5.363 | 75.095 | **76.466** | 7.882 | 0x559c7435ca40 | ok |
| r-middlefinger01->r-middlefinger02 | finger | 24.366 | 69.382 | **55.982** | 2.901 | 0x559c7435cda0 | ok |
| r-middlefinger02->r-middlefinger03 | finger | 17.732 | 60.222 | **50.145** | 1.656 | 0x559c7435d100 | ok |
| r-hand->r-ringfinger01 | finger_base | 7.385 | 87.316 | **88.258** | 6.788 | 0x559c7435e8a0 | ok |
| r-ringfinger01->r-ringfinger02 | finger | 26.126 | 66.97 | **55.26** | 2.55 | 0x559c7435ec00 | ok |
| r-ringfinger02->r-ringfinger03 | finger | 5.74 | 42.703 | **39.476** | 1.638 | 0x559c7435ef60 | ok |
| r-hand->r-thumb01 | finger_base | 123.723 | 103.174 | **118.655** | 5.448 | 0x559c74360a60 | ok |
| r-thumb01->r-thumb02 | finger | 23.562 | 0.313 | **23.566** | 1.568 | 0x559c74360dc0 | ok |
| r-thumb02->r-thumb03 | finger | 11.948 | 9.258 | **15.251** | 0.719 | 0x559c74361120 | ok |

