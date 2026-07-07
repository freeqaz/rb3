#include "InlineHelp.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Trig.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "os/System.h"
#include "ui/UI.h"
#include "ui/UIComponent.h"
#include "utl/Locale.h"
#include "utl/Symbols.h"
#ifdef HX_NATIVE
#include <cstdio>  // RB3_HOLDLABEL_DBG probe (Wave12 W4.3-C34, C3/C4 diagnosis)
#include <cstdlib> // getenv
#endif


#ifdef __MWERKS__
// Fused Multiply(Transform, Matrix3, Transform): computes out.v = t.v * m and
// out.m = t.m * m in a single paired-single region so the compiler can share the
// m.x row loads (f0/f1) between the vector and matrix products.
inline void Multiply(const Transform &t, const Hmx::Matrix3 &m, Transform &out) {
    register const Hmx::Matrix3 *_m = &m;
    register Hmx::Matrix3 *_om = &out.m;
    register Vector3 *_ov = &out.v;
    register const Vector3 *_tv = &t.v;
    register const Hmx::Matrix3 *_tm = &t.m;
    register __vec2x32float__ g2, g3, g4, g5, g6, g7, g8, g9, g10, g11, g12, g13, g30, g31, g0, g1;
    // out.v = t.v * m  (leaves g0 = m.x.x, g1 = m.x.z for the matrix product)
    asm {
        psq_l  g3, 0x4(_tv), 0, 0
        psq_l  g0, 0x18(_m), 0, 0
        psq_l  g1, 0x20(_m), 1, 0
        ps_muls1 g4, g0, g3
        psq_l  g0, 0xc(_m), 0, 0
        ps_muls1 g5, g1, g3
        psq_l  g1, 0x14(_m), 1, 0
        psq_l  g2, 0x0(_tv), 0, 0
        ps_madds0 g4, g0, g3, g4
        psq_l  g0, 0x0(_m), 0, 0
        ps_madds0 g5, g1, g3, g5
        psq_l  g1, 0x8(_m), 1, 0
        ps_madds0 g4, g0, g2, g4
        ps_madds0 g5, g1, g2, g5
        psq_st g4, 0x0(_ov), 0, 0
        psq_st g5, 0x8(_ov), 1, 0
    }
    if (_m != _om) {
        // out.m = t.m * m  (non-alias; reuses g0=m.x.x, g1=m.x.z)
        asm {
            psq_l  g6, 0x4(_tm), 0, 0
            psq_l  g5, 0x18(_m), 0, 0
            psq_l  g4, 0x20(_m), 1, 0
            ps_muls1 g3, g5, g6
            psq_l  g5, 0xc(_m), 0, 0
            ps_muls1 g2, g4, g6
            psq_l  g4, 0x14(_m), 1, 0
            psq_l  g10, 0x10(_tm), 0, 0
            psq_l  g7, 0x18(_m), 0, 0
            psq_l  g8, 0x20(_m), 1, 0
            ps_madds0 g3, g5, g6, g3
            ps_muls1 g9, g7, g10
            psq_l  g7, 0x0(_tm), 0, 0
            ps_muls1 g8, g8, g10
            psq_l  g31, 0x1c(_tm), 0, 0
            psq_l  g11, 0x18(_m), 0, 0
            psq_l  g12, 0x20(_m), 1, 0
            ps_muls1 g13, g11, g31
            psq_l  g11, 0xc(_tm), 0, 0
            ps_muls1 g12, g12, g31
            psq_l  g30, 0x18(_tm), 0, 0
            ps_madds0 g2, g4, g6, g2
            ps_madds0 g9, g5, g10, g9
            ps_madds0 g8, g4, g10, g8
            ps_madds0 g13, g5, g31, g13
            ps_madds0 g12, g4, g31, g12
            ps_madds0 g3, g0, g7, g3
            ps_madds0 g2, g1, g7, g2
            ps_madds0 g9, g0, g11, g9
            psq_st g3, 0x0(_om), 0, 0
            ps_madds0 g8, g1, g11, g8
            ps_madds0 g13, g0, g30, g13
            psq_st g2, 0x8(_om), 1, 0
            ps_madds0 g12, g1, g30, g12
            psq_st g9, 0xc(_om), 0, 0
            psq_st g8, 0x14(_om), 1, 0
            psq_st g13, 0x18(_om), 0, 0
            psq_st g12, 0x20(_om), 1, 0
        }
    } else {
        // out.m = t.m * m  (aliased; compute rows into temps, then copy)
        float row0[3], row1[3], row2[3];
        register float *_r0 = row0;
        register float *_r1 = row1;
        register float *_r2 = row2;
        asm {
            psq_l  g6, 0x4(_tm), 0, 0
            psq_l  g5, 0x18(_m), 0, 0
            psq_l  g4, 0x20(_m), 1, 0
            ps_muls1 g3, g5, g6
            psq_l  g5, 0xc(_m), 0, 0
            ps_muls1 g2, g4, g6
            psq_l  g4, 0x14(_m), 1, 0
            psq_l  g10, 0x10(_tm), 0, 0
            psq_l  g7, 0x18(_m), 0, 0
            psq_l  g9, 0x20(_m), 1, 0
            ps_madds0 g3, g5, g6, g3
            ps_muls1 g8, g7, g10
            psq_l  g7, 0x0(_tm), 0, 0
            ps_muls1 g9, g9, g10
            psq_l  g30, 0x1c(_tm), 0, 0
            psq_l  g11, 0x18(_m), 0, 0
            psq_l  g12, 0x20(_m), 1, 0
            ps_muls1 g13, g11, g30
            psq_l  g11, 0xc(_tm), 0, 0
            ps_muls1 g12, g12, g30
            psq_l  g31, 0x18(_tm), 0, 0
            ps_madds0 g2, g4, g6, g2
            ps_madds0 g8, g5, g10, g8
            ps_madds0 g9, g4, g10, g9
            ps_madds0 g13, g5, g30, g13
            ps_madds0 g12, g4, g30, g12
            ps_madds0 g3, g0, g7, g3
            ps_madds0 g2, g1, g7, g2
            ps_madds0 g8, g0, g11, g8
            psq_st g3, 0x0(_r2), 0, 0
            ps_madds0 g9, g1, g11, g9
            psq_st g8, 0x0(_r1), 0, 0
            ps_madds0 g13, g0, g31, g13
            ps_madds0 g12, g1, g31, g12
            psq_st g13, 0x0(_r0), 0, 0
            psq_st g2, 0x8(_r2), 1, 0
            psq_st g9, 0x8(_r1), 1, 0
            psq_st g12, 0x8(_r0), 1, 0
        }
        out.m.x.x = row0[0];
        out.m.x.y = row0[1];
        out.m.x.z = row0[2];
        out.m.y.x = row1[0];
        out.m.y.y = row1[1];
        out.m.y.z = row1[2];
        out.m.z.x = row2[0];
        out.m.z.y = row2[1];
        out.m.z.z = row2[2];
    }
}
#else
inline void Multiply(const Transform &t, const Hmx::Matrix3 &m, Transform &out) {
    Multiply(t.v, m, out.v);
    Multiply(t.m, m, out.m);
}
#endif

