#include "ui/UIList.h"
#include "ui/UIListDir.h"
#include "ui/UIListWidget.h"
#include <algorithm>
#include "obj/ObjVersion.h"
#include "utl/Std.h"
#include "utl/Messages.h"
#include "utl/Symbols.h"

INIT_REVS(UIListDir)

DECOMP_FORCEACTIVE(UIListDir, __FILE__, "( 0) <= (change) && (change) <= ( 1)")

namespace {
    class WidgetDrawSort {
    public:
        bool operator()(UIListWidget *w1, UIListWidget *w2) {
            return w1->DrawOrder() < w2->DrawOrder();
        }
    };
}

UIListDir::UIListDir()
    : mOrientation(kUIListVertical), mFadeOffset(0), mElementSpacing(50.0f),
      mScrollHighlightChange(0.5f), mTestMode(0), mTestState(this, this),
      mTestNumData(100), mTestGapSize(0.0f), mTestComponentState(UIComponent::kFocused),
      mTestDisableElements(0), unk1fc(), mDirection(0) {
    mTestState.SetNumDisplay(5, true);
    mTestState.SetGridSpan(1, true);
    mTestState.SetSelected(0, -1, true);
}

UIListDir::~UIListDir() { DeleteAll(unk1fc); }

UIListOrientation UIListDir::Orientation() const { return mOrientation; }

float UIListDir::ElementSpacing() const { return mElementSpacing; }

// fn_805693B0
void UIListDir::BuildDrawState(
    UIListWidgetDrawState &drawState,
    const UIListState &state,
    UIComponent::State compState,
    float subListOffset
) const {
    int numDisplay = state.NumDisplay();
    int numDisplayWithData = state.NumDisplayWithData();

    int fadeCountEnd = mFadeOffset;
    if (numDisplay / 2 < mFadeOffset) {
        fadeCountEnd = numDisplay / 2;
    }
    int fadeCountStart = fadeCountEnd;
    if (mFadeOffset != 0) {
        if (state.mCircular) {
            int selectedDisp = state.SelectedDisplay();
            if (selectedDisp < fadeCountStart) {
                fadeCountStart = selectedDisp;
            }
            int fadeEndCalc = (numDisplay - state.SelectedDisplay()) - 1;
            if (fadeEndCalc < fadeCountEnd) {
                fadeCountEnd = fadeEndCalc;
            }
        } else {
            int firstShowing = state.mFirstShowing;
            if (state.ScrollPastMinDisplay()) {
                firstShowing -= state.MinDisplay();
            }
            if (firstShowing < 0) {
                firstShowing = 0;
            }
            if (firstShowing < fadeCountStart) {
                fadeCountStart = firstShowing;
            }
            auto _tmp0 = state.Provider()->NumData();
            int fadeEndCalc =
                _tmp0 - (firstShowing + numDisplay);
            if (fadeEndCalc < fadeCountEnd) {
                fadeCountEnd = fadeEndCalc;
            }
        }
    }
    float fadeStartDist = (float)fadeCountStart * mElementSpacing;
    float fadeEndDist = mElementSpacing * (float)((numDisplay - 1) - fadeCountEnd);

    int direction = state.CurrentScroll() > 0 ? 1 : -1;
    int selected = state.Selected();
    int selectedData = state.SelectedData();
    int selectedDisplay = state.SelectedDisplay();
    drawState.mHighlightDisplay = selectedDisplay;

    if (state.IsScrolling()) {
        if (state.Speed() > mScrollHighlightChange) {
            selected += direction;
            drawState.mHighlightDisplay += direction;
        }
        numDisplayWithData++;
    }

    drawState.mElements.clear();
    drawState.mElements.reserve(numDisplayWithData);
    drawState.mHighlightElementState = kUIListWidgetActive;

    int prevData = 0;
    Vector3 elemPos;
    float firstGap = 0.0f;
    float totalGap = 0.0f;
    float lastPosBase = 0.0f;
    float highlightBase = 0.0f;
    float scrollOffset = (float)direction * state.StepPercent();

    int dispIndex;
    for (int i = 0; i < numDisplayWithData; i++) {
                dispIndex = i;
        if (state.IsScrolling() && direction == -1) {
            dispIndex = i - 1;
        }
        int data = state.Display2Data(dispIndex);
        if (data == -1) {
            UIListElementDrawState elem;
            elem.unk0 = false;
            drawState.mElements.push_back(elem);
            continue;
        }
        if (state.mCircular || prevData <= data) {
            int showing = state.Display2Showing(dispIndex);
            prevData = data;
            int snapped = state.SnappedDataForDisplay(dispIndex);
            if (snapped >= 0) {
                prevData = snapped;
            }
            float gap = state.Provider()->GapSize(showing, prevData, selectedData, direction);
            if (i == 0) {
                firstGap = gap;
            }
            float pos;
            if (state.ShouldHoldDisplayInPlace(dispIndex)) {
                if (direction == -1) {
                    pos = SetElementPos(
                        elemPos, 1.0f + (float)dispIndex, state.mGridSpan, totalGap, 0.0f
                    );
                } else {
                    pos = SetElementPos(
                        elemPos, (float)dispIndex, state.mGridSpan, totalGap, 0.0f
                    );
                }
            } else {
                pos = SetElementPos(
                    elemPos,
                    (float)dispIndex - scrollOffset,
                    state.mGridSpan,
                    -((scrollOffset * firstGap) - totalGap),
                    0.0f
                );
            }
            float alpha = 1.0f;
            if (!state.ShouldHoldDisplayInPlace(dispIndex)) {
                float dist = pos - -((scrollOffset * firstGap) - totalGap);
                if (dist < fadeStartDist) {
                    alpha -= (fadeStartDist - dist)
                        / ((float)(fadeCountStart + 1) * mElementSpacing);
                } else if (dist > fadeEndDist) {
                    alpha -= (dist - fadeEndDist)
                        / ((float)(fadeCountEnd + 1) * mElementSpacing);
                }
            }
            UIListWidgetState elemState;
            if (state.Provider()->IsActive(prevData)) {
                elemState = showing == selected ? kUIListWidgetHighlight
                                                : kUIListWidgetActive;
            } else {
                elemState = kUIListWidgetInactive;
            }
            UIListWidgetState widgetState =
                state.Provider()->ElementStateOverride(showing, prevData, elemState);
            if (showing == selected) {
                drawState.mHighlightElementState = widgetState;
            }
            UIComponent::State componentState =
                state.Provider()->ComponentStateOverride(showing, prevData, compState);
            UIListElementDrawState elem;
            elem.unk0 = true;
            elem.mPos = elemPos;
            elem.mAlpha = alpha;
            elem.mElementState = widgetState;
            elem.mComponentState = componentState;
            elem.mDisplay = dispIndex;
            elem.mShowing = showing;
            elem.mData = prevData;
            drawState.mElements.push_back(elem);

            totalGap += gap;
            if (dispIndex > 0 && dispIndex < numDisplay - 1) {
                lastPosBase += gap;
            }
            if (dispIndex < selectedDisplay) {
                highlightBase += gap;
            }
        }
    }

    SetElementPos(drawState.mFirstPos, 0.0f, state.mGridSpan, 0.0f, 0.0f);
    SetElementPos(
        drawState.mLastPos,
        (float)(state.NumDisplay() - 1),
        state.mGridSpan,
        lastPosBase,
        0.0f
    );
    SetElementPos(
        drawState.mHighlightPos,
        (float)selectedDisplay,
        state.mGridSpan,
        highlightBase,
        subListOffset
    );
}

