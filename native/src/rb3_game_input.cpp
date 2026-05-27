// rb3 native — headless synthetic-input driver + screen-flow trace.
//
// RB3 has no Joypad_Native linked, so the real button path (Joypad ->
// JoypadClient -> UIManager sink -> focus screen/panel/component) never gets a
// physical button. This file injects synthetic ButtonDownMsg's into TheUI at
// scripted frames, exactly the way Automator::Poll does (UI.cpp:204-212):
//
//     static ButtonDownMsg b_msg(user, button, action, padnum);
//     TheUI.Handle(b_msg, false);
//
// which routes UIManager -> mCurrentScreen (HANDLE_MEMBER_PTR) -> FocusPanel()
// -> focus UIComponent. A focused UIButton turns a kAction_Confirm into a
// SendSelect -> UIComponentSelectMsg ("component_select" = SELECT_MSG), which is
// what the splash/main_hub/song-select panel DTAs gate their state machines on.
//
// Driven by RB3_GAME_INPUT="@30:start,@90:confirm,@120:down,@150:confirm"
// (frame:action pairs; actions: start/confirm/cancel/up/down/left/right/option).
//
// Native-only glue — no DTA edits, no matched-fork source edits.

#include "ui/UI.h"
#include "ui/UIScreen.h"
#include "ui/UIPanel.h"
#include "ui/UIComponent.h"
#include "os/Joypad.h"
#include "os/JoypadMsgs.h"
#include "os/User.h"
#include "game/BandUserMgr.h"
#include "game/Defines.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/BandSongMgr.h"
#include "obj/DataFile.h"
#include "os/System.h"
#include "obj/Object.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "utl/Symbol.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <vector>