INIT_REVS(InlineHelp)
float InlineHelp::sLastUpdatedTime;
float InlineHelp::sRotationTime;
float InlineHelp::sLabelRot;
bool InlineHelp::sHasFlippedTextThisRotation = 0;
bool InlineHelp::sNeedsTextUpdate = 0;
bool InlineHelp::sRotated = 0;

void InlineHelp::ActionElement::SetToken(Symbol s, bool secondary) {
    if (!secondary) {
        mPrimaryToken = s;
        mPrimaryStr = Localize(s, NULL);
    } else {
        mSecondaryToken = s;
        mSecondaryStr = Localize(s, NULL);
    }
}

void InlineHelp::ActionElement::SetString(const char *s, bool b) {
    if (!b) {
        mPrimaryToken = gNullStr;
        mPrimaryStr = s;
    } else {
        mSecondaryToken = gNullStr;
        mSecondaryStr = s;
    }
}

Symbol InlineHelp::ActionElement::GetToken(bool b) const {
    if (b)
        return mSecondaryToken;
    return mPrimaryToken;
}

const char *InlineHelp::ActionElement::GetText(bool b) const {
    if (b && HasSecondaryStr())
        return mSecondaryStr.c_str();
    return mPrimaryStr.c_str();
}

void InlineHelp::ActionElement::SetConfig(DataNode &dn, bool b) {
    if (dn.Type() == kDataArray) {
        DataArray *da = dn.Array();
        if (da->Size() == 0)
            return;
        FormatString fs(Localize(da->Sym(0), NULL));
        for (int i = 1; i < da->Size(); i++) {
            const DataNode &dn2 = da->Evaluate(i);
            if (dn2.Type() == kDataSymbol) {
                fs << Localize(dn2.Sym(), NULL);
            } else {
                fs << dn2;
            }
        }
        SetString(fs.Str(), b);
    } else {
        SetToken(dn.Sym(), b);
    }
}

