# Wave D Follow-up: Scanner Findings & Action Plan

**Date**: 2026-05-27
**Status**: Wave D execution in-progress

---

## 1. Header-Cluster Opportunities (top 10)

From `scripts/analysis/header_cluster.py --mode opportunities --min-cluster 5`:

| Rank | Match% | Fn Count | TUs | Impact Score | Sample Functions |
|------|--------|----------|-----|-------------|-----------------|
| 1 | 99.9% | 178 | 106 | 1971.8 | `__push_heap<pair<String,float>*,...>`, `Quazal::ProfilingUnit::ProfilingUnit`, `Quazal::MD5Checksum::ComputeChecksum` |
| 2 | 99.8% | 87 | 67 | 623.9 | `NetSession::NetSession()`, `VocalTrack::RebuildHUD()`, `GemPlayer::FillInProgress` |
| 3 | 99.6% | 76 | 66 | 535.3 | `Quazal::operator+`, `Latin1ToUtf8`, `Quazal::HMACChecksum::ComputeChecksum` |
| 4 | 99.7% | 68 | 58 | 425.8 | `CharStatKeeper::MaxEq`, `GemManager::SetupRealGuitarImportantStrings`, `VocalTrack::IdenticalLyric` |
| 5 | 99.5% | 52 | 50 | 283.1 | `Quazal::MD5::update`, `TrainerGemTab::Render`, `VocalPlayer::GetBestPercentage` |
| 6 | 99.4% | 49 | 42 | 227.5 | `Gem::AddWidgetInstanceImpl`, `Band::CheckCoda`, `PerfectSectionTracker::Poll_` |
| 7 | 99.3% | 47 | 43 | 222.4 | `UIScreen::~UIScreen`, `NetSession::Disconnect`, `VocalTrack::BuildScrollingDeployZones` |
| 8 | 99.2% | 40 | 38 | 169.2 | `Quazal::OperationManager::OperationEnds`, `GameMic::SetInputFile`, `GamePanel::IsLoaded` |
| 9 | 99.1% | 36 | 31 | 127.3 | `__introsort_loop<float*,...>`, `SongDB::SetupCommonPhrasesForTrack`, `VocalPart::GetBestHit` |
| 10 | 99.0% | 30 | 29 | 100.0 | `Quazal::Core::Core`, `CrowdRating::CalculateValue`, `GemTrainerPanel::IsGemInFutureLoop` |

**Key observation**: The #1 cluster (178 fns at exactly 99.9% across 106 TUs) is the biggest single opportunity. The cross-TU signature suggests a shared-header root cause — likely a one-instruction difference in a widely-included utility template (STL, math, or a Milo base class). Investigating 3-4 representative functions from this cluster and comparing their diff signature should reveal the root cause.

**Recommended investigation**: Pick one function from each of clusters #1-#3, run `run_diff_inspect --mode mismatches` on each. If all show the same mismatch signature, there's a single header fix worth doing.

---

## 2. Mismatch-Cluster Root Causes (top 5)

From `scripts/analysis/mismatch_cluster.py --mode all` (590 cached diffs):

| Rank | Type | Signature | Fn Count | Range | Root Cause Hypothesis | Fix Direction |
|------|------|-----------|----------|-------|-----------------------|---------------|
| 1 | DELETE | `lfs lfs` | 35 | 68-93% | Extra float loads in target — inline FP helper differs | Likely psq_*/ps_* cascade from Mtx.h/Vec.h inline asm; skip unless new psq_ theory |
| 2 | INSERT | `lis lfs` | 20 | 93-97% | Missing constant-pool load in base | Possible float-const-static pattern; check @F_ vs direct `fmr` |
| 3 | INSERT | `cmpwi bne` | 19 | 82-95% | Missing null/range check in base | Candidate for `null_guard_insert` pattern; check if MWCC adds guard vs source |
| 4 | DELETE | `stfs stfs` | 14 | 85-92% | Extra float stores in base | Same cascade as #1 — Mtx.h/Vec.h inline FP |
| 5 | DELETE | `stfd psq_st` | 12 | 92-96% | psq_st/stfd mismatch in paired-singles code | Classic psq_ cascade; permuter-class unless new psq_ pattern added |

Clusters #1 and #4 are the same root cause (35+14 = 49 functions). The `lfs lfs` / `stfs stfs` pattern is the psq_/ps_* inline FP cascade from Vec.h/Mtx.h — confirmed permuter-class based on memory entry [Mesh.cpp at-limit Vec.h/Mtx.h cascade]. Do NOT re-attack without a new psq_ theory.

