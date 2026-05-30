#include "types.h"

void VFiPFCLUSTER_InitLastAccessCluster(void *vol) {
    (void)vol;
}

s32 VFiPFCLUSTER_UpdateLastAccessCluster(void *vol, u32 cluster) {
    (void)vol; (void)cluster;
    return 0;
}

void VFiPFCLUSTER_SetLastAccessCluster(void *vol, u32 cluster) {
    (void)vol; (void)cluster;
}

s32 VFiPFCLUSTER_AppendCluster(void *vol, u32 cluster, u32 count, u32 *newCluster) {
    (void)vol; (void)cluster; (void)count; (void)newCluster;
    return 0;
}

s32 VFiPFCLUSTER_AdjustCluster(void *vol, u32 cluster, u32 count) {
    (void)vol; (void)cluster; (void)count;
    return 0;
}

s32 VFiPFCLUSTER_GetAppendSize(void *vol, u32 cluster, u32 *size) {
    (void)vol; (void)cluster; (void)size;
    return 0;
}
