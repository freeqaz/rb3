#include "char/CharIKHand.h"
#include <cstdlib>
#include <cstring>
#include "decomp.h"
#include "math/Color.h"
#include "math/Rot.h"
#include "math/Vec.h"
#include "obj/ObjMacros.h"
#include "rndobj/Rnd.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "utl/Symbols.h"

INIT_REVS(CharIKHand)

CharIKHand::CharIKHand()
    : mHand(this), mFinger(this), mTargets(this), mOrientation(1), mStretch(1),
      mScalable(0), mMoveElbow(1), mElbowSwing(0.0f), mAlwaysIKElbow(0), mAAPlusBB(0.0f),
      mConstrainWrist(0), mWristRadians(0.0f), mElbowCollide(this), mClockwise(0) {}

CharIKHand::~CharIKHand() {}

#ifdef HX_NATIVE
// W26-PROP (default-OFF flag RB3_PROP_POSE): instrument-prop IK-target redirect.
//
// DISCRIMINATOR (STEP 0, evidence/step0-ikprop.log): the guitar/drum playing-hand
// IK targets are *tip* bones (bone_pick_strum, bone_[RL]-tip_<piece>) whose parent
// is a correctly-posed authored "at-hand" frame (bone_target_strum / bone_target_
// <piece>), but the tip carries a large STATIC LocalXfm offset (|local|~48-51u,
// e.g. bone_pick_strum LocalXfm.v=(2.28,-48.90,-15.29)) that the animation clip is
// supposed to drive down to the strings/head each beat. On native that prop-bone
// clip track is never bound, so the tip stays at its rest offset and flings the IK
// target far past the arm's reach (d_hand~51u vs reach~20u) -> the RB3_IK_REACH_CLAMP
// safety net then clip-poses the arm (dormant-IK look). The chain is CORRECT
// (IK_ROOTCMP same=1); this is a CLIP-BINDING gap, not an attach/proxy gap.
//
// FIX: when the resolved IK target's TransParent is a `bone_target_*` frame that is
// itself meaningfully CLOSER to the hand than the tip (proving the parent is the
// correctly-posed at-hand frame and the tip's own local offset is the fault),
// redirect the IK destination to that parent frame. This restores an in-reach IK
// target (the clamp goes dormant) WITHOUT touching any target whose parent is not
// already at the hand (the vocalist mic case, where the whole prop chain is
// displaced, is deliberately NOT matched). Strictly scoped + default-OFF; the Wii
// object is byte-identical (whole thing is #ifdef HX_NATIVE + env-gated).
//
// A5-i safety: this only ever fires when the *tip* target is out of reach
// (d_tip_hand > d_parent_hand, and parent is closer) — it can never pull an
// already-in-reach target away, because an in-reach tip is never redirected.
static RndTransformable *sPropPoseRedirect(RndTransformable *tgt,
                                           RndTransformable *hand, float reach) {
    static int sOn = -1;
    static int sDbg = 0;
    if (sOn < 0) {
        const char *e = getenv("RB3_PROP_POSE");
        // E7 (W26 close-out): require a non-empty non-'0' value (RB3_PROP_POSE=""
        // previously enabled). Default-OFF; unset (e==NULL) stays 0.
        sOn = (e && e[0] && e[0] != '0') ? 1 : 0;
        sDbg = getenv("RB3_PROP_POSE_DBG") ? 1 : 0;
    }
    if (!sOn || !tgt || !hand)
        return tgt;
    RndTransformable *par = tgt->TransParent();
    if (!par)
        return tgt;
    const char *pn = par->Name();
    // Scope: parent must be an authored instrument at-hand target frame.
    if (!pn || std::strncmp(pn, "bone_target_", 12) != 0)
        return tgt;
    const Vector3 &hp = hand->WorldXfm().v;
    const Vector3 &tp = tgt->WorldXfm().v;
    const Vector3 &pp = par->WorldXfm().v;
    float dTip = (tp.x - hp.x) * (tp.x - hp.x) + (tp.y - hp.y) * (tp.y - hp.y)
        + (tp.z - hp.z) * (tp.z - hp.z);
    float dPar = (pp.x - hp.x) * (pp.x - hp.x) + (pp.y - hp.y) * (pp.y - hp.y)
        + (pp.z - hp.z) * (pp.z - hp.z);
    // Only redirect a tip that is OUT of reach AND whose parent is closer to the
    // hand than the tip (the clip-binding-fling signature). An in-reach tip
    // (dTip <= reach^2) is left untouched -> A5-i holds.
    float r2 = reach > 0.0f ? reach * reach : 0.0f;
    if (dTip <= r2 || dPar >= dTip)
        return tgt;
    if (sDbg) {
        fprintf(stderr,
            "[PROP_POSE] redirect tgt='%s' -> parent='%s' dTip=%.1f dPar=%.1f reach=%.1f\n",
            tgt->Name() ? tgt->Name() : "?", pn,
            std::sqrt(dTip), std::sqrt(dPar), reach);
    }
    return par;
}
#endif