float UIListDir::SetElementPos(Vector3 &v, float f1, int i2, float f3, float f4) const {
    v.Zero();
    int floored = std::floor(f1);
    float spacing = mElementSpacing;
    float f3toset =
        spacing * ((f1 - (float)floored) + (float)(floored / i2)) + f3;
    float f2toset = spacing * (float)(floored % i2) + f4;
    if (mOrientation == kUIListVertical) {
        v.z -= f3toset;
        v.x += f2toset;
    } else {
        v.x += f3toset;
        v.z -= f2toset;
    }
    return f3toset;
}

UIList *UIListDir::SubList(int i, std::vector<UIListWidget *> &vec) {
    for (std::vector<UIListWidget *>::iterator it = vec.begin(); it != vec.end(); it++) {
        UIList *l = (*it)->SubList(i);
        if (l)
            return l;
    }
    return nullptr;
}

void UIListDir::CreateElements(UIList *uilist, std::vector<UIListWidget *> &vec, int i) {
    DeleteAll(vec);
    for (ObjDirItr<UIListWidget> it(this, true); it != 0; ++it) {
        UIListWidget *widget =
            dynamic_cast<UIListWidget *>(Hmx::Object::NewObject(it->ClassName()));
        widget->ResourceCopy(it);
        widget->SetParentList(uilist);
        vec.push_back(widget);
    }
    std::sort(vec.begin(), vec.end(), WidgetDrawSort());
    for (std::vector<UIListWidget *>::iterator it = vec.begin(); it != vec.end(); ++it) {
        (*it)->CreateElements(uilist, i);
    }
}

