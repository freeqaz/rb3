// ===========================================================================
// rb3_uidump.cpp — W17 R3-UIDUMP authored UI scene-graph dump (GET /api/uidump)
//
// The standing UI-forensics endpoint. Walks TheUI's screens -> active panels ->
// loaded PanelDirs (recursively, incl. nested dirs + mSubDirs) and emits every
// authored UI object's name/class/show-state/draw-order/world-xfm/sphere plus,
// per class, its material color+blend+zmode (RndMesh) or text + main/alt font
// materials with LIVE colors + focus state (UILabel/RndText). Each object is
// then JOINED against the just-drawn frame's drawlog + RB3_DRAWLOG_PROV sidecar
// (RB3DebugGetDrawProv, index-aligned with RB3DebugGetDrawLog): named meshes by
// prov.meshName, labels/text by prov.scopeOwner (the game-fed scope stack pushed
// in RndText/UILabel/PanelDir::DrawShowing). Per object -> {count, rects[],
// lastDrawIdx, passIdx} or "draws":0 (authored-but-not-drawn, e.g. a hidden
// highlight quad). When RB3_DRAWLOG_PROV is unset the authored half still returns
// (draws=null + joinDisabled note).
//
// This TU includes decomp headers (ui/, rndobj/, math/) and NO httplib, so it
// rides the target-wide MWCC compat flags exactly like rb3_http_handlers.cpp
// (see native/CMakeLists.txt). Runs on the main thread between frames via
// kCmdUIDump (same safety envelope as kCmdDtaEval). #ifdef HX_NATIVE by
// construction (native-only debug endpoint; the Wii build never compiles it).
// ===========================================================================
#include "rb3_http_server.h"

#include "obj/Object.h"
#include "obj/Dir.h"                    // ObjectDir, ObjDirItr, mSubDirs
#include "ui/UI.h"                      // UIManager TheUI, CurrentScreen, mPushedScreens
#include "ui/UIScreen.h"                // PanelRef, GetPanelRefs
#include "ui/UIPanel.h"                 // LoadedDir, Showing
#include "ui/PanelDir.h"                // PanelDir : RndDir
#include "ui/UILabel.h"                 // text + main/alt fonts + state
#include "ui/UIComponent.h"             // UIComponent::State
#include "rndobj/Dir.h"                 // RndDir::mDraws
#include "rndobj/Draw.h"                // RndDrawable Showing/GetSphere
#include "rndobj/Trans.h"               // RndTransformable WorldXfm
#include "rndobj/Mesh.h"               // RndMesh Mat()
#include "rndobj/Mat.h"                 // RndMat color/blend/zmode
#include "rndobj/Text.h"                // RndText TextASCII/GetFont
#include "rndobj/Font.h"                // RndFont GetMat
#include "math/Mtx.h"                   // Transform

#include "platform/RB3DrawLogDebug.h"   // RB3DebugGetDrawLog/Prov, RB3DrawProvEnabled
#include "platform/Rnd_Wgpu_RB3.h"      // gBandRnd.mFrameCount

#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {

// Minimal JSON string escape for milo object names (quotes/backslashes/control).
std::string JEsc(const char* s) {
    std::string o;
    if (!s) return o;
    for (; *s; ++s) {
        char c = *s;
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\t': o += "\\t";  break;
            case '\r': o += "\\r";  break;
            default:
                if ((unsigned char)c < 0x20) { char t[8]; snprintf(t, sizeof(t), "\\u%04x", c); o += t; }
                else o += c;
        }
    }
    return o;
}

const char* StateName(int s) {
    switch (s) {
        case 0: return "kNormal";
        case 1: return "kFocused";
        case 2: return "kDisabled";
        case 3: return "kSelecting";
        case 4: return "kSelected";
        default: return "?";
    }
}

// Join tables built from the prov sidecar (empty unless RB3_DRAWLOG_PROV).
struct JoinTables {
    bool provAvail = false;
    std::multimap<std::string, int> byMesh;   // prov.meshName  -> draw index
    std::multimap<std::string, int> byOwner;  // prov.scopeOwner-> draw index
};

