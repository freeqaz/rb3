#include "types.h"

s32 VFiPFFAT16_ReadFATEntry(void *vol, u32 cluster, u32 *value) {
    (void)vol; (void)cluster; (void)value;
    return 0;
}

s32 VFiPFFAT16_ReadFATEntryPage(void *vol, void *page, u32 cluster, u32 *value) {
    (void)vol; (void)page; (void)cluster; (void)value;
    return 0;
}

s32 VFiPFFAT16_WriteFATEntry(void *vol, u32 cluster, u32 value) {
    (void)vol; (void)cluster; (void)value;
    return 0;
}

s32 VFiPFFAT16_WriteFATEntryPage(void *vol, u32 cluster, u32 value, void **page) {
    (void)vol; (void)cluster; (void)value; (void)page;
    return 0;
}
