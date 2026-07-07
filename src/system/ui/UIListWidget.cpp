#include "ui/UIListWidget.h"
#include "rndobj/Utl.h"
#ifdef HX_NATIVE
#include "rndobj/Mat.h"
#include "rndobj/Tex.h"
#include "rndobj/Mesh.h"
#endif
#include "math/Rot.h"
#include "utl/Symbols.h"
#include "decomp.h"

#ifdef HX_NATIVE
// W4.4-ROWFIX flag + per-list fill latch (see UIListWidget.h).
bool RB3RowfixActive() {
    static const bool on = getenv("RB3_ROWFIX") != 0;
    return on;
}
static bool sRowfixFillDrawn = false;
void RB3RowfixResetFill()    { sRowfixFillDrawn = false; }
void RB3RowfixSetFillDrawn() { sRowfixFillDrawn = true; }
bool RB3RowfixFillDrawn()    { return sRowfixFillDrawn; }
#endif

INIT_REVS(UIListWidget);

UIListWidget::UIListWidget()
    : mDrawOrder(0.0f), mDisabledAlphaScale(1.0f), mDefaultColor(this),
      mWidgetDrawType(kUIListWidgetDrawAlways), mParentList(nullptr) {
    for (int i = 0; i < kNumUIListWidgetStates; i++) {
        std::vector<ObjPtr<UIColor> > vec;
        for (int j = 0; j < UIComponent::kNumStates; j++) {
            vec.push_back(ObjPtr<UIColor>(this));
        }
        mColors.push_back(vec);
    }
}

float UIListWidget::DrawOrder() const { return mDrawOrder; }
float UIListWidget::DisabledAlphaScale() const { return mDisabledAlphaScale; }
UIListWidgetDrawType UIListWidget::WidgetDrawType() const { return mWidgetDrawType; }
UIList *UIListWidget::ParentList() { return mParentList; }

void UIListWidget::ResourceCopy(const UIListWidget *widget) {
    Copy(widget, kCopyShallow);
}

void UIListWidget::SetParentList(UIList *list) { mParentList = list; }

void UIListWidget::DrawMesh(
    RndMesh *mesh,
    UIListWidgetState wstate,
    UIComponent::State cstate,
    const Transform &tf,
    Box *box
) {
    MILO_ASSERT(mesh, 0x40);
    mesh->SetWorldXfm(tf);
    if (box) {
        Box localbox = *box;
        CalcBox(mesh, localbox);
        box->GrowToContain(localbox.mMin, false);
        box->GrowToContain(localbox.mMax, false);
    } else {
        UIColor *col = DisplayColor(wstate, cstate);
        if (col) {
            RndMat *mat = mesh->mMat;
            if (mat)
                mat->SetColor(col->GetColor());
        }
#ifdef HX_NATIVE
        // W4.4-ROWFIX probe (RB3_ROWFIX_DBG): log the color/state/material of
        // every UIListWidget mesh draw so the focused-row highlight fill can be
        // isolated. Inert unless the env var is set. Wii build untouched.
        if (getenv("RB3_ROWFIX_DBG")) {
            RndMat *mat = mesh->mMat;
            const Hmx::Color c = col ? col->GetColor() : Hmx::Color(0,0,0,0);
            const Hmx::Color mc = mat ? mat->GetColor() : Hmx::Color(0,0,0,0);
            RndTex *tex = mat ? mat->GetDiffuseTex() : 0;
            RndMesh *owner = mesh->GeomOwner();
            fprintf(stderr,
                "[ROWFIX] mesh='%s' verts=%d ownverts=%d ownself=%d faces=%d wstate=%d cstate=%d col=(%.2f,%.2f,%.2f,a%.2f) hascol=%d "
                "mat='%s' matcol=(%.2f,%.2f,%.2f,a%.2f) tex='%s' blend=%d zmode=%d\n",
                mesh->Name(), mesh->NumVerts(),
                owner ? owner->NumVerts() : -1, owner == mesh ? 1 : 0,
                owner ? owner->NumFaces() : -1,
                (int)wstate, (int)cstate,
                c.red, c.green, c.blue, c.alpha, col ? 1 : 0,
                mat ? mat->Name() : "(null)",
                mc.red, mc.green, mc.blue, mc.alpha,
                tex ? tex->Name() : "(notex)",
                mat ? (int)mat->GetBlend() : -1, mat ? (int)mat->GetZMode() : -1);
        }
#endif
        mesh->DrawShowing();
    }
}

