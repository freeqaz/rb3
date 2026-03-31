#include "rndobj/PartLauncher.h"
#include "rndobj/MultiMesh.h"
#include "math/Rand.h"
#include "utl/Symbols.h"

INIT_REVS(RndPartLauncher)

RndPartLauncher::RndPartLauncher()
    : mPart(this, 0), mTrans(this, 0), mMeshEmitter(this, 0), mNumParts(0),
      mEmitRate(0.0f, 0.0f), mEmitCount(0.0f), mPartOverride(&gNoPartOverride) {}

// fn_80621D48
void RndPartLauncher::LaunchParticles() {
    if (mPart) {
        float b1x = mPart->mBoxExtent1.x;
        float b1y = mPart->mBoxExtent1.y;
        float b1z = mPart->mBoxExtent1.z;
        float b2x = mPart->mBoxExtent2.x;
        float b2y = mPart->mBoxExtent2.y;
        float b2z = mPart->mBoxExtent2.z;
        if (mTrans) {
            const Vector3 &partvec = mPart->WorldXfm().v;
            float pvx = partvec.x;
            float pvy = partvec.y;
            float pvz = partvec.z;
            const Vector3 &transvec = mTrans->WorldXfm().v;

            float dx = transvec.x - pvx;
            float dy = transvec.y - pvy;
            float dz = transvec.z - pvz;
            RndParticleSys *part = mPart;
            part->mBoxExtent1.Set(b1x + dx, b1y + dy, b1z + dz);
            part->mBoxExtent2.Set(b2x + dx, b2y + dy, b2z + dz);
        }

        mPartOverride->mesh = mMeshEmitter;
        mPart->ExplicitParticles(mNumParts, true, *mPartOverride);
        mPartOverride->mesh = 0;

        if (mTrans) {
            RndParticleSys *part = mPart;
            part->mBoxExtent1.Set(b1x, b1y, b1z);
            part->mBoxExtent2.Set(b2x, b2y, b2z);
        }
    }
}

// fn_80621EA4
void RndPartLauncher::Poll() {
    if (mEmitRate.x > 0.0f || mEmitRate.y > 0.0f) {
        float delta = TheTaskMgr.DeltaSeconds();
        if (delta > 0.0f) {
            float random = RandomFloat(mEmitRate.x, mEmitRate.y);
            mEmitCount = delta * random * 30.0f + mEmitCount;
            if (mEmitCount >= 1.0f) {
                int parts = mNumParts;
                // something here with modf
                LaunchParticles();
                mNumParts = parts;
            }
        }
    }
}

void RndPartLauncher::SetBit(unsigned int bitmask, bool high) {
    if (high)
        mPartOverride->mask |= bitmask;
    else
        mPartOverride->mask &= ~bitmask;
}

// fn_8062202C
void RndPartLauncher::CopyPropsFromPart() {
    if (mPart) {
        if (!(mPartOverride->mask & 1)) {
            mPartOverride->life = Average(mPart->Life());
        }
        if (!(mPartOverride->mask & 2)) {
            mPartOverride->speed = Average(mPart->Speed());
        }
        if (!(mPartOverride->mask & 4)) {
            mPartOverride->size = Average(mPart->StartSize());
        }
        if (!(mPartOverride->mask & 8)) {
            mPartOverride->deltaSize = Average(mPart->mDeltaSize);
        }
        if (!(mPartOverride->mask & 0x10)) {
            Average(
                mPartOverride->startColor,
                mPart->StartColorLow(),
                mPart->StartColorHigh()
            );
        }
        if (!(mPartOverride->mask & 0x20)) {
            Average(
                mPartOverride->midColor, mPart->mMidColorLow, mPart->mMidColorHigh
            );
        }
        if (!(mPartOverride->mask & 0x40)) {
            Average(
                mPartOverride->endColor,
                mPart->EndColorLow(),
                mPart->EndColorHigh()
            );
        }
        if (!(mPartOverride->mask & 0x80)) {
            mPartOverride->pitch = mPart->mPitch;
            mPartOverride->yaw = mPart->mYaw;
        }
        if (!(mPartOverride->mask & 0x100)) {
            mPartOverride->mesh = mPart->GetMesh();
        }
        if (!(mPartOverride->mask & 0x200)) {
            mPartOverride->box.mMin = mPart->mBoxExtent1;
            mPartOverride->box.mMax = mPart->mBoxExtent2;
        }
    }
}

SAVE_OBJ(RndPartLauncher, 0x91)

