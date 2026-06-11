# PlayerBehavior::PlayerBehavior()
.fn __ct__14PlayerBehaviorFv, global
/* 801C7380 001BBDA0  94 21 FF F0 */	stwu r1, -0x10(r1)
/* 801C7384 001BBDA4  7C 08 02 A6 */	mflr r0
/* 801C7388 001BBDA8  3C 80 80 B8 */	lis r4, "@stringBase0"@ha
/* 801C738C 001BBDAC  38 A0 00 01 */	li r5, 0x1
/* 801C7390 001BBDB0  90 01 00 14 */	stw r0, 0x14(r1)
/* 801C7394 001BBDB4  38 00 00 00 */	li r0, 0x0
/* 801C7398 001BBDB8  38 84 0B E0 */	addi r4, r4, "@stringBase0"@l
/* 801C739C 001BBDBC  93 E1 00 0C */	stw r31, 0xc(r1)
/* 801C73A0 001BBDC0  7C 7F 1B 78 */	mr r31, r3
/* 801C73A4 001BBDC4  98 A3 00 00 */	stb r5, 0x0(r3)
/* 801C73A8 001BBDC8  98 03 00 01 */	stb r0, 0x1(r3)
/* 801C73AC 001BBDCC  98 03 00 02 */	stb r0, 0x2(r3)
/* 801C73B0 001BBDD0  98 03 00 03 */	stb r0, 0x3(r3)
/* 801C73B4 001BBDD4  98 03 00 04 */	stb r0, 0x4(r3)
/* 801C73B8 001BBDD8  98 03 00 05 */	stb r0, 0x5(r3)
/* 801C73BC 001BBDDC  38 63 00 08 */	addi r3, r3, 0x8
/* 801C73C0 001BBDE0  48 2F 5E 01 */	bl __ct__6SymbolFPCc
/* 801C73C4 001BBDE4  38 00 00 02 */	li r0, 0x2
/* 801C73C8 001BBDE8  90 1F 00 0C */	stw r0, 0xc(r31)
/* 801C73CC 001BBDEC  7F E3 FB 78 */	mr r3, r31
/* 801C73D0 001BBDF0  83 E1 00 0C */	lwz r31, 0xc(r1)
/* 801C73D4 001BBDF4  80 01 00 14 */	lwz r0, 0x14(r1)
/* 801C73D8 001BBDF8  7C 08 03 A6 */	mtlr r0
/* 801C73DC 001BBDFC  38 21 00 10 */	addi r1, r1, 0x10
/* 801C73E0 001BBE00  4E 80 00 20 */	blr
.endfn __ct__14PlayerBehaviorFv