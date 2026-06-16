// Debug-verb OVER-PRESS hardening regression tests (render-polish wave 7).
//
// Two crash classes the wave-6 track-load review surfaced when an /api/input
// harness spams abnormal / out-of-order debug verbs. They are NOT normal play
// — they are our own headless harness's failure mode driving the debug HTTP
// surface every future review wave uses.
//
// CRASH (1): PlayerTrackConfigList::ChangeDifficulty std::vector<int> OOB.
//   `mTrackDiffs[cfg.TrackNum()] = i;` (PlayerTrackConfigList.cpp:107) indexes
//   mTrackDiffs by the config's track number with NO bounds check. Before the
//   song is Process()'d (e.g. a chartless song where Process() never ran, or any
//   config that never resolved a track), cfg.TrackNum() is the -1 default, so the
//   index is (size_t)-1 → with _GLIBCXX_ASSERTIONS the vector subscript aborts
//   ("vector::_M_range_check"/__glibcxx_assert_fail). The `difficulty:` debug
//   verb (ExecDifficulty → BandUser::SetDifficulty → Player::ChangeDifficulty →
//   GameConfig::ChangeDifficulty → here) reaches this when over-pressed.
//
// This test drives the EXACT crashing function with the EXACT condition (an
// unprocessed config, TrackNum()==-1) so the repro is deterministic and the fix
// (a native-only bounds guard) is regression-locked. The pre-fix build aborts
// here; the post-fix build returns inertly (the difficulty is still recorded on
// the config so a later Process() applies it).
//
// CRASH (2): OvershellPanel::EndOverrideFlow Debug::Fail "InOverrideFlow(type)"
//   (OvershellPanel.cpp) is exercised end-to-end in the live game (a double
//   `end_override_flow` verb). It is NOT unit-tested here — building a real
//   OvershellPanel requires the full meta_band UI graph; see the impl doc for
//   the in-game gdb backtrace. The native guard makes a redundant end a no-op.

#include "test_helpers.h"

#include "beatmatch/PlayerTrackConfig.h"
#include "beatmatch/TrackType.h"
#include "utl/HxGuid.h"

// A distinct non-null UserGuid (the actual bytes are irrelevant — only identity
// vs. gNullUserGuid and self-equality matter for the config lookup).
static UserGuid MakeUser(unsigned int seed) {
    UserGuid u;
    // HxGuid stores 16 bytes; poke a non-zero pattern so it != gNullUserGuid and
    // GetConfigByUserGuid finds it by ==.
    unsigned char *p = reinterpret_cast<unsigned char *>(&u);
    for (int i = 0; i < (int)sizeof(UserGuid); i++)
        p[i] = (unsigned char)(seed + i + 1);
    return u;
}

class OverPress : public EngineTestFixture {};

// CRASH (1) repro / regression. An unprocessed PlayerTrackConfigList: a config
// exists (added via AddConfig) but Process() never ran, so mTrackDiffs is empty
// and the config's TrackNum() is the -1 default. Pre-fix this aborts in the
// std::vector<int> subscript; post-fix it must return without crashing.
TEST_F(OverPress, ChangeDifficultyUnprocessedConfigIsInert) {
    PlayerTrackConfigList list(4);
    UserGuid u = MakeUser(0x10);
    // Add a real config; AddConfig leaves mTrackNum at its -1 default until
    // Process() assigns a track. mTrackDiffs is still empty (Process not run).
    list.AddConfig(u, kTrackDrum, /*difficulty*/ 1, /*slot*/ 0, /*remote*/ false);
    ASSERT_EQ(list.NumConfigs(), 1);
    ASSERT_TRUE(list.mTrackDiffs.empty());
    ASSERT_EQ(list.GetTrackNumByUserGuid(u), -1)
        << "precondition: config is unprocessed (TrackNum == -1)";

    // The crashing call. Pre-fix: mTrackDiffs[-1] -> __glibcxx_assert_fail abort.
    // Post-fix: inert (no write), difficulty still updated on the config.
    list.ChangeDifficulty(u, 3);

    // The config's recorded difficulty is updated regardless (Update runs before
    // the guarded index), so a later Process() picks it up.
    EXPECT_EQ(list.GetConfigByUserGuid(u).mDifficulty, 3);
}

// Sanity: the NORMAL processed path still writes mTrackDiffs at the valid index.
TEST_F(OverPress, ChangeDifficultyProcessedConfigUpdatesTrackDiff) {
    PlayerTrackConfigList list(4);
    UserGuid u = MakeUser(0x20);
    list.AddConfig(u, kTrackDrum, /*difficulty*/ 0, /*slot*/ 0, /*remote*/ false);

    // Process() with a track-type table that contains the config's type so its
    // TrackNum resolves to a valid index into mTrackDiffs.
    std::vector<TrackType> types;
    types.push_back(kTrackDrum);
    list.Process(types);

    int trk = list.GetTrackNumByUserGuid(u);
    ASSERT_GE(trk, 0) << "config should have resolved a real track after Process()";
    ASSERT_LT(trk, (int)list.mTrackDiffs.size());

    list.ChangeDifficulty(u, 2);
    EXPECT_EQ(list.GetConfigByUserGuid(u).mDifficulty, 2);
    EXPECT_EQ(list.mTrackDiffs[trk], 2)
        << "processed path must still write the per-track difficulty";
}

// Redundant ChangeDifficulty spam on an unprocessed list must stay inert across
// many calls (the over-press pattern: 24 difficulty: verbs in a burst).
TEST_F(OverPress, ChangeDifficultySpamUnprocessedIsInert) {
    PlayerTrackConfigList list(4);
    UserGuid u = MakeUser(0x30);
    list.AddConfig(u, kTrackGuitar, 0, 0, false);
    const int diffs[] = {3, 0, 2, 1, 0, 3, 2, 0};
    for (int rep = 0; rep < 3; rep++)
        for (int i = 0; i < 8; i++)
            list.ChangeDifficulty(u, diffs[i]); // must not abort
    EXPECT_EQ(list.GetConfigByUserGuid(u).mDifficulty, 0);
}