BEGIN_LOADS(RndPartLauncher)
    LOAD_REVS(bs);
    ASSERT_REVS(4, 0);
    LOAD_SUPERCLASS(Hmx::Object)
    if (gRev < 2) {
        LOAD_SUPERCLASS(Hmx::Object)
        ObjPtr<RndMultiMesh, ObjectDir> mMeshPtr(this, 0);
        bs >> mMeshPtr;
        if (gRev != 0) {
            bs >> mPart;
            bs >> mTrans;
            bs >> mNumParts;

            bool bit1;
            bs >> bit1;
            SetBit(1, bit1);
            bs >> mPartOverride->life;

            bool bit2;
            bs >> bit2;
            SetBit(2, bit2);
            bs >> mPartOverride->speed;

            bool bit4;
            bs >> bit4;
            SetBit(4, bit4);
            bs >> mPartOverride->size;

            bool bit8;
            bs >> bit8;
            SetBit(8, bit8);
            bs >> mPartOverride->deltaSize;

            bool bit10;
            bs >> bit10;
            SetBit(0x10, bit10);
            bs >> mPartOverride->startColor;

            bool bit20;
            bs >> bit20;
            SetBit(0x20, bit20);
            bs >> mPartOverride->midColor;

            bool bit40;
            bs >> bit40;
            SetBit(0x40, bit40);
            bs >> mPartOverride->endColor;

            bool bit80;
            bs >> bit80;
            SetBit(0x80, bit80);
            bs >> mPartOverride->pitch;
            bs >> mPartOverride->yaw;
        }
    } else {
        bs >> mPart;
        bs >> mTrans;
        bs >> mNumParts;
        bs >> mPartOverride->mask;
        bs >> mPartOverride->life;
        bs >> mPartOverride->speed;
        bs >> mPartOverride->size;
        bs >> mPartOverride->deltaSize;
        bs >> mPartOverride->startColor;
        bs >> mPartOverride->midColor;
        bs >> mPartOverride->endColor;
        bs >> mPartOverride->pitch;
        bs >> mPartOverride->yaw;
        if (gRev > 2) {
            bs >> mPartOverride->box;
            bs >> mMeshEmitter;
        }
        if (gRev > 3) {
            bs >> mEmitRate;
        }
    }
END_LOADS

BEGIN_COPYS(RndPartLauncher)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(RndPartLauncher)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mPart)
        COPY_MEMBER(mTrans)
        COPY_MEMBER(mNumParts)
        COPY_MEMBER(mEmitRate)
        *mPartOverride = *c->mPartOverride;
        COPY_MEMBER(mMeshEmitter)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(RndPartLauncher)
    HANDLE_ACTION(launch_particles, LaunchParticles())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x104)
END_HANDLERS

BEGIN_PROPSYNCS(RndPartLauncher)
    SYNC_PROP_MODIFY_ALT(part, mPart, CopyPropsFromPart())
    SYNC_PROP(trans, mTrans)
    SYNC_PROP(num_parts, mNumParts)
    SYNC_PROP(emit_rate, mEmitRate)
    SYNC_PROP_SET(override_life, CheckPartBit(0), SetBit(1, _val.Int()))
    SYNC_PROP(life, mPartOverride->life)
    SYNC_PROP_SET(override_speed, int(mPartOverride->mask >> 1 & 1), SetBit(2, _val.Int()))
    SYNC_PROP(speed, mPartOverride->speed)
    SYNC_PROP_SET(override_size, int(mPartOverride->mask >> 2 & 1), SetBit(4, _val.Int()))
    SYNC_PROP(size, mPartOverride->size)
    SYNC_PROP_SET(
        override_delta_size, int(mPartOverride->mask >> 3 & 1), SetBit(8, _val.Int())
    )
    SYNC_PROP(delta_size, mPartOverride->deltaSize)
    SYNC_PROP_SET(
        override_start_color, int(mPartOverride->mask >> 4 & 1), SetBit(0x10, _val.Int())
    )
    SYNC_PROP(start_color, mPartOverride->startColor)
    SYNC_PROP(start_alpha, mPartOverride->startColor.alpha)
    SYNC_PROP_SET(
        override_mid_color, int(mPartOverride->mask >> 5 & 1), SetBit(0x20, _val.Int())
    )
    SYNC_PROP(mid_color, mPartOverride->midColor)
    SYNC_PROP(mid_alpha, mPartOverride->midColor.alpha)
    SYNC_PROP_SET(
        override_end_color, int(mPartOverride->mask >> 6 & 1), SetBit(0x40, _val.Int())
    )
    SYNC_PROP(end_color, mPartOverride->endColor)
    SYNC_PROP(end_alpha, mPartOverride->endColor.alpha)
    SYNC_PROP_SET(
        override_emit_direction,
        int(mPartOverride->mask >> 7 & 1),
        SetBit(0x80, _val.Int())
    )
    SYNC_PROP(pitch_low, mPartOverride->pitch.x)
    SYNC_PROP(pitch_high, mPartOverride->pitch.y)
    SYNC_PROP(yaw_low, mPartOverride->yaw.x)
    SYNC_PROP(yaw_high, mPartOverride->yaw.y)
    SYNC_PROP_SET(
        override_box_emitter, int(mPartOverride->mask >> 9 & 1), SetBit(0x200, _val.Int())
    )
    SYNC_PROP(box_extent_1, mPartOverride->box.mMin)
    SYNC_PROP(box_extent_2, mPartOverride->box.mMax)
    SYNC_PROP_SET(
        override_mesh_emitter,
        int(mPartOverride->mask >> 8 & 1),
        SetBit(0x100, _val.Int())
    )
    SYNC_PROP(mesh, mMeshEmitter)
END_PROPSYNCS