namespace {

struct ScriptedInput {
    int frame;
    JoypadAction action;
    JoypadButton button;
};

// A "select:<name>" directive: find a UIComponent by name in the current screen's
// focus panel (or anywhere in the current screen) and SendSelect — the exact real
// flow a focused UIButton runs on Confirm (UIComponent::SendSelect ->
// UIComponentSelectMsg = the DTA SELECT_MSG/component_select). Lets a script drive
// a specific hub/menu button without depending on the milo's d-pad nav graph.
struct ScriptedSelect {
    int frame;
    std::string button;
};

// A "msg:<object>:<action>[:arg]..." directive: send a DTA message to a named
// ObjectDir::Main() object — the exact handler a DTA SELECT_MSG case invokes
// (e.g. {music_library select_highlighted_node $user}). With no extra args the
// single arg is $synthUser (the song-confirm case). Extra colon-separated args
// are appended literally: an integer literal, a `$user` placeholder (the synth
// user object), or a bare symbol name. e.g.
//   msg:overshell:end_override_flow:1:0   -> {overshell end_override_flow kOverrideFlow_SongSettings FALSE}
struct ScriptedMsg {
    int frame;
    std::string object;
    std::string action;
    std::vector<std::string> args; // empty => default {action $user}
};

std::vector<ScriptedInput> gScript;
std::vector<ScriptedSelect> gSelectScript;
std::vector<ScriptedMsg> gMsgScript;
bool gScriptParsed = false;
Symbol gLastScreen;
LocalUser *gSynthUser = nullptr;

JoypadAction ActionFromName(const std::string &name, JoypadButton &btnOut) {
    // Default the raw button to the nav d-pad equivalent so list nav (UIList)
    // and OverloadHorizontalNav see a consistent button<->action pairing.
    btnOut = kPad_NumButtons;
    if (name == "confirm") { btnOut = kPad_X;      return kAction_Confirm; }
    if (name == "start")   { btnOut = kPad_Start;  return kAction_Start; }
    if (name == "cancel" || name == "back")
                           { btnOut = kPad_Circle; return kAction_Cancel; }
    if (name == "option")  { btnOut = kPad_Tri;    return kAction_Option; }
    if (name == "up")      { btnOut = kPad_DUp;   return kAction_Up; }
    if (name == "down")    { btnOut = kPad_DDown; return kAction_Down; }
    if (name == "left")    { btnOut = kPad_DLeft; return kAction_Left; }
    if (name == "right")   { btnOut = kPad_DRight;return kAction_Right; }
    return kAction_None;
}

void ParseScript() {
    gScriptParsed = true;
    const char *spec = getenv("RB3_GAME_INPUT");
    if (!spec || !*spec)
        return;
    // Tokens are comma-separated "@<frame>:<action>".
    std::string s(spec);
    size_t pos = 0;
    while (pos < s.size()) {
        size_t comma = s.find(',', pos);
        std::string tok = s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        pos = (comma == std::string::npos) ? s.size() : comma + 1;
        // trim spaces
        size_t a = tok.find_first_not_of(" \t");
        if (a == std::string::npos) continue;
        size_t b = tok.find_last_not_of(" \t");
        tok = tok.substr(a, b - a + 1);
        if (tok.empty() || tok[0] != '@') continue;
        size_t colon = tok.find(':');
        if (colon == std::string::npos) continue;
        int frame = atoi(tok.substr(1, colon - 1).c_str());
        std::string action = tok.substr(colon + 1);
        // "select:<button>" directive — drive the real SELECT_MSG on a named
        // UIComponent in the current screen (bypasses milo d-pad nav).
        if (action.rfind("select:", 0) == 0) {
            ScriptedSelect ss = { frame, action.substr(7) };
            gSelectScript.push_back(ss);
            MILO_LOG("RB3 input: scheduled @%d -> select '%s'\n", frame, ss.button.c_str());
            continue;
        }
        // "msg:<object>:<action>[:arg]..." directive — send a message to a named
        // ObjectDir::Main() object (the real DTA-handler path).
        if (action.rfind("msg:", 0) == 0) {
            std::string rest = action.substr(4);
            // Split rest on ':' into [object, action, arg, arg, ...].
            std::vector<std::string> parts;
            size_t p = 0;
            while (p <= rest.size()) {
                size_t c = rest.find(':', p);
                if (c == std::string::npos) { parts.push_back(rest.substr(p)); break; }
                parts.push_back(rest.substr(p, c - p));
                p = c + 1;
            }
            if (parts.size() >= 2) {
                ScriptedMsg sm;
                sm.frame = frame;
                sm.object = parts[0];
                sm.action = parts[1];
                for (size_t k = 2; k < parts.size(); ++k)
                    sm.args.push_back(parts[k]);
                gMsgScript.push_back(sm);
                MILO_LOG("RB3 input: scheduled @%d -> msg {%s %s} (+%d args)\n", frame,
                         sm.object.c_str(), sm.action.c_str(), (int)sm.args.size());
            }
            continue;
        }
        JoypadButton btn;
        JoypadAction act = ActionFromName(action, btn);
        if (act == kAction_None) {
            MILO_LOG("RB3 input: unknown action '%s' in RB3_GAME_INPUT\n", action.c_str());
            continue;
        }
        ScriptedInput si = { frame, act, btn };
        gScript.push_back(si);
        MILO_LOG("RB3 input: scheduled @%d -> %s (action %d)\n", frame, action.c_str(), act);
    }
}

LocalUser *SynthUser() {
    if (gSynthUser)
        return gSynthUser;
    // The first local BandUser (BandUserMgr(4,3) builds 4 LocalBandUsers in its
    // ctor, App.cpp:214 BandUserMgrInit). Bind it to pad 0 so the whole pad/user
    // resolution path (UserMgr::GetLocalUserFromPadNum, OvershellPanel add-user,
    // ProfileMgr set_primary_profile_by_user) sees a real, pad-associated user.
    if (TheBandUserMgr) {
        std::vector<LocalBandUser *> &locals = TheBandUserMgr->GetLocalBandUsers();
        if (!locals.empty()) {
            gSynthUser = locals[0];
            AssociateUserAndPad(gSynthUser, 0);
            // Populate the shared joypad config tables (gControllersCfg /
            // gButtonMeanings). The real JoypadInit() lives in the Wii-only
            // Joypad_Wii.cpp (excluded on native) so its config-loading half —
            // JoypadInitCommon(SystemConfig("joypad")) — never ran; without it
            // JoypadControllerTypePadNum / ShellInputInterceptor::FilterAction
            // assert on a null gControllersCfg. This is the hardware-free part
            // of JoypadInit (no UsbWii / WPAD), so it is safe natively.
            JoypadInitCommon(SystemConfig("joypad"));
            // Headless has no physical Joypad_Native. The overshell add-user /
            // slot-join flow (which gates splash -> main_hub) requires the user
            // to report a connected controller (OvershellPanel::AddJoinUserEntry
            // -> ConnectedControllerType != kControllerNone). Mark pad 0
            // connected in the joypad table (only pad 0 — using the global
            // `fake_controllers` DataVariable would make the 3 *unassociated*
            // BandUsers report connected too, then crash in
            // DebugGetControllerTypeOverride(GetPadNum()=-1)). Pair it with a
            // debug controller-type override so ConnectedControllerType()
            // yields a real instrument (guitar) for pad 0.
            JoypadData *pad0 = JoypadGetPadData(0);
            if (pad0)
                pad0->mConnected = true;
            locals[0]->DebugSetControllerTypeOverride(kControllerGuitar);
            MILO_LOG("RB3 input: bound synthetic user %p to pad 0 "
                     "(connected, controller=guitar)\n", (void *)gSynthUser);
        }
    }
    return gSynthUser;
}

// === Native DTA-manager stubs (DTA_MANAGER_STUBS §4) =======================
// Two boot-path managers live in subsystems excluded from the native link:
//   - saveload_mgr (SaveLoadManager.cpp is _NATIVE_FORK_EXCLUDE'd — it is 2266
//     lines deeply tied to MemcardMgr_Wii/WiiProfileMgr; its is_idle gate would
//     never reach idle natively anyway: DTA_MANAGER_STUBS §4 verification #1
//     explicitly sanctions the NativeSaveLoadStub fallback, mirroring DC3's).
//   - net_cache_mgr (the Wii net/store cache subsystem is not on the link).
// Both answer the splash/boot DTAs with safe single-player/offline defaults so
// the splash state machine advances. Registered by name into ObjectDir::Main()
// after TheUI.Init() (DTA_MANAGER_STUBS §4 placement). registerStub no-ops if a
// real singleton already claimed the name.

class NativeSaveLoadStub : public Hmx::Object {
public:
    virtual DataNode Handle(DataArray *msg, bool warn) {
        Symbol s = msg->Sym(1);
        if (s == "activate")             return DataNode(0);
        if (s == "is_idle")              return DataNode(1);
        if (s == "is_initial_load_done") return DataNode(1);
        if (s == "is_autosave_enabled")  return DataNode(0);
        if (s == "autosave")             return DataNode(0);
        if (s == "enable_autosave" || s == "disable_autosave") return DataNode(0);
        return Hmx::Object::Handle(msg, warn);
    }
};

class NativeNetCacheMgrStub : public Hmx::Object {
public:
    virtual DataNode Handle(DataArray *msg, bool warn) {
        Symbol s = msg->Sym(1);
        if (s == "init")            return DataNode(0); // no-op offline
        if (s == "is_ready")        return DataNode(1);
        if (s == "is_done_loading") return DataNode(1);
        return Hmx::Object::Handle(msg, warn);
    }
};

} // namespace