void InlineHelp::Init() {
    Register();
    TheUI.InitResources("InlineHelp");
}

InlineHelp::InlineHelp()
    : mUseConnectedControllers(0), mHorizontal(1), mSpacing(0), unk_0x12C(0),
      mTextColor(this, NULL) {}

InlineHelp::~InlineHelp() {
    int siz = mTextLabels.size();
    for (int i = 0; i < siz; i++) {
        delete mTextLabels[i];
    }
}

BEGIN_COPYS(InlineHelp)
    CREATE_COPY_AS(InlineHelp, h)
    MILO_ASSERT(h, 129);
    COPY_SUPERCLASS_FROM(UIComponent, h)
    Update();
END_COPYS

void InlineHelp::CopyMembers(const UIComponent *o, Hmx::Object::CopyType ty) {
    UIComponent::CopyMembers(o, ty);
    CREATE_COPY_AS(InlineHelp, h);
    MILO_ASSERT(h, 139);
    COPY_MEMBER_FROM(h, mHorizontal)
    COPY_MEMBER_FROM(h, mSpacing)
    COPY_MEMBER_FROM(h, mConfig)
    COPY_MEMBER_FROM(h, mTextColor)
    COPY_MEMBER_FROM(h, mUseConnectedControllers)
    UpdateIconTypes(false);
}

SAVE_OBJ(InlineHelp, 158)

BEGIN_LOADS(InlineHelp)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void InlineHelp::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(4, 0)
    bs >> mHorizontal;
    bs >> mSpacing;
    bs >> mConfig;
    if (gRev >= 1)
        bs >> mTextColor;
    if (u16(gRev + 0xFFFE) <= 1) {
        int x;
        bs >> x;
    }
    if (gRev >= 3) {
        bs >> mUseConnectedControllers;
    }
    UIComponent::PreLoad(bs);
}

void InlineHelp::PostLoad(BinStream &bs) {
    UIComponent::PostLoad(bs);
    Update();
}

void InlineHelp::Enter() {
    UIComponent::Enter();
    UpdateIconTypes(true);
    SyncLabelsToConfig();
}

