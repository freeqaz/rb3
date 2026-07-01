# BandUser::SetLoadedPrefabChar(int)
.fn SetLoadedPrefabChar__8BandUserFi, global
/* 80162C30 00157650  94 21 FF F0 */	stwu r1, -0x10(r1)
/* 80162C34 00157654  7C 08 02 A6 */	mflr r0
/* 80162C38 00157658  90 01 00 14 */	stw r0, 0x14(r1)
/* 80162C3C 0015765C  93 E1 00 0C */	stw r31, 0xc(r1)
/* 80162C40 00157660  7C 9F 23 78 */	mr r31, r4
/* 80162C44 00157664  93 C1 00 08 */	stw r30, 0x8(r1)
/* 80162C48 00157668  7C 7E 1B 78 */	mr r30, r3
/* 80162C4C 0015766C  48 1E 22 C5 */	bl GetPrefabMgr__9PrefabMgrFv
/* 80162C50 00157670  7F E4 FB 78 */	mr r4, r31
/* 80162C54 00157674  48 1E 3C 9D */	bl GetDefaultPrefab__9PrefabMgrCFi
/* 80162C58 00157678  7C 64 1B 78 */	mr r4, r3
/* 80162C5C 0015767C  7F C3 F3 78 */	mr r3, r30
/* 80162C60 00157680  4B FF FD E1 */	bl SetChar__8BandUserFP8CharData
/* 80162C64 00157684  80 01 00 14 */	lwz r0, 0x14(r1)
/* 80162C68 00157688  83 E1 00 0C */	lwz r31, 0xc(r1)
/* 80162C6C 0015768C  83 C1 00 08 */	lwz r30, 0x8(r1)
/* 80162C70 00157690  7C 08 03 A6 */	mtlr r0
/* 80162C74 00157694  38 21 00 10 */	addi r1, r1, 0x10
/* 80162C78 00157698  4E 80 00 20 */	blr
.endfn SetLoadedPrefabChar__8BandUserFi