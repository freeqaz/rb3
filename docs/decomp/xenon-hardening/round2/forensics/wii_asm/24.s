# ProfileMgr::GlobalOptionsNeedsSave()
.fn GlobalOptionsNeedsSave__10ProfileMgrFv, global
/* 8034AA10 0033F430  80 03 05 54 */	lwz r0, 0x554(r3)
/* 8034AA14 0033F434  2C 00 00 01 */	cmpwi r0, 0x1
/* 8034AA18 0033F438  41 82 00 0C */	beq .L_8034AA24
/* 8034AA1C 0033F43C  38 60 00 00 */	li r3, 0x0
/* 8034AA20 0033F440  4E 80 00 20 */	blr
.L_8034AA24:
/* 8034AA24 0033F444  88 63 05 58 */	lbz r3, 0x558(r3)
/* 8034AA28 0033F448  4E 80 00 20 */	blr
.endfn GlobalOptionsNeedsSave__10ProfileMgrFv