// Append the "draws" object for one authored key, looking up matching draw
// indices in `table`. Emits {count, rects[[x,y,w,h]...], lastDrawIdx, passIdx}.
// Non-degenerate rects only (rectKind 2 / negative w are skipped in the rect
// list but still counted). Returns nothing; writes into `json`.
void EmitDraws(std::string& json, const JoinTables& jt,
               const std::multimap<std::string, int>& table, const std::string& key,
               const std::vector<RB3DrawProv>& prov) {
    if (!jt.provAvail) { json += "\"draws\": null"; return; }
    auto range = table.equal_range(key);
    int count = 0, lastIdx = -1, lastPass = -1;
    // Rects can be numerous (a shared mesh name draws many times); append the rect
    // list directly to `json` rather than through a fixed buffer (that truncation
    // was the M2 malformed-JSON bug). Cap the rect list to keep the dump bounded;
    // count/lastDrawIdx/passIdx remain exact regardless of the cap.
    const int kMaxRects = 64;
    std::string rects;
    int rectsShown = 0;
    for (auto it = range.first; it != range.second; ++it) {
        int i = it->second;
        const RB3DrawProv& p = prov[i];
        count++;
        lastIdx = i;
        lastPass = p.passIdx;
        if (p.rectKind != 2 && p.rect[2] >= 0 && rectsShown < kMaxRects) {
            char rb[96];
            snprintf(rb, sizeof(rb), "%s[%.1f,%.1f,%.1f,%.1f]",
                     rects.empty() ? "" : ",", p.rect[0], p.rect[1], p.rect[2], p.rect[3]);
            rects += rb;
            rectsShown++;
        }
    }
    char buf[96];
    snprintf(buf, sizeof(buf), "\"draws\": { \"count\": %d, \"rects\": [", count);
    json += buf;
    json += rects;
    snprintf(buf, sizeof(buf), "], \"lastDrawIdx\": %d, \"passIdx\": %d }", lastIdx, lastPass);
    json += buf;
}

