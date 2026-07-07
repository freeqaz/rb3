#include "ui/UIListHighlight.h"
#include "ui/UIList.h"
#include "utl/Symbols.h"
#ifdef HX_NATIVE
#include "rndobj/Mesh.h"
#include "rndobj/Mat.h"
#include "obj/Dir.h"
#include "math/Color.h"
#include <cstdlib>
#include <cstring>
#endif

INIT_REVS(UIListHighlight)

UIListHighlight::UIListHighlight() : mMesh(this, 0) {}

SAVE_OBJ(UIListHighlight, 0x28)

BEGIN_LOADS(UIListHighlight)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(UIListWidget)
    bs >> mMesh;
END_LOADS

BEGIN_COPYS(UIListHighlight)
    COPY_SUPERCLASS(UIListWidget)
    CREATE_COPY_AS(UIListHighlight, h)
    MILO_ASSERT(h, 0x38);
    COPY_MEMBER_FROM(h, mMesh)
END_COPYS

void UIListHighlight::Draw(
    const UIListWidgetDrawState &drawstate,
    const UIListState &liststate,
    const Transform &tf,
    UIComponent::State compstate,
    Box *box,
    DrawCommand cmd
) {
    if (!mMesh || cmd == kDrawFirst)
        return;
    Transform &worldxfm = mMesh->WorldXfm();
    Transform xfm1 = worldxfm;
    Transform xfm2 = xfm1;
    if (ParentList()) {
        ParentList()->AdjustTransSelected(xfm2);
    }
    CalcXfm(tf, drawstate.mHighlightPos, xfm2);
#ifdef HX_NATIVE
    // W4.4-ROWFIX Part A (RB3_ROWFIX, default-OFF): ml_highlight_glasstopp is the
    // full-row focus fill quad, but authored as a near-invisible additive sheen
    // (white, alpha 0.08) — natively there is no bright bar (the focus-anim that
    // would light it does not run). Repaint it as a SOLID fill in the highlight's
    // own authored color (read from the sibling highlight_main.mat, the yellow
    // frame color) so the focused row gets the retail bright bar. Also latch that
    // this list drew the fill, so Part B (UIListSlot) darkens only THIS list's
    // highlighted-row text. Scoped to the glasstopp quad -> zero cross-list /
    // gameplay effect. DisplayColor for this widget returns null (no per-state
    // override), so DrawMesh below won't stomp the color we set here.
    if (!box && RB3RowfixActive() && mMesh && mMesh->mMat &&
        mMesh->Name() && strstr(mMesh->Name(), "glasstopp")) {
        Hmx::Color fill(0.82f, 0.76f, 0.02f, 1.0f); // authored highlight fallback
        if (mMesh->Dir()) {
            RndMat *frame = mMesh->Dir()->Find<RndMat>("highlight_main.mat", false);
            if (frame) {
                const Hmx::Color &fc = frame->GetColor();
                fill.Set(fc.red, fc.green, fc.blue, 1.0f);
            }
        }
        mMesh->mMat->SetColor(fill);
        mMesh->mMat->SetDiffuseTex(0);
        // Opaque overwrite (kBlendSrc): the glasstopp quad carries a sheen
        // vertex-alpha gradient (bright-left, fading-right); srcAlpha blending
        // would reproduce that fade, so use an opaque fill for a uniform bar.
        mMesh->mMat->SetBlend(RndMat::kBlendSrc);
        RB3RowfixSetFillDrawn();
    }
#endif
    DrawMesh(mMesh, drawstate.mHighlightElementState, compstate, xfm2, box);
    mMesh->SetWorldXfm(xfm1);
}

BEGIN_HANDLERS(UIListHighlight)
    HANDLE_SUPERCLASS(UIListWidget)
    HANDLE_CHECK(0x59)
END_HANDLERS

BEGIN_PROPSYNCS(UIListHighlight)
    SYNC_PROP(mesh, mMesh)
    SYNC_SUPERCLASS(UIListWidget)
END_PROPSYNCS