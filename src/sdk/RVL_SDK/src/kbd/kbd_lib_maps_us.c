#include "types.h"

u8 kbdKeyMapCANADA[0x140];
u8 kbdModifierMapCANADA[0x28];
u8 kbdKeyMapCANADA_FR[0x140];
u8 kbdModifierMapCANADA_FR[0x28];
u8 kbdKeyMapLATIN_AMERICA[0x140];
u8 kbdModifierMapLATIN_AMERICA[0x28];
u8 kbdKeyMapUSA[0x258];
u8 kbdModifierMapUSA[0x30];

void KBDInitRegionUS(void *region) { (void)region; }
