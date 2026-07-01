typedef struct Tail {
    /* 0x000 */ char pad0[0xC];
    /* 0x00C */ void *unkC;                         /* inferred */
    /* 0x010 */ char pad10[8];                      /* maybe part of unkC[3]? */
    /* 0x018 */ s32 unk18;                          /* inferred */
    /* 0x01C */ char pad1C[0xC];                    /* maybe part of unk18[4]? */
    /* 0x028 */ u8 unk28;                           /* inferred */
    /* 0x029 */ char pad29[3];                      /* maybe part of unk28[4]? */
    /* 0x02C */ ? unk2C;                            /* inferred */
    /* 0x02C */ char pad2C[0x4B0];
    /* 0x4DC */ s32 unk4DC;                         /* inferred */
    /* 0x4E0 */ char pad4E0[0x18];                  /* maybe part of unk4DC[7]? */
    /* 0x4F8 */ u8 unk4F8;                          /* inferred */
} Tail;                                             /* size >= 0x4F9 */

? memset(? *, ?, ?);                                /* extern */

/* Tail::Hit (void) */
void Hit__4TailFv(Tail *this) {
    void *temp_r4;

    this->unk18 = 2;
    if ((s32) this->unk4F8 == 0) {
        temp_r4 = this->unkC;
        temp_r4->unk8 = (u8) (temp_r4->unk8 | 0x80);
    }
    if ((s32) this->unk28 != 0) {
        memset(&this->unk2C, 0, 0x4B0);
        this->unk4DC = 0;
    }
}