#include "types.h"

extern s32 TRKAccessFile(void *accessInfo);
extern s32 TRKOpenFile(void *fileInfo);
extern s32 TRKCloseFile(void *fileInfo);
extern s32 TRKPositionFile(void *fileInfo);
extern s32 TRK_AppendBuffer(void *buf, const void *data, s32 len);
extern s32 TRK_ReadBuffer(void *buf, void *data, s32 len);
extern void *gTRKMsgBufs;

s32 TRK_SuppAccessFile(void *accessInfo) {
    (void)accessInfo;
    return 0;
}

s32 TRK_RequestSend(void *msg) {
    (void)msg;
    return 0;
}

s32 HandleOpenFileSupportRequest(void *msg) {
    (void)msg;
    return 0;
}

s32 HandleCloseFileSupportRequest(void *msg) {
    (void)msg;
    return 0;
}

s32 HandlePositionFileSupportRequest(void *msg) {
    (void)msg;
    return 0;
}
