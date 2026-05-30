#include "types.h"

#define HASH_SIZE 0x22E

u16 l_tmpWName[8];
u8 hashTable[HASH_SIZE];

s32 _MakeWStr(u16 *dst, const char *src, s32 maxLen) {
    s32 i = 0;
    while (i < maxLen && *src != '\0') {
        dst[i++] = (u16)(unsigned char)*src++;
    }
    if (i < maxLen) dst[i] = 0;
    return i;
}

void dHash_InitHashTable(void) {
    s32 i;
    for (i = 0; i < HASH_SIZE; i++) hashTable[i] = 0;
}

s32 dHash_SearchHashW(const u16 *name, s32 len) {
    (void)name;
    (void)len;
    return -1;
}

s32 dHash_GetArg(s32 idx, void *out) {
    (void)idx;
    (void)out;
    return -1;
}

s32 dHash_SetArg(s32 idx, const void *data, s32 len) {
    (void)idx;
    (void)data;
    (void)len;
    return -1;
}

s32 dHash_DeleteData(s32 idx) {
    (void)idx;
    return 0;
}
