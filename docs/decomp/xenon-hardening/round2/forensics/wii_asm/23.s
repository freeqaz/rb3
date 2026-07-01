# Lyric::PitchNote() const
.fn PitchNote__5LyricCFv, global
/* 80142CF0 00137710  80 63 00 30 */	lwz r3, 0x30(r3)
/* 80142CF4 00137714  80 63 00 00 */	lwz r3, 0x0(r3)
/* 80142CF8 00137718  88 03 00 2A */	lbz r0, 0x2a(r3)
/* 80142CFC 0013771C  7C 00 00 34 */	cntlzw r0, r0
/* 80142D00 00137720  54 03 D9 7E */	srwi r3, r0, 5
/* 80142D04 00137724  4E 80 00 20 */	blr
.endfn PitchNote__5LyricCFv