Cluster #3 (`cmpwi bne`) is more interesting — 19 functions, likely fixable via a guard insertion. Inspect `StoreMetadataManager::OnMsg`, `SaveLoadManager::Poll`, and `GXSetTexCoordGen2` to find the common guard.

---

## 3. Top 30 Individual Workable Functions (from report.json, 90-98%)

Functions ordered by match% descending, prioritizing band3/ and system/ (excluding os/, rndwii/):

| % | Size | Unit | Symbol (short) |
|---|------|------|----------------|
| 98.0% | 2028 | system/rndobj/TransAnim | `RndTransAnim::MakeTransform` |
| 98.0% | 3116 | band3/meta_band/MusicLibraryNetSetlists | `ParseDataResultsIntoSetlists` |
| 98.0% | 744 | system/bandobj/ChordShapeGenerator | `ChordShapeGenerator::NameMesh` |
| 98.0% | 712 | band3/game/BandPerformer | `BandPerformer::WeightedCrowdLevel` |
| 98.0% | 2100 | system/char/CharCollide | `CharCollide::Deform` |
| 97.9% | 1356 | system/synth/Sfx | `_Vector_impl<MoggClipMap>::operator=` |
| 97.9% | 1168 | system/bandobj/BandList | `BandList::StartFocusAnim` |
| 97.9% | 1004 | system/beatmatch/KeyboardTrackWatcherImpl | `NextGemAfter` |
| 97.9% | 808 | system/bandobj/BandCharacter | `BandCharacter::SetDircuts` |
| 97.9% | 1056 | system/obj/Task | `TaskMgr::Poll` |
| 97.9% | 692 | system/char/CharIKFingers | `CharIKFingers::FixSingleFinger` |
| 97.9% | 1140 | band3/game/RGTrainerPanel | `RGTrainerPanel::HandleLegendLefty` |
| 97.8% | 556 | system/rndobj/Utl | `SetLocalScale` |
| 97.8% | 364 | band3/bandtrack/Gem | `Gem::Poll` |
| 97.8% | 1916 | band3/meta_band/SongSortMgr | `SongSortMgr::GetRandomSongs` |
| 97.8% | 908 | band3/bandtrack/VocalTrack | `VocalTrack::BuildStaticDeployZone` |
| 97.8% | 3360 | system/bandobj/BandDirector | `BandDirector::Enter` |
| 97.7% | 1884 | band3/bandtrack/VocalTrack | `VocalTrack::UpdatePitchArrow` |
| 97.6% | 908 | band3/bandtrack/Track | `Track::SetGem` |
| 97.6% | 692 | system/char/CharIKFingers | `CharIKFingers::FixSingleFinger` |
| 97.5% | 484 | system/rndobj/Mesh | `RndMesh::VertVector::operator=` |
| 97.5% | 380 | system/bandobj/BandIKEffector | `BandIKEffector::CalcLength` |
| 97.4% | 640 | band3/bandtrack/GemManager | `GemManager::EnableTailBlend` |
| 97.3% | 788 | band3/game/Band | `Band::SetupSections` |
| 97.2% | 760 | band3/meta_band/SaveLoadManager | `SaveLoadManager::GetDialogMsg` |
| 96.7% | 476 | band3/meta_band/SaveLoadManager | `SaveLoadManager::PrintoutSaveSizeInfo` |
| 96.5% | 648 | system/rndobj/MatAnim | `RndMatAnim::TexKeys::operator=` → **FIXED 99.4%** |
| 95.8% | — | band3/meta_band/SaveLoadManager | `SaveLoadManager::Poll` |
| 95.5% | — | system/char/CharLookAt | `CharLookAt::SyncLimits` |
| 95.0% | — | system/bandobj/BandIKEffector | `BandIKEffector::Poll` |

**Note**: `RndMatAnim::TexKeys::operator=` was fixed 96.5→99.4% in this wave (commit `d15f0f79`).

---

## 4. Recommended Follow-up Actions (prioritized)

### High priority

1. **Investigate the 99.9% cluster (178 fns)** — Run `run_diff_inspect --mode mismatches` on 3 representative functions from the #1 header cluster. If they share the same mismatch signature, a single header fix unlocks 178 functions. Sample: `VocalTrack::RebuildHUD`, `GemPlayer::FillInProgress`, `CharStatKeeper::MaxEq`.

