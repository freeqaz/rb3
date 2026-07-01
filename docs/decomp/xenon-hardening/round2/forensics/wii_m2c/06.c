typedef struct Tracker {
    /* 0x00 */ void *unk0;                          /* inferred */
    /* 0x04 */ u8 unk4;                             /* inferred */
    /* 0x05 */ char pad5[0x24];                     /* maybe part of unk4[0x25]? */
    /* 0x29 */ u8 unk29;                            /* inferred */
    /* 0x2A */ u8 unk2A;                            /* inferred */
} Tracker;                                          /* size >= 0x2B */

TrackPanel *GetTrackPanel__Fv();                    /* extern */
? SetSuppressPlayerFeedback__10TrackPanelFb(TrackPanel *this, u8 arg0); /* extern */
? SetSuppressTambourineDisplay__10TrackPanelFb(TrackPanel *this, u8 arg0); /* extern */
? ReachedTargetLevel__7TrackerFi(Tracker *this, s32 arg0); /* static */
? SetupDisplays__7TrackerFv(Tracker *this);         /* static */

/* Tracker::Poll (float) */
void Poll__7TrackerFf(Tracker *this, f32 arg0) {
    if ((s32) this->unk4 != 0) {
        ReachedTargetLevel__7TrackerFi(this, -1);
        this->unk0->unk38(this, arg0);
        this->unk4 = 0;
        SetSuppressTambourineDisplay__10TrackPanelFb(GetTrackPanel__Fv(), this->unk29);
        SetSuppressPlayerFeedback__10TrackPanelFb(GetTrackPanel__Fv(), this->unk2A);
        SetupDisplays__7TrackerFv(this);
    }
    this->unk0->unk3C(this, arg0);
}