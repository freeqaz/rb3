#include "meta_band/MetaPanel.h"
#include "AccomplishmentPanel.h"
#include "AppInlineHelp.h"
#include "AppLabel.h"
#include "AppMiniLeaderboardDisplay.h"
#include "AppScoreDisplay.h"
#include "BandScreen.h"
#include "BandStorePanel.h"
#include "BandStoreUIPanel.h"
#include "Calibration.h"
#include "CampaignCareerLeaderboardPanel.h"
#include "CampaignGoalsLeaderboardChoicePanel.h"
#include "CampaignSongInfoPanel.h"
#include "CharacterCreatorPanel.h"
#include "ChooseColorPanel.h"
#include "ClosetPanel.h"
#include "ContentDeletePanel.h"
#include "ContentLoadingPanel.h"
#include "CustomizePanel.h"
#include "EditSetlistPanel.h"
#include "GameTimePanel.h"
#include "ManageBandPanel.h"
#include "ModifierMgr.h"
#include "MultiSelectListPanel.h"
#include "NewAwardPanel.h"
#include "NextSongPanel.h"
#include "ParentalControlPanel.h"
#include "PassiveMessenger.h"
#include "PatchPanel.h"
#include "PatchSelectPanel.h"
#include "ProfileMgr.h"
#include "RetryAudioPanel.h"
#include "SaveLoadStatusPanel.h"
#include "SelectDifficultyPanel.h"
#include "SessionMgr.h"
#include "SetlistMergePanel.h"
#include "SetlistToStorePanel.h"
#include "SigninScreen.h"
#include "SongSelectPanel.h"
#include "SongSortMgr.h"
#include "StoreInfoPanel.h"
#include "StoreMainPanel.h"
#include "StoreMenuPanel.h"
#include "StoreRootPanel.h"
#include "TexLoadPanel.h"
#include "TokenRedemptionPanel.h"
#include "TrainingPanel.h"
#include "UGCPurchasePanel.h"
#include "UploadErrorMgr.h"
#include "Utl.h"
#include "VoiceoverPanel.h"
#include "game/BandUserMgr.h"
#include "game/GameMode.h"
#include "meta/CreditsPanel.h"
#include "meta/HAQManager.h"
#include "meta/HeldButtonPanel.h"
#include "meta/MemcardMgr_Wii.h"
#include "meta/Meta.h"
#include "meta/MetaMusicManager.h"
#include "meta/MoviePanel.h"
#include "meta_band/BandPreloadPanel.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/BandUI.h"
#include "meta_band/CampaignGoalsLeaderboardPanel.h"
#include "meta_band/EventDialogPanel.h"
#include "meta_band/InterstitialPanel.h"
#include "meta_band/MainHubPanel.h"
#include "meta_band/MetaNetMsgs.h"
#include "meta_band/MetaPerformer.h"
#include "meta_band/NameGenerator.h"
#include "meta_band/OvershellPanel.h"
#include "net/NetMessage.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "os/PlatformMgr.h"
#include "obj/DataFunc.h"
#include "tour/QuestFilterPanel.h"
#include "tour/TourChallengeResultsPanel.h"

void UtlInit();

// Classes with no header yet — defined inline for factory registration
class TourDescPanel : public UIPanel {
public:
    OBJ_CLASSNAME(TourDescPanel);
    NEW_OBJ(TourDescPanel);
};

class JoinInvitePanel : public UIPanel {
public:
    OBJ_CLASSNAME(JoinInvitePanel);
    NEW_OBJ(JoinInvitePanel);
};

class WiiFriendsScreen : public UIPanel {
public:
    static void Init();
    OBJ_CLASSNAME(WiiFriendsScreen);
    NEW_OBJ(WiiFriendsScreen);
};

class WiiProfilePanel : public UIPanel {
public:
    OBJ_CLASSNAME(WiiProfilePanel);
    NEW_OBJ(WiiProfilePanel);
};

class WiiFriendsDetailsProvider : public Hmx::Object {
public:
    OBJ_CLASSNAME(WiiFriendsDetailsProvider);
    NEW_OBJ(WiiFriendsDetailsProvider);
};