void RB3RegisterNativeManagerStubs() {
    auto registerStub = [](const char *name, Hmx::Object *obj) {
        if (!ObjectDir::sMainDir ||
            !ObjectDir::sMainDir->FindObject(name, false)) {
            obj->SetName(name, ObjectDir::sMainDir);
            MILO_LOG("RB3 native: registered manager stub '%s'\n", name);
        } else {
            delete obj; // real singleton already claimed this name
        }
    };
    registerStub("saveload_mgr",  new NativeSaveLoadStub());
    registerStub("net_cache_mgr", new NativeNetCacheMgrStub());

    // Mark first-time calibration as already seen. The splash kSplashScreen_End
    // Overshell step does `{cond ({! {profile_mgr get_has_seen_first_time_
    // calibration}} {ui push_screen first_time_calibration}) {ui goto_screen
    // main_hub_screen}}` — with a profile-less native boot the flag defaults to
    // 0, detouring boot into the calibration screen (an interactive audio/video
    // A/V-sync flow we cannot complete headless). The real game sets this flag
    // when calibration finishes; do the same up front so boot goes straight to
    // main_hub. This is the real game mechanism (ProfileMgr::SetHasSeenFirst
    // TimeCalibration), not a DTA/splash edit.
    TheProfileMgr.SetHasSeenFirstTimeCalibration(true);

    // Populate TheSongMgr with the extracted song metadata. On console the songs
    // come from a disc/content scan via TheContentMgr; that content pipeline isn't
    // wired natively (WiiContentMgr is stubbed), so the song list would be empty
    // and song-select reports "no valid songs". Load songs/songs.dta directly
    // (the same file rb3-dta parses to 138 songs) and feed it through the real
    // BandSongMgr::AddSongs path so the song-select list + Game::LoadSong flow see
    // real songs. RB3_DATA roots the extract.
    {
        const char *dataRoot = getenv("RB3_DATA");
        if (dataRoot) {
            std::string songsPath = std::string(dataRoot) + "/songs/songs.dta";
            DataArray *songs = DataReadFile(songsPath.c_str(), true);
            if (songs) {
                TheSongMgr.AddSongs(songs);
                std::vector<int> ranked;
                TheSongMgr.GetRankedSongs(ranked, false, false);
                MILO_LOG("RB3 native: loaded songs.dta — TheSongMgr now has %d ranked songs\n",
                         (int)ranked.size());
                songs->Release();
            } else {
                MILO_LOG("RB3 native: could not load %s (song list will be empty)\n",
                         songsPath.c_str());
            }
        }
    }
}

