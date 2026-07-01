
80246770 <clear__Q211stlpmtx_std120_List_base<Q211stlpmtx_std21pair<6Symbol,6Symbol>,Q211stlpmtx_std52StlNodeAlloc<Q211stlpmtx_std21pair<6Symbol,6Symbol>>>Fv>:
80246770: 94 21 ff f0  	stwu 1, -16(1)
80246774: 7c 08 02 a6  	mflr 0
80246778: 90 01 00 14  	stw 0, 20(1)
8024677c: 93 e1 00 0c  	stw 31, 12(1)
80246780: 93 c1 00 08  	stw 30, 8(1)
80246784: 7c 7e 1b 78  	mr	30, 3
80246788: 83 e3 00 00  	lwz 31, 0(3)
8024678c: 48 00 00 18  	b 0x802467a4 <clear__Q211stlpmtx_std120_List_base<Q211stlpmtx_std21pair<6Symbol,6Symbol>,Q211stlpmtx_std52StlNodeAlloc<Q211stlpmtx_std21pair<6Symbol,6Symbol>>>Fv+0x34>
80246790: 7f e5 fb 78  	mr	5, 31
80246794: 83 ff 00 00  	lwz 31, 0(31)
80246798: 38 60 00 10  	li 3, 16
8024679c: 38 80 00 01  	li 4, 1
802467a0: 48 25 b8 b1  	bl 0x804a2050 <_MemOrPoolFreeSTL__Fi8PoolTypePv>
802467a4: 7c 1f f0 40  	cmplw	31, 30
802467a8: 40 82 ff e8  	bf	2, 0x80246790 <clear__Q211stlpmtx_std120_List_base<Q211stlpmtx_std21pair<6Symbol,6Symbol>,Q211stlpmtx_std52StlNodeAlloc<Q211stlpmtx_std21pair<6Symbol,6Symbol>>>Fv+0x20>
802467ac: 93 de 00 00  	stw 30, 0(30)
802467b0: 93 de 00 04  	stw 30, 4(30)
802467b4: 83 e1 00 0c  	lwz 31, 12(1)
802467b8: 83 c1 00 08  	lwz 30, 8(1)
802467bc: 80 01 00 14  	lwz 0, 20(1)
802467c0: 7c 08 03 a6  	mtlr 0
802467c4: 38 21 00 10  	addi 1, 1, 16
802467c8: 4e 80 00 20  	blr
