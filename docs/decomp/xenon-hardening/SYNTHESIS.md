# SYNTHESIS — Wii↔Xenon precision-hardening workflow (2026-06-10)

**Author:** final synthesis agent (Fable).
**Human-facing session record (authoritative, full detail):**
[`../xenon-precision-hardening-2026-06-10.md`](../xenon-precision-hardening-2026-06-10.md).
This doc is the agent-handoff digest: verdict matrix, what synthesis itself changed, and the
open-item queue.

## Verdict matrix

| Task | Verdict | Load-bearing result | Rerun-blocking defect? |
|---|---|---|---|
| T1 string gate (ghidriff `5b9cc4e`) | **CONFIRMED** | 25/25 judged-wrong killed; SRH 610→283, STU 45→10; 3 oracle-error survivors kept; byte-identical independent gzf reproduction | No |
| T2 score export + VT STL gate (ghidriff `31a6f6c`) | **CONFIRMED** | optional `scores` field (PLAN §3) + 19/722 STL exclusions (5 judged-wrong); 18/18 tests; thresholds now offline-sweepable | No |
| T3 BSim deploy + runner (ghidra `1220f13915`+`eebbd8ba3a`, rb3 `d53240b8`) | **PARTIAL → fixed** | patches on branch + jar-swap reachable; `--no-decomp-correlate` landed. REFUTED: RB3_XENON_BSIM toggle crashed argparse / silently disabled BSim — **fixed in synthesis, rb3 `53f7a6aa`**, re-verified per task-T3-verify §4 recipe | Was — now cleared |
| T4 eval/seeds (rb3 `da52aac0`) | **CONFIRMED** | default-off + byte-identical replay; `--credit-platform-alias` → VT 0.486 / OVERALL 0.500 PROVEN on existing artifacts; seeds 1,189→1,213 (+24 honest, not +41); caught the Bank5-addr landmine in rb3wii | No |
| T5 vetting tool (rb3 `d17d5e55`) | **PARTIAL** | tiering core + calibration confirmed (1268/280/442/655); REFUTED: `rb3wii_check` is Bank8-vs-Bank5 garbage (28/637 "contradicted" are name agreements), `category` 79% mislabeled, ExactMnemonics in default ACCEPT, 65 null wii_addr | No (post-run tool) — but consumers must not use the two refuted fields |

## What synthesis changed (beyond docs)

1. **rb3 master fast-forwarded** `d935f117..9658d75e` (`xenon-hardening-t1` → master; T1-verify
   required followup). Working tree back on master; concurrent agents' unstaged files untouched.
2. **`tools/ghidra/run_ghidriff_xenon.sh` BSim toggle fixed** (rb3 `53f7a6aa`): `BSIM_FLAG`
   conditional (unset/`=1` → `--bsim` ON default; `=0` → `--no-bsim`), comment aligned, JAVA_HOME
   marked dead. Verified: `bash -n`; 3-env dry-run table (unset→`--bsim`, 1→`--bsim`,
   0→`--no-bsim`, `--no-decomp-correlate` present in all); exact-CMD replay through ghidriff's
   real argparse → `PARSE OK; bsim=True; decomp_correlate=False` (and `bsim=False` for `=0`).
3. Committed all untracked handoff docs (PLAN, scouts, T2–T5 verify docs, forensics scripts)
   plus this doc and the session record.

## Readiness call

**READY for the human-gated re-run.** Every rerun-blocking defect found by the adversarial
verifiers is closed. Exact playbook, proven-vs-predicted outcome table, and risk list: session
record §3–§5. Short form:

```bash
cd /home/free/code/milohax/rb3
./tools/ghidra/run_ghidriff_xenon.sh    # defaults now correct: fork dist, BSim ON, --no-decomp-correlate
build/SZBE69_B8/ghidra/ghidriff-venv/bin/python tools/ghidra/eval_xenon_matches.py \
  --run-dir build/SZBE69_B8/ghidra/ghidriff-xenon --credit-platform-alias --stratify
# then: --sweep-vt-score 9.5:14:0.5 offline; then vet_xenon_identities.py (AFTER T5 fixes)
```

## For the next agent (open queue, in order)

1. **T5 fixes** (task-T5-verify.md §For-the-next-agent): Bank5-ELF mangled-name join for
   `rb3wii_check` (recipe + measured 28/637 flips in the doc); replace `_categorize_full` with the
   eval's `categorize_tu`; drop `ExactMnemonicsFunctionHasher` from default accept; emit
   `wii_addr` unconditionally; regenerate `vetted_identities.json`.
2. **Sub-mode-B oracle decision** before reading the next eval: xenon `0x827ffbf8`, `0x827d2588`,
   `0x827f7cc0` are BinDiff-oracle errors; without an exception SRH mechanically re-reports 0.000
   on a 3-pair judged set (task-T1-verify §5.2). Do NOT read that as gate failure.
3. **Jar-swap fragility**: if anyone rebuilds the ghidra fork dist, re-apply the BSim jar swap
   (task-T3-impl.md; rollback `.orig` exists).
4. Post-run: sanity-check every VT entry carries `scores...product` and every Implied entry
   `ratio` (bare-hex addresses, no `0x`); compare per-type precision vs baseline, not raw counts
   (BSim ON shifts pools); re-baseline T2's `== 722`/`== 19` replay test if the VT pool moved.
5. Optional: `--skip-correlators StrUniqueFuncRefsHasher` (near-duplicate of gated SRH).
