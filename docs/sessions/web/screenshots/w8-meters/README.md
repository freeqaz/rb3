# W8 meters capture (2026-05-30) — investigation, no fix landed

Branch: `wt-web-w8-meters` (parent rb3 `3daf7100`, engine pin `e6c8f86`).

Built from `wt-web-w8-meters` with no source changes; HUD-meter analysis probes (added then reverted) confirmed:

- Score digit ("0|0FF" → ticks up to 6,694 over 30s of autohit gameplay) RENDERS correctly post-V5.
- Streak `×N` text NEVER appears even though `StreakMeter::UpdateMultiplierText(mult)` is called with mult=2,3,4 as the streak ramps.
- OD meter fill stays empty — `Player::SetEnergyAutomatically(0.000)` is called per-frame from an unidentified caller, stomping any accrued band energy.
- 5 multiplier-dot icons (`multi-meter_anim.tnm`) never animate — no source-side `SetFrame` exists; retail behavior is DTA-driven from a handler not in extracted assets.

See `docs/plans/web-port/W6_VISUAL_POLISH.md` → "V6 — W8 HUD meters investigation" for full root-cause analysis + recommended next step.

Most informative: `05_gameplay_t30s.png` (score "6,694" top-center, left vertical bar = single tick at top, right OD bar empty, no `×N` label).

Files in this dir come from two capture passes:
- `01_*` through `08_*` — initial w8 capture script (didn't progress past `main_hub_screen` due to environment race, retained for record).
- `01_main_hub.png` through `05_gameplay_t30s.png` — `scripts/web/w3c-gameplay-test.mjs` smoke run on the probed release build, which reached `game_screen` and ran 30s of autohit gameplay; the probe log went to the smoke run's `console.jsonl`.
