# R1-DOLPHIN M1 — Wii ground-truth inter-bone table (D2, both hands)

Source: Bank-8 debug DOL booted on retail disc via patched apploader (D2/Option 1).
Map vtable __vt__8CharBone=0x80bfeaa8 (992 hits). 989/992 CharBones rigid.
Bank-8 offsets (empirical, G2): {"cb_name": 12, "cb_mtrans": 72, "world_xfm": 76, "local_xfm": 28, "mat_row_stride": 12}

D_pair = inv(W_parent)·W_child (matrix-relative, lint 1). relRot° = geodesic angle of D_pair rotation; |relT| = |D_pair translation| (bone length).

## L-hand

| pair | kind | relRot° | \|relT\| | relTrans | parent→child (VA) |
|---|---|---:|---:|---|---|
| L-forearm->L-hand | anchor | 91.310 | 8.95 | [8.8716, -1.1459, -0.0462] | 0x92b82448→0x92b8249c |
| L-hand->L-middlefinger01 | finger_base | 5.404 | 6.31 | [3.7933, 4.8688, 1.3097] | 0x92b8249c→0x92b6c330 |
| L-middlefinger01->L-middlefinger02 | finger | 24.366 | 2.19 | [0.4944, 1.948, 0.8763] | 0x92b6c330→0x92b6c384 |
| L-middlefinger02->L-middlefinger03 | finger | 17.732 | 1.14 | [0.0625, 1.1412, 0.0552] | 0x92b6c384→0x92b6c3d8 |
| L-hand->L-ringfinger01 | finger_base | 7.428 | 6.15 | [2.8644, 5.1402, 1.7815] | 0x92b8249c→0x92b6c624 |
| L-ringfinger01->L-ringfinger02 | finger | 26.126 | 1.91 | [0.2851, 1.7481, 0.7009] | 0x92b6c624→0x92b6c678 |
| L-ringfinger02->L-ringfinger03 | finger | 5.739 | 0.98 | [-0.0161, 0.9805, -0.0455] | 0x92b6c678→0x92b6c6cc |
| L-hand->L-thumb01 | finger_base | 123.644 | 4.16 | [2.3723, 3.3179, -0.8007] | 0x92b8249c→0x92b6c918 |
| L-thumb01->L-thumb02 | finger | 23.562 | 2.29 | [-0.2226, -1.4279, -1.7833] | 0x92b6c918→0x92b6c96c |
| L-thumb02->L-thumb03 | finger | 11.948 | 1.15 | [0.3664, -1.0123, -0.4108] | 0x92b6c96c→0x92b6c9c0 |

## R-hand

| pair | kind | relRot° | \|relT\| | relTrans | parent→child (VA) |
|---|---|---:|---:|---|---|
| R-forearm->R-hand | anchor | 91.310 | 8.95 | [8.9436, -0.1825, -0.0221] | 0x92b826e8→0x92b8273c |
| R-hand->R-middlefinger01 | finger_base | 5.363 | 6.31 | [-0.6416, -4.9743, -3.8258] | 0x92b8273c→0x92b6d2f0 |
| R-middlefinger01->R-middlefinger02 | finger | 24.366 | 2.19 | [0.3241, -1.9461, -0.9563] | 0x92b6d2f0→0x92b6d344 |
| R-middlefinger02->R-middlefinger03 | finger | 17.732 | 1.14 | [-0.0329, -1.1424, -0.056] | 0x92b6d344→0x92b6d398 |
| R-hand->R-ringfinger01 | finger_base | 7.385 | 6.14 | [0.2488, -5.1986, -3.2638] | 0x92b8273c→0x92b6d5e4 |
| R-ringfinger01->R-ringfinger02 | finger | 26.127 | 1.91 | [0.3279, -1.7476, -0.6832] | 0x92b6d5e4→0x92b6d638 |
| R-ringfinger02->R-ringfinger03 | finger | 5.740 | 0.98 | [0.0075, -0.9806, 0.0445] | 0x92b6d638→0x92b6d68c |
| R-hand->R-thumb01 | finger_base | 123.723 | 4.16 | [-1.7883, -3.4167, -1.5593] | 0x92b8273c→0x92b6d8d8 |
| R-thumb01->R-thumb02 | finger | 23.562 | 2.29 | [2.2678, -0.2166, -0.2805] | 0x92b6d8d8→0x92b6d92c |
| R-thumb02->R-thumb03 | finger | 11.948 | 1.15 | [1.1464, 0.0966, 0.0645] | 0x92b6d92c→0x92b6d980 |

Bilateral symmetry (L/R identical relRot°, mirrored relTrans) independently validates these as real posed matrices.
