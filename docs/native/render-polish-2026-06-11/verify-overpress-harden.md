# verify-overpress-harden — independent adversarial review (wave 7 close-out)

**Verdict: CONFIRM.** Both debug-verb over-press crash classes are hardened on the
composed master build. Independently reproduced BOTH crashes on a from-scratch
pre-fix binary, proved the fix inert-on-abuse, confirmed the happy path still
works, and verified both touched functions are Wii byte-identical (objdiff 100.0%).
`crashReproducesPostFix = false` — neither crash recurs post-fix.

- Reviewer: independent Opus, ports 9721–9729. Evidence: `/tmp/rp7rev-overpress-harden/`.
- Reviewed fix: rb3 `5f6f2379` (in history of master HEAD `996e2182`; my snapshot
  was `a36bfcf9` — the only intervening commit `996e2182` is a sibling's unrelated
  dta-eval verify doc, and the three fix files are untouched since).
- Freshly-built post-fix binary: `native/build-native/rb3-native` md5 `a442a3a9…`,
  mtime 2026-06-16 22:25 (verified it corresponds to the fix state).
- Engine pin unchanged (`15ce606`); no engine change in the fix.

---

## (a) gtest — `OverPress.*` 3/3 PASS, and the test genuinely exercises the OOB

`cmake --build native/build-native --target rb3-tests` (ninja: no work — already
current). Post-fix:
```
[ OK ] OverPress.ChangeDifficultyUnprocessedConfigIsInert
[ OK ] OverPress.ChangeDifficultyProcessedConfigUpdatesTrackDiff
[ OK ] OverPress.ChangeDifficultySpamUnprocessedIsInert
[ PASSED ] 3 tests.
```
Full suite (`-CharLoad5b.*`): **17/17 PASSED** — no regression.

I read `native/tests/test_overpress.cpp`. The crash-(1) test sets up the EXACT OOB
condition before the call and asserts the preconditions:
`list.AddConfig(...)`, `ASSERT_TRUE(list.mTrackDiffs.empty())`,
`ASSERT_EQ(list.GetTrackNumByUserGuid(u), -1)` → then `list.ChangeDifficulty(u, 3)`.
That is `mTrackDiffs[(size_t)-1]` on an empty `std::vector<int>`. `_GLIBCXX_ASSERTIONS`
is active in the test binary (verified: `std::__glibcxx_assert_fail` is referenced
and `"__n < this->size()"` / `"vector::_M_range_check"` strings are present), so the
unguarded subscript aborts. `mTrackDiffs` is `std::vector<int>` and **public** in
`PlayerTrackConfig.h:73` — the test's direct read is legitimate, not a synthetic
mock. The processed-path test (`Process({kTrackDrum})` → valid `TrackNum()`) confirms
the guard does NOT break the normal write (`mTrackDiffs[trk] == 2`).

## (b) LIVE post-fix — over-press abuse on charted 20thcenturyboy → no crash

`live_overpress.py` (pad bits matched to `keyboard-to-gameplay.py`: START=11,
CONFIRM=6, DDOWN=14) boots `native/build-native/rb3-native` to **game_screen**
(20thcenturyboy, hard, is_playing=1), then:
- ABUSE 1 — `difficulty:` spam ×30 out-of-order/redundant (easy/medium/hard/expert/
  0/1/2/3 interleaved) → **survived, alive=True**.
- ABUSE 2 — double + ×4 `msg:overshell:end_override_flow:1:0` + variant types
  (`1:1`, `2:0`, `0:0`) → **survived, alive=True**. The post-fix engine log shows the
  no-op gate firing 6× ("EndOverrideFlow no-op — flow not active (curFlow=0)"), no
  `Line: 432`, no SIGABRT.
- Happy path after abuse: `difficulty:expert` → diff=`expert`, `difficulty:easy`
  → diff=`easy`; process still alive on game_screen.

(`/tmp/rp7rev-overpress-harden/postfix/result.json`:
`reached_gameplay:true, diff_spam_survived:true, double_end_survived:true`.)

The canonical `keyboard-to-gameplay.py --diff hard` also PASSes on this binary
(reaches game_screen, song playing) — boot path is healthy.

## (c) Attribution — pre-fix binary, built from scratch, crashes on BOTH

Reverted ONLY the two source fixes (`git checkout 5f6f2379~1 -- PlayerTrackConfigList.cpp
OvershellPanel.cpp`) in a worktree, kept the test + CMake wire-in, and built fresh
pre-fix `rb3-tests` + `rb3-native` (md5 `46d212cf…`, ≠ post-fix `a442a3a9…`).

- **Crash (1)** — pre-fix gtest:
  `OverPress.ChangeDifficultyUnprocessedConfigIsInert` aborts with
  `stl_vector.h:1253: std::vector<int>::operator[]: Assertion '__n < this->size()'
  failed.` → core dumped. Post-fix: OK. (Frame-for-frame the documented backtrace.)
- **Crash (2)** — pre-fix LIVE on charted 20thcenturyboy: the **2nd** (redundant)
  `end_override_flow:1:0` SIGABRTs (rc=134 = 128+6). Engine log:
  `GAME_DBG: …EndOverrideFlow(type=1, cancel=0) curFlow=0` →
  `FAIL-MSG: …OvershellPanel.cpp Line: 432 Error: InOverrideFlow(type)` →
  `caught SIGABRT (signal 6)`. Exactly the documented `:432` assert.

Note: the `difficulty:` spam did **not** crash live on the pre-fix binary
(`diff_spam_survived:true`), because by game_screen the config is Process()'d so
`TrackNum()` is valid and `mTrackDiffs` non-empty. Crash (1) only manifests in the
unprocessed/chartless condition — which is precisely why the impl made it a
deterministic gtest (the live chartless path can't be booted headlessly). The
gtest before/after IS the authoritative crash-(1) attribution and it is decisive.
This matches the impl doc's own framing — not a gap.

## (d) Happy path intact
- gtest `ChangeDifficultyProcessedConfigUpdatesTrackDiff`: processed config still
  writes `mTrackDiffs[trk]` at the valid index (PASS).
- Live: normal in-order difficulty changes track correctly (easy→expert→easy);
  the FIRST `end_override_flow` still properly ends the active SongSettings flow
  (curFlow 1→0) — only redundant ends are no-op'd.
- Full boot-to-gameplay PASSes on the post-fix binary.

## (e) Wii byte-identity — verified myself
objdiff on the composed master build:
- `ChangeDifficulty__21PlayerTrackConfigListFRC8UserGuidi` — **100.0%** (56/56 equal).
- `EndOverrideFlow__14OvershellPanelF21OvershellOverrideFlowb` — **100.0%** (70/70 equal).
Both edits are strictly inside `#ifdef HX_NATIVE`; the Wii compile is unchanged.

---

## Residuals / notes (none block; none are regressions)
- Crash (1) has no headless LIVE repro (chartless boot fails on `RB3_DATA` overlay
  per the impl doc; all current songs ship a `.mid`). The gtest is the standing
  regression lock and it is genuine. This is a verification-method note, not a defect.
- The fix is purely robustness for the debug HTTP `/api/input` surface; normal play
  never hits either path. Strictly better than baseline.

crashReproducesPostFix = false.