// Emit a single authored object. `drawOrder` = index in the owning dir's mDraws
// (-1 if not a registered draw).
void EmitObject(std::string& json, Hmx::Object* o, int drawOrder, bool leadComma,
                const JoinTables& jt, const std::vector<RB3DrawProv>& prov) {
    const char* name = o->Name();
    const char* cls  = o->ClassName().Str();
    RndDrawable* drw = dynamic_cast<RndDrawable*>(o);
    RndTransformable* trn = dynamic_cast<RndTransformable*>(o);
    RndMesh* mesh = dynamic_cast<RndMesh*>(o);
    UILabel* lbl  = dynamic_cast<UILabel*>(o);

    char buf[512];
    snprintf(buf, sizeof(buf),
             "%s\n        { \"name\": \"%s\", \"class\": \"%s\", \"showing\": %s, \"drawOrder\": %d",
             leadComma ? "," : "", JEsc(name).c_str(), JEsc(cls).c_str(),
             (drw && drw->Showing()) ? "true" : "false", drawOrder);
    json += buf;

    if (trn) {
        Transform& wx = trn->WorldXfm();
        snprintf(buf, sizeof(buf),
                 ",\n          \"world\": [%.4g,%.4g,%.4g, %.4g,%.4g,%.4g, %.4g,%.4g,%.4g, %.4g,%.4g,%.4g]",
                 wx.m.x.x, wx.m.x.y, wx.m.x.z, wx.m.y.x, wx.m.y.y, wx.m.y.z,
                 wx.m.z.x, wx.m.z.y, wx.m.z.z, wx.v.x, wx.v.y, wx.v.z);
        json += buf;
    }
    if (drw) {
        const Sphere& s = drw->GetSphere();
        snprintf(buf, sizeof(buf),
                 ",\n          \"sphere\": { \"c\": [%.3f,%.3f,%.3f], \"r\": %.3f }",
                 s.center.x, s.center.y, s.center.z, s.radius);
        json += buf;
    }
    if (mesh) {
        RndMat* m = mesh->Mat();
        if (m) {
            const Hmx::Color& c = m->GetColor();
            snprintf(buf, sizeof(buf),
                     ",\n          \"mat\": { \"name\": \"%s\", \"color\": [%.4g,%.4g,%.4g,%.4g], "
                     "\"blend\": %d, \"zmode\": %d }",
                     JEsc(m->Name()).c_str(), c.red, c.green, c.blue, c.alpha,
                     (int)m->GetBlend(), (int)m->GetZMode());
            json += buf;
        } else {
            json += ",\n          \"mat\": null";
        }
    }
    if (lbl) {
        RndText* txt = lbl->TextObj();
        String textStr = txt ? txt->TextASCII() : String();
        RndMat* mainMat = (txt && txt->GetFont()) ? txt->GetFont()->GetMat() : nullptr;
        RndFont* altF = lbl->AltFont();
        RndMat* altMat = altF ? altF->GetMat() : nullptr;
        json += ",\n          \"label\": { \"text\": \"";
        json += JEsc(textStr.c_str());
        snprintf(buf, sizeof(buf), "\", \"state\": \"%s\"", StateName((int)lbl->GetState()));
        json += buf;
        if (mainMat) {
            const Hmx::Color& c = mainMat->GetColor();
            snprintf(buf, sizeof(buf),
                     ", \"fontMat\": \"%s\", \"fontMatColor\": [%.4g,%.4g,%.4g,%.4g]",
                     JEsc(mainMat->Name()).c_str(), c.red, c.green, c.blue, c.alpha);
            json += buf;
        }
        if (altMat) {
            const Hmx::Color& c = altMat->GetColor();
            snprintf(buf, sizeof(buf),
                     ", \"altFontMat\": \"%s\", \"altFontMatColor\": [%.4g,%.4g,%.4g,%.4g]",
                     JEsc(altMat->Name()).c_str(), c.red, c.green, c.blue, c.alpha);
            json += buf;
        }
        json += " }";
    }

    // Join: labels/text by scopeOwner (glyph meshes are unnamed), meshes by name.
    json += ",\n          ";
    if (lbl) {
        EmitDraws(json, jt, jt.byOwner, name ? name : "", prov);
    } else if (mesh) {
        EmitDraws(json, jt, jt.byMesh, name ? name : "", prov);
    } else {
        // Non-mesh, non-label drawable: try owner scope by name, else meshes.
        if (dynamic_cast<RndText*>(o))
            EmitDraws(json, jt, jt.byOwner, name ? name : "", prov);
        else
            EmitDraws(json, jt, jt.byMesh, name ? name : "", prov);
    }
    json += " }";
}

// Recursively walk one dir, emitting its authored objects. `visited` dedups dirs
// reachable from multiple roots. `first` tracks lead-comma across the whole panel.
void WalkDir(std::string& json, ObjectDir* dir, std::set<ObjectDir*>& visited,
             bool& first, const JoinTables& jt, const std::vector<RB3DrawProv>& prov) {
    if (!dir || visited.count(dir)) return;
    visited.insert(dir);

    // Build draw-order index for objects directly registered in this dir's draw
    // list (RndDir::mDraws). Objects not in the list report drawOrder -1.
    std::map<RndDrawable*, int> order;
    if (RndDir* rd = dynamic_cast<RndDir*>(dir)) {
        for (size_t i = 0; i < rd->mDraws.size(); ++i)
            if (rd->mDraws[i]) order[rd->mDraws[i]] = (int)i;
    }

    for (ObjDirItr<Hmx::Object> it(dir, false); it; ++it) {
        Hmx::Object* o = it;
        if (!o) continue;
        int drawOrder = -1;
        if (RndDrawable* d = dynamic_cast<RndDrawable*>(o)) {
            auto f = order.find(d);
            if (f != order.end()) drawOrder = f->second;
        }
        EmitObject(json, o, drawOrder, !first, jt, prov);
        first = false;

        // A nested ObjectDir object (e.g. song_select_details) holds its content
        // inline, not in mSubDirs — recurse in (census pattern).
        ObjectDir* nested = dynamic_cast<ObjectDir*>(o);
        if (nested && nested != dir)
            WalkDir(json, nested, visited, first, jt, prov);
    }
    // Inlined subdirs.
    for (size_t i = 0; i < dir->mSubDirs.size(); ++i)
        WalkDir(json, dir->mSubDirs[i], visited, first, jt, prov);
}

} // namespace

