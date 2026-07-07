#include "ui/UIListSlot.h"
#include "ui/UIList.h"
#include "ui/UIColor.h"
#include "utl/Std.h"
#include "utl/Symbols.h"

INIT_REVS(UIListSlot)

UIListSlot::UIListSlot() : mSlotDrawType(kUIListSlotDrawAlways), mNextElement(0) {}

UIListSlot::~UIListSlot() { ClearElements(); }

void UIListSlot::ClearElements() {
    std::vector<UIListSlotElement *>::iterator it = mElements.begin();
    std::vector<UIListSlotElement *>::iterator itEnd = mElements.end();
    for (; it != itEnd; it++) {
        delete *it;
    }
    mElements.clear();
    delete mNextElement;
    mNextElement = 0;
}

bool UIListSlot::Matches(const char *cc) const {
    return strcmp(mMatchName.c_str(), cc) == 0;
}

const char *UIListSlot::MatchName() const { return mMatchName.c_str(); }

void UIListSlot::CreateElements(UIList *uilist, int count) {
    if (RootTrans()) {
        ClearElements();
        for (int i = 0; i < count; i++) {
            mElements.push_back(CreateElement(uilist));
        }
        mNextElement = CreateElement(uilist);
    }
}

// fn_8056EA14 - draw
void UIListSlot::Draw(
    const UIListWidgetDrawState &drawstate,
    const UIListState &liststate,
    const Transform &ctf,
    UIComponent::State compstate,
    Box *box,
    DrawCommand cmd
) {
    RndTransformable *root = RootTrans();
    if (root) {
        int thesize = drawstate.mElements.size();
        if (thesize > mElements.size()) {
            MILO_FAIL("%i isn't enough elements (need %i)", mElements.size(), thesize);
        }
        Transform tf78(root->WorldXfm());
        Transform tfa8;
        UIListProvider *prov = liststate.Provider();
        float mxz = tf78.m.x.z;
        float myz = tf78.m.y.z;
        float mzz = tf78.m.z.z;
        float mvz = tf78.v.z;
        for (int i = 0; i < thesize; i++) {
            const UIListElementDrawState &curdrawstate = drawstate.mElements[i];
            if (curdrawstate.unk0) {
                float d10 = 1.0f;
                UIColor *uicolor = 0;
                if (!box) {
                    if (mSlotDrawType == kUIListSlotDrawHighlight
                            && curdrawstate.mDisplay != drawstate.mHighlightDisplay
                        || mSlotDrawType == kUIListSlotDrawNoHighlight
                            && curdrawstate.mDisplay == drawstate.mHighlightDisplay) {
                        continue;
                    }

                    UIListWidgetState slotoverride = prov->SlotElementStateOverride(
                        curdrawstate.mShowing,
                        curdrawstate.mData,
                        this,
                        curdrawstate.mElementState
                    );
                    UIComponent::State curcompstate = curdrawstate.mComponentState;
                    uicolor = DisplayColor(slotoverride, curcompstate);
                    uicolor = prov->SlotColorOverride(
                        curdrawstate.mShowing, curdrawstate.mData, this, uicolor
                    );
                    d10 = curdrawstate.mAlpha;
                    if (curcompstate == UIComponent::kDisabled)
                        d10 *= DisabledAlphaScale();
                    prov->PreDraw(curdrawstate.mShowing, curdrawstate.mData, this);
                }
                *(__vec2x32float__ *)&tfa8.m.x.x = *(__vec2x32float__ *)&tf78.m.x.x;
                tfa8.m.x.z = mxz;
                *(__vec2x32float__ *)&tfa8.m.y.x = *(__vec2x32float__ *)&tf78.m.y.x;
                tfa8.m.y.z = myz;
                *(__vec2x32float__ *)&tfa8.m.z.x = *(__vec2x32float__ *)&tf78.m.z.x;
                tfa8.m.z.z = mzz;
                *(__vec2x32float__ *)&tfa8.v.x = *(__vec2x32float__ *)&tf78.v.x;
                tfa8.v.z = mvz;
                if (ParentList())
                    ParentList()->AdjustTrans(tfa8, curdrawstate);
                CalcXfm(ctf, curdrawstate.mPos, tfa8);
#ifdef HX_NATIVE
                // W4.4-TEXTCOLOR (RB3_ROWFIX Part B, default-OFF): on the row that
                // Part A painted a solid bright fill under (this list latched
                // RB3RowfixFillDrawn), the highlighted-row text must read DARK on
                // the bright bar (target polarity) instead of the default light.
                //
                // W4.4-TEXTCOLOR observation (evidence/): the focused-row label
                // draws with NO provider color (uicolor==null: DisplayColor/
                // SlotColorOverride return 0 for these rows) and stays at kNormal
                // state, so its text falls to GetStateColor (light, 0.87). The
                // Wave-15 attempt only mutated a non-null uicolor, so its `uicolor
                // &&` guard SKIPPED every focused row (probe: colOv=0, mainCol
                // stayed 0.87). Fix: for the highlighted display element of the
                // fill-bearing list, force a dark label color — reuse the
                // provider's UIColor when present (mutate/restore in place), else
                // supply a file-static dark UIColor. Immediate draw (see engine
                // DrawMeshImmediate) snapshots the color, so restore-after is safe.
                Hmx::Color rowfixSaved;
                UIColor *rowfixMutated = 0;
                if (!box && RB3RowfixActive() && RB3RowfixFillDrawn() &&
                    curdrawstate.mDisplay == drawstate.mHighlightDisplay) {
                    static UIColor sRowfixDark;
                    Hmx::Color dark(0.06f, 0.05f, 0.02f, 1.0f);
                    if (uicolor) {
                        rowfixSaved = uicolor->GetColor();
                        dark.alpha = rowfixSaved.alpha;
                        uicolor->SetColor(dark);
                        rowfixMutated = uicolor;
                    } else {
                        sRowfixDark.SetColor(dark);
                        uicolor = &sRowfixDark;
                    }
                }
#endif
                if (cmd != kExcludeFirst || i > 0) {
                    mElements[i]->Draw(tfa8, d10, uicolor, box);
                }
#ifdef HX_NATIVE
                if (rowfixMutated)
                    rowfixMutated->SetColor(rowfixSaved);
#endif
                if (cmd == kDrawFirst)
                    return;
            }
        }
    }
}

