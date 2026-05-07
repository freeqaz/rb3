# Session 2026-05-05 — README rewrite + DC3 sync planning

## Context entering the session

- Just merged `fork/master` (older work, 18 commits) into local `master` (56 commits ahead of upstream `DarkRTA/rb3`). Conflict resolution favored ours; 4 merge-artifact compile bugs caught and fixed by build verification (`floor0.c` dup `int rate`, `Player.cpp` dup `result` + dup `maxMult`, `GameConfig.cpp` dup `b11`).
- Renamed remotes: `origin` is now freeqaz's fork, `upstream` is `DarkRTA/rb3`.
- Fork status (still a fork on GitHub) — discussed detaching to make commits count toward the contribution graph; decision pending.
- User asked to reposition the README to honestly reflect this is an AI-assisted decomp project, then expanded scope to "look at DC3 too — that's where the tooling actually came from."

## Project framing (decided this session)

The user clarified the project's identity, which shapes everything downstream:

> "The work is done by me (@freeqaz) and not super aligned with the MiloHax community. The goal is not to upstream our code. This is independent and is a decomp + native engine rewrite."

**Implications:**
- README framing is "freeqaz's AI decomp + native engine rewrite," not "milohax AI decomp."
- DC3 (`/home/free/code/milohax/dc3-decomp/`) is the sister project — same author, same methodology.
- Don't propose PRs back to DarkRTA/rb3 unless explicitly asked.
- Cross-link DC3 ↔ RB3 as related personal projects; don't impose a shared org identity.

Saved as memory: `project_framing.md`.

## What DC3 has that RB3 doesn't (inventory)

DC3 is the more mature of the two — the AI decomp methodology was built and proven there first. Concrete gaps:

| Capability | DC3 | RB3 |
|---|---|---|
| `AGENTS.md` (canonical agent context) | yes | missing |
| `docs/INDEX.md` sitemap | yes | missing |
| `docs/decomp/patterns/` catalog (fixable-bool-mask, fixable-casting, harmful-avoid, unfixable-compiler, etc.) | 18 categorized files | one flat `docs/cw-compiler-patterns.md` |
| `scripts/measure_progress.sh` | yes | missing |
| `scripts/setup_worktree.sh` (configures ninja + symlinks) | yes | missing — would have saved real time during the merge work |
| `scripts/clean_stale_objects.sh` | yes | missing |
| Slash commands | 25 | 10 |
| Permuter framework | ~60 scripts (BSF engine, beam search, classifier, climber) | basic |
| Orchestrator MCP | full (`run_analyze_function`, `lookup_rb3`, `mcp_server.py`, `merger_agent.py`, `worktree_pool.py`) | basic 4 tools |
| Native port | Linux WebGPU + WASM + HTTP debug API + DTA overlay system | none |
| VMX128 Ghidra processor | yes (Xenon-specific) | n/a (Wii Gekko) |
| Unicorn function runner | yes | none |
| `agent-home/` workspace | yes | none |
| Pragma matching docs (`PRAGMA_INDEX.md`, checklist) | yes | none |

**Progress comparison (point-in-time, this session):**

| Project | Code matched | Functions matched |
|---|---|---|
| DC3 | 41.39% | 27,814 / 47,031 (59.14%) |
| RB3 | 55.79% | 29,878 / 41,254 (72.42%) |

RB3 is *higher* % matched despite less tooling — partly because the codebase is smaller, partly because Wii Gekko/MWCC is more decomp-friendly than Xbox 360 Xenon/MSVC. The narrative: tooling built on the harder problem (DC3) accelerates the easier one (RB3).

## Narrative for the rewritten README

The story reframes RB3 from "fork of DarkRTA" to "second step in a personal AI decomp methodology":

1. **First step: DC3** — built the tooling stack from scratch on a hard problem (Xbox 360 MSVC PowerPC, no DWARF, custom VMX128, ICF/COMDAT folding, link-time pragma matching). Reached 41% matched code, plus a working native Linux port. Proved the loop: agent → orchestrator → objdiff → patch → measure.
2. **Second step: RB3** — brought the methodology to a sister codebase (Wii Gekko/MWCC, full DWARF, shared Milo engine). Adapted the tooling, leveraged DC3's already-decompiled engine code as ground truth, jumped from the upstream DarkRTA baseline to 72% function match.
3. **What carries over** — orchestrator MCP, Ghidra MCP, m2c, source permuter, slash command pattern, persistent agent memory, MWCC/MSVC pattern catalogs.
4. **What's RB3-specific** — `dc3_compare.py` (the inverse direction: pull DC3's decomp as reference), MWCC vs MSVC pattern differences, Wii-specific SDK code.

## Decisions taken

| Question | Answer |
|---|---|
| Scope of README rewrite | RB3 + cross-link in DC3 — full rewrite of RB3's, small section in DC3's acknowledging RB3 as sister |
| Umbrella identity | Personal project, not milohax-community. Frame as "freeqaz's AI decomp + native engine rewrite series." |
| Plan doc home | This session log + permanent reference at `docs/SYNC_WITH_DC3.md` |
| Port style for DC3 → RB3 tooling | Long-term goal: shared toolkit (Tier 3 of question). Initial pass already partially done as identical mirror (Tier 1). Some RB3-specific differences are required (Tier 2). Practical path: copy-and-adapt now, refactor toward shared toolkit later. |

## What this session did NOT touch

- Detaching the fork on GitHub — pros/cons discussed, user has the info, decision pending.
- Email standardization (commits split between `00free@gmail.com` and `me@freeqaz.com`) — discussed, user decides.
- Any actual README edits — analysis only this session.
- The README of DC3 — read but not yet updated.
- Sync-plan execution — Tier 1 items identified but not started.

## Followups (next session)

1. Draft the new RB3 README (waiting on this session's plan).
2. Add the cross-link section to DC3's README.
3. Begin Tier 1 sync items (see `docs/SYNC_WITH_DC3.md`).
4. Decide on fork detach + email standardization.
