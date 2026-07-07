# R1-DOLPHIN — lane hub (PLAN pointer)

Canonical plan: `../RETROSPECTIVE/plans/PLAN-R1-dolphin-probe.md` (Fable-authored),
executed under the Wave-17 coordinator resolutions in `../RETROSPECTIVE/plans/INDEX.md`
(M-1 sequencing, M-2 forearm→wrist pair, M-3 bone-world comparand) and the kickoff
acceptance in `../WAVE17_KICKOFF.md` (A1–A6).

- **M-2 applied:** the deliverable pair list includes `forearm→wrist` (in addition to
  `wrist→hand`, `hand→finger01`, `01→02`, `02→03`, both hands) — recorded here for the
  Wave-B implementer; not reached this wave (M1 stopped at the boot/discovery gate).
- **M-3 applied:** the cross-instrument comparand is defined over bone WORLDS:
  `D_side = inv(W_parent)·W_child`, `delta = angle(D_wii · inv(D_native))`.

**M1 outcome:** see `STATUS.md`. Initial pass: Boot-GO (route B, retail) but clean
named-bone-matrix NO-GO (retail layout diverges from every symbol source). **D2 /
Option 1 (2026-07-07) flips M1 to GO** — patched the retail disc to boot OUR Bank-8
DOL under the retail apploader (1-instruction apploader dev-mode patch clears the
production 0x80900000 section-limit gate; `_r`↔`_s` companion files reconciled), so
the Bank-8 map is valid by construction. The **Wii-side** inter-bone table (D=inv(W_p)·
W_c, both hands, format §3.7) IS produced: `evidence/D2_wii_bones.json` +
`D2_interbone_table.md` + `D2_boot_apploader_patch.md`. The Wii-vs-**native** join is
Wave-B (native `/api/call` dump + matched clock, §3.6) — now unblocked by the
high-confidence map-symbol-driven Wii ground truth R1's premise required.