void RB3HttpServer::HandleUIDump(Command& cmd) {
    const std::string& panelFilter = cmd.param1;      // optional name substring
    bool wantJoin = (cmd.param2 != "0");

    const std::vector<RB3DrawRecord>& log  = RB3DebugGetDrawLog();
    const std::vector<RB3DrawProv>&   prov = RB3DebugGetDrawProv();

    JoinTables jt;
    jt.provAvail = wantJoin && !prov.empty() && prov.size() == log.size();
    if (jt.provAvail) {
        for (size_t i = 0; i < prov.size(); ++i) {
            if (!prov[i].meshName.empty())   jt.byMesh.insert({prov[i].meshName, (int)i});
            if (!prov[i].scopeOwner.empty()) jt.byOwner.insert({prov[i].scopeOwner, (int)i});
        }
    }

    std::string json;
    json.reserve(64 * 1024);
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{ \"ok\": true, \"data\": { \"frame\": %d, \"coverage\": \"BandRnd::DrawMesh only\", "
             "\"joinEnabled\": %s%s,\n  \"screens\": [",
             gBandRnd.mFrameCount, jt.provAvail ? "true" : "false",
             (wantJoin && !jt.provAvail) ? ", \"joinDisabled\": \"RB3_DRAWLOG_PROV unset (or size mismatch)\"" : "");
    json += buf;

    // Collect screens: pushed stack (bottom->top) + current, dedup, mark current.
    UIScreen* cur = TheUI.CurrentScreen();
    std::vector<UIScreen*> screens = TheUI.mPushedScreens;
    if (cur && std::find(screens.begin(), screens.end(), cur) == screens.end())
        screens.push_back(cur);

    bool firstScreen = true;
    for (size_t si = 0; si < screens.size(); ++si) {
        UIScreen* scr = screens[si];
        if (!scr) continue;
        snprintf(buf, sizeof(buf), "%s\n    { \"name\": \"%s\", \"current\": %s, \"panels\": [",
                 firstScreen ? "" : ",", JEsc(scr->Name()).c_str(),
                 (scr == cur) ? "true" : "false");
        json += buf;
        firstScreen = false;

        const std::vector<PanelRef>& refs = scr->GetPanelRefs();
        bool firstPanel = true;
        for (size_t pi = 0; pi < refs.size(); ++pi) {
            UIPanel* panel = refs[pi].mPanel;
            if (!panel) continue;
            PanelDir* pdir = panel->LoadedDir();
            const char* pname = panel->Name();
            if (!panelFilter.empty() && (!pname || !strstr(pname, panelFilter.c_str())))
                continue;
            snprintf(buf, sizeof(buf),
                     "%s\n      { \"name\": \"%s\", \"dir\": \"%s\", \"showing\": %s, \"active\": %s, "
                     "\"objects\": [",
                     firstPanel ? "" : ",", JEsc(pname).c_str(),
                     pdir ? JEsc(pdir->Name()).c_str() : "",
                     panel->Showing() ? "true" : "false",
                     refs[pi].Active() ? "true" : "false");
            json += buf;
            firstPanel = false;

            if (pdir) {
                std::set<ObjectDir*> visited;
                bool firstObj = true;
                WalkDir(json, pdir, visited, firstObj, jt, prov);
            }
            json += " ] }";   // close objects[] + panel
        }
        json += " ] }";       // close panels[] + screen
    }
    json += " ] } }\n";       // close screens[] + data + root

    cmd.result.ok = true;
    cmd.result.jsonData = std::move(json);
}
