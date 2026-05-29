// rb3_disc_label_classes.cpp — make the retail-disc milos' base-class label /
// inline-help objects instantiate as the App* game subclasses the proto-matched
// code expects, so their dynamic text (song name, player names, intro names,
// star rating, MOTD, ...) actually populates.
//
// THE PROBLEM
// -----------
// The decomp matches the SZBE69_B8 *proto* build, whose UI milos authored label
// objects as the game subclass `AppLabel` (and the review inline-help as
// `AppInlineHelp`). The retail-disc assets we actually run against (Xbox360 /
// Wii ARK) author the SAME `.lbl` / `.ihp` objects as the BASE engine classes
// `BandLabel` / `InlineHelp`.
//
//   class AppLabel      : public BandLabel  { ... }   // adds NO data members
//   class AppInlineHelp : public InlineHelp { LocalBandUser* mOverrideUser; }
//
// The dynamic-text messages (`set_user_name`, `set_song_name`, `set_intro_name`,
// `set_star_rating`, `set_profile_name`, `set_motd`, ...) are handled ONLY by
// AppLabel::Handle (BEGIN_HANDLERS(AppLabel) in band3/meta_band/AppLabel.cpp).
// At runtime the disc labels are base BandLabel, whose Handle table has only
// `count`/`finish_count` then falls through to UILabel → "unhandled msg" NOTIFY
// (BandLabel.cpp:354) → the label text is never set → blank on screen
// (endgame/results screen, overshell player widgets, song-select status, ...).
//
// THE FIX (faithful path)
// -----------------------
// Substitute the engine factory so a milo asking for a `BandLabel` actually
// constructs an `AppLabel` (and `InlineHelp` → `AppInlineHelp`). Because AppLabel
// adds NO data members, the object layout is identical and only the vtable +
// Handle table change — exactly the proto behaviour. This is more faithful and
// far less brittle than per-screen glue that re-derives each label's text by
// hand: every disc BandLabel everywhere gets the App* message handlers, and the
// matched NextSongPanel / Track / MetaPanel code that does
// `dynamic_cast<AppLabel*>(...)` now sees the real subclass.
//
// THE BOOT PITFALL (and why the config injection below is REQUIRED)
// ----------------------------------------------------------------
// A naive `RegisterFactory("BandLabel", AppLabel::NewObject)` breaks boot. The
// `objects` system config (config/band_keep.dta → objects.dta → ui_objects.dta)
// has per-class entries keyed by class name — `(BandLabel ...)`, `(InlineHelp
// ...)` — but NONE for the App* subclasses. Several load-path sites look that
// entry up by the object's LIVE ClassName() *fatally* (FindArray default
// fail=true), e.g.:
//   - obj/Utl.cpp InitObject():  objects->FindArray(obj->ClassName())     (fatal)
//   - ui/UIComponent.cpp:62:     SystemConfig("objects", ClassName())     (fatal)
//   - ui/UIComponent.cpp:325:    SystemConfig("objects", ClassName(),...) (fatal)
//   - obj/Utl.cpp:119:           SystemConfig(objects, from->ClassName()) (fatal)
// Once a label is an AppLabel, ClassName() returns "AppLabel", these lookups
// miss, and the engine aborts:
//   FAIL-MSG: Couldn't find 'AppInlineHelp' in array (file config/band_keep.dta...)
// (the HX_NATIVE OBJ_SET_TYPE macro is already non-failing, so it is NOT the
// abort source — these other ClassName()-keyed sites are.)
//
// So before remapping the factory we inject native-only supplemental `objects`
// config entries `(AppLabel ...)` / `(AppInlineHelp ...)` cloned verbatim from
// the base `(BandLabel ...)` / `(InlineHelp ...)` entries (same init / types /
// resources — correct, since the App* class IS the base class for config
// purposes). Then every fatal ClassName()-keyed lookup resolves and boot is
// unaffected. We edit only the in-memory parsed config (gSystemConfig), never
// on-disc data.
//
// WHERE THIS RUNS
// ---------------
// RB3UpgradeDiscLabelClasses() is called once from MetaPanel::Init() (App.cpp
// boot, after SystemInit loaded the config and after MetaInit registered the
// App* factories under their own names, and before TheUI.Init / any screen milo
// loads). Same point for native and web (both define HX_NATIVE). Idempotent.

#include "obj/Object.h"
#include "obj/Data.h"
#include "os/System.h"
#include "utl/Symbol.h"
#include "meta_band/AppLabel.h"
#include "meta_band/AppInlineHelp.h"
#include <cstdio>

// Clone the base-class `objects` config entry `(<baseName> ...)` into a sibling
// `(<appName> ...)` entry (identical children, only the tag symbol changed) and
// append it to the `objects` array, so the fatal ClassName()-keyed config
// lookups for the App* subclass resolve. No-op if the app entry already exists
// (idempotent) or the base entry is missing.
static void CloneObjectsEntry(DataArray *objects, const char *baseName,
                              const char *appName) {
    if (!objects)
        return;
    if (objects->FindArray(Symbol(appName), false))
        return; // already injected
    DataArray *base = objects->FindArray(Symbol(baseName), false);
    if (!base || base->Size() < 1)
        return; // base entry absent — nothing to clone

    // Build a new array with the same node count, copy every child node (the
    // DataNode copy ctor AddRefs nested arrays so they stay shared/alive), then
    // overwrite node 0 (the class-name tag) with the App* symbol.
    DataArray *clone = new DataArray(base->Size());
    for (int i = 0; i < base->Size(); i++)
        clone->Node(i) = base->Node(i);
    clone->Node(0) = DataNode(Symbol(appName));

    objects->Insert(objects->Size(), DataNode(clone, kDataArray));
    clone->Release(); // Insert copied (AddRef'd) the node; drop our ctor ref
    printf("rb3-native: injected objects config entry (%s) cloned from (%s)\n",
           appName, baseName);
}

// Public entry point — see file header. Safe to call more than once.
void RB3UpgradeDiscLabelClasses() {
    DataArray *objects = SystemConfig("objects");
    if (!objects) {
        printf("rb3-native: RB3UpgradeDiscLabelClasses — no 'objects' config; "
               "skipping label class upgrade\n");
        return;
    }

    // 1) Supplemental config so ClassName()-keyed fatal lookups resolve for the
    //    App* subclasses once instances start reporting those class names.
    CloneObjectsEntry(objects, "BandLabel", "AppLabel");
    CloneObjectsEntry(objects, "InlineHelp", "AppInlineHelp");

    // 2) Remap the factories: a milo asking for the base class now constructs the
    //    App* subclass (same layout, App* Handle table + dynamic-text handlers).
    //    The App* factories were already registered under their own names by
    //    MetaInit (MetaPanel.cpp) — this overwrites the BASE name's slot.
    Hmx::Object::RegisterFactory(BandLabel::StaticClassName(), AppLabel::NewObject);
    Hmx::Object::RegisterFactory(InlineHelp::StaticClassName(),
                                 AppInlineHelp::NewObject);
    printf("rb3-native: remapped milo factories BandLabel->AppLabel, "
           "InlineHelp->AppInlineHelp (disc base-class labels now handle "
           "set_* dynamic text)\n");
}
