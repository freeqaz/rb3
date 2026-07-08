# T2-WORLDROI — CLOSE-OUT validation (ALL GREEN)

Final build: `native/build-native/{rb3-native,rb3-tests}` (engine HEAD beb89e5 + T2 commits
ad01ca6/515f617; rb3 4259d4ae/fd9e3af3/116fcfe0/69a246aa/ba253bdb). No pin bump, no default flips.

1. **Full rb3-tests: 0 FAIL.** `123 tests / 23 suites: 116 PASSED, 7 SKIPPED (external capture
   fixtures), 0 FAILED.` (Includes the DrawLogGolden suite.)
2. **Flag-OFF drawlog golden (792 canonical) byte-identical on the FINAL build.**
   `drawlog-golden.py --fixed-clock --scene splash_screen --canonical-order` -> PASS (792 draws;
   281 known-residual within the documented fixed-clock bound — unchanged by T2, since prov-off =>
   mDrawProv empty => RecordDrawProv never called). `--fail-red-audit` -> comparator reads RED.
3. **G3 batch_objdiff on the touched src/system unit (`system/char/Character`): no regression.**
   ```
   DrawShowing__9CharacterFv                          98.2%  REAL_DIFF   (identical to M2 pre-edit)
   Poll__9CharacterFv                                 94.0%  REAL_DIFF   (untouched by T2)
   DrawLod__9CharacterFi                              98.9%  BORDERLINE
   DrawLodOrShadow__9CharacterFiQ29Character8DrawMode 100.0% COSMETIC
   ```
   All at PRE-EXISTING levels. The `#ifdef HX_NATIVE` owner-scope hook is native-only
   (HX_NATIVE defined solely in native/CMakeLists.txt:371, never in the Wii config), so the
   MWCC `Character.o` is byte-identical — the hook cannot move these numbers.

## Hazard discipline (verified clean)
Never staged: engine `src/platform/FxSendNative.cpp`; rb3 `native/src/rb3_session_trace.cpp`.
No default-ON flips, no engine pin bump (coordinator close-out regenerates classjson once).
