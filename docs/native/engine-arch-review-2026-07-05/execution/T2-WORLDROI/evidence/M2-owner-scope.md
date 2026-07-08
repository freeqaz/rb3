# T2-WORLDROI — M2 owner-scope hook + Wii-match evidence (GREEN)

## E3 — Character::DrawShowing owner scope
Added a `#ifdef HX_NATIVE` `RB3ProvOwnerScope` RAII (copy of the shipped Text.cpp prov-scope
pattern) at the top of `src/system/char/Character.cpp` `DrawShowing()` pushing `Name()` as the
kind=1 OWNER scope. Covers all return paths incl. the crowd early-return.

## Runtime (RB3_DRAWLOG_PROV=1, band gameplay frame)
Distinct skinned-draw owners now populate (band + 3D crowd — the Crowd.cpp:574 dispatch to
Character::DrawShowing):
```
player0:39  player1:39  player2:37  player3:56
crowd_female01..04, crowd_male01..04  (15-18 draws each)
```
Non-empty + distinct per member. Example rows now carry owner='player0' alongside mesh+bones.
(Coverage unchanged: 304/304 skinned -> rectKind:3, boneFallback 4@9 + 2@14.)

## B8 — Wii byte-identical (evidence, not assertion)
- `HX_NATIVE` is defined ONLY in `native/CMakeLists.txt:371` (native/web target). It is NOT in
  the Wii MWCC config (`config/SZBE69_B8/config.json`, `configure.py` — grep empty). So the
  matching build never compiles the hook → `Character.o` is byte-identical.
- `run_objdiff DrawShowing__9CharacterFv` (unit `system/char/Character`): **98.2% normalized** —
  a PRE-EXISTING residual (REGISTER_SWAP r5<->r6 + ADDRESS_RELOCATION_NOISE, permuter-class),
  unchanged by T2. The `#ifdef HX_NATIVE` guard cannot move the MWCC match.
