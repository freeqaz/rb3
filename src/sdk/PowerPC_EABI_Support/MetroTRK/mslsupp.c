#include "types.h"

extern s32 TRK_SuppAccessFile(void *accessInfo);
extern void TRKTestForPacket(void);
extern void TRKGetInput(void);

s32 __read_console(s32 unk1, void *buf, s32 *n, void *unk2) {
    return 0;
}

s32 __TRK_write_console(s32 unk1, const void *buf, s32 *n, void *unk2) {
    return 0;
}

s32 __read_file(s32 fd, void *buf, s32 *n, void *unk) {
    return 0;
}

s32 __write_file(s32 fd, const void *buf, s32 *n, void *unk) {
    return 0;
}

s32 __access_file(void *accessInfo) {
    return TRK_SuppAccessFile(accessInfo);
}
