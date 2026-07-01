typedef struct LocalBandMachine {
    /* 0x00 */ char pad0[0x78];
    /* 0x78 */ s32 unk78;                           /* inferred */
    /* 0x7C */ LocalBandMachine *unk7C;             /* inferred */
} LocalBandMachine;                                 /* size >= 0x80 */

/* LocalBandMachine::SetPrimaryMetaScore (int) */
LocalBandMachine *SetPrimaryMetaScore__16LocalBandMachineFi(LocalBandMachine *this, s32 arg0) {
    if (arg0 != (s32) this->unk78) {
        this->unk78 = arg0;
        return this->unk7C;
    }
    return this;
}