# r2-skinning goldens (Wave 17 Lane S)

Committed `PaletteFrame` captures for the skinning oracle (`native/tests/test_skinning_oracle.cpp`).
Format: `# R2 PaletteFrame v1` text — header (`mesh/owner/frame/nb/arm`), per-bone
`bone <idx> <name> <parent> <off[12]> <world[12]> <palette[16]>`, per-vert `v <pos[3]> <idx[4]> <w[4]>`.

## Arms present
- `good-body/`  — coherent body meshes (known-GOOD reference). M_BlendSpread <= 3.
- `bad-torn/`   — hands_naked under `RB3_HANDS_AUTHORED_REPOINT=1` (Wave-16 spike-fan),
                  curated to the gameplay-pose frames where the tear manifests (M_BlendSpread 20-33).

`OracleValidation.BlendSpreadSeparatesTornBlend` reads these when present (`UseRealFixtures()`),
proving the tear metric VALID-separates on REAL captured data; falls back to the synthetic
in-vitro population when the goldens are absent. Tier-1/Tier-2/wext tests always use the clean
synthetic populations (the offline Tier-1 has a world-basis-flip artifact at gameplay frames;
see execution/R2-FIXTURES/STATUS.md).

## Refresh
`scripts/native/skinning-fixture-capture.py --arm <tag>` then re-run
`rb3-tests --gtest_filter='OracleValidation.*'` before committing new goldens.
