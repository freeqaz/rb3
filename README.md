Rock Band 3 (Wii) — AI-Assisted Fork
====================================

> ⚠️ **Unofficial fork.** This repository is **not** the canonical RB3
> decomp. It is a personal experiment by
> [@freeqaz](https://github.com/freeqaz) exploring how far AI agents can
> push a clean-room Wii decompilation project. Code, commits, decisions,
> tooling, and most of the documentation in this fork are **AI-assisted**
> (primarily via Claude Code). Do not assume anything here represents the
> views, code quality bar, or roadmap of the upstream maintainers.
>
> The community RB3 decomp this fork started from lives at
> **[DarkRTA/rb3](https://github.com/DarkRTA/rb3)**. If you want to
> contribute to or follow the canonical effort, please go there instead.
> This fork has diverged in goals and methodology and is **not** aimed at
> upstreaming back.
>
> No game assets, no Wii assembly, and no copyrighted binaries are
> stored in this repo. An existing copy of the game is required to do
> anything useful with it.

A decompilation of Rock Band 3 for the Nintendo Wii (Build 100901_A,
target binary `SZBE69_B8`), with an in-progress native engine rewrite to
follow. Sister project to
[Dance Central 3 (AI-assisted fork)](https://github.com/freeqaz/dc3-decomp) —
same author, same methodology, shared Milo engine codebase.

Status
------

Headline numbers track *cross-platform code* — the engine, game, and
third-party libraries that would actually need to run in a native port.
The Wii SDK (`main/sdk/`) and Wii-specific singletons are excluded
because they get replaced or stubbed in a desktop port anyway.

- **Cross-platform fuzzy match:** ~79% (~61% byte-exact, ~78% of
  functions matched, ~10.3 MiB of code in scope)
- **Overall (with Wii SDK included):** ~73% fuzzy, ~56% byte-exact
- **Build:** `ninja` produces a matching `main.dol` for everything
  decompiled so far; the rest is filled in from the original binary
- **Native port:** not started; tracked under
  [docs/SYNC_WITH_DC3.md](docs/SYNC_WITH_DC3.md) Tier 3

Project relationship
--------------------

Two-project series (same author, same toolchain):

|                  | DC3                              | RB3 (this repo)              |
|------------------|----------------------------------|------------------------------|
| Platform         | Xbox 360 (PowerPC Xenon)         | Wii (PowerPC Gekko/Broadway) |
| Compiler         | MSVC for Xbox 360 (ICF, debug)   | MetroWorks CodeWarrior 4.3   |
| Symbols          | None — Ghidra inferred           | Full DWARF in debug ELF      |
| Cross-platform fuzzy match¹ | **~93%**              | **~79%**                     |
| Native port      | Linux WebGPU + WASM (working)    | Not started                  |
| Ghidra MCP port  | 8000                             | 8001                         |

¹ Excludes platform SDK (Xbox 360 XDK in DC3, Wii RVL_SDK / MSL / DWC /
NW4R in RB3) and platform-specific singletons. These get replaced or
stubbed in a native port and don't reflect work-toward-playable
progress.

DC3 was step one — the AI decomp methodology was built and proven there
on the harder target (no DWARF, ICF, link-time pragmas). RB3 is step
two: same methodology, easier target, and DC3's already-decompiled Milo
engine code serves as ground truth for the ~50% of source the two
projects share.

AI-assisted decomp
------------------

Tooling that travels between DC3 and RB3:

- **Orchestrator MCP** — task queue, build/diff integration, persistent
  results database (`mcp__orchestrator__*` tools)
- **Ghidra MCP** — DWARF-rich pseudo-C decompilation served on
  `ghidra.local:8001`
- **m2c** — asm → C decompiler at `../m2c/m2c.py`
- **Source permuter** — randomized rewrite + match% scoring
- **Slash commands** — `/analyze-function`, `/compare-asm`,
  `/stack-layout`, `/dc3-pair`, `/ghidra-decompile`, `/permute`,
  `/progress`, `/refactor-staff`, `/struct-check`, `/pcode-inspect`
- **Persistent agent memory** — `~/.claude/projects/.../memory/` carries
  patterns, feedback, and project context across sessions

RB3-specific:

- **`scripts/dc3_compare.py`** — finds shared Milo engine functions DC3
  has decompiled to 100% that RB3 is still missing, ranked by
  portability
- **MWCC pattern catalog** — [docs/decomp/patterns/INDEX.md](docs/decomp/patterns/INDEX.md)
  documents register allocation, bool materialization, pragma tricks,
  and STL inlining quirks specific to MetroWorks CodeWarrior

For the agent context and full workflow, see
[CLAUDE.md](CLAUDE.md). For the cross-project sync plan, see
[docs/SYNC_WITH_DC3.md](docs/SYNC_WITH_DC3.md).

Dependencies
============

Windows
-------

- Install [Python](https://www.python.org/downloads/) and add it to
  `%PATH%` (also available from the
  [Windows Store](https://apps.microsoft.com/detail/9pnrbtzxmb4z)).
- Download [ninja](https://github.com/ninja-build/ninja/releases) and
  add it to `%PATH%` (or `pip install ninja`).
- (Optional) Run `Add-Exclusion.ps1` (right-click → "Run with
  PowerShell") to avoid degraded performance from Windows Defender
  scans.

macOS
-----

- Install ninja: `brew install ninja`
- Install [wine-crossover](https://github.com/Gcenx/homebrew-wine):
  ```sh
  brew install --cask --no-quarantine gcenx/wine/wine-crossover
  ```

After OS upgrades, if macOS complains about `Wine Crossover.app` being
unverified:

```sh
sudo xattr -rd com.apple.quarantine '/Applications/Wine Crossover.app'
```

Linux
-----

- Install [ninja](https://github.com/ninja-build/ninja/wiki/Pre-built-Ninja-packages).
- For non-`x86_64` platforms: install wine from your package manager.
- For `x86_64`: [wibo](https://github.com/decompals/wibo), a minimal
  32-bit Windows binary wrapper, will be downloaded automatically.

Building
========

```sh
git clone https://github.com/freeqaz/rb3.git
```

Using [Dolphin Emulator](https://dolphin-emu.org/), extract your game
to `orig/SZBE69_B8`:

![](assets/dolphin-extract.png)

Configure and build:

```sh
python3 configure.py
ninja
```

Diffing
=======

After the initial build, an `objdiff.json` exists in the project root.
Download a release from
[encounter/objdiff](https://github.com/encounter/objdiff). Under
**Project → Settings**, set **Project directory** to this repo. The
configuration loads automatically. Selecting an object from the left
sidebar starts diffing; changes to source files, headers,
`configure.py`, `splits.txt`, or `symbols.txt` trigger automatic
rebuilds.

![](assets/objdiff.png)

Acknowledgments
===============

- [DarkRTA/rb3](https://github.com/DarkRTA/rb3) — the original
  decompilation project this fork started from. The base symbol map,
  splits, and large amounts of early decomp work came from there and
  from the wider milohax community.
- [decomp-toolkit](https://github.com/encounter/decomp-toolkit),
  [objdiff](https://github.com/encounter/objdiff), and
  [m2c](https://github.com/matt-kempster/m2c) — the foundational
  third-party tools every PowerPC decomp project depends on.
- [DC3 decomp](https://github.com/freeqaz/dc3-decomp) — sister project
  where the AI-assisted methodology was first developed.
