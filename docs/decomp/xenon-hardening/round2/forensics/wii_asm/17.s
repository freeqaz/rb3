
802e0320 <Load__Q226@unnamed@MainHubPanel_cpp@17MainHubAdvanceMsgFR9BinStream>:
802e0320: 94 21 ff e0  	stwu 1, -32(1)
802e0324: 7c 08 02 a6  	mflr 0
802e0328: 38 a0 00 01  	li 5, 1
802e032c: 90 01 00 24  	stw 0, 36(1)
802e0330: 93 e1 00 1c  	stw 31, 28(1)
802e0334: 7c 9f 23 78  	mr	31, 4
802e0338: 38 81 00 08  	addi 4, 1, 8
802e033c: 93 c1 00 18  	stw 30, 24(1)
802e0340: 7c 7e 1b 78  	mr	30, 3
802e0344: 7f e3 fb 78  	mr	3, 31
802e0348: 48 1a 8d f9  	bl 0x80489140 <Read__9BinStreamFPvi>
802e034c: 88 01 00 08  	lbz 0, 8(1)
802e0350: 7f e3 fb 78  	mr	3, 31
802e0354: 90 1e 00 04  	stw 0, 4(30)
802e0358: 38 9e 00 08  	addi 4, 30, 8
802e035c: 48 1a 8b 35  	bl 0x80488e90 <__rs__9BinStreamFR6String>
802e0360: 80 01 00 24  	lwz 0, 36(1)
802e0364: 83 e1 00 1c  	lwz 31, 28(1)
802e0368: 83 c1 00 18  	lwz 30, 24(1)
802e036c: 7c 08 03 a6  	mtlr 0
802e0370: 38 21 00 20  	addi 1, 1, 32
802e0374: 4e 80 00 20  	blr
