# W32-ARG-ORDER-AUDIT — PLAN

Lane D. Industrialize the W31 SyncProperty arg-order lesson as a static-sweep
pilot: find call-site argument-order decomp bugs (same-typed adjacent args
swapped at a call) that hide at >=99% match and can silence subsystems.

Base SHA rb3 30546499 (acceptance commit). Engine pin 24c4f95.

## Method (as amended by A3/A4/A5/A6)

1. **A4 first action:** regen `build/SZBE69_B8/report.json` on base tree; quote
   regen timestamp.
2. Enumerate fuzzy >=99.0 <100 in `src/band3/` + platform-agnostic
   `src/system/` (exclude rndwii/os, sdk, network, lib). Size >=16 bytes.
3. Build a fast **no-`--build` objdiff classifier** (reads already-built `.o`;
   `objdiff-cli diff --format json --include-instructions`, no ninja) to triage
   each candidate's residual by TYPE.
4. Filter to the true call-arg-swap signature; verify against Bank 8 / retail
   asm that RETAIL's order differs from source (not regalloc/scheduling noise).
5. Fix = swap args in source. **A5 gate:** accept ONLY at raw 100.0% COMPLETE;
   after each landing run batch_objdiff over the unit for sibling neutrality.
6. **A6 at-limit skip:** orchestrator DB `query_functions status='at_limit'` +
   `get_attempts`; deep-dive AVOID lists.
7. **A3 claim protocol:** check union of `/tmp/wave32-claims/*.txt` + seed
   exclusions (BandDirector/BandCharacter/OvershellDir/CharDriver*/CharDriverMidi
   /CharIKMidi/CharIKSliderMidi) before landing; claimed/excluded -> BACKLOG.
8. Behavioral triage per landed fix; flag any SyncProperty-class find
   PROMINENTLY. Scope: shortlist <=25, land <=10, rest = ranked backlog.

## Classifier evolution (the core deliverable)

The naive "some register differs" filter yields 605/1185 — useless (regalloc
noise). Progressive refinement to isolate the TRUE call-arg value-swap:
- argscan2: strict 2-register transposition -> 174 (still regalloc renames)
- argscan4: split commutative-operand-reversal (A) vs call-arg transposition (B)
- argscan5: two arg-reg dests transposed + bl near -> 40 (renames + reorders)
- argscan6: exclude renamed source regs -> 19; +per-index-same-dest -> 0
- **argscan7 (definitive):** per-call arg-register -> value-signature mapping is
  a non-identity permutation between target/base — catches CROSS-OPCODE swaps
  (the actual SyncProperty shape: `addi r4,r1,0xN` + `mr r5,r28` swapped). 52
  candidates; all but one are FPR-cascade / stack-slot / regalloc noise.
