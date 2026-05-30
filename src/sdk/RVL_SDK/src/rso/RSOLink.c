#include "types.h"

void *__RSOObjectInfoList;

void RSONotifyModuleLoaded(void *mod) {
    (void)mod;
}

void RSONotifyModuleUnloaded(void *mod) {
    (void)mod;
}

void RSONotifyPreRSOLink(void *mod) {
    (void)mod;
}

void RSONotifyPostRSOLink(void *mod) {
    (void)mod;
}

void RSONotifyPreRSOLinkFar(void *mod) {
    (void)mod;
}

void RSONotifyPostRSOLinkFar(void *mod) {
    (void)mod;
}

s32 LocateObject(void *list, const char *name, void **obj) {
    (void)list; (void)name; (void)obj;
    return 0;
}

s32 RSOStaticLocateObject(void *list, const char *name, void **obj) {
    (void)list; (void)name; (void)obj;
    return 0;
}

s32 RSOUnLocateObject(void *list, void *obj) {
    (void)list; (void)obj;
    return 0;
}

s32 RSOLink(void *mod, void *list) {
    (void)mod; (void)list;
    return 0;
}

s32 RSOGetNumImportSymbols(void *mod) {
    (void)mod;
    return 0;
}

const char *RSOGetImportSymbolName(void *mod, s32 idx) {
    (void)mod; (void)idx;
    return 0;
}

s32 RSOIsImportSymbolResolved(void *mod, s32 idx) {
    (void)mod; (void)idx;
    return 0;
}

s32 RSOIsImportSymbolResolvedAll(void *mod) {
    (void)mod;
    return 0;
}

s32 FindExportIndex(void *mod, const char *name) {
    (void)mod; (void)name;
    return -1;
}

void *RSOFindExportSymbolAddr(void *mod, const char *name) {
    (void)mod; (void)name;
    return 0;
}

s32 RSORelocate(void *mod, void *list) {
    (void)mod; (void)list;
    return 0;
}

s32 RSORelocateSmallDataSection(void *mod, void *list) {
    (void)mod; (void)list;
    return 0;
}

void RSOListInit(void *list) {
    (void)list;
}

s32 LinkList(void *list, void *mod) {
    (void)list; (void)mod;
    return 0;
}

s32 RSOLinkList(void *list, void *mod) {
    (void)list; (void)mod;
    return 0;
}

s32 RSOUnLinkList(void *list, void *mod) {
    (void)list; (void)mod;
    return 0;
}

s32 cnvFarCode(void *mod, void *buf) {
    (void)mod; (void)buf;
    return 0;
}

s32 cnvJumpCode(void *mod, void *buf) {
    (void)mod; (void)buf;
    return 0;
}

s32 RSOGetFarCodeSize(void *mod) {
    (void)mod;
    return 0;
}

s32 RSOLinkFar(void *mod, void *list) {
    (void)mod; (void)list;
    return 0;
}

s32 RSOGetJumpCodeSize(void *mod) {
    (void)mod;
    return 0;
}

s32 RSOMakeJumpCode(void *mod, void *buf) {
    (void)mod; (void)buf;
    return 0;
}

s32 RSOLinkJump(void *mod, void *list) {
    (void)mod; (void)list;
    return 0;
}