void InlineHelp::Poll() {
    UIComponent::Poll();
    float uisecs = TheTaskMgr.UISeconds();
#ifdef HX_NATIVE
    // Wave12 W4.3-C34 (C3 diagnosis): the flip-rotation clock is process-wide
    // static state shared by every InlineHelp instance. Log every branch so we
    // can see whether uisecs is actually advancing under RB3_FIXED_CLOCK and
    // whether sLabelRot ever escapes the formula's [-120,0]-equivalent range
    // (it should be mathematically impossible for it to reach +-180).
    static bool sHoldLabelDbg = getenv("RB3_HOLDLABEL_DBG") != NULL;
    if (sHoldLabelDbg) {
        fprintf(
            stderr,
            "[holdlabeldbg poll] this=%p uisecs=%.4f last=%.4f rotTime=%.4f "
            "labelRot=%.3f rotated=%d willUpdate=%d\n",
            (void *)this, uisecs, sLastUpdatedTime, sRotationTime, sLabelRot,
            (int)sRotated, (int)(uisecs != sLastUpdatedTime)
        );
    }
#endif
    if (uisecs != sLastUpdatedTime) {
        sNeedsTextUpdate = false;
        if (uisecs > sRotationTime) {
            float f1 = uisecs - sRotationTime;
            if (f1 >= 1.0f) {
                sHasFlippedTextThisRotation = false;
                sRotationTime = uisecs + 5.0f;
                SetLabelRotationPcts(0.0f);
            } else {
                if (!sHasFlippedTextThisRotation && f1 >= 0.5f) {
                    sHasFlippedTextThisRotation = true;
                    sRotated = sRotated == 0;
                    sNeedsTextUpdate = true;
                }
                SetLabelRotationPcts(f1);
            }
#ifdef HX_NATIVE
            if (sHoldLabelDbg) {
                fprintf(
                    stderr,
                    "[holdlabeldbg poll] f1=%.4f -> labelRot=%.3f rotated=%d\n",
                    f1, sLabelRot, (int)sRotated
                );
            }
#endif
        }
        sLastUpdatedTime = uisecs;
    }
    if (sNeedsTextUpdate)
        UpdateLabelText();
}

void InlineHelp::DrawShowing() {
    int numLabels = mTextLabels.size();
    Transform offsetXfm;
    Transform rotXfm;
    Transform labelXfm;
    Transform worldXfm = WorldXfm();
    const DataArray *t = TypeDef();
    MILO_ASSERT(t, 0x10F);

    offsetXfm.m.Identity();
    offsetXfm.v.Zero();

    if (sLabelRot != 0.0f) {
        Vector3 angles(DegreesToRadians(sLabelRot), 0.0f, 0.0f);
        Hmx::Matrix3 rotMtx;
        MakeRotMatrix(angles, rotMtx, true);
        Multiply(offsetXfm, rotMtx, rotXfm);
    } else {
        rotXfm.m.Identity();
        rotXfm.v.Zero();
    }

#ifdef HX_NATIVE
    static bool sHoldLabelDbg = getenv("RB3_HOLDLABEL_DBG") != NULL;
    if (sHoldLabelDbg) {
        const char *nm = Name();
        fprintf(
            stderr,
            "[holdlabeldbg draw] this=%p name='%s' numLabels=%d labelRot=%.3f "
            "localXfm.v=(%.2f,%.2f,%.2f) worldXfm.v=(%.2f,%.2f,%.2f)\n",
            (void *)this, nm ? nm : "?", numLabels, sLabelRot, LocalXfm().v.x,
            LocalXfm().v.y, LocalXfm().v.z, worldXfm.v.x, worldXfm.v.y, worldXfm.v.z
        );
    }
#endif
    for (int i = 0; i < numLabels; i++) {
        if (i > 0) {
            if (mHorizontal) {
                offsetXfm.v.x += mSpacing;
            } else {
                offsetXfm.v.z += mSpacing;
            }
        }
        Multiply(offsetXfm, worldXfm, labelXfm);
        bool hasSecondary = *mConfig[i].mSecondaryStr.c_str() != '\0';
        if (hasSecondary) {
            Multiply(rotXfm, labelXfm, labelXfm);
        }
#ifdef HX_NATIVE
        if (sHoldLabelDbg) {
            const Hmx::Matrix3 &m = labelXfm.m;
            float det = m.x.x * (m.y.y * m.z.z - m.y.z * m.z.y)
                - m.x.y * (m.y.x * m.z.z - m.y.z * m.z.x)
                + m.x.z * (m.y.x * m.z.y - m.y.y * m.z.x);
            fprintf(
                stderr,
                "[holdlabeldbg draw]   label[%d] hasSecondary=%d text='%s' "
                "labelXfm.v=(%.2f,%.2f,%.2f) det=%.4f\n",
                i, (int)hasSecondary, mConfig[i].GetText(sRotated),
                labelXfm.v.x, labelXfm.v.y, labelXfm.v.z, det
            );
        }
#endif
        mTextLabels[i]->SetWorldXfm(labelXfm);
        mTextLabels[i]->Draw();
    }
}

