# Singer::GetFrameMatchType()
.fn GetFrameMatchType__6SingerFv, global
/* 801D96B0 001CE0D0  80 03 00 70 */	lwz r0, 0x70(r3)
/* 801D96B4 001CE0D4  2C 00 FF FF */	cmpwi r0, -0x1
/* 801D96B8 001CE0D8  41 82 00 1C */	beq .L_801D96D4
/* 801D96BC 001CE0DC  80 63 00 00 */	lwz r3, 0x0(r3)
/* 801D96C0 001CE0E0  54 00 10 3A */	slwi r0, r0, 2
/* 801D96C4 001CE0E4  80 63 03 58 */	lwz r3, 0x358(r3)
/* 801D96C8 001CE0E8  7C 63 00 2E */	lwzx r3, r3, r0
/* 801D96CC 001CE0EC  80 63 00 98 */	lwz r3, 0x98(r3)
/* 801D96D0 001CE0F0  4E 80 00 20 */	blr
.L_801D96D4:
/* 801D96D4 001CE0F4  38 60 00 04 */	li r3, 0x4
/* 801D96D8 001CE0F8  4E 80 00 20 */	blr
.endfn GetFrameMatchType__6SingerFv