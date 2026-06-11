typedef struct LockStepMgr {
    /* 0x00 */ char pad0[0x28];
    /* 0x28 */ u8 unk28;                            /* inferred */
} LockStepMgr;                                      /* size >= 0x29 */

typedef struct NetSync {
    /* 0x00 */ char pad0[0x20];
    /* 0x20 */ UIScreen *unk20;                     /* inferred */
    /* 0x24 */ s32 unk24;                           /* inferred */
    /* 0x28 */ char pad28[4];
    /* 0x2C */ LockStepMgr *unk2C;                  /* inferred */
} NetSync;                                          /* size >= 0x30 */

s32 InLock__11LockStepMgrCFv(LockStepMgr *this);    /* extern */
? RespondToLock__11LockStepMgrFb(LockStepMgr *this, s32 arg0); /* extern */
? AttemptTransition__7NetSyncFP8UIScreeni(NetSync *this, UIScreen *arg0, s32 arg1); /* static */
extern void *TheUI;

/* NetSync::Poll (void) */
void Poll__7NetSyncFv(NetSync *this) {
    LockStepMgr *temp_r3_2;
    UIScreen *temp_r4;
    void **temp_r31;
    void **temp_r3;

    if ((s32) TheUI->unk8 != 0) {
        temp_r3 = TheUI->unk24;
        temp_r31 = TheUI->unk20;
        if (((temp_r3 == NULL) || ((*temp_r3)->unk68() != 0)) && ((temp_r31 == NULL) || ((*temp_r31)->unk84(temp_r31) == 0)) && (InLock__11LockStepMgrCFv(this->unk2C) != 0)) {
            temp_r3_2 = this->unk2C;
            if ((s32) temp_r3_2->unk28 == 0) {
                RespondToLock__11LockStepMgrFb(temp_r3_2, 1);
            }
        }
    } else {
        temp_r4 = this->unk20;
        if ((temp_r4 != NULL) && ((void **) TheUI->unk20 != NULL)) {
            AttemptTransition__7NetSyncFP8UIScreeni(this, temp_r4, this->unk24);
        }
    }
}