void InlineHelp::SetActionToken(JoypadAction a, DataNode &node) {
    bool found = false;
    for (std::vector<ActionElement>::iterator it = mConfig.begin(); it != mConfig.end();
         ++it) {
        if ((*it).mAction == a) {
            (*it).SetConfig(node, false);
            found = true;
            break;
        }
    }
    if (!found) {
        ActionElement el(a);
        el.SetConfig(node, false);
        mConfig.push_back(el);
    }
    SyncLabelsToConfig();
}

void InlineHelp::ClearActionToken(JoypadAction a) {
    for (std::vector<ActionElement>::iterator it = mConfig.begin(); it != mConfig.end();
         ++it) {
        if ((*it).mAction == a) {
            mConfig.erase(it);
            SyncLabelsToConfig();
            return;
        }
    }
}

BinStream &operator>>(BinStream &bs, InlineHelp::ActionElement &ae) {
    {
        int x;
        bs >> x;
        ae.mAction = (JoypadAction)x;
    }
    Symbol s;
    bs >> s;
    ae.SetToken(s, false);
    if (InlineHelp::gRev >= 2) {
        bs >> s;
        ae.SetToken(s, true);
    }
    return bs;
}

DataNode InlineHelp::OnSetConfig(const DataArray *da) {
    mConfig.clear();
    DataArray *arr = da->Array(2);
    for (int i = 0; i < arr->Size(); i++) {
        DataArray *loopArr = arr->Array(i);
        ActionElement el((JoypadAction)loopArr->Int(0));
#ifdef HX_NATIVE
        // The per-action help text/icon lives in the inner loopArr
        // (kAction primary [secondary]); the matched build's arr->Node(1/2) reads
        // the OUTER array and only happens to stay in-bounds when every action
        // shares the outer layout. The 360-ARK song_select help config passes a
        // single-element outer array (e.g. ((kAction_Confirm $primary $secondary)))
        // → arr->Node(1) is out of range and aborts. Read from loopArr (the
        // faithful per-action source) and bound-check it.
        if (loopArr->Size() > 1)
            el.SetConfig(loopArr->Node(1), false);
        if (loopArr->Size() > 2)
            el.SetConfig(loopArr->Node(2), true);
#else
        el.SetConfig(arr->Node(1), false);
        if (loopArr->Size() > 2)
            el.SetConfig(arr->Node(2), true);
#endif
        mConfig.push_back(el);
    }
    SyncLabelsToConfig();
    return DataNode(1);
}

void InlineHelp::UpdateTextColors() {
    for (std::vector<UILabel *>::iterator it = mTextLabels.begin();
         it != mTextLabels.end();
         ++it) {
        (*it)->SetColorOverride(mTextColor);
    }
}

void InlineHelp::Update() {
    UIComponent::Update();
    const DataArray *t = TypeDef();
    MILO_ASSERT(t, 0x187);
    RndDir *dir = mResource->Dir();
    MILO_ASSERT(dir, 0x18A);
    unk_0x12C = dir->Find<BandLabel>(t->FindStr(text_label), true);
    SyncLabelsToConfig();
}