#pragma push
#pragma dont_inline on
// fn_804E02E4 - https://decomp.me/scratch/5zJNZ
void CharIKHand::Poll() {
#ifdef HX_NATIVE
    { static int g=-1; if(g<0)g=getenv("RB3_NO_IK")?1:0; if(g)return; }
#endif
    float charWeight = Weight();
    RndTransformable *trans = mHand;
    if (!trans || mTargets.empty())
        return;
    Vector3 vec(0.0f, 0.0f, 0.0f);
    Hmx::Quat quat(0.0f, 0.0f, 0.0f, 0.0f);
#ifdef HX_NATIVE
    // V32 diagnostic — gated, render-inert. Reports hand world position,
    // each target's name + world pos + distance, and the parent (proxy)
    // chain. Investigative tool for the residual crowd hand-IK "target ~300u
    // away" pose error documented in VENUE_RENDER V26 / V32. NOTE: in the
    // current build state CharIKHand::Poll() is NOT exercised in the menu /
    // song-select / pre-game-screen phases (no IK_TGT lines on a 12000-frame
    // run); diagnostic stays for the next agent who reaches in-song
    // crowd-cinematic gameplay where the IKHand path is actually hit.
    static int sDbgCount = 0;
    static const char* sIkDbg = getenv("IK_TGT_DBG");
    if (sIkDbg && sDbgCount < 200) {
        const Vector3& hp = trans->WorldXfm().v;
        for (ObjVector<IKTarget>::iterator it = mTargets.begin();
             it != mTargets.end(); ++it) {
            RndTransformable* t = it->mTarget;
            if (!t) continue;
            const Vector3& tp = t->WorldXfm().v;
            float dx = tp.x - hp.x, dy = tp.y - hp.y, dz = tp.z - hp.z;
            float d = std::sqrt(dx*dx + dy*dy + dz*dz);
            if (d > 50.0f) {
                const char* tname = t->Name();
                RndTransformable* tp_parent = t->TransParent();
                const char* pname = tp_parent ? tp_parent->Name() : "(null)";
                fprintf(stderr,
                    "[IK_TGT] ikhand='%s' hand='%s' wpos=(%.1f,%.1f,%.1f) "
                    "tgt='%s' twpos=(%.1f,%.1f,%.1f) d=%.1f parent='%s' "
                    "w=%.3f reach=%.2f\n",
                    Name(), trans->Name(), hp.x, hp.y, hp.z,
                    tname, tp.x, tp.y, tp.z, d, pname, charWeight, mAAPlusBB);
                sDbgCount++;
                if (sDbgCount >= 200) break;
            }
        }
    }
    // W25-FOREARM discriminator (H-A/H-B/H-C): walk the FULL TransParent
    // chain-to-root for BOTH the IK hand bone and the first far target bone,
    // print each node's world pos and the terminal root object. If the two
    // chains bottom out at DIFFERENT world roots (~100u apart) -> H-A/H-C
    // (target parented in the wrong frame / mis-resolved proxy). If they share
    // a root but the arm rest is already high pre-IK -> H-B. Gated, inert by
    // default.
    static int sRootCount = 0;
    static const char* sRootDbg = getenv("IK_ROOTCMP");
    if (sRootDbg && sRootCount < 40 && !mTargets.empty()) {
        RndTransformable* tgt0 = 0;
        for (ObjVector<IKTarget>::iterator it = mTargets.begin();
             it != mTargets.end(); ++it) {
            if (it->mTarget) { tgt0 = it->mTarget; break; }
        }
        if (tgt0) {
            const Vector3& hp = trans->WorldXfm().v;
            const Vector3& tp = tgt0->WorldXfm().v;
            float dd = std::sqrt((tp.x-hp.x)*(tp.x-hp.x) + (tp.y-hp.y)*(tp.y-hp.y)
                                 + (tp.z-hp.z)*(tp.z-hp.z));
            if (dd > 50.0f) {
                sRootCount++;
                fprintf(stderr, "[IK_ROOTCMP] ikhand='%s' d=%.1f\n", Name(), dd);
                // hand chain
                RndTransformable* n = trans;
                int depth = 0;
                RndTransformable* handRoot = trans;
                while (n && depth < 24) {
                    const Vector3& w = n->WorldXfm().v;
                    RndTransformable* par = n->TransParent();
                    fprintf(stderr,
                        "[IK_ROOTCMP]   HAND depth=%d node='%s' world=(%.2f,%.2f,%.2f) parent='%s'\n",
                        depth, n->Name(), w.x, w.y, w.z, par ? par->Name() : "(root)");
                    handRoot = n;
                    if (!par) break;
                    n = par; depth++;
                }
                // target chain
                n = tgt0; depth = 0;
                RndTransformable* tgtRoot = tgt0;
                while (n && depth < 24) {
                    const Vector3& w = n->WorldXfm().v;
                    RndTransformable* par = n->TransParent();
                    fprintf(stderr,
                        "[IK_ROOTCMP]   TGT  depth=%d node='%s' world=(%.2f,%.2f,%.2f) parent='%s'\n",
                        depth, n->Name(), w.x, w.y, w.z, par ? par->Name() : "(root)");
                    tgtRoot = n;
                    if (!par) break;
                    n = par; depth++;
                }
                const Vector3& hr = handRoot->WorldXfm().v;
                const Vector3& tr = tgtRoot->WorldXfm().v;
                fprintf(stderr,
                    "[IK_ROOTCMP]   ROOTS hand_root='%s' (%.2f,%.2f,%.2f) tgt_root='%s' (%.2f,%.2f,%.2f) same=%d\n",
                    handRoot->Name(), hr.x, hr.y, hr.z,
                    tgtRoot->Name(), tr.x, tr.y, tr.z, (handRoot == tgtRoot) ? 1 : 0);
            }
        }
    }
    // W26-PROP discriminator (A4): separate the (a) PARENT-CHAIN gap from the
    // (c) CLIP-BINDING gap. IK_ROOTCMP already proved same=1 (roots match) —
    // so the chain resolves to the correct member root, refuting the proxy-root
    // gap. What remains: is the far target's LOCAL transform relative to its
    // (correctly-posed) parent the thing that flings it out of reach? Dump the
    // far bone's LocalXfm.v (offset from parent) + the parent's world + LocalXfm.
    //   correct root, but far bone LocalXfm.v large        => clip-binding (c):
    //       the prop bone's local rest is a static authored pose never animated
    //       to the playing position (the parent tracks the hand, the tip does not).
    //   parent world already far / root mismatch           => parent-chain (a).
    static int sPropCount = 0;
    static const char* sPropDbg = getenv("IK_PROP_DBG");
    if (sPropDbg && sPropCount < 60 && !mTargets.empty()) {
        RndTransformable* tgt0 = 0;
        for (ObjVector<IKTarget>::iterator it = mTargets.begin();
             it != mTargets.end(); ++it) {
            if (it->mTarget) { tgt0 = it->mTarget; break; }
        }
        if (tgt0) {
            const Vector3& hp = trans->WorldXfm().v;
            const Vector3& tw = tgt0->WorldXfm().v;
            float dd = std::sqrt((tw.x-hp.x)*(tw.x-hp.x) + (tw.y-hp.y)*(tw.y-hp.y)
                                 + (tw.z-hp.z)*(tw.z-hp.z));
            if (dd > 50.0f) {
                sPropCount++;
                const Vector3& tl = tgt0->LocalXfm().v;
                float locLen = std::sqrt(tl.x*tl.x + tl.y*tl.y + tl.z*tl.z);
                RndTransformable* par = tgt0->TransParent();
                if (par) {
                    const Vector3& pw = par->WorldXfm().v;
                    const Vector3& pl = par->LocalXfm().v;
                    // distance from PARENT world to the hand: if the parent is
                    // AT the hand (small) but the tip is far, the fault is the
                    // tip's LocalXfm (clip-binding) not the chain (attach).
                    float pdd = std::sqrt((pw.x-hp.x)*(pw.x-hp.x) + (pw.y-hp.y)*(pw.y-hp.y)
                                          + (pw.z-hp.z)*(pw.z-hp.z));
                    fprintf(stderr,
                        "[IK_PROP] ikhand='%s' hand='%s' hwpos=(%.1f,%.1f,%.1f) "
                        "tgt='%s' d_hand=%.1f tgtLocal=(%.2f,%.2f,%.2f) |local|=%.1f "
                        "parent='%s' pworld=(%.1f,%.1f,%.1f) pLocal=(%.2f,%.2f,%.2f) "
                        "d_parent_hand=%.1f w=%.3f reach=%.2f\n",
                        Name(), trans->Name(), hp.x, hp.y, hp.z,
                        tgt0->Name(), dd, tl.x, tl.y, tl.z, locLen,
                        par->Name(), pw.x, pw.y, pw.z, pl.x, pl.y, pl.z,
                        pdd, charWeight, mAAPlusBB);
                } else {
                    fprintf(stderr,
                        "[IK_PROP] ikhand='%s' tgt='%s' d_hand=%.1f "
                        "tgtLocal=(%.2f,%.2f,%.2f) |local|=%.1f parent=(root) "
                        "w=%.3f reach=%.2f\n",
                        Name(), tgt0->Name(), dd, tl.x, tl.y, tl.z, locLen,
                        charWeight, mAAPlusBB);
                }
            }
        }
    }
#endif
    UpdateHand();
    if (mTargets.size() == 1) {
        RndTransformable *frontTrans = mTargets.front().mTarget;
        if (frontTrans) {
#ifdef HX_NATIVE
            frontTrans = sPropPoseRedirect(frontTrans, trans, mAAPlusBB);
#endif
            vec = frontTrans->WorldXfm().v;
            if (mOrientation) {
                Hmx::Matrix3 mtx;
                Normalize(frontTrans->WorldXfm().m, mtx);
                quat.Set(mtx);
            }
        }
    } else {
        // W27-PROP-PROBE nit (W26 close-out review ii): when the RB3_PROP_POSE
        // redirect is active it is applied only to the world-accumulation loop
        // below (:299-317, sPropPoseRedirect on itTrans), NOT to this weight loop,
        // which still derives each target's blend weight from the UN-redirected
        // tip LocalXfm (line below). That is acceptable for the discriminator /
        // honest-partial redirect (weights are approximate) but would be wrong for
        // a real fix — a genuine binding fix must redirect the target BEFORE
        // computing its weight so weight and world position agree. (Comment-only;
        // codegen-inert, Wii object byte-identical.)
        float startlocfloats[16];
        float *locfloats = startlocfloats;
        float sumfloat = 0.0f;
        for (std::vector<IKTarget>::iterator it = mTargets.begin(); it != mTargets.end();
             it++) {
            RndTransformable *itTrans = (*it).mTarget;
            float itExtent = (*it).mExtent;
            if (itTrans) {
                Vector3 vec(itTrans->LocalXfm().v);
                if (itExtent > 0.0f) {
                    if (itExtent < -vec.z) {
                        *locfloats = 0.001f;
                    } else {
                        vec.z = 0.0f;
                        *locfloats = 144.0f / Max(0.001f, LengthSquared(vec));
                    }
                } else {
                    *locfloats = 144.0f / Max(0.001f, LengthSquared(vec));
                }

                sumfloat += *locfloats++;
            }
        }
        if (sumfloat < 1.0f) {
            charWeight = charWeight - (charWeight * (1.0f - sumfloat));
        }

        locfloats = startlocfloats;
        for (std::vector<IKTarget>::iterator it = mTargets.begin(); it != mTargets.end();
             it++) {
            RndTransformable *itTrans = (*it).mTarget;
            if (itTrans) {
#ifdef HX_NATIVE
                itTrans = sPropPoseRedirect(itTrans, trans, mAAPlusBB);
#endif
                float curFloat = *locfloats;
                const Transform &worldtf = itTrans->WorldXfm();
                ScaleAddEq(vec, worldtf.v, curFloat / sumfloat);
                if (mOrientation) {
                    Hmx::Matrix3 m100;
                    Normalize(worldtf.m, m100);
                    Hmx::Quat q268(m100);
                    ScaleAddEq(quat, q268, curFloat / sumfloat);
                }
            }
            locfloats++;
        }
        if (mOrientation)
            Normalize(quat, quat);
    }
    RndTransformable *parent2 = 0;
#ifdef HX_NATIVE
    // W27-PROP-PROBE (a): env-gated bypass of the mFinger finger-compensation
    // re-projection, to A/B-test the E7 inference that this re-projection is what
    // keeps RB3_IK_REACH_CLAMP non-dormant even with RB3_PROP_POSE. Probe-only,
    // default-OFF; when unset the condition is exactly `if (mFinger)` as on Wii,
    // so the Wii object is byte-identical (whole gate is #ifdef HX_NATIVE).
    static int sFingerBypass = -1;
    if (sFingerBypass < 0) {
        const char *e = getenv("RB3_PROP_FINGER_BYPASS");
        sFingerBypass = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    if (mFinger && !sFingerBypass) {
#else
    if (mFinger) {
#endif
        Transform tf;
        tf.v = vec;
        MakeRotMatrix(quat, tf.m);
        Transform tf2;
        Invert(mFinger->WorldXfm(), tf2);
        Multiply(mHand->WorldXfm(), tf2, tf2);
        Multiply(tf2, tf, tf);
        vec = tf.v;
        quat.Set(tf.m);
    }
    Interp(mHand->WorldXfm().v, vec, charWeight, mWorldDst);
#ifdef HX_NATIVE
    {
        static const char *sDstDbg = getenv("RB3_PROP_DST_DBG");
        static int sDstN = 0;
        if (sDstDbg && sDstN < 120 && mHand) {
            const Vector3 &hw = mHand->WorldXfm().v;
            float dd = std::sqrt((mWorldDst.x - hw.x) * (mWorldDst.x - hw.x)
                + (mWorldDst.y - hw.y) * (mWorldDst.y - hw.y)
                + (mWorldDst.z - hw.z) * (mWorldDst.z - hw.z));
            if (dd > 30.0f) {
                sDstN++;
                fprintf(stderr,
                    "[PROP_DST] ikhand='%s' finger=%d dst_from_hand=%.1f reach=%.2f\n",
                    Name(), mFinger ? 1 : 0, dd, mAAPlusBB);
            }
        }
    }
#endif
#ifdef HX_NATIVE
    // W25-FOREARM: reach-aware IK clamp (flag-gated, default-OFF). Wii object is
    // byte-identical (whole block is #ifdef HX_NATIVE + env-gated). The in-song
    // band-arm spike-fan is CharIKHand running at full weight toward an
    // instrument-tip target that sits FAR beyond the arm's reachable radius
    // (measured: target d=54-87u vs mAAPlusBB reach=20.3u). IKElbow then
    // over-rotates the upperArm into the visible fan. This is NOT a member/frame
    // resolution error (the target's chain-to-root shares the member root) and
    // NOT a decomp infidelity (Poll math is faithful, bank_divergence=TRUST) — it
    // is the IK being commanded past its kinematic limit.
    //
    // GRADUATED response, keyed on how far past reach the target is:
    //   d <= reach            : reachable -> NO-OP (correct fret/strum/drum posing
    //                           is untouched; this is the common in-tune case).
    //   reach < d <= k*reach  : moderately far -> clamp mWorldDst onto the
    //                           shoulder-centred reach sphere (radius mAAPlusBB).
    //                           A real arm at a just-out-of-reach point extends
    //                           straight toward it; this bounds the upperArm fling.
    //   d > k*reach           : grossly unreachable (measured up to 273u vs 20u
    //                           reach) -> the target is almost certainly a
    //                           mis-authored / not-yet-resolved instrument tip;
    //                           reaching it AT ALL only produces the screen-fan.
    //                           Neutralise the IK displacement for this hand by
    //                           targeting its CURRENT world pos (= leave the clip's
    //                           own pose, the RB3_NO_IK-correct fallback) — but only
    //                           for THIS pathological hand, keeping IK for the rest.
    // k defaults to 2.0 (override RB3_IK_REACH_K). Shoulder pivot =
    // mHand->TransParent()->TransParent() (upperArm world), matching the reach
    // origin PullShoulder/IKElbow use. Whole block is #ifdef HX_NATIVE + env-gated
    // (default-OFF) so the Wii object is byte-identical.
    {
        static int sReachClamp = -1;
        static float sReachK = 2.0f;
        if (sReachClamp < 0) {
            // Coordinator flip (Wave-25 close-out, E1-confirmed + review-endorsed):
            // default-ON, opt-out-wins. Default now clamps over-reach IK;
            // RB3_IK_REACH_CLAMP_OFF=1 (or RB3_IK_REACH_CLAMP=0) restores raw over-reach
            // (spike-fan). 14th default-ON. Wii object byte-identical (HX_NATIVE).
            if (getenv("RB3_IK_REACH_CLAMP_OFF")) sReachClamp = 0;
            else {
                const char *e = getenv("RB3_IK_REACH_CLAMP");
                sReachClamp = (e && e[0] == '0') ? 0 : 1;
            }
            const char *ke = getenv("RB3_IK_REACH_K");
            if (ke) { float v = (float)atof(ke); if (v >= 1.0f) sReachK = v; }
        }
        if (sReachClamp && mMoveElbow && mAAPlusBB > 0.0f && mHand) {
            RndTransformable *fa = mHand->TransParent();
            RndTransformable *sh = fa ? fa->TransParent() : 0;
            if (sh) {
                const Vector3 &shoulder = sh->WorldXfm().v;
                Vector3 toDst;
                Subtract(mWorldDst, shoulder, toDst);
                float distSq = LengthSquared(toDst);
                float reach = mAAPlusBB;
                if (distSq > reach * reach && distSq > 1e-8f) {
                    float dist = std::sqrt(distSq);
                    const char* sClampDbg = getenv("RB3_IK_CLAMP_DBG");
                    static int sClampN = 0;
                    const char *mode;
                    if (dist > sReachK * reach) {
                        // grossly unreachable: keep the clip pose (neutralise IK
                        // displacement for this hand).
                        mWorldDst = mHand->WorldXfm().v;
                        mode = "skip";
                    } else {
                        // moderately far: clamp to the reach sphere.
                        float scale = reach / dist;
                        mWorldDst.x = shoulder.x + toDst.x * scale;
                        mWorldDst.y = shoulder.y + toDst.y * scale;
                        mWorldDst.z = shoulder.z + toDst.z * scale;
                        mode = "clamp";
                    }
                    if (sClampDbg && sClampN < 300) {
                        sClampN++;
                        fprintf(stderr,
                            "[IK_CLAMP] ikhand='%s' preDist=%.1f reach=%.2f mode=%s\n",
                            Name(), dist, reach, mode);
                    }
                }
            }
        }
    }
#endif
    RndTransformable *parent1 = mHand->TransParent();
    if (!mMoveElbow)
        parent1 = 0;
    if (charWeight != 0 || mAlwaysIKElbow) {
        if (parent1) {
            parent2 = parent1->TransParent();
            if (!parent2)
                parent1 = 0;
        }
        IKElbow(parent1, parent2);
    }
    if (charWeight != 0) {
        if ((!parent1 || mOrientation || mStretch)) {
        Transform tf(mHand->WorldXfm());
        if (!parent1 || mStretch) {
            tf.v = mWorldDst;
        }
        if (mOrientation) {
            if (charWeight < 1.0f) {
                Hmx::Quat q(mHand->WorldXfm().m);
                Interp(q, quat, charWeight, quat);
            }
            MakeRotMatrix(quat, tf.m);
        }
        mHand->SetWorldXfm(tf);
    }
    }

    if (mConstrainWrist && charWeight > 0.0f && parent1) {
        Vector3 v284(mFinger->WorldXfm().v);
        Hmx::Matrix3 m1b4(parent1->WorldXfm().m);
        Hmx::Matrix3 m1d8(mHand->WorldXfm().m);
        Vector3 v290, v29c, v2a8;
        v290 = m1d8.x;
        v29c = m1d8.y;
        v2a8 = m1d8.z;
        float acosdot = std::acos(Dot(m1b4.x, v2a8)) - 1.570796370506287f;
        float rads = mWristRadians;
        if (Abs(acosdot) > rads) {
            if (acosdot > 0.0f)
                acosdot -= rads;
            else
                acosdot += rads;
            Hmx::Quat q2b8;
            Transform tf208;
            Hmx::Matrix3 m22c;
            q2b8.Set(v29c, acosdot);
            MakeRotMatrix(q2b8, m22c);
            Multiply(v290, m22c, v290);
            Cross(v290, v29c, v2a8);
            tf208.m.Set(v290, v29c, v2a8);
            tf208.v = mHand->WorldXfm().v;
            mHand->SetWorldXfm(tf208);
            Vector3 v2c8(mFinger->WorldXfm().v);
            Subtract(v2c8, v284, v2c8);
            Subtract(tf208.v, v2c8, tf208.v);
            mHand->SetWorldXfm(tf208);
            mWorldDst = tf208.v;
            IKElbow(parent1, parent2);
            mHand->SetWorldXfm(tf208);
        }
    }
}

// fn_804E09B4 - https://decomp.me/scratch/X8Imr
void CharIKHand::IKElbow(RndTransformable *trans1, RndTransformable *trans2) {
    if (!trans1 || !trans2)
        return;
    Vector3 v100;
    PullShoulder(v100, trans2->WorldXfm(), mWorldDst, mAAPlusBB);
    Transform tf78(trans2->WorldXfm());
    tf78.v += v100;
    trans2->SetWorldXfm(tf78);
    float loc210 = mInv2ab * (DistanceSquared(trans2->WorldXfm().v, mWorldDst) - mAABB);
    ClampEq(loc210, -1.0f, 1.0f);
    float sqrted = -std::sqrt(-(loc210 * loc210 - 1.0f));
    float negSqrted = -sqrted;
    trans1->DirtyLocalXfm().m.Set(loc210, sqrted, 0, negSqrted, loc210, 0, 0, 0, 1);
    Vector3 v10c;
    Vector3 v118;
    Multiply(trans2->WorldXfm(), mHand->WorldXfm().v, v118);
    Multiply(trans2->WorldXfm(), mWorldDst, v10c);
    if (mElbowSwing > 0) {
        Vector2 v200(v118.y, v118.z);
        Vector2 v208(v10c.y, v10c.z);
        float max208 = Max(LengthSquared(v208), 16.0f);
        float max200 = Max(LengthSquared(v200), 16.0f);
        float sqrted2 = std::sqrt(max200 * max208);
        float crossed = Cross(v208, v200);
        float negSwing = -mElbowSwing;
        float clamped = Clamp(negSwing, mElbowSwing, crossed / sqrted2);
        Transform &dirty_temp = trans1->DirtyLocalXfm();
        RotateAboutX(dirty_temp.m, -clamped, dirty_temp.m);
        Multiply(trans2->WorldXfm(), mHand->WorldXfm().v, v118);
    }
    Hmx::Quat q128;
    MakeRotQuat(v118, v10c, q128);
    Hmx::Matrix3 ma0;
    MakeRotMatrix(q128, ma0);
    Multiply(ma0, trans2->LocalXfm().m, trans2->DirtyLocalXfm().m);
    if (mElbowCollide) {
        PullShoulder(v100, trans2->WorldXfm(), mWorldDst, mAAPlusBB);
        Transform tfd0(trans2->WorldXfm());
        tfd0.v += v100;
        trans2->SetWorldXfm(tfd0);
        if (mElbowCollide->GetShape() != CharCollide::kSphere)
            MILO_WARN("%s: elbow collision object not sphere.\n", Name());
        else {
            Vector3 v134(mElbowCollide->WorldXfm().v);
            float sphereRadius = mElbowCollide->Radius();
            if (Distance(v134, trans1->WorldXfm().v) < sphereRadius) {
                Vector3 v140(trans2->WorldXfm().v);
                v140 -= mWorldDst;
                Vector3 v14c, v158;
                Normalize(v140, v158);
                Subtract(trans1->WorldXfm().v, mWorldDst, v14c);
                Scale(v158, Dot(v14c, v158), v140);
                Add(v140, mWorldDst, v140);
                Vector3 v164(trans1->WorldXfm().v);
                v164 -= v140;
                float v164len = Length(v164);
                Vector3 v170(trans2->WorldXfm().v);
                v170 -= v140;
                Normalize(v170, v170);
                Vector3 v17c;
                Add(v140, v170, v17c);
                Subtract(v140, v134, v17c);
                Scale(v170, Dot(v170, v17c), v17c);
                Add(v134, v17c, v17c);
                float a = Distance(v17c, v134);
                MILO_ASSERT(a <= sphereRadius, 0x1A1);
                float sqrted2 = std::sqrt(sphereRadius * sphereRadius - a * a);
                v134.Set(v17c.x, v17c.y, v17c.z);
                float dist134and140 = Distance(v134, v140);
                float d10 = (dist134and140 * dist134and140 + -(v164len * v164len - sqrted2 * sqrted2))
                    / (dist134and140 * 2.0f);
                float sqrted3 = std::sqrt(-(d10 * d10 - sqrted2 * sqrted2));
                float asined = std::asin(sqrted3 / v164len);
                if (IsNaN(asined))
                    return;
                Vector3 v188(v134);
                v188 -= v140;
                Normalize(v188, v188);
                Scale(v188, v164len, v188);
                float sine, cosine;
                {
                    double half = asined / 2.0;
                    sine = sin(half);
                    cosine = cos(half);
                }
                Hmx::Quat q198(v188.x, v188.y, v188.z, 0.0f);
                Hmx::Quat q1a8(v170.x * sine, v170.y * sine, v170.z * sine, cosine);
                Hmx::Quat q1b8;
                Multiply(q198, q1a8, q1b8);
                Multiply(q1b8, q1a8, q1b8);
                Vector3 v1c4(q1b8.x, q1b8.y, q1b8.z);
                Add(v1c4, v140, v1c4);
                Multiply(q1a8, q198, q1b8);
                Multiply(q1a8, q1b8, q1b8);
                Vector3 v1d0(q1b8.x, q1b8.y, q1b8.z);
                Add(v1d0, v140, v1d0);
                Vector3 v1dc, v1e8;
                Multiply(trans2->WorldXfm(), trans1->WorldXfm().v, v1e8);
                if (mClockwise)
                    Multiply(trans2->WorldXfm(), v1d0, v1dc);
                else
                    Multiply(trans2->WorldXfm(), v1c4, v1dc);
                Hmx::Quat q1f8;
                MakeRotQuat(v1e8, v1dc, q1f8);
                Hmx::Matrix3 mf4;
                MakeRotMatrix(q1f8, mf4);
                Multiply(mf4, trans2->LocalXfm().m, trans2->DirtyLocalXfm().m);
                Multiply(trans1->WorldXfm(), mHand->WorldXfm().v, v1e8);
                Multiply(trans1->WorldXfm(), mWorldDst, v1dc);
                MakeRotQuat(v1e8, v1dc, q1f8);
                MakeRotMatrix(q1f8, mf4);
                Multiply(mf4, trans1->LocalXfm().m, trans1->DirtyLocalXfm().m);
            }
        }
    }
    PullShoulder(v100, trans2->WorldXfm(), mWorldDst, mAAPlusBB);
    tf78 = trans2->WorldXfm();
    tf78.v += v100;
    trans2->SetWorldXfm(tf78);
}
#pragma pop

void CharIKHand::PullShoulder(
    Vector3 &v, const Transform &tf, const Vector3 &vconst, float fff
) {
    Subtract(vconst, tf.v, v);
    float lensq = LengthSquared(v);
    float f2 = fff * 0.95f;
    if (lensq > f2 * f2) {
        v *= 1.0f - f2 / std::sqrt(lensq);
    } else
        v.Zero();
}

void CharIKHand::SetHand(RndTransformable *t) {
    mHand = t;
    mHandChanged = true;
}

void CharIKHand::UpdateHand() {
    if (mScalable || mHandChanged) {
        MeasureLengths();
        mHandChanged = false;
    }
}

void CharIKHand::MeasureLengths() {
    if (mHand) {
        if (mHand->TransParent()) {
            if (mHand->TransParent()->TransParent()) {
                float len = Length(mHand->mLocalXfm.v);
                float parentlen = Length(mHand->TransParent()->mLocalXfm.v);
                mInv2ab = parentlen * 2.0f * len;
                if (mInv2ab != 0.0f)
                    mInv2ab = 1.0f / mInv2ab;
                mAABB = (parentlen * parentlen) + len * len;
                mAAPlusBB = len + parentlen;
            }
        }
    }
}

void CharIKHand::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(mHand);
    changedBy.push_back(mHand);
    change.push_back(mFinger);
    changedBy.push_back(mFinger);
    for (ObjVector<IKTarget>::iterator it = mTargets.begin(); it != mTargets.end();
         ++it) {
        changedBy.push_back(it->mTarget);
    }
    if (mMoveElbow && mHand) {
        RndTransformable *handParent = mHand->TransParent();
        if (handParent) {
            change.push_back(handParent);
            changedBy.push_back(handParent);
            handParent = handParent->TransParent();
            if (handParent) {
                change.push_back(handParent);
                changedBy.push_back(handParent);
            }
        }
    }
}

void CharIKHand::Highlight() {
#ifdef MILO_DEBUG
    float f2 = Weight();
    float f1 = 0;
    float floatArr[16];
    if (f2 == 0 || !mHand || mTargets.empty())
        return;
    else {
        if (mTargets.size() != 1) {
            float *fp = &floatArr[0];
            for (ObjVector<IKTarget>::iterator it = mTargets.begin();
                 it != mTargets.end();
                 ++it, fp++) {
                RndTransformable *curTarget = it->mTarget;
                if (curTarget) {
                    float f3 = 144.0f / LengthSquared(curTarget->mLocalXfm.v);
                    *fp = f3;
                    f1 += f3;
                }
            }
            float f3 = 0;
            if (f1 < 1.0f) {
                f3 = f2 * (1.0f - f1);
                f2 -= f3;
            }
            TheRnd->DrawString(
                MakeString("weight %g", f2),
                Vector2(100.0f, 100.0f),
                Hmx::Color(1, 1, 1),
                true
            );
            TheRnd->DrawString(
                MakeString("leftover %g", f3),
                Vector2(100.0f, 114.0f),
                Hmx::Color(1, 1, 1),
                true
            );
            fp = &floatArr[0];
            int idx = 0;
            for (ObjVector<IKTarget>::iterator it = mTargets.begin();
                 it != mTargets.end();
                 ++it, fp++, idx++) {
                f3 = *fp;
                float fdiv = f3 / f1;
                if (it->mTarget) {
                    Transform &curWorld = it->mTarget->WorldXfm();
                    TheRnd->DrawString(
                        MakeString("%s %g", it->mTarget->Name(), f2 * fdiv),
                        Vector2(100.0f, (idx + 2) * 14.0f + 100.0f),
                        Hmx::Color(1, 1, 1),
                        true
                    );
                    UtilDrawAxes(curWorld, 1.0f, Hmx::Color(1, 1, 1));
                    UtilDrawSphere(curWorld.v, fdiv, Hmx::Color(1, 0, 0));
                    TheRnd->DrawLine(
                        curWorld.v,
                        it->mTarget->TransParent()->WorldXfm().v,
                        Hmx::Color(1, 0, 0),
                        false
                    );
                }
            }
        }
        UtilDrawAxes(mHand->WorldXfm(), 1.0f, Hmx::Color(1, 1, 1));
        UtilDrawSphere(mHand->WorldXfm().v, 1.0f, Hmx::Color(0, 1, 0));
    }
#endif
}

SAVE_OBJ(CharIKHand, 0x2A8)

BEGIN_LOADS(CharIKHand)
    LOAD_REVS(bs)
    ASSERT_REVS(0xC, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(CharWeightable)
    bs >> mHand;
    if (gRev > 4)
        bs >> mFinger;
    else
        mFinger = 0;
    if (gRev < 3) {
        ObjPtr<RndTransformable> tPtr(this, 0);
        bs >> tPtr;
        mTargets.clear();
        mTargets.push_back(IKTarget(ObjPtr<RndTransformable>(tPtr), 0));
    } else if (gRev < 0xB) {
        ObjPtrList<RndTransformable> tList(this, kObjListNoNull);
        bs >> tList;
        mTargets.clear();
        for (ObjPtrList<RndTransformable>::iterator it = tList.begin(); it != tList.end();
             ++it) {
            ObjPtr<RndTransformable> tPtr(this, *it);
            mTargets.push_back(IKTarget(ObjPtr<RndTransformable>(tPtr), 0));
        }
    } else
        bs >> mTargets;

    bs >> mOrientation;
    bs >> mStretch;
    if (gRev > 1)
        bs >> mScalable;
    else
        mScalable = false;

    if (gRev > 3)
        bs >> mMoveElbow;
    else
        mMoveElbow = true;

    if (gRev > 5)
        bs >> mElbowSwing;
    else
        mElbowSwing = 0.0f;

    if (gRev > 6)
        bs >> mAlwaysIKElbow;
    if (gRev > 7) {
        bs >> mConstrainWrist;
        bs >> mWristRadians;
    }
    if (gRev == 9) {
        String s;
        bs >> s;
        bool b;
        bs >> b;
    }
    if (gRev > 0xB) {
        bs >> mElbowCollide;
        bs >> mClockwise;
    }
    SetHand(mHand);
END_LOADS

DECOMP_FORCEACTIVE(CharIKHand, "ObjPtr_p.h", "f.Owner()", "")

BEGIN_COPYS(CharIKHand)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharIKHand)
    BEGIN_COPYING_MEMBERS
        SetHand(c->mHand);
        COPY_MEMBER(mFinger)
        COPY_MEMBER(mTargets)
        COPY_MEMBER(mOrientation)
        COPY_MEMBER(mStretch)
        COPY_MEMBER(mScalable)
        COPY_MEMBER(mMoveElbow)
        COPY_MEMBER(mElbowSwing)
        COPY_MEMBER(mAlwaysIKElbow)
        COPY_MEMBER(mConstrainWrist)
        COPY_MEMBER(mWristRadians)
        COPY_MEMBER(mTargets)
        COPY_MEMBER(mElbowCollide)
        COPY_MEMBER(mClockwise)
    END_COPYING_MEMBERS
END_COPYS

BinStream &operator>>(BinStream &bs, CharIKHand::IKTarget &t) {
    bs >> t.mTarget;
    bs >> t.mExtent;
    return bs;
}

BEGIN_HANDLERS(CharIKHand)
    HANDLE_ACTION(measure_lengths, MeasureLengths())
    HANDLE_SUPERCLASS(CharWeightable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x33D)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(CharIKHand::IKTarget)
    SYNC_PROP(target, o.mTarget)
    SYNC_PROP(extent, o.mExtent)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(CharIKHand)
    SYNC_PROP_SET(hand, mHand, SetHand(_val.Obj<RndTransformable>()))
    SYNC_PROP(finger, mFinger)
    SYNC_PROP(targets, mTargets)
    SYNC_PROP(orientation, mOrientation)
    SYNC_PROP(stretch, mStretch)
    SYNC_PROP(scalable, mScalable)
    SYNC_PROP(move_elbow, mMoveElbow)
    SYNC_PROP(elbow_swing, mElbowSwing)
    SYNC_PROP(always_ik_elbow, mAlwaysIKElbow)
    SYNC_PROP(constrain_wrist, mConstrainWrist)
    SYNC_PROP(wrist_radians, mWristRadians)
    SYNC_PROP(elbow_collide, mElbowCollide)
    SYNC_PROP(clockwise, mClockwise)
    SYNC_SUPERCLASS(CharWeightable)
END_PROPSYNCS