void UIListWidget::CalcXfm(const Transform &tf1, const Vector3 &vec, Transform &tf2) {
    tf2.v.x += vec.x;
    tf2.v.z += vec.z;
    Multiply(tf2, tf1, tf2);
}

UIColor *UIListWidget::DisplayColor(
    UIListWidgetState element_state, UIComponent::State list_state
) const {
    MILO_ASSERT(element_state < kNumUIListWidgetStates, 99);
    MILO_ASSERT(list_state < UIComponent::kNumStates, 100);
    UIColor *theColor = mColors[element_state][list_state];
    if (theColor)
        return theColor;
    else if (mDefaultColor)
        return mDefaultColor;
    else
        return 0;
}

void UIListWidget::SetColor(UIListWidgetState ws, UIComponent::State cs, UIColor *color) {
    MILO_ASSERT(ws < kNumUIListWidgetStates, 0x7C);
    MILO_ASSERT(cs < UIComponent::kNumStates, 0x7D);
    ObjPtr<UIColor> &theColor = mColors[ws][cs];
    theColor = color;
}

SAVE_OBJ(UIListWidget, 0x97);

DECOMP_FORCEACTIVE(UIListWidget, "ObjPtr_p.h", "f.Owner()", "")

BEGIN_LOADS(UIListWidget)
    LOAD_REVS(bs);
    ASSERT_REVS(2, 0);
    LOAD_SUPERCLASS(Hmx::Object)
    bs >> mDrawOrder;
    int x;
    if (gRev < 1) {
        int i, j;
        bs >> i >> j;
    }
    bs >> mDefaultColor >> x;
    mWidgetDrawType = (UIListWidgetDrawType)x;
    if (gRev >= 2)
        bs >> mDisabledAlphaScale;
    for (int i = 0; i < kNumUIListWidgetStates; i++) {
        for (int j = 0; j < UIComponent::kNumStates; j++) {
            ObjPtr<UIColor, ObjectDir> colPtr(this, 0);
            bs >> colPtr;
            mColors[i][j] = colPtr;
        }
    }
END_LOADS

BEGIN_COPYS(UIListWidget)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY_AS(UIListWidget, w)
    MILO_ASSERT(w, 0xC6);
    COPY_MEMBER_FROM(w, mDrawOrder)
    COPY_MEMBER_FROM(w, mDisabledAlphaScale)
    COPY_MEMBER_FROM(w, mDefaultColor)
    COPY_MEMBER_FROM(w, mColors)
    COPY_MEMBER_FROM(w, mWidgetDrawType)
END_COPYS

BEGIN_HANDLERS(UIListWidget)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0xD0)
END_HANDLERS

