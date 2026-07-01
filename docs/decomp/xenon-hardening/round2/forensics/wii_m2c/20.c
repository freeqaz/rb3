typedef struct PlayerBehavior {
    /* 0x00 */ s8 unk0;                             /* inferred */
    /* 0x01 */ s8 unk1;                             /* inferred */
    /* 0x02 */ s8 unk2;                             /* inferred */
    /* 0x03 */ s8 unk3;                             /* inferred */
    /* 0x04 */ s8 unk4;                             /* inferred */
    /* 0x05 */ s8 unk5;                             /* inferred */
    /* 0x06 */ char pad6[2];                        /* maybe part of unk5[3]? */
    /* 0x08 */ Symbol unk8;                         /* inferred */
    /* 0x08 */ char pad8[4];
    /* 0x0C */ s32 unkC;                            /* inferred */
} PlayerBehavior;                                   /* size >= 0x10 */

void *__ct__6SymbolFPCc(Symbol *this, s8 *arg0);    /* extern */
static s8 @stringBase0[8] = "default";

/* PlayerBehavior::PlayerBehavior (void) */
PlayerBehavior *__ct__14PlayerBehaviorFv(PlayerBehavior *this) {
    this->unk0 = 1;
    this->unk1 = 0;
    this->unk2 = 0;
    this->unk3 = 0;
    this->unk4 = 0;
    this->unk5 = 0;
    __ct__6SymbolFPCc(&this->unk8, "default");
    this->unkC = 2;
    return this;
}