class WiiFriendsProvider {
public:
    void Init();
    int pad[1]; // size > 2 to avoid sda21 addressing
};
extern WiiFriendsProvider TheWiiFriendsProvider;

class WiiInvitationsProvider {
public:
    void Init();
    int pad[1]; // size > 2 to avoid sda21 addressing
};
extern WiiInvitationsProvider TheWiiInvitationsProvider;

bool MetaPanel::sUnlockAll;
bool MetaPanel::sIsPlaytest;
bool MetaPanel::sLaunchedGoalMsgsOnly;

NetMessage *BandEventPreviewMsg::NewNetMessage() { return new BandEventPreviewMsg(); }
NetMessage *TriggerBackSoundMsg::NewNetMessage() { return new TriggerBackSoundMsg(); }
NetMessage *VerifyBuildVersionMsg::NewNetMessage() { return new VerifyBuildVersionMsg(); }
NetMessage *AppendSongToSetlistMsg::NewNetMessage() {
    return new AppendSongToSetlistMsg();
}
NetMessage *RemoveLastSongFromSetlistMsg::NewNetMessage() {
    return new RemoveLastSongFromSetlistMsg();
}

DataNode MetaPanel::ToggleUnlockAll(DataArray *) { return sUnlockAll = !sUnlockAll; }
DataNode MetaPanel::ToggleIsPlaytest(DataArray *) { return sIsPlaytest = !sIsPlaytest; }
DataNode MetaPanel::ToggleLaunchedGoalMsgsOnly(DataArray *) {
    return sLaunchedGoalMsgsOnly = !sLaunchedGoalMsgsOnly;
}

