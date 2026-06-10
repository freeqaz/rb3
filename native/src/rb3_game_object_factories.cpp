// rb3_game_object_factories.cpp — register the band3/bandobj/track/world milo
// object-class factories the native render + boot harness needs.
//
// WHY THIS EXISTS
// ---------------
// DirLoader::CreateObjects calls Hmx::Object::NewObject(className) for every
// serialized object in a .milo. If a class has no registered factory, NewObject
// returns NULL and the loader emits "Can't make <Class>". For a plain LEAF
// object that is recoverable: LoadObjs ReadDead-skips the NULL object's bytes to
// the next 0xADDEADDE marker. But a *Dir subclass* (OverdriveMeterDir,
// GemTrackDir, TrackPanelDir, ...) serializes a full nested directory — rev +
// classname + inlined-subdir blobs — whose extent ReadDead cannot skip
// correctly (its inner objects each have their own dead markers, so ReadDead
// stops at the first inner marker and leaves the rest of the dir's bytes in the
// stream). That desyncs the parent stream and the next object's PreLoad reads a
// string length as a std::vector<Viewport> count -> runaway resize -> SIGSEGV
// (native) / wasm-heap OOM (web).
//
// The core engine harness (BandRnd::PreInitRender + RegisterCommonFactories)
// registers only the rndobj base classes (RndDir/RndMesh/RndTex/...). The
// band3/bandobj Dir subclasses live in matched-fork TUs that ARE compiled into
// rb3-native / rb3-web (ENGINE_BANDOBJ / ENGINE_TRACK / ENGINE_WORLD /
// GAME_* globs) but whose factory registration only runs from the macro-gated
// BandInit()/GameInit() clusters that the synthetic render harness never calls.
//
// This file registers them directly (each class's static Init()/Register(),
// which is just Hmx::Object::RegisterFactory(StaticClassName(), NewObject) — no
// singleton/GPU dependency), so the loader can construct the real class and its
// matched PreLoad/PostLoad consume the correct byte extent. This is the correct
// fix layer for the native port: it is additive (only registers factories),
// touches NO matched decomp read logic, and is identical in intent to what the
// real game boot does via App.cpp's BandInit()/TrackInit()/WorldInit() calls.
//
// NOT AN ENDIANNESS BUG (multi-chunk decode): the .milo_xbox chunk header is
// little-endian on disk and the milo *body* is big-endian (swapped correctly by
// BinStream::ReadEndian when mLittleEndian=false). Both single- and multi-chunk
// milos decode the same way; single-chunk happened to work only because
// gem_smasher_guitar.milo_xbox's object classes were all already registered.
//
// (Endian footnote, corrected per milo-trace W9B: EndianSwapEq<int> used to be a
// native no-op stub. That was correct-by-luck for the LE .milo_xbox header but
// made the <int> primitive behave OPPOSITELY to its byteswapping <unsigned int>
// sibling — a latent hazard for any genuine BE int field. The primitive is now a
// REAL byteswap on native (dta_link_stubs.s, matching the DOL + the sibling), and
// the host-LE correctness of the LE ChunkStream header is preserved by an
// HX_NATIVE guard on the now-real swap in ChunkStream::Eof()/~ChunkStream — the
// same host-aware split BinStream::ReadEndian already uses.)

#include "bandobj/OverdriveMeter.h"
#include "bandobj/GemTrackDir.h"
#include "bandobj/TrackPanelDir.h"
#include "bandobj/VocalTrackDir.h"
#include "bandobj/StreakMeter.h"
#include "bandobj/MeterDisplay.h"
#include "bandobj/StarDisplay.h"
#include "bandobj/ScoreDisplay.h"
#include "bandobj/CrowdMeterIcon.h"
#include "bandobj/EndingBonus.h"
#include "bandobj/PitchArrow.h"
#include "bandobj/PlayerDiffIcon.h"
#include "bandobj/InstrumentDifficultyDisplay.h"
#include "bandobj/ReviewDisplay.h"
#include "bandobj/MiniLeaderboardDisplay.h"
#include "bandobj/MicInputArrow.h"
#include "bandobj/InlineHelp.h"
#include "bandobj/ChordShapeGenerator.h"
#include "bandobj/UnisonIcon.h"
#include "bandobj/LayerDir.h"
#include "bandobj/PatchRenderer.h"
#include "bandobj/ScrollbarDisplay.h"
#include "bandobj/CheckboxDisplay.h"

// world / track milo classes (venues, light presets, track widgets).
#include "world/Dir.h"
#include "track/TrackDir.h"
#include "track/TrackWidget.h"

// rndobj leaf milo classes the bandobj Dir subclasses reference by name in their
// SyncObjects()/PostLoad (".trig"/".part"/".lbl"/anims). If these are not
// registered the named children don't exist and a Dir's SyncObjects MILO_FAILs
// ("Could not find <child> in dir ..."). Registering them lets the full tree
// load + sync. (These are the rndobj-side analog of the core factory block in
// main_native.cpp's RegisterCommonFactories, which the synthetic render harness
// does not call.)
#include "rndobj/EventTrigger.h"
#include "rndobj/PropAnim.h"
#include "rndobj/TransAnim.h"
#include "rndobj/MatAnim.h"
#include "rndobj/MeshAnim.h"
#include "rndobj/AnimFilter.h"
#include "rndobj/Part.h"
#include "rndobj/Flare.h"
#include "rndobj/Text.h"
#include "bandobj/BandLabel.h"
#include "bandobj/BandButton.h"

