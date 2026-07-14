# Knowledge Base — Migrated Agent-Memory History

This directory holds durable knowledge migrated out of the persistent agent-memory
store (`~/.claude/projects/-home-free-code-milohax-rb3/memory/`) so it lives in
version control and isn't lost when memory is curated. It captures **completed /
closed** investigations, campaign narratives, and dead-end catalogs — the "we
found/fixed X, here's the root cause" material. The live memory index keeps only
must-know-before-acting hooks and genuinely active work; everything historical
points here.

These are point-in-time notes: commit hashes, `file:line` citations, and match%
figures are historical and may have drifted. Verify against current code before
relying on a specific detail.

| Doc | Covers |
|-----|--------|
| [render-and-visual-history.md](render-and-visual-history.md) | Render/visual fixes: char skinning deform, crowd origin/imposters, DC3 feet-in-floor, glow/emissive (A2/A3/A4), hit-flame FX, C8 faces, track-depth occlusion, walk-on pose, venue lighting convergence, render-polish waves, song-select/part-diff UI, rb3-viewer wig, native visual repro loop, and the engine-arch-review wave-loop durable conclusions (crowd chain CLOSED, hands closed, set_play arg-swap, exit-trap, yellow-square stale-build, prop-fans, the Ack rule). |
| [web-port-history.md](web-port-history.md) | Emscripten/WASM web-port waves (W0–W5+), dual-build caching, web guitar/USB input, web audio state (Vorbis CTR-seek), load-perf findings + incremental-load, web data-symbol-zero, release heap-corruption, intro cinematic, songstart content-skip, songlib web crash, DC3 songstart audio fixes, DC3 render-bridge study (verdict: NO BRIDGE). |
| [decomp-campaign-history.md](decomp-campaign-history.md) | Wave-dispatch strategy lessons, decomp-push maturity, decomp-synth tooling, ghidriff divergence index, Ghidra VT optimization, milo-trace fuzzer, native hack-audit + gtest, semantic-diff classifier (negative), App.cpp cleanup, native-port start, song-end native, native joypad input, DC3 native/engine masking, GemPlayer hit-session, SetState workstream, linking sweep. |
| [at-limit-catalog.md](at-limit-catalog.md) | Functions/TUs/families stuck below 100% where further LLM/structural attempts are known dead ends (unless a new hypothesis appears). Per-TU/family walls, per-function at-limits, and general at-limit guidance (no-raw-asm, DC3-logic-only, LLM-bail-verify-with-permuter). Read before re-grinding a residual. |
| [infra-and-tooling-history.md](infra-and-tooling-history.md) | Build/objdiff gotchas (missing base_path 0%/0-fn artifact, normalized-masking, ninja split-loop), MessageTimer.h / WrapText / OutfitConfig / STLPORT fixes, tooling setup, native-port start, audio_verify.py tool, RB3E same-instrument patch, hop-reward join. |
