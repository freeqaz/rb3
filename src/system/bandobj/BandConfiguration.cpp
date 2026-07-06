#include "bandobj/BandConfiguration.h"
#include "bandobj/BandCharacter.h"
#include "bandobj/BandWardrobe.h"
#include "obj/DataUtl.h"
#include "utl/Symbols.h"
#ifdef HX_NATIVE
#include <cstdio>  // fprintf — W2.1-flip.S2 drum placement probe
#include <cstdlib> // getenv
#endif

INIT_REVS(BandConfiguration)
int BandConfiguration::TargTransforms::sNumPlayModes;

BandConfiguration::BandConfiguration() {
    for (int i = 0; i < 4; i++) {
        Waypoint *wp = Hmx::Object::New<Waypoint>();
        wp->SetRadius(1000.0f);
        wp->SetStrictRadiusDelta(0);
        mXfms[i].mWay = wp;
        for (int j = 0; j < 3; j++) {
            mXfms[i].xfms[j].xfm.Reset();
            mXfms[i].xfms[j].targName = "";
        }
    }
}

BandConfiguration::~BandConfiguration() {
    for (int i = 0; i < 4; i++) {
        delete mXfms[i].mWay;
    }
}

#define kNumPlayModes 3

int BandConfiguration::ConfigIndex() {
    Symbol playmode = TheBandWardrobe->GetPlayMode();
    DataArray *macro = DataGetMacro("BAND_PLAY_MODES");
    MILO_ASSERT(macro->Size() == kNumPlayModes, 0x33);
    for (int i = 0; i < 3; i++) {
        if (macro->Sym(i) == playmode)
            return i;
    }
    MILO_FAIL("invalid mode %s", playmode);
    return 0;
}

void BandConfiguration::SyncPlayMode() {
    int idx = ConfigIndex();
    for (int i = 0; i < 4; i++) {
        TargTransform &curtargxfm = mXfms[i].xfms[idx];
        mXfms[i].mWay->SetLocalXfm(curtargxfm.xfm);
        BandCharacter *bchar = TheBandWardrobe->FindTarget(
            curtargxfm.targName, TheBandWardrobe->mVenueNames
        );
        if (bchar) {
            bchar->Teleport(mXfms[i].mWay);
#ifdef HX_NATIVE
            // W2.1-flip.S2 drum placement-gate probe (diagnosis-only, default-OFF).
            // mWay->WorldXfm().v is the FAITHFUL band-member placement the decomp
            // resolved for this play-mode slot -- the waypoint the drum kit (and
            // other bone-attached instrument props) hang off. The renderer must
            // draw this member's skinned meshes (incl. the kit prop) with an
            // obj.world translation near THIS waypoint. On the current (unfixed)
            // build DrawMesh forces obj.world = identity for every skinned draw
            // (Rnd_Wgpu_RB3.cpp), so the kit co-locates at the origin -- the drum
            // branch of the placement oracle (native/tests/placement_oracle.h)
            // reads these lines + the drawlog and goes RED; flag-ON they carry the
            // waypoint world and it goes GREEN. Reuses the existing
            // RB3_PLACEMENT_PROBE flag (no new flag / classification edit);
            // Wii-compile-inert (HX_NATIVE undefined under MWCC). No behavior change.
            {
                static int sPlacementProbe = -1;
                if (sPlacementProbe < 0)
                    sPlacementProbe = getenv("RB3_PLACEMENT_PROBE") ? 1 : 0;
                if (sPlacementProbe) {
                    Transform &wxfm = mXfms[i].mWay->WorldXfm();
                    fprintf(stderr,
                            "RB3_PLACEMENT_PROBE drum inst=%d x=%.4f y=%.4f z=%.4f\n",
                            i, wxfm.v.x, wxfm.v.y, wxfm.v.z);
                }
            }
#endif
        }
#ifdef HX_NATIVE
        else if (!curtargxfm.targName.Null()) {
            MILO_WARN(
                "BandConfiguration::SyncPlayMode: waypoint slot %d targName '%s' did "
                "not resolve to a BandCharacter (venue targets: '%s' '%s' '%s' '%s') -- "
                "this member will not be placed",
                i,
                curtargxfm.targName,
                TheBandWardrobe->mVenueNames.names[0],
                TheBandWardrobe->mVenueNames.names[1],
                TheBandWardrobe->mVenueNames.names[2],
                TheBandWardrobe->mVenueNames.names[3]
            );
        }
#endif
    }
}

BinStream &operator>>(BinStream &bs, BandConfiguration::TargTransforms &tts) {
    int i;
    for (i = 0; i < Min(3, BandConfiguration::TargTransforms::sNumPlayModes); i++) {
        bs >> tts.xfms[i].targName;
        bs >> tts.xfms[i].xfm;
    }
    for (; i < BandConfiguration::TargTransforms::sNumPlayModes; i++) {
        Symbol s;
        Transform t;
        bs >> s;
        bs >> t;
    }
    return bs;
}

SAVE_OBJ(BandConfiguration, 0x6E)

BEGIN_LOADS(BandConfiguration)
    LOAD_REVS(bs)
    ASSERT_REVS(0, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    bs >> TargTransforms::sNumPlayModes;
    for (int i = 0; i < 4; i++) {
        bs >> mXfms[i];
    }
    if (TheBandWardrobe) {
        TheBandWardrobe->SetModeSink(this);
    }
END_LOADS

BEGIN_COPYS(BandConfiguration)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(BandConfiguration)
    BEGIN_COPYING_MEMBERS
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 3; j++) {
                COPY_MEMBER(mXfms[i].xfms[j])
            }
        }
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(BandConfiguration)
    HANDLE(store_configuration, OnStoreConfiguration)
    HANDLE(release_configuration, OnReleaseConfiguration)
    HANDLE_ACTION(sync_play_mode, SyncPlayMode())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x9F)
END_HANDLERS

DataNode BandConfiguration::OnStoreConfiguration(DataArray *da) {
    int cfgidx = ConfigIndex();
    for (int i = 0; i < 4; i++) {
        TargTransform &curtarg = mXfms[i].xfms[cfgidx];
        BandCharacter *bchar = TheBandWardrobe->GetCharacter(i);
        if (bchar) {
            curtarg.targName = TheBandWardrobe->VenueNames().names[i];
            curtarg.xfm = bchar->LocalXfm();
        }
    }
    SyncPlayMode();
    return 0;
}

DataNode BandConfiguration::OnReleaseConfiguration(DataArray *da) {
    for (int i = 0; i < 4; i++) {
        BandCharacter *bchar = TheBandWardrobe->GetCharacter(i);
        if (bchar)
            bchar->Teleport(0);
    }
    return 0;
}

BEGIN_PROPSYNCS(BandConfiguration)
END_PROPSYNCS