// ui/ container + widget milo classes. PanelDir/UIScreen/UIPanel are Dir
// containers (same desync hazard as the bandobj Dirs); the rest are leaf
// widgets the UI dirs reference by name. main_hub.milo_xbox embeds these.
#include "ui/UIScreen.h"
#include "ui/UIPanel.h"
#include "ui/PanelDir.h"
#include "ui/UIComponent.h"
#include "ui/UIButton.h"
#include "ui/UIColor.h"
#include "ui/UILabel.h"
#include "ui/UIList.h"
#include "ui/UIPicture.h"
#include "ui/UIProxy.h"
#include "ui/UISlider.h"
#include "ui/UITrigger.h"
#include "ui/UIGuide.h"
#include "ui/UILabelDir.h"
#include "rndobj/Font.h"

#include "os/Debug.h"

// Registers the band3/bandobj/track/world milo object-class factories. Wrapped
// in try-free direct Register() calls; each is idempotent (RegisterFactory just
// overwrites the map slot). Call AFTER BandRnd::PreInitRender (which registers
// the rndobj base classes these Dir subclasses ultimately derive from) and
// AFTER SystemInit (the config boot).
void RB3RegisterGameObjectFactories() {
    // bandobj Dir subclasses + HUD/meter milo objects (the BandInit() list,
    // minus the band-character / wardrobe / camera classes not used by the
    // track/ui milos and gated behind heavier deps).
    OverdriveMeter::Init();
    GemTrackDir::Init();
    TrackPanelDir::Init();
    VocalTrackDir::Init();
    StreakMeter::Init();
    MeterDisplay::Init();
    StarDisplay::Init();
    ScoreDisplay::Init();
    CrowdMeterIcon::Init();
    EndingBonus::Init();
    PitchArrow::Init();
    PlayerDiffIcon::Init();
    InstrumentDifficultyDisplay::Init();
    ReviewDisplay::Init();
    MiniLeaderboardDisplay::Init();
    MicInputArrow::Init();
    InlineHelp::Init();
    ChordShapeGenerator::Init();
    UnisonIcon::Init();
    LayerDir::Init();
    PatchRenderer::Init();
    ScrollbarDisplay::Init();
    CheckboxDisplay::Init();

    // track + world Dir classes. Use the pure factory Register() (not the
    // heavier Init() cluster) — we only need NewObject(className) to resolve.
    TrackDir::Register();
    TrackWidget::Register();
    WorldDir::Register();

    // rndobj leaf milo classes referenced by the bandobj Dirs' SyncObjects.
    EventTrigger::Init();
    RndPropAnim::Init();
    RndTransAnim::Init();
    RndMatAnim::Init();
    RndMeshAnim::Init();
    RndAnimFilter::Init();
    RndParticleSys::Init();
    RndFlare::Init();
    BandLabel::Register();

    // Register RndText under its CANONICAL OBJ_CLASSNAME "RndText" (the harness's
    // RB3RegisterLegacyRndAliases only registers the short "Text" on-disc alias).
    // ChordShapeGenerator/ArpeggioShape ctors call Hmx::Object::New<RndText>()
    // which resolves NewObject(Symbol("RndText")) — without this it MILO_FAILs
    // "Unknown class RndText".
    RndText::Register();

    // ui/ container + widget classes. PanelDir/UIScreen/UIPanel are Dir
    // containers whose unregistered absence desyncs the same way the bandobj
    // Dirs did (e.g. main_hub.milo_xbox -> "String chars <garbage> > 256");
    // the widgets are their named children. All Init()s here are pure
    // REGISTER_OBJ_FACTORY (no UIManager-singleton setup — we deliberately do
    // NOT call UIManager::Init(), which builds the UI cam/env/automator).
    UIScreen::Init();
    UIPanel::Init();
    PanelDir::Init();
    UIComponent::Register();
    UIButton::Register();
    UIColor::Init();
    UILabel::Register();
    UIList::Register();
    UIPicture::Init();
    UIProxy::Init();
    UISlider::Register();
    UITrigger::Init();
    UIGuide::Init();
    BandButton::Register();
    // NOTE: LabelShrinkWrapper is deliberately NOT registered. It is a leaf
    // UIComponent (not a Dir container, so leaving it unregistered does NOT
    // desync — it is ReadDead-skipped). Its PostLoad/Update derefs
    // mResource->Dir() which is null without the UI resource-manager subsystem
    // the synthetic render harness doesn't boot, so registering it would SIGSEGV
    // in LabelShrinkWrapper::Update. Same rule applies to any leaf widget whose
    // PostLoad needs runtime subsystem state: register Dir CONTAINERS (to avoid
    // the nested-dir desync) but skip leaves that need state we don't set up.

    // Font label-dir + font resource. A UILabel's LabelUpdate does
    // dynamic_cast<UILabelDir*>(ResourceDir()) and MILO_ASSERTs the result
    // (UILabel.cpp:977). Font .milo dirs are class UILabelDir; without the
    // factory they default to RndDir and the cast yields null -> assert. RndFont
    // (OBJ_CLASSNAME "Font") is the font glyph resource those dirs contain.
    UILabelDir::Init();
    RndFont::Init();
}
