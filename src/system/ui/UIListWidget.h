#pragma once
#include "obj/Object.h"
#include "ui/UIColor.h"
#include "math/Vec.h"
#include "ui/UIComponent.h"
#include "ui/UIListState.h"
#include "ui/UIListElementDrawState.h"
#include "ui/UIEnums.h"
#include "obj/ObjPtr_p.h"
#include "utl/MemMgr.h"
#include <vector>

class UIList;

#ifdef HX_NATIVE
// W4.4-ROWFIX (RB3_ROWFIX, default-OFF): the song_select focused-row selection
// bar. On the target the focused list row is a solid bright bar (yellow/white)
// with DARK text; natively the row shows only the yellow FRAME (highlight_main,
// zmode=0 so NOT depth-occluded) over a dark navy fill and WHITE text. Root
// cause (W4.4 diagnosis): the solid FILL is produced by the list highlight's
// focus animation/trigger (list_song_select_browser.milo highlight_bar.grp /
// highlight_light.trig / highlight_yellow.mesh + highlight_bar_color.tex) which
// does not run natively, so the full-row fill quad (ml_highlight_glasstopp) is
// drawn only as its near-invisible additive sheen (white, alpha 0.08). This
// flag approximates the focus bar in two coupled parts, scoped to the list that
// actually draws the ml_highlight fill quad: (A) render the glasstopp full-row
// quad as a solid fill in the highlight's authored color; (B) darken the
// highlighted-row text so it reads on the bright bar. Default-OFF -> the whole
// path is a no-op and flag-OFF is byte-identical (Wii build never compiles this
// under HX_NATIVE either). NOT a depth fix: the menu-flush depth LoadOp::Load
// (SETLISTS red-band fix) is untouched.
bool RB3RowfixActive();
void RB3RowfixResetFill();     // UIList::DrawShowing, per-list, before widgets
void RB3RowfixSetFillDrawn();  // Part A, when the ml_highlight fill quad drew
bool RB3RowfixFillDrawn();     // Part B, scopes the text-darken to that list
#endif

class UIListWidgetDrawState {
public:
    UIListWidgetDrawState() {}
    ~UIListWidgetDrawState() {}
    Vector3 mFirstPos; // 0x0
    Vector3 mLastPos; // 0xc
    Vector3 mHighlightPos; // 0x18
    int mHighlightDisplay; // 0x24
    UIListWidgetState mHighlightElementState; // 0x28
    std::vector<UIListElementDrawState> mElements; // 0x2c
};

/** "Base functionality for UIList widgets" */
class UIListWidget : public Hmx::Object {
public:
    UIListWidget();
    virtual ~UIListWidget() {}
    OBJ_CLASSNAME(UIListWidget);
    OBJ_SET_TYPE(UIListWidget);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual UIList *SubList(int) { return 0; }
    virtual void ResourceCopy(const UIListWidget *);
    virtual void CreateElements(UIList *, int) {}
    virtual void Draw(
        const UIListWidgetDrawState &,
        const UIListState &,
        const Transform &,
        UIComponent::State,
        Box *,
        DrawCommand
    ) {}
    virtual void Fill(const class UIListProvider &, int, int, int) {}
    virtual void StartScroll(int, bool) {}
    virtual void CompleteScroll(const UIListState &, int) {}
    virtual void Poll() {}

    float DrawOrder() const;
    float DisabledAlphaScale() const;
    UIListWidgetDrawType WidgetDrawType() const;
    UIList *ParentList();
    void
    DrawMesh(RndMesh *, UIListWidgetState, UIComponent::State, const Transform &, Box *);
    UIColor *DisplayColor(UIListWidgetState, UIComponent::State) const;
    void SetColor(UIListWidgetState, UIComponent::State, UIColor *);
    void SetParentList(UIList *);
    void CalcXfm(const Transform &, const Vector3 &, Transform &);

    DECLARE_REVS
    NEW_OVERLOAD
    DELETE_OVERLOAD
    NEW_OBJ(UIListWidget)
    static void Init() { REGISTER_OBJ_FACTORY(UIListWidget) }

    float mDrawOrder; // 0x1c
    float mDisabledAlphaScale; // 0x20
    ObjPtr<UIColor> mDefaultColor; // 0x24
    std::vector<std::vector<ObjPtr<UIColor> > > mColors; // 0x30 - a vector of vectors of
                                                         // ObjPtrs...wonderful
    UIListWidgetDrawType mWidgetDrawType; // 0x38
    UIList *mParentList; // 0x3c
};