void UIListSlot::Fill(const UIListProvider &prov, int display, int j, int k) {
    if (RootTrans()) {
        MILO_ASSERT(display < mElements.size(), 0x8F);
        mElements[display]->Fill(prov, j, k);
    }
}

void UIListSlot::StartScroll(int i, bool b) {
    if (b && RootTrans()) {
        UIListSlotElement *next = mNextElement;
        mElements.insert(i < 0 ? mElements.begin() : mElements.end(), next);
        mNextElement = 0;
    }
}

void UIListSlot::CompleteScroll(const UIListState &liststate, int i) {
    if (RootTrans()) {
        if (mElements.size() == (unsigned)(liststate.NumDisplay() + 1)) {
            int idx = i > 0 ? 0 : liststate.NumDisplay();
            UIListSlotElement *elem = mElements[idx];
            mElements.erase(std::find(mElements.begin(), mElements.end(), elem));
            mNextElement = elem;
        }
    }
}

void UIListSlot::Poll() {
    for (std::vector<UIListSlotElement *>::iterator it = mElements.begin();
         it != mElements.end();
         it++) {
        (*it)->Poll();
    }
}

void UIListSlot::ResourceCopy(const UIListWidget *w) {
    UIListWidget::ResourceCopy(w);
    mMatchName = w->Name();
}

SAVE_OBJ(UIListSlot, 0xC6)

BEGIN_LOADS(UIListSlot)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(UIListWidget)
    int ty;
    bs >> ty;
    mSlotDrawType = (UIListSlotDrawType)ty;
END_LOADS

BEGIN_COPYS(UIListSlot)
    COPY_SUPERCLASS(UIListWidget)
    CREATE_COPY_AS(UIListSlot, s)
    MILO_ASSERT(s, 0xD8);
    COPY_MEMBER_FROM(s, mSlotDrawType)
END_COPYS

BEGIN_HANDLERS(UIListSlot)
    HANDLE_SUPERCLASS(UIListWidget)
    HANDLE_CHECK(0xDE)
END_HANDLERS

BEGIN_PROPSYNCS(UIListSlot)
    SYNC_PROP_SET(
        slot_draw_type, (int)mSlotDrawType, mSlotDrawType = (UIListSlotDrawType)_val.Int()
    )
    SYNC_SUPERCLASS(UIListWidget)
END_PROPSYNCS