2. **Run batch_auto sweep on 90-97% band3/ targets** — The permuter returned no improvements on TransAnim::MakeTransform (97%) and TaskMgr::Poll (97.6%) in 4 beam depths. These may need more rounds or a different strategy. Try `batch_auto --target unit --unit 'band3/bandtrack/*'` and `band3/meta_band/*` on idle cores.

3. **Apply `return_this_op_assign` to confirmed hits** — Pattern was confirmed working on `RndMatAnim::TexKeys::operator=` (+2.9pp). Review the remaining 4 candidate sites:
   - `network/Platform/DateTime.cpp` — `Quazal::DateTime::operator=` (out of scope for native port)
   - `network/Platform/Time.cpp` — `Quazal::Time::operator=` × 2 (out of scope)
   - `system/rndobj/Mesh.cpp` — `RndMesh::VertVector::operator=` — pattern NOT the fix here (register cascade instead; confirmed by mismatch analysis)

4. **Reclassify 466 stale AT_LIMIT entries** — `reclassify_at_limit.py` (dry-run) shows 466 AT_LIMIT candidates flagged as REOPEN (~80% MIXED, ~20% REGSWAP_PLUS). Run `--apply` to refresh the database, then `batch_auto` sweeps will attack these fresh.

5. **Investigate `cmpwi bne` mismatch cluster (19 fns at 82-95%)** — Likely a missing null-guard or range-check in base. Candidates: `StoreMetadataManager::OnMsg` (94.9%), `SaveLoadManager::Poll` (95.8%), `Band::CheckCoda`. This cluster is actionable without psq_ expertise.

### Medium priority

6. **Run longer permuter sweep on SaveLoadManager::Poll (95.8%)** — Switch-case reorder is the suspected fix (the switch state machine). The permuter's `switch_case_reorder` pattern needs asm-guided Phase 2 which requires the `.s` file. Ensure `build/SZBE69_B8/asm/SaveLoadManager.s` exists before running.

7. **function_health.py unit filter fix** — The `--top N` mode pulls from decomp.db which is mostly AT_LIMIT entries. The script should fall back to report.json and filter for `workability_score > 0`. The current tool picks "top priority" by AT_LIMIT significance, not "top workable". This makes the D1 recommendation less useful until fixed.

8. **Add bin/objdiff-cli symlink to worktree setup script** — `tools/setup-worktree.sh` doesn't symlink `bin/objdiff-cli` into the new worktree's `bin/`. The permuter can't run in a worktree without this symlink. Fix: add `ln -s "$MAIN_REPO/bin/objdiff-cli" "$WT/bin/objdiff-cli"` to `tools/setup-worktree.sh`.

### Low priority / stretch

9. **makestring_wrap_literal** — Pattern is implemented but the frame-size gate (needs ~0x82C delta) didn't fire for any of the 10 detected functions. The VocalTrack::UpdatePitchArrow case shows string offset shifts of only 0x183 (not 0x82C). The gate condition in the pattern may need tuning, or the issue is IPA-shared string pool ordering rather than per-function frame layout.

10. **Investigate 99.0% cluster (30 fns) shared root** — `Quazal::Core::Core`, `CrowdRating::CalculateValue`, `GemTrainerPanel::IsGemInFutureLoop` — a mix of game and network code at exactly 99.0%. Likely a single 1-2 instruction diff. Worth a 10-minute investigation.

---

## 5. Infrastructure Issues Found

- **Permuter requires `bin/objdiff-cli` symlink in worktrees** — see action item #8 above
- **function_health `--top N` returns AT_LIMIT entries** — not useful for "find workable" queries; filter should exclude `workability_score == 0`
- **reclassify_at_limit.py** must be run as `python3 -m scripts.analysis.reclassify_at_limit` (module form), not `python3 scripts/analysis/reclassify_at_limit.py` (import error with bare invocation)

---

## 6. Wave D Permuter Runs Summary

| Target | Start% | End% | Delta | Duration | Notes |
|--------|--------|------|-------|----------|-------|
| `RndMatAnim::TexKeys::operator=` | 96.5% | 99.4% | **+2.9pp** | ~5s | Manual fix — `return *this;` |
| `RndTransAnim::MakeTransform` | 97.1% | 97.1% | +0.0pp | 97s | 4 beam depths, 503 variants |
| `TaskMgr::Poll` | 97.6% | 97.6% | +0.0pp | 65s | 4 beam depths, 316 variants |
| `SaveLoadManager::Poll` | 95.6% | 95.6% | +0.0pp | 357s | 4 beam depths, 310 variants, 44 SEMANTIC_OK — no wins |
