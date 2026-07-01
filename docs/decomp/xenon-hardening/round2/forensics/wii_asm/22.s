# LocalBandMachine::SetPrimaryMetaScore(int)
.fn SetPrimaryMetaScore__16LocalBandMachineFi, global
/* 802D6050 002CAA70  80 03 00 78 */	lwz r0, 0x78(r3)
/* 802D6054 002CAA74  7C 04 00 00 */	cmpw r4, r0
/* 802D6058 002CAA78  4D 82 00 20 */	beqlr
/* 802D605C 002CAA7C  90 83 00 78 */	stw r4, 0x78(r3)
/* 802D6060 002CAA80  38 80 00 04 */	li r4, 0x4
/* 802D6064 002CAA84  80 63 00 7C */	lwz r3, 0x7c(r3)
/* 802D6068 002CAA88  48 00 1E 58 */	b SyncLocalMachine__14BandMachineMgrFUc
/* 802D606C 002CAA8C  4E 80 00 20 */	blr
.endfn SetPrimaryMetaScore__16LocalBandMachineFi