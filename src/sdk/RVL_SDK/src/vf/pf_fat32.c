#include "types.h"

s32 VFiPFFAT32_ReadFATEntry(void *vol, u32 cluster, u32 *value) {
    (void)vol; (void)cluster; (void)value;
    return 0;
}

s32 VFiPFFAT32_ReadFATEntryPage(void *vol, void *page, u32 cluster, u32 *value) {
    (void)vol; (void)page; (void)cluster; (void)value;
    return 0;
}

s32 VFiPFFAT32_WriteFATEntry(void *vol, u32 cluster, u32 value) {
    (void)vol; (void)cluster; (void)value;
    return 0;
}

s32 VFiPFFAT32_WriteFATEntryPage(void *vol, u32 cluster, u32 value, void **page) {
    (void)vol; (void)cluster; (void)value; (void)page;
    return 0;
}