BEGIN_PROPSYNCS(UIListWidget)
    SYNC_PROP(draw_order, mDrawOrder)
    SYNC_PROP(disabled_alpha_scale, mDisabledAlphaScale)
    SYNC_PROP(default_color, mDefaultColor)
    SYNC_PROP_SET(
        widget_draw_type,
        (int &)mWidgetDrawType,
        mWidgetDrawType = (UIListWidgetDrawType)_val.Int()
    )
    SYNC_PROP_SET(
        active_normal_color,
        (Hmx::Object *)mColors[kUIListWidgetActive][UIComponent::kNormal],
        SetColor(kUIListWidgetActive, UIComponent::kNormal, _val.Obj<UIColor>())
    )
    SYNC_PROP_SET(
        active_focused_color,
        (Hmx::Object *)mColors[kUIListWidgetActive][UIComponent::kFocused],
        SetColor(kUIListWidgetActive, UIComponent::kFocused, _val.Obj<UIColor>())
    )
    SYNC_PROP_SET(
        active_disabled_color,
        (Hmx::Object *)mColors[kUIListWidgetActive][UIComponent::kDisabled],
        SetColor(kUIListWidgetActive, UIComponent::kDisabled, _val.Obj<UIColor>())
    )
    SYNC_PROP_SET(
        active_selecting_color,
        (Hmx::Object *)mColors[kUIListWidgetActive][UIComponent::kSelecting],
        SetColor(kUIListWidgetActive, UIComponent::kSelecting, _val.Obj<UIColor>())
    )
    SYNC_PROP_SET(
        active_selected_color,
        (Hmx::Object *)mColors[kUIListWidgetActive][UIComponent::kSelected],
        SetColor(kUIListWidgetActive, UIComponent::kSelected, _val.Obj<UIColor>())
    )
    SYNC_PROP_SET(
        highlight_normal_color,
        (Hmx::Object *)mColors[kUIListWidgetHighlight][UIComponent::kNormal],
        SetColor(kUIListWidgetHighlight, UIComponent::kNormal, _val.Obj<UIColor>())
    )
    SYNC_PROP_SET(
        highlight_focused_color,
        (Hmx::Object *)mColors[kUIListWidgetHighlight][UIComponent::kFocused],
        SetColor(kUIListWidgetHighlight, UIComponent::kFocused, _val.Obj<UIColor>())
    )
    SYNC_PROP_SET(
        highlight_disabled_color,
        (Hmx::Object *)mColors[kUIListWidgetHighlight][UIComponent::kDisabled],
        SetColor(kUIListWidgetHighlight, UIComponent::kDisabled, _val.Obj<UIColor>())
    )
    SYNC_PROP_SET(
        highlight_selecting_color,
        (Hmx::Object *)mColors[kUIListWidgetHighlight][UIComponent::kSelecting],
        SetColor(kUIListWidgetHighlight, UIComponent::kSelecting, _val.Obj<UIColor>())
    )
    SYNC_PROP_SET(
        highlight_selected_color,
        (Hmx::Object *)mColors[kUIListWidgetHighlight][UIComponent::kSelected],
        SetColor(kUIListWidgetHighlight, UIComponent::kSelected, _val.Obj<UIColor>())
    )
    SYNC_PROP_SET(
        inactive_normal_color,
        (Hmx::Object *)mColors[kUIListWidgetInactive][UIComponent::kNormal],
        SetColor(kUIListWidgetInactive, UIComponent::kNormal, _val.Obj<UIColor>())
    )
    SYNC_PROP_SET(
        inactive_focused_color,
        (Hmx::Object *)mColors[kUIListWidgetInactive][UIComponent::kFocused],
        SetColor(kUIListWidgetInactive, UIComponent::kFocused, _val.Obj<UIColor>())
    )
    SYNC_PROP_SET(
        inactive_disabled_color,
        (Hmx::Object *)mColors[kUIListWidgetInactive][UIComponent::kDisabled],
        SetColor(kUIListWidgetInactive, UIComponent::kDisabled, _val.Obj<UIColor>())
    )
    SYNC_PROP_SET(
        inactive_selecting_color,
        (Hmx::Object *)mColors[kUIListWidgetInactive][UIComponent::kSelecting],
        SetColor(kUIListWidgetInactive, UIComponent::kSelecting, _val.Obj<UIColor>())
    )
    SYNC_PROP_SET(
        inactive_selected_color,
        (Hmx::Object *)mColors[kUIListWidgetInactive][UIComponent::kSelected],
        SetColor(kUIListWidgetInactive, UIComponent::kSelected, _val.Obj<UIColor>())
    )
END_PROPSYNCS
