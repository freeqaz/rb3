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

**M1 outcome:** see `STATUS.md`. Boot-GO (route B, retail); clean named-bone-matrix
NO-GO within the ~1-day box (retail layout diverges from every symbol source we hold).
Priced options for continuation are in `STATUS.md §Priced options`. The delta table
(the R5 handoff artifact, format per PLAN §3.7) is **not** produced this wave — it is
gated on one of the priced paths being funded.