// Called once per frame from App::RunWithoutDebugging's native frame loop, AFTER
// TheUI.Poll() (so a transition kicked off by a prior frame's input has advanced
// before the next input fires).
void RB3GameInputPoll(int frame) {
    if (!gScriptParsed)
        ParseScript();

    // Screen-flow trace: log every currentScreen change.
    UIScreen *cur = TheUI.CurrentScreen();
    Symbol curName = cur ? cur->Name() : Symbol();
    if (curName != gLastScreen) {
        MILO_LOG("RB3 screen: frame %d  currentScreen = '%s'%s\n",
                 frame, cur ? cur->Name() : "(none)",
                 TheUI.InTransition() ? "  (in transition)" : "");
        gLastScreen = curName;
    }

    // RB3_SCREEN_DBG=1: log transition state (and the transition screen) every N
    // frames so a stalled transition is visible. Dump per-panel load states of
    // the transition screen so a stuck panel is visible.
    if (getenv("RB3_SCREEN_DBG") && (frame % 40) == 0) {
        UIScreen *ts = TheUI.TransitionScreen();
        MILO_LOG("RB3 screen-dbg: frame %d  cur='%s' trans=%d transScreen='%s'\n",
                 frame, cur ? cur->Name() : "(none)", TheUI.InTransition(),
                 ts ? ts->Name() : "(none)");
        if (ts) {
            const std::vector<PanelRef> &refs = ts->GetPanelRefs();
            for (size_t r = 0; r < refs.size(); ++r) {
                UIPanel *p = refs[r].mPanel;
                MILO_LOG("    panel[%zu] '%s' active=%d loaded=%d state=%d isLoaded=%d\n",
                         r, p ? p->Name() : "(null)", refs[r].mActive, refs[r].mLoaded,
                         p ? (int)p->GetState() : -1, p ? (int)p->IsLoaded() : -1);
            }
        }
    }

    // Optional overshell-slot state dump (PART_DBG=1): the overshell panel's
    // override flow + all-slots-ready, queried via the real DTA handlers.
    if (getenv("PART_DBG") && ObjectDir::sMainDir && (frame % 40) == 0) {
        Hmx::Object *ov = ObjectDir::sMainDir->FindObject("overshell", false);
        if (ov) {
            Message qf(Symbol("in_override_flow"), DataNode(1)); // SongSettings=1
            DataNode r = ov->Handle(qf, false);
            Message qr(Symbol("all_slots_ready_to_play"));
            DataNode rr = ov->Handle(qr, false);
            MILO_LOG("PART_DBG: frame %d screen='%s' overshell inSongSettingsFlow=%d allReady=%d\n",
                     frame, cur ? cur->Name() : "(none)",
                     (r.Type() == kDataInt) ? r.Int() : -1,
                     (rr.Type() == kDataInt) ? rr.Int() : -1);
        }
    }

    // Optional splash/overshell state dump (RB3_INPUT_DEBUG=1).
    if (getenv("RB3_INPUT_DEBUG") && ObjectDir::sMainDir) {
        Hmx::Object *sp = ObjectDir::sMainDir->FindObject("splash_panel", false);
        Hmx::Object *ov = ObjectDir::sMainDir->FindObject("overshell", false);
        const DataNode *st = sp ? sp->Property(Symbol("splash_state"), false) : nullptr;
        int splashState = st ? st->Int() : -99;
        int allowing = -1;
        if (ov) {
            Message q(Symbol("is_any_slot_allowing_input_to_shell"));
            DataNode r = ov->Handle(q, false);
            allowing = (r.Type() == kDataInt) ? r.Int() : -2;
        }
        static int lastSplash = -100, lastAllow = -100;
        if (splashState != lastSplash || allowing != lastAllow) {
            MILO_LOG("RB3 dbg: frame %d  splash_state=%d  overshell_allowing=%d\n",
                     frame, splashState, allowing);
            lastSplash = splashState;
            lastAllow = allowing;
        }
    }

    // "msg:<object>:<action>" directives — send {action $synthUser} to a named
    // ObjectDir::Main() object, the real DTA-handler path (e.g. the song-select
    // {music_library select_highlighted_node $user} that a confirmed song row
    // ultimately runs -> SelectNode(kNodeSong) -> PlaySetlist -> Game::LoadSong).
    for (size_t i = 0; i < gMsgScript.size(); i++) {
        if (gMsgScript[i].frame != frame)
            continue;
        LocalUser *user = SynthUser();
        Hmx::Object *obj = ObjectDir::sMainDir
            ? ObjectDir::sMainDir->FindObject(gMsgScript[i].object.c_str(), true)
            : nullptr;
        if (obj) {
            const std::vector<std::string> &args = gMsgScript[i].args;
            if (args.empty()) {
                MILO_LOG("RB3 input: frame %d  msg {%s %s $user}\n", frame,
                         gMsgScript[i].object.c_str(), gMsgScript[i].action.c_str());
                Message m(Symbol(gMsgScript[i].action.c_str()),
                          DataNode((Hmx::Object *)user));
                obj->Handle(m, true);
            } else {
                // Build {action arg arg ...} with literal args. mData->Node(0) is
                // the message Symbol slot the handler macros skip; Node(1)=action,
                // Node(2..)=args (matching Message's Node layout).
                DataArray *da = new DataArray((int)args.size() + 2);
                da->Node(1) = Symbol(gMsgScript[i].action.c_str());
                std::string argdump;
                for (size_t k = 0; k < args.size(); ++k) {
                    const std::string &a = args[k];
                    if (a == "$user")
                        da->Node((int)k + 2) = DataNode((Hmx::Object *)user);
                    else if (!a.empty() &&
                             (isdigit((unsigned char)a[0]) ||
                              (a[0] == '-' && a.size() > 1)))
                        da->Node((int)k + 2) = DataNode(atoi(a.c_str()));
                    else
                        da->Node((int)k + 2) = DataNode(Symbol(a.c_str()));
                    argdump += " " + a;
                }
                MILO_LOG("RB3 input: frame %d  msg {%s %s%s}\n", frame,
                         gMsgScript[i].object.c_str(), gMsgScript[i].action.c_str(),
                         argdump.c_str());
                obj->Handle(da, true);
                da->Release();
            }
        } else {
            MILO_LOG("RB3 input: frame %d  msg target '%s' NOT FOUND\n", frame,
                     gMsgScript[i].object.c_str());
        }
    }

    // "select:<button>" directives — drive the real SELECT_MSG on a named
    // UIComponent in the current screen (the same UIComponentSelectMsg a focused
    // UIButton sends on Confirm). Finds the component anywhere in the current
    // screen's focus panel dir, sets focus to it, then SendSelect.
    for (size_t i = 0; i < gSelectScript.size(); i++) {
        if (gSelectScript[i].frame != frame)
            continue;
        LocalUser *user = SynthUser();
        UIComponent *comp = nullptr;
        UIPanel *ownerPanel = nullptr;
        if (cur) {
            UIPanel *fp = cur->FocusPanel();
            if (fp && fp->LoadedDir()) {
                comp = fp->LoadedDir()->Find<UIComponent>(gSelectScript[i].button.c_str(), false);
                if (comp)
                    ownerPanel = fp;
            }
            // Fall back: scan all panels of the current screen for the component.
            if (!comp) {
                const std::vector<PanelRef> &refs = cur->GetPanelRefs();
                for (size_t r = 0; !comp && r < refs.size(); ++r) {
                    UIPanel *p = refs[r].mPanel;
                    if (p && p->LoadedDir()) {
                        comp = p->LoadedDir()->Find<UIComponent>(
                            gSelectScript[i].button.c_str(), false);
                        if (comp)
                            ownerPanel = p;
                    }
                }
            }
        }
        if (comp) {
            MILO_LOG("RB3 input: frame %d  SELECT '%s' on screen '%s'\n",
                     frame, gSelectScript[i].button.c_str(),
                     cur ? cur->Name() : "(none)");
            // UIComponent::SendSelect only fires when the component is focused
            // (mState == kFocused). Focus it through its owning panel first (the
            // real `set_focus` path), then SendSelect (the real Confirm path).
            if (ownerPanel)
                ownerPanel->SetFocusComponent(comp);
            comp->SendSelect(user);
        } else {
            MILO_LOG("RB3 input: frame %d  SELECT '%s' NOT FOUND on screen '%s'\n",
                     frame, gSelectScript[i].button.c_str(),
                     cur ? cur->Name() : "(none)");
        }
    }

    if (gScript.empty())
        return;

    for (size_t i = 0; i < gScript.size(); i++) {
        if (gScript[i].frame != frame)
            continue;
        LocalUser *user = SynthUser();
        ButtonDownMsg msg(user, gScript[i].button, gScript[i].action, 0);
        const char *focusBtn = "(none)";
        if (cur && cur->FocusPanel() && cur->FocusPanel()->FocusComponent())
            focusBtn = cur->FocusPanel()->FocusComponent()->Name();
        MILO_LOG("RB3 input: frame %d  injecting action %d (button %d) on '%s' focus='%s'\n",
                 frame, gScript[i].action, gScript[i].button,
                 cur ? cur->Name() : "(none)", focusBtn);
        TheUI.Handle(msg, false);
    }
}
