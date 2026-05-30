#include "types.h"

s32 VFiPFDIR_p_mkdir(void *vol, const void *path) {
    (void)vol; (void)path;
    return 0;
}

s32 VFiPFDIR_p_chdir(void *vol, const void *path) {
    (void)vol; (void)path;
    return 0;
}

s32 VFiPFDIR_p_fstat(void *vol, const void *path, void *stat) {
    (void)vol; (void)path; (void)stat;
    return 0;
}

s32 VFiPFDIR_p_fsnext(void *vol, void *dirent) {
    (void)vol; (void)dirent;
    return 0;
}

s32 VFiPFDIR_p_fsfirst(void *vol, const void *path, void *dirent) {
    (void)vol; (void)path; (void)dirent;
    return 0;
}

s32 VFiPFDIR_FinalizeAllDirs(void *vol) {
    (void)vol;
    return 0;
}

s32 VFiPFDIR_fsfirst(void *vol, const void *path, void *dirent) {
    (void)vol; (void)path; (void)dirent;
    return 0;
}

s32 VFiPFDIR_fsnext(void *vol, void *dirent) {
    (void)vol; (void)dirent;
    return 0;
}

s32 VFiPFDIR_fstat(void *vol, const void *path, void *stat) {
    (void)vol; (void)path; (void)stat;
    return 0;
}

s32 VFiPFDIR_mkdir(void *vol, const void *path) {
    (void)vol; (void)path;
    return 0;
}

s32 VFiPFDIR_chdir(void *vol, const void *path) {
    (void)vol; (void)path;
    return 0;
}