void MetaPanel::Init() {
    MetaInit();
    REGISTER_OBJ_FACTORY(CampaignGoalsLeaderboardPanel);
    REGISTER_OBJ_FACTORY(CampaignCareerLeaderboardPanel);
    REGISTER_OBJ_FACTORY(CampaignGoalsLeaderboardChoicePanel);
    REGISTER_OBJ_FACTORY(CampaignSongInfoPanel);
    REGISTER_OBJ_FACTORY(AccomplishmentPanel);
    REGISTER_OBJ_FACTORY(NewAwardPanel);
    REGISTER_OBJ_FACTORY(BackdropPanel);
    REGISTER_OBJ_FACTORY(BandPreloadPanel);
    REGISTER_OBJ_FACTORY(BandScreen);
    REGISTER_OBJ_FACTORY(BandStorePanel);
    REGISTER_OBJ_FACTORY(BandStoreUIPanel);
    REGISTER_OBJ_FACTORY(CalibrationPanel);
    REGISTER_OBJ_FACTORY(CalibrationWelcomePanel);
    REGISTER_OBJ_FACTORY(CharacterCreatorPanel);
    REGISTER_OBJ_FACTORY(ChooseColorPanel);
    REGISTER_OBJ_FACTORY(ClosetPanel);
    REGISTER_OBJ_FACTORY(ContentDeletePanel);
    REGISTER_OBJ_FACTORY(ContentLoadingPanel);
    REGISTER_OBJ_FACTORY(CreditsPanel);
    REGISTER_OBJ_FACTORY(CustomizePanel);
    REGISTER_OBJ_FACTORY(EditSetlistPanel);
    REGISTER_OBJ_FACTORY(EventDialogPanel);
    REGISTER_OBJ_FACTORY(GameTimePanel);
    REGISTER_OBJ_FACTORY(HeldButtonPanel);
    REGISTER_OBJ_FACTORY(InterstitialPanel);
    REGISTER_OBJ_FACTORY(OvershellPanel);
    REGISTER_OBJ_FACTORY(MainHubPanel);
    REGISTER_OBJ_FACTORY(ManageBandPanel);
    REGISTER_OBJ_FACTORY(MetaPanel);
    REGISTER_OBJ_FACTORY(MoviePanel);
    REGISTER_OBJ_FACTORY(MultiSelectListPanel);
    REGISTER_OBJ_FACTORY(NextSongPanel);
    REGISTER_OBJ_FACTORY(PassiveMessagesPanel);
    REGISTER_OBJ_FACTORY(PatchPanel);
    REGISTER_OBJ_FACTORY(PatchSelectPanel);
    REGISTER_OBJ_FACTORY(ParentalControlPanel);
    REGISTER_OBJ_FACTORY(RetryAudioPanel);
    REGISTER_OBJ_FACTORY(QuestFilterPanel);
    REGISTER_OBJ_FACTORY(TourDescPanel);
    REGISTER_OBJ_FACTORY(TourChallengeResultsPanel);
    REGISTER_OBJ_FACTORY(JoinInvitePanel);
    REGISTER_OBJ_FACTORY(SaveLoadStatusPanel);
    REGISTER_OBJ_FACTORY(SetlistMergePanel);
    REGISTER_OBJ_FACTORY(SetlistToStorePanel);
    REGISTER_OBJ_FACTORY(SelectDifficultyPanel);
    REGISTER_OBJ_FACTORY(SigninScreen);
    REGISTER_OBJ_FACTORY(SongSelectPanel);
    REGISTER_OBJ_FACTORY(StoreInfoPanel);
    REGISTER_OBJ_FACTORY(StoreMainPanel);
    REGISTER_OBJ_FACTORY(StoreMenuPanel);
    REGISTER_OBJ_FACTORY(StoreRootPanel);
    REGISTER_OBJ_FACTORY(TexLoadPanel);
    REGISTER_OBJ_FACTORY(TokenRedemptionPanel);
    REGISTER_OBJ_FACTORY(TrainingPanel);
    REGISTER_OBJ_FACTORY(UGCPurchasePanel);
    REGISTER_OBJ_FACTORY(VoiceoverPanel);
    OvershellPanel::Init();
    WiiFriendsScreen::Init();
    REGISTER_OBJ_FACTORY(WiiFriendsScreen);
    TheWiiFriendsProvider.Init();
    TheWiiInvitationsProvider.Init();
    REGISTER_OBJ_FACTORY(WiiProfilePanel);
    REGISTER_OBJ_FACTORY(WiiFriendsDetailsProvider);
    GameModeInit();
    ModifierMgr::Init();
    SongSortMgr::Init();
    SessionMgr::Init();
    TheMemcardMgr.Init();
    TheProfileMgr.Init();
    MetaPerformer::Init();
    UploadErrorMgr::Init();
    REGISTER_OBJ_FACTORY(AppInlineHelp);
    REGISTER_OBJ_FACTORY(AppScoreDisplay);
    REGISTER_OBJ_FACTORY(AppLabel);
    AppMiniLeaderboardDisplay::Init();
    BandEventPreviewMsg::Register();
    TriggerBackSoundMsg::Register();
    VerifyBuildVersionMsg::Register();
    AppendSongToSetlistMsg::Register();
    RemoveLastSongFromSetlistMsg::Register();
    UtlInit();
    DataRegisterFunc("toggle_unlock_all", ToggleUnlockAll);
    DataRegisterFunc("toggle_playtest_flag", ToggleIsPlaytest);
    DataRegisterFunc("toggle_launched_goal_msgs_only", ToggleLaunchedGoalMsgsOnly);
}

MetaPanel::MetaPanel()
    : mTour(new Tour(SystemConfig("tour"), TheSongMgr, *TheBandUserMgr, true)),
      mCampaign(new Campaign(SystemConfig("campaign"))),
      mNameGenerator(new NameGenerator(SystemConfig("name_generator"))),
      mMetaMusicMgr(new MetaMusicManager(SystemConfig("synth", "metamusic"))),
      mHAQMgr(new HAQManager()), unk58(0), mMusic(0), mSongPreview(TheSongMgr), unkd4(0) {
    mSongPreview.SetName("song_preview", ObjectDir::Main());
    MusicLibrary::Init(mSongPreview);
    mRecentIndices.reserve(3);
    for (int i = 0; i < 3; i++)
        mRecentIndices.push_back(-1);
    ThePlatformMgr.AddSink(this, "xmp_state_changed");
    TheBandUI.AddSink(this, "current_screen_changed");
    mHAQMgr->Init();
}

MetaPanel::~MetaPanel() {
    RELEASE(mTour);
    RELEASE(mCampaign);
    RELEASE(mNameGenerator);
    RELEASE(mMetaMusicMgr);
    RELEASE(mHAQMgr);
    TheBandUI.RemoveSink(this, "current_screen_changed");
}