void InlineHelp::UpdateIconTypes(bool b) {
    mIconTypes.clear();
    mIconTypes.push_back(vocals);
    mIconTypes.push_back(guitar);
    mIconTypes.push_back(drums);
    mIconTypes.push_back(keys);
}

void InlineHelp::SyncLabelsToConfig() {
    ResetRotation();
    int cfg_size = mConfig.size();
    int labels_size = mTextLabels.size();
    if (cfg_size > labels_size) {
        for (; labels_size < cfg_size; labels_size++) {
            BandLabel *lbl = Hmx::Object::New<BandLabel>();
            lbl->ResourceCopy(unk_0x12C);
            lbl->SetColorOverride(mTextColor);
            mTextLabels.push_back(lbl);
        }
    } else {
        if (labels_size > cfg_size) {
            for (int i = cfg_size; i < labels_size; i++) {
                delete mTextLabels[i];
            }
            mTextLabels.resize(cfg_size);
        }
    }
    UpdateLabelText();
}

String InlineHelp::GetIconStringFromAction(int idx) {
    String ret;
    const DataArray *t = TypeDef();
    MILO_ASSERT(t, 0x1C4);
    DataArray *actionArr = t->FindArray(action_chars);
    for (std::vector<Symbol>::iterator it = mIconTypes.begin(); it != mIconTypes.end(); ++it) {
        DataArray *itArr = actionArr->FindArray(*it);
        const char *str = itArr->Str(idx + 1);
        char c = *str;
        if (ret.find(c) == String::npos)
            ret += c;
    }
    return ret;
}

void InlineHelp::ResetRotation() {
    sRotated = 0;
    sHasFlippedTextThisRotation = 0;
    sRotationTime = TheTaskMgr.UISeconds() + 5.0f;
    SetLabelRotationPcts(0.0f);
}

void InlineHelp::SetLabelRotationPcts(float f) {
    if (f < 0.5f)
        sLabelRot = f * -240.0f;
    else
        sLabelRot = f * -240.0f - 120.0f;
}

void InlineHelp::UpdateLabelText() {
    int size = mConfig.size();
    for (int i = 0; i < size; i++) {
        String icon = GetIconStringFromAction(mConfig[i].mAction);
        if (icon.empty())
            mTextLabels[i]->SetTextToken(gNullStr);
        else
            mTextLabels[i]->SetTokenFmt(
                inline_help_fmt, icon.c_str(), mConfig[i].GetText(sRotated)
            );
    }
}

BEGIN_HANDLERS(InlineHelp)
    HANDLE_ACTION(
        set_action_token, SetActionToken((JoypadAction)_msg->Int(2), _msg->Node(3))
    )
    HANDLE_ACTION(clear_action_token, ClearActionToken((JoypadAction)_msg->Int(2)))
    HANDLE(set_config, OnSetConfig)
    HANDLE_SUPERCLASS(UIComponent)
    HANDLE_CHECK(0x201)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(InlineHelp::ActionElement)
    SYNC_PROP(action, (int &)o.mAction)
    SYNC_PROP_SET(text_token, o.GetToken(false), o.SetToken(_val.Sym(), false))
    SYNC_PROP_SET(secondary_token, o.GetToken(true), o.SetToken(_val.Sym(), true))
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(InlineHelp)
    SYNC_PROP_MODIFY_ALT(config, mConfig, SyncLabelsToConfig())
    SYNC_PROP(horizontal, mHorizontal)
    SYNC_PROP(spacing, mSpacing)
    SYNC_PROP_MODIFY_ALT(text_color, mTextColor, UpdateTextColors())
    SYNC_PROP(use_connected_controllers, mUseConnectedControllers)
    SYNC_SUPERCLASS(UIComponent)
END_PROPSYNCS
