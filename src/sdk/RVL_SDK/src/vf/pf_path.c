#include "types.h"

s32 VFiPFPATH_DoSplitPath(const void *path, void *dir, void *name) {
    (void)path; (void)dir; (void)name;
    return 0;
}

s32 VFiPFPATH_GetNextCharOfPattern(const void *pattern, void *next) {
    (void)pattern; (void)next;
    return 0;
}

s32 VFiPFPATH_DoMatchFileNameWithPattern(const void *name, const void *pattern) {
    (void)name; (void)pattern;
    return 0;
}

s32 VFiPFPATH_MatchFileNameWithPattern(void *vol, const void *name, const void *pattern) {
    (void)vol; (void)name; (void)pattern;
    return 0;
}

s32 VFiPFPATH_cmpTailSFN(const void *sfn, const void *name) {
    (void)sfn; (void)name;
    return 0;
}

void VFiPFPATH_InitTokenOfPath(void *tok) {
    (void)tok;
}

s32 VFiPFPATH_GetNextTokenOfPath(void *tok, void *token) {
    (void)tok; (void)token;
    return 0;
}

s32 VFiPFPATH_SplitPath(const void *path, void *dir, void *name) {
    (void)path; (void)dir; (void)name;
    return 0;
}

s32 VFiPFPATH_SplitPathPattern(const void *path, void *dir, void *pattern) {
    (void)path; (void)dir; (void)pattern;
    return 0;
}

s32 VFiPFPATH_GetVolumeFromPath(const void *path, void *vol) {
    (void)path; (void)vol;
    return 0;
}

s32 VFiPFPATH_putShortName(void *dst, const void *sfn) {
    (void)dst; (void)sfn;
    return 0;
}

s32 VFiPFPATH_getShortName(void *sfn, const void *src) {
    (void)sfn; (void)src;
    return 0;
}

s32 VFiPFPATH_getLongNameformShortName(void *dst, const void *sfn) {
    (void)dst; (void)sfn;
    return 0;
}

s32 VFiPFPATH_GetLengthFromShortname(const void *sfn) {
    (void)sfn;
    return 0;
}

s32 VFiPFPATH_GetLengthFromUnicode(const void *uni) {
    (void)uni;
    return 0;
}

s32 VFiPFPATH_transformFromUnicodeToNormal(void *dst, const void *uni) {
    (void)dst; (void)uni;
    return 0;
}

s32 VFiPFPATH_transformInUnicode(void *dst, const void *src) {
    (void)dst; (void)src;
    return 0;
}

s32 VFiPFPATH_parseShortName(void *vol, void *sfn, const void *name) {
    (void)vol; (void)sfn; (void)name;
    return 0;
}

s32 VFiPFPATH_parseShortNameNumeric(void *vol, void *sfn, const void *name) {
    (void)vol; (void)sfn; (void)name;
    return 0;
}

s32 VFiPFPATH_SetSearchPattern(void *vol, void *sfn, const void *pattern) {
    (void)vol; (void)sfn; (void)pattern;
    return 0;
}

s32 VFiPFPATH_CheckExtShortNameSignature(const void *sfn) {
    (void)sfn;
    return 0;
}

s32 VFiPFPATH_CheckExtShortName(void *vol, void *sfn) {
    (void)vol; (void)sfn;
    return 0;
}

s32 VFiPFPATH_GetExtShortNameIndex(void *vol, const void *sfn, s32 *idx) {
    (void)vol; (void)sfn; (void)idx;
    return 0;
}

s32 VFiPFPATH_AdjustExtShortName(void *vol, void *sfn, s32 idx) {
    (void)vol; (void)sfn; (void)idx;
    return 0;
}
