# tmux Session Persistence & Teleport

How our tmux sessions (i3-style layout + working dirs + Claude Code conversations)
survive reboots and can be shipped to a remote box. Captured 2026-07-13.

## The pieces

| Piece | Path | Role |
|---|---|---|
| tmux config | `~/.config/tmux/tmux.conf` | i3-style keybinds + persistence wiring |
| tpm | `~/.config/tmux/plugins/tpm/` | plugin manager |
| tmux-resurrect | `~/.config/tmux/plugins/tmux-resurrect/` | save/restore layout, cwds, scrollback, processes |
| tmux-continuum | `~/.config/tmux/plugins/tmux-continuum/` | auto-save every 10 min, auto-restore on server start |
| resurrect state | `~/.config/tmux/resurrect/tmux_resurrect_*.txt` (+ `last` symlink) | the saved snapshots |
| **tmux-teleport** | `~/.local/bin/tmux-teleport` | move one session (layout + repo + Claude state) to a remote host over ssh |

## How persistence works (`tmux.conf`)

```tmux
set -g @plugin 'tmux-plugins/tmux-resurrect'
set -g @plugin 'tmux-plugins/tmux-continuum'

set -g @resurrect-dir '~/.config/tmux/resurrect'
set -g @resurrect-capture-pane-contents 'on'
# relaunch claude resuming the pane's most recent conversation
set -g @resurrect-processes '"~claude->claude --continue --dangerously-skip-permissions" ssh'

set -g @continuum-save-interval '10'   # auto-save every 10 min
set -g @continuum-restore 'on'         # auto-restore on tmux server start
```

- **Save**: continuum snapshots every 10 min; manual save is `prefix + Ctrl-s`.
- **Restore**: automatic on tmux server start; manual is `prefix + Ctrl-r`.
- On restore, resurrect re-spawns each pane **in its saved `cwd`** and re-runs the
  captured process. For Claude panes it relaunches
  `claude --continue --dangerously-skip-permissions` (the running snapshots also
  capture `--resume <uuid>` so each pane comes back on its own conversation).

## tmux-teleport (session → remote box)

`tmux-teleport <session> <remote> [opts]` ships a whole session to another machine:

1. fresh resurrect save, extract just that session's panes/windows + scrollback
2. rsync the repo working tree (incl. `.git`, uncommitted + untracked)
3. any paths listed in `<repo>/.teleport-includes`
4. `~/.claude/projects/*` conversation state for the repo + subdirs (so
   `claude --continue` resumes the same conversations remotely)
5. rewrites `$HOME` → remote `$HOME` in state + `.jsonl` if they differ
6. bootstraps tmux config/plugins on the remote if missing, then triggers restore

Key flags: `--repo PATH`, `--mirror` (rsync `--delete`), `--kill-local`,
`--no-restore`, `--dry-run`.

---

## ⚠️ BUG: Python venv is not re-activated on restore/teleport

**Symptom.** When a session is restored (reboot / continuum auto-restore) or
teleported, panes for **`rb3`, `rb3-xenon`, and `dc3-decomp`** come back with the
Python venv **inactive**. Our decomp tooling (objdiff-cli's `custom_make`,
`configure.py`, `m2c`, the orchestrator MCP, the analysis scripts) depends on the
repo's venv, so those panes resume broken until the venv is activated by hand.

**The manual step that has to happen and currently doesn't.** As of 2026-07-13
each repo has its **own** uv-managed venv (see
`docs/dev/python-tooling-uv.md`), so activation is now simply, per repo:

```bash
cd ~/code/milohax/<game> && source venv/bin/activate   # <game> = rb3 | rb3-xenon | dc3-decomp
```

> Historical note: before the uv migration all three repos shared one venv, so
> the step was `cd ~/code/milohax/dc3-decomp && source venv/bin/activate && cd
> ../<game>`. The per-repo layout removes that cross-repo dance and makes the fix
> below a straight "activate this pane's own repo venv".

**Root cause.** `@resurrect-processes` only re-runs the captured command in the
pane's `cwd`. It does not run any shell setup (venv activation) first. resurrect
restores *processes*, not *shell environment*, so the `source venv/bin/activate`
prelude is lost. tmux-teleport inherits the same gap (it uses the same restore
path).

**Not yet fixed.** Options to consider when we build the fix (now simpler with
per-repo venvs):

- A restore hook / wrapper that, for panes whose `cwd` is inside a repo
  containing a `venv/`, prepends `source ./venv/bin/activate &&` (relative to the
  pane cwd's repo root) to the relaunched command — e.g. a custom
  `@resurrect-processes` entry or a `pane-restore`/`after-restore` hook.
- Or auto-activate the repo's own venv from `~/.zshrc` on `chpwd` (or via
  `direnv` + a per-repo `.envrc`) when `PWD` is inside a repo that has a `venv/`,
  so any new shell — restored or not — enters with the venv live.