void UIListDir::FillElements(const UIListState &state, std::vector<UIListWidget *> &vec) {
    int num = state.NumDisplayWithData();
    for (int i = 0; i < num; i++) {
        FillElement(state, vec, i);
    }
}

// fn_8056AEBC
void UIListDir::FillElement(
    const UIListState &state, std::vector<UIListWidget *> &vec, int i
) {
    int disp = state.Display2Data(i);
    if (disp != -1) {
        int snapped = state.SnappedDataForDisplay(i);
        if (snapped >= 0)
            disp = snapped;
        int disp2show = state.Display2Showing(i);
        bool isnegone = i == -1;
        ClampEq(i, 0, state.NumDisplay());
        for (std::vector<UIListWidget *>::iterator it = vec.begin(); it != vec.end();
             ++it) {
            (*it)->Fill(*state.Provider(), i, disp2show, disp);
            if (isnegone && snapped >= 0) {
                (*it)->Fill(
                    *state.Provider(), 1, state.Display2Showing(0), state.Display2Data(0)
                );
            }
        }
    }
}

// fn_8056B014
void UIListDir::DrawWidgets(
    const UIListState &state,
    std::vector<UIListWidget *> &vec,
    const Transform &tf,
    UIComponent::State compstate,
    Box *box,
    bool bptr
) {
    // some ctor
    UIListWidgetDrawState drawstate;
    float f1;
    UIList *sublist = SubList(state.SelectedDisplay(), vec);
    if (sublist) {
        f1 =
            (float)sublist->SelectedDisplay() * sublist->GetUIListDir()->ElementSpacing();
    } else
        f1 = 0;
    BuildDrawState(drawstate, state, compstate, f1);
    bool scrolling = state.IsScrolling();
    bool isFocused = (compstate == UIComponent::kFocused);
    for (std::vector<UIListWidget *>::iterator it = vec.begin(); it != vec.end(); ++it) {
        UIListWidget *curWidget = *it;
        UIListWidgetDrawType drawType = curWidget->WidgetDrawType();
        if (drawType == 0 || (drawType == 3 && (bptr || isFocused))
            || (drawType == 1 && isFocused)) {
            curWidget->Draw(drawstate, state, tf, compstate, box, scrolling ? kExcludeFirst : kDrawAll);
        }
    }
    if (scrolling) {
        for (std::vector<UIListWidget *>::iterator it = vec.begin(); it != vec.end();
             ++it) {
            UIListWidget *curWidget = *it;
            UIListWidgetDrawType drawType = curWidget->WidgetDrawType();
            if (drawType == 0 || (drawType == 1 && compstate == 1)) {
                curWidget->Draw(drawstate, state, tf, compstate, box, kDrawFirst);
            }
        }
    }
}

void UIListDir::PollWidgets(std::vector<UIListWidget *> &widgets) {
    for (std::vector<UIListWidget *>::iterator it = widgets.begin(); it != widgets.end();
         ++it) {
        (*it)->Poll();
    }
}

void UIListDir::ListEntered() { Handle(start_msg, false); }

void UIListDir::StartScroll(const UIListState &state, int i, bool b) {
    StartScroll(state, unk1fc, i, b);
}

void UIListDir::CompleteScroll(const UIListState &state) {
    CompleteScroll(state, unk1fc);
}

void UIListDir::StartScroll(
    const UIListState &state, std::vector<UIListWidget *> &widgets, int i, bool b
) {
    mDirection = i;
    MILO_ASSERT(mDirection, 499);
    for (std::vector<UIListWidget *>::iterator it = widgets.begin(); it != widgets.end();
         ++it) {
        (*it)->StartScroll(mDirection, b);
    }
    if (b) {
        FillElement(state, widgets, mDirection > 0 ? state.mNumDisplay : -1);
    }
}

void UIListDir::CompleteScroll(
    const UIListState &state, std::vector<UIListWidget *> &widgets
) {
    for (std::vector<UIListWidget *>::iterator it = widgets.begin(); it != widgets.end();
         ++it) {
        (*it)->CompleteScroll(state, mDirection);
    }
    if (mDirection == 1 && state.SnappedDataForDisplay(0) >= 0) {
        FillElement(state, widgets, 0);
    }
}

void UIListDir::Reset() {
    mTestState.SetSelected(0, -1, true);
    FillElements(mTestState, unk1fc);
}

