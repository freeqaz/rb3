# `grep` is shadowed here, and it silently returns nothing on engine logs

**Discovered 2026-08-04.** This voided an entire investigation
(`OUTFIT-CHAIN-DEAD.md`) that concluded a live code path was dead. Read this
before trusting any "0 occurrences" measured against generated output.

## The mechanism

Claude Code's shell snapshot defines a **shell function** named `grep` that
shadows `/usr/bin/grep`:

```
$ type grep
grep is a shell function from ~/.claude/shell-snapshots/snapshot-zsh-*.sh
```

It re-executes the `claude` binary as `ugrep`:

```sh
exec -a ugrep "$_cc_bin" -G --ignore-files --hidden -I \
     --exclude-dir=.git --exclude-dir=.svn ... "$@"
```

The load-bearing flag is **`-I`** — *ignore binary files*. ugrep decides
"binary" by scanning content, and **an engine log that contains a single
invalid UTF-8 byte sequence is classified as binary in its entirety.** ugrep
then prints **nothing** and exits **1**.

That output is **byte-identical to a genuine "no matches"** result. There is no
warning, no stderr, no distinguishing exit code.

## Measured instance

`/tmp/rb3-bandcloseup-SY-57449.log` — 4578 lines, `file(1)` reports
`Unicode text, UTF-8 text`, one invalid sequence at **byte offset 112481**
(~line 1777, found with `iconv -f UTF-8 -t UTF-8 >/dev/null`).

```
shadowed grep -c 'dirloader-cleanup'  ->  (empty)      exit 1
/usr/bin/grep -c 'dirloader-cleanup'  ->  60           exit 0
```

Every probe count in the outfit-chain investigation was `0` under the shadowed
grep. The real counts: `SyncObjects` 40+, `SyncDrawables` 40+,
`UpdatePreClearState` 20+, `DrawPreClear` 20+, `MatSwap::Compose` 40+ — i.e.
**the chain that was declared dead was fully alive**, and every count was at
its probe cap rather than at zero.

## Why the usual safeguards did not catch it

The project already requires confirming the probe string is in the binary and
that the binary predates the log. **Both of those passed.** They validate the
*producer*; this defect is in the *reader*. A whole class of "verify the
instrument" checks sits upstream of the failure and cannot see it.

It also survives bisection-by-prefix in a misleading way: `head -1000 log` greps
fine (the bad byte is at line ~1777), so a small reproduction **works**, which
reads as evidence the tool is healthy.

## The rule

**Use `/usr/bin/grep` explicitly for any generated output** — engine logs,
capture harness logs, build logs, objdiff dumps, anything a program wrote.

```sh
G=/usr/bin/grep
$G -c '\[syncobjects\]' "$LOG"
```

Bare `grep` remains fine for source trees (that is what the wrapper is tuned
for, and `--ignore-files` is a feature there).

**Zero-cost sanity check** before believing any zero: grep for a string you
*know* is present in the same file.

```sh
grep -c '^' "$LOG"        # shadowed: empty on a "binary" file
/usr/bin/grep -c '^' "$LOG"   # real: the line count
```

If a pattern that must match returns nothing, the reader is broken, not the
code under test.

## Related

- `docs/decomp/ninja-dry-run-false-negative.md` — same shape: a tool that
  reports an empty result as a pass, indistinguishable from success.
- The `cmd | tail -N; echo $?` trap (reports *tail's* exit status), which bit
  this same investigation three times in one session.

Common thread: **silence is not a measurement.** Any instrument that can emit
"nothing found" without having actually looked will eventually do so, and the
output will be indistinguishable from a real negative.