SAVE_OBJ(UIListDir, 0x21F);

void UIListDir::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(1, 0);
    PushRev(packRevs(gAltRev, gRev), this);
    RndDir::PreLoad(bs);
}

void UIListDir::PostLoad(BinStream &bs) {
    RndDir::PostLoad(bs);
    int revs = PopRev(this);
    gRev = getHmxRev(revs);
    gAltRev = getAltRev(revs);
    int orientation;
    bs >> orientation;
    bs >> mFadeOffset;
    mOrientation = (UIListOrientation)orientation;
    bs >> mTestMode;
    int numdisplay, compstate;
    float speed;
    bs >> numdisplay >> mElementSpacing >> speed >> mTestNumData >> compstate
        >> mTestGapSize;
    bs >> mTestDisableElements;
    if (gRev != 0)
        bs >> mScrollHighlightChange;
    mTestState.SetNumDisplay(numdisplay, true);
    mTestState.SetSpeed(speed);
    mTestComponentState = (UIComponent::State)compstate;
}

BEGIN_COPYS(UIListDir)
    COPY_SUPERCLASS(RndDir)
    CREATE_COPY(UIListDir)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mOrientation)
        COPY_MEMBER(mFadeOffset)
        COPY_MEMBER(mElementSpacing)
        COPY_MEMBER(mScrollHighlightChange)
        COPY_MEMBER(mTestMode)
        mTestState.SetNumDisplay(c->mTestState.mNumDisplay, true);
        mTestState.SetGridSpan(c->mTestState.mGridSpan, true);
        mTestState.SetSpeed(c->mTestState.Speed());
        COPY_MEMBER(mTestNumData)
        COPY_MEMBER(mTestComponentState)
        COPY_MEMBER(mTestGapSize)
        COPY_MEMBER(mTestDisableElements)
    END_COPYING_MEMBERS
END_COPYS

void UIListDir::SyncObjects() {
    RndDir::SyncObjects();
    if (LOADMGR_EDITMODE) {
        CreateElements(0, unk1fc, mTestState.mNumDisplay);
        FillElements(mTestState, unk1fc);
    }
}

void UIListDir::DrawShowing() {
    if (mTestMode && LOADMGR_EDITMODE) {
        DrawWidgets(mTestState, unk1fc, WorldXfm(), mTestComponentState, 0, false);
    } else
        RndDir::DrawShowing();
}

void UIListDir::Poll() {
    if (LOADMGR_EDITMODE) {
        RndDir::Poll();
        if (mTestMode) {
            mTestState.Poll(TheTaskMgr.Seconds(TaskMgr::kRealTime));
            PollWidgets(unk1fc);
        }
    }
}

int UIListDir::NumData() const { return mTestNumData; }
float UIListDir::GapSize(int, int, int, int) const { return mTestGapSize; }
bool UIListDir::IsActive(int i) const {
    if (mTestDisableElements)
        return !(i % 2);
    else
        return true;
}

BEGIN_PROPSYNCS(UIListDir)
    SYNC_PROP_SET(orientation, mOrientation, mOrientation = (UIListOrientation)_val.Int())
    SYNC_PROP(fade_offset, mFadeOffset)
    SYNC_PROP(element_spacing, mElementSpacing)
    SYNC_PROP(scroll_highlight_change, mScrollHighlightChange)
    SYNC_PROP(test_mode, mTestMode)
    SYNC_PROP(test_num_data, mTestNumData)
    SYNC_PROP(test_gap_size, mTestGapSize)
    SYNC_PROP_SET(
        test_num_display,
        mTestState.mNumDisplay,
        mTestState.SetNumDisplay(_val.Int(), true)
    )
    SYNC_PROP_SET(
        test_grid_span, mTestState.mGridSpan, mTestState.SetGridSpan(_val.Int(), true)
    )
    SYNC_PROP_SET(test_scroll_time, mTestState.Speed(), mTestState.SetSpeed(_val.Float()))
    SYNC_PROP_SET(
        test_list_state,
        mTestComponentState,
        mTestComponentState = (UIComponent::State)_val.Int()
    )
    SYNC_PROP_MODIFY(test_disable_elements, mTestDisableElements, Reset())
    SYNC_SUPERCLASS(RndDir)
END_PROPSYNCS

BEGIN_HANDLERS(UIListDir)
    HANDLE_ACTION(test_scroll, mTestState.Scroll(_msg->Int(2), false))
    HANDLE_SUPERCLASS(RndDir)
    HANDLE_CHECK(0x2C2)
END_HANDLERS
