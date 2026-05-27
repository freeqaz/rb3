/* d_vf_sys.c - VF (Virtual Filesystem) system layer for RVL_SDK */

#include <revolution/mem/mem_expHeap.h>
#include <revolution/DVD.h>
#include <revolution/NAND.h>
#include "types.h"

typedef struct MEMiHeapHead *MEMHeapHandle;

/* External symbols from other VF units */
extern s32 VF_nand_sleep_msec;
extern s32 VF_nand_retry_max;

/* Forward declarations of static helpers */
static void *VFiSysCheckExistPrfFileNandFlash(void *devP, void *path, void *unk5, s32 mode);
static s32 VFiSysCheckExistPrfFileNANDFlashCommon(u8 *path, s32 mode);
static void *VFiSysCheckExistPrfFileRam(void *unk3, void *unk4, void *header);
static void *VFiSysCheckExistPrfFileDVD(void *devP, void *path);
static void VFiSysCreateCache(void *handleP, s32 numSectors, MEMHeapHandle heapHandle);

/* External VF internal functions */
extern void VFipf_memset(void *dst, s32 val, s32 size);
extern void VFipf_memcpy(void *dst, void *src, s32 size);
extern s32 VFipf2_errnum(void);
extern s32 VFipf2_format(s8 drv, s32 arg1);
extern s32 VFipf2_create(const char *path, s32 flags);
extern s32 VFipf2_fopen(const char *path, const char *mode);
extern s32 VFipf2_fclose(void *fp);
extern s32 VFipf2_fseek(void *fp, s32 offset, s32 whence);
extern s32 VFipf2_finfo(void *fp, s32 *size);
extern u32 VFipf2_fread(s32 buf, s32 size, s32 count, void *fp);
extern u32 VFipf2_fwrite(const void *buf, s32 size, s32 count, void *fp);
extern s32 VFipf2_fsync(void *fp);
extern s32 VFipf2_remove(const char *path);
extern s32 VFipf2_mkdir(const char *path);
extern s32 VFipf2_chdir(const char *path);
extern s32 VFipf2_fstat(const char *path, void *stat);
extern s32 VFipf2_devinf(s8 drv, void *info);
extern s32 VFipf2_fsfirst(const char *path, const char *pattern, void *finddata);
extern s32 VFipf2_fsnext(void *finddata);
extern s32 VFipf2_unmount(s8 drv, s32 force);
extern s32 VFipf2_detach(s8 drv);
extern void *VFiPFVOL_GetVolumeFromDrvChar(s8 drv);
extern void VFiPFVOL_SetCurrentVolume(void);
extern void *VFiPFVOL_GetCurrentVolume(void);
extern s32 VFipdm_open_disk(void *initInfo, void *diskOut);
extern s32 VFipdm_close_disk(s32 disk);
extern s32 VFipdm_open_partition(s32 disk, s32 partIdx, void *partOut);
extern s32 VFipdm_close_partition(s32 part);
extern void dCommon_initDriveInfo(void);
extern void dCommon_setLastDeviceErrorToDisk(s32 disk, s32 err);
extern void dCommon_setFileSizeToDisk(s32 disk, s32 size);
extern s32 dCommon_FlushFromHandleIdx(u32 idx, s32 flag);
extern u32 dCommon_getHandleIdxFromDisk(s32 disk);
extern s32 dCommon_getLastDeviceErrorFromDisk(s32 disk);
extern s32 dCommon_IsPrfFile(void *header);
extern s32 NAND_CreatePrfFile(s32 attr, u8 *path, s32 size);
extern s32 VFi_NandOpenSp(u8 *path, void *fileOut, s32 mode, s32 flags);
extern s32 VFi_NandGetLength(void *file, u32 *size);
extern s32 VFi_NandRead(void *file, void *buf, s32 size);
extern void VFi_NandClose(void *file);
extern void VFi_NandDelete(void);
extern void VFi_NandSetNANDFuncNormal(void);
extern void VFi_InitSDWrok(s32 a, s32 b);
extern void *VFi_nanddrv_init_drv_tbl;

/* Device init info struct (8 bytes) */
typedef struct {
    void *drvTbl;
    u32 index;
} VFDevInitInfo;

/* Table initializer for NAND flash */
static VFDevInitInfo l_dev_nandflash_init_info = {
    &VFi_nanddrv_init_drv_tbl,
    0
};

/* File exist check function table */
typedef void *(*CheckExistFunc)(void *);

static CheckExistFunc l_check_exist_file[4] = {
    (CheckExistFunc)VFiSysCheckExistPrfFileNandFlash,
    (CheckExistFunc)VFiSysCheckExistPrfFileRam,
    (CheckExistFunc)VFiSysCheckExistPrfFileDVD,
    NULL,
};

/* .sbss variables */
static s32 l_vfsys_vol_max;
static s32 l_vfsys_last_err;
static MEMHeapHandle l_vfsys_exp_heap_handle;
static s32 l_vfsys_dev_table_init;
static void *l_sys_handle_table_p;
static s32 l_timeStampCallback;

/* .bss variables */
static void *l_vfsys_dev_table[0x1A];
static VFDevInitInfo l_dev_init_info_table[0x1A];

void VFSysInit(void *heapStart, u32 heapSize) {
    void *handleP;
    void *handleEnd;
    s32 i;

    s32 vol_from_heap = (s32)(heapSize >> 14);
    if (heapStart != NULL && (s32)heapSize != 0 && l_vfsys_exp_heap_handle == NULL) {
        l_vfsys_exp_heap_handle = MEMCreateExpHeapEx(heapStart, (u32)heapSize, 0);
    }

    {
        s32 volMax = 0x1A;
        if ((u32)vol_from_heap <= 0x1AU) {
            volMax = vol_from_heap;
        }
        l_vfsys_vol_max = volMax;

        if (l_sys_handle_table_p == NULL) {
            void *tbl;
            if (l_vfsys_exp_heap_handle == NULL) {
                tbl = NULL;
            } else {
                tbl = MEMAllocFromExpHeapEx(l_vfsys_exp_heap_handle,
                                            (u32)(volMax * 0x140), 0x20);
            }
            l_sys_handle_table_p = tbl;
        }
    }

    if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
        handleP = l_sys_handle_table_p;
    } else {
        handleP = NULL;
    }

    handleEnd = (char *)handleP + l_vfsys_vol_max * 0x140;
    for (; handleP != handleEnd; handleP = (char *)handleP + 0x140) {
        VFipf_memset(handleP, 0, 0x140);
        if ((char *)handleP + 0x1C != NULL) {
            *(s32 *)((char *)handleP + 0x1C) = 0;
            *(s32 *)((char *)handleP + 0x20) = 0;
            *(s32 *)((char *)handleP + 0x24) = 0;
            *(s32 *)((char *)handleP + 0x28) = 0;
        }
    }

    {
        s32 volCnt = l_vfsys_vol_max;
        if (l_vfsys_dev_table_init == 0) {
            void **dp = l_vfsys_dev_table;
            for (i = 0; (u32)i < (u32)volCnt; i++) {
                void *entry;
                if (l_vfsys_exp_heap_handle == NULL) {
                    entry = NULL;
                } else {
                    entry = MEMAllocFromExpHeapEx(l_vfsys_exp_heap_handle, 0xA0, 0x20);
                }
                *dp++ = entry;
            }
            l_vfsys_dev_table_init = 1;
        }
    }

    dCommon_initDriveInfo();
    l_vfsys_last_err = 0;
    VF_nand_sleep_msec = 2;
    VF_nand_retry_max = 8;
    VFi_InitSDWrok(2, 0);
}

void VFSysFinalize(void) {
    void *handleP;
    void *end;
    void *cacheP;
    MEMHeapHandle cacheHeap;
    s32 volMax;
    s32 i;

    handleP = l_sys_handle_table_p;
    if (handleP != NULL) {
        if (l_vfsys_vol_max == 0 || handleP == NULL) {
            handleP = NULL;
        }

        if (handleP != NULL) {
            end = (char *)handleP + l_vfsys_vol_max * 0x140;
            for (; handleP != end; handleP = (char *)handleP + 0x140) {
                if (handleP != NULL) {
                    cacheP = (char *)handleP + 0x1C;
                    cacheHeap = *(MEMHeapHandle *)cacheP;
                    if (cacheP != NULL) {
                        if (*(void **)((char *)cacheP + 0x8) != NULL && cacheHeap != NULL) {
                            MEMFreeToExpHeap(cacheHeap, *(void **)((char *)cacheP + 0x8));
                        }
                        if (*(void **)((char *)cacheP + 0xC) != NULL && *(MEMHeapHandle *)cacheP != NULL) {
                            MEMFreeToExpHeap(*(MEMHeapHandle *)cacheP, *(void **)((char *)cacheP + 0xC));
                        }
                        if (cacheP != NULL) {
                            *(s32 *)cacheP = 0;
                            *(s32 *)((char *)cacheP + 0x4) = 0;
                            *(s32 *)((char *)cacheP + 0x8) = 0;
                            *(s32 *)((char *)cacheP + 0xC) = 0;
                        }
                    }
                    if ((u32)cacheHeap != (u32)l_vfsys_exp_heap_handle && cacheHeap != NULL) {
                        MEMDestroyExpHeap(cacheHeap);
                    }
                }
            }
        }

        if (l_vfsys_exp_heap_handle != NULL) {
            MEMFreeToExpHeap(l_vfsys_exp_heap_handle, l_sys_handle_table_p);
        }
        l_sys_handle_table_p = NULL;
    }

    volMax = l_vfsys_vol_max;
    if (l_vfsys_dev_table_init != 0) {
        for (i = 0; (u32)i < (u32)volMax; i++) {
            if (l_vfsys_dev_table[i] != NULL) {
                if (l_vfsys_exp_heap_handle != NULL) {
                    MEMFreeToExpHeap(l_vfsys_exp_heap_handle, l_vfsys_dev_table[i]);
                }
                l_vfsys_dev_table[i] = NULL;
            }
        }
        l_vfsys_dev_table_init = 0;
    }

    if (l_vfsys_exp_heap_handle != NULL) {
        MEMDestroyExpHeap(l_vfsys_exp_heap_handle);
        l_vfsys_exp_heap_handle = NULL;
    }
    l_vfsys_vol_max = 0;
}

void VFSysSetNandFuncNormal(void) {
    VFi_NandSetNANDFuncNormal();
}

s32 VFSysCreatePrfFileNANDFlash(u8 *path, s32 attr) {
    u8 convPath[0xFF];
    u8 *dst = convPath;
    u8 *src = path;

    VFipf_memset(convPath, 0, 0xFF);
    {
        s32 i = 0;
        while (i < 0xFF && (s8)*src != 0) {
            if ((s8)*src == '\\') {
                *dst = '/';
            } else {
                *dst = *src;
            }
            src++;
            dst++;
            i++;
        }
    }

    if (VFiSysCheckExistPrfFileNANDFlashCommon(convPath, -0xA) == (s32)0xB001) {
        return NAND_CreatePrfFile(attr, convPath, 0x100);
    }
    return 0;
}

void VFSysDeletePrfFileNANDFlash(void) {
    VFi_NandDelete();
}

void *VFSysCheckExistPrfFile(u32 idx) {
    void *handleP;
    void *devP;
    u32 devType;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }

    if (handleP == NULL || *(void **)handleP == NULL) {
        return (void *)0xB003;
    }

    devP = *(void **)handleP;
    devType = *(u32 *)((char *)devP + 0x8);
    if (devType < 4U) {
        if (l_check_exist_file[devType] != NULL) {
            return l_check_exist_file[devType](devP);
        }
        return NULL;
    }
    return (void *)-1;
}

static void *VFiSysCheckExistPrfFileNandFlash(void *devP, void *path, void *unk5, s32 mode) {
    (void)devP;
    return (void *)(s32)VFiSysCheckExistPrfFileNANDFlashCommon((u8 *)path, mode);
}

static s32 VFiSysCheckExistPrfFileNANDFlashCommon(u8 *path, s32 mode) {
    u8 convPath[0xFF];
    u8 fileBuf[0x20];
    void *nandFile[0x40]; /* NAND file handle */
    u8 *src = path;
    u8 *dst = convPath;
    s32 i = 0;
    u32 fileSize = 0;
    s32 ret = 0xB001;

    VFipf_memset(convPath, 0, 0xFF);
    while (i < 0xFF && (s8)*src != 0) {
        if ((s8)*src == '\\') {
            *dst = '/';
        } else {
            *dst = *src;
        }
        src++;
        dst++;
        i++;
    }

    if (VFi_NandOpenSp(convPath, nandFile, 1, mode) == 0) {
        s32 lenRet = VFi_NandGetLength(nandFile, &fileSize);
        if (VFi_NandRead(nandFile, fileBuf, 0x20) >= 0 &&
            lenRet >= 0 &&
            dCommon_IsPrfFile(fileBuf) != 0) {
            ret = 0;
        } else {
            ret = 0xB006;
        }
        VFi_NandClose(nandFile);
    }
    return ret;
}

static void *VFiSysCheckExistPrfFileRam(void *unk3, void *unk4, void *header) {
    (void)unk3;
    (void)unk4;
    s32 result = dCommon_IsPrfFile(header);
    return (void *)(s32)(0xB006 & ~((-result | result) >> 0x1F));
}

static void *VFiSysCheckExistPrfFileDVD(void *devP, void *path) {
    u8 convPath[0xFF];
    u8 fileBuf[0x20];
    DVDFileInfo dvdFile;
    u8 *src = (u8 *)path;
    u8 *dst = convPath;
    s32 i = 0;
    s32 ret = 0xB001;

    (void)devP;
    VFipf_memset(convPath, 0, 0xFF);
    while (i < 0xFF && (s8)*src != 0) {
        if ((s8)*src == '/') {
            *dst = '\\';
        } else {
            *dst = *src;
        }
        src++;
        dst++;
        i++;
    }

    if (DVDOpen((char *)convPath, &dvdFile) != 0) {
        if (DVDReadPrio(&dvdFile, fileBuf, 0x20, 0, 2) != 0 &&
            dCommon_IsPrfFile(fileBuf) != 0) {
            ret = 0;
        } else {
            ret = 0xB006;
        }
        DVDClose(&dvdFile);
    }
    return (void *)(s32)ret;
}

static void VFiSysCreateCache(void *handleP, s32 numSectors, MEMHeapHandle heapHandle) {
    void *cacheP;
    s32 numSectors2;
    s32 bufSize1;
    s32 bufSize2;
    void *buf1;
    void *buf2;

    if (handleP == NULL) return;

    cacheP = (char *)handleP + 0x1C;
    numSectors2 = numSectors * 2;

    if (*(s32 *)((char *)cacheP + 0x8) != 0) return;
    if (*(s32 *)((char *)cacheP + 0xC) != 0) return;

    bufSize2 = numSectors2 << 9;
    bufSize1 = numSectors2 * 0x28;

    if (heapHandle == NULL) {
        buf1 = NULL;
    } else {
        buf1 = MEMAllocFromExpHeapEx(heapHandle, (u32)bufSize1, 4);
    }
    *(void **)((char *)cacheP + 0x8) = buf1;

    if (heapHandle == NULL) {
        buf2 = NULL;
    } else {
        buf2 = MEMAllocFromExpHeapEx(heapHandle, (u32)bufSize2, 4);
    }
    *(void **)((char *)cacheP + 0xC) = buf2;

    if (*(void **)((char *)cacheP + 0x8) != NULL && buf2 != NULL) {
        VFipf_memset(*(void **)((char *)cacheP + 0x8), 0, bufSize1);
        VFipf_memset(*(void **)((char *)cacheP + 0xC), 0, bufSize2);
        *(MEMHeapHandle *)cacheP = heapHandle;
        *(s32 *)((char *)cacheP + 0x4) = numSectors;
        return;
    }

    if (cacheP != NULL) {
        if (*(void **)((char *)cacheP + 0x8) != NULL && *(MEMHeapHandle *)cacheP != NULL) {
            MEMFreeToExpHeap(*(MEMHeapHandle *)cacheP, *(void **)((char *)cacheP + 0x8));
        }
        if (*(void **)((char *)cacheP + 0xC) != NULL && *(MEMHeapHandle *)cacheP != NULL) {
            MEMFreeToExpHeap(*(MEMHeapHandle *)cacheP, *(void **)((char *)cacheP + 0xC));
        }
        if (cacheP != NULL) {
            *(s32 *)cacheP = 0;
            *(s32 *)((char *)cacheP + 0x4) = 0;
            *(s32 *)((char *)cacheP + 0x8) = 0;
            *(s32 *)((char *)cacheP + 0xC) = 0;
        }
    }
}

/* Inline helper: find handle from fp */
#define FIND_HANDLE_FROM_FP(fp, out_r6, out_vol) \
    do { \
        void *_bpb; \
        (out_r6) = 0; \
        if ((fp) != NULL) { \
            _bpb = *(void **)((char *)(fp) + 0x8); \
            if (_bpb != NULL) { \
                (out_vol) = *(void **)((char *)_bpb + 0x34); \
                if ((out_vol) != NULL) { \
                    void *_p, *_end; \
                    if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) { \
                        _p = l_sys_handle_table_p; \
                    } else { \
                        _p = NULL; \
                    } \
                    _end = (char *)_p + l_vfsys_vol_max * 0x140; \
                    for (; (u32)_p != (u32)_end; _p = (char *)_p + 0x140) { \
                        if (*(s32 *)_p != 0 && (s8)*(char *)((char *)_p + 0x14) == (s8)*(char *)((char *)(out_vol) + 0x1862)) { \
                            (out_r6) = _p; \
                            break; \
                        } \
                    } \
                } \
            } \
        } \
    } while (0)

s32 VFSysFileSync(void *fp) {
    void *driveP1;
    void *driveP2;
    void *handleP;
    void *volP;
    void *devP;
    u32 driveIdx;
    u32 savedWriteBack;
    void *end;
    void *end2;

    driveP1 = 0;
    if (fp != NULL) {
        void *bpb = *(void **)((char *)fp + 0x8);
        if (bpb != NULL) {
            volP = *(void **)((char *)bpb + 0x34);
            if (volP != NULL) {
                if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
                    driveP1 = l_sys_handle_table_p;
                } else {
                    driveP1 = NULL;
                }
                end = (char *)driveP1 + l_vfsys_vol_max * 0x140;
                for (; (u32)driveP1 != (u32)end; driveP1 = (char *)driveP1 + 0x140) {
                    if (*(s32 *)driveP1 != 0 && (s8)*(char *)((char *)driveP1 + 0x14) == (s8)*(char *)((char *)volP + 0x1862)) {
                        break;
                    }
                }
                if ((u32)driveP1 == (u32)end) {
                    driveP1 = NULL;
                }
            }
        }
    }

    /* Find drive index */
    if (driveP1 != NULL) {
        void *p;
        void *pend;
        if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
            p = l_sys_handle_table_p;
        } else {
            p = NULL;
        }
        driveIdx = 0;
        pend = (char *)p + l_vfsys_vol_max * 0x140;
        for (; (u32)p != (u32)pend; p = (char *)p + 0x140) {
            if ((u32)driveP1 == (u32)p) {
                break;
            }
            driveIdx++;
        }
    } else {
        driveIdx = (u32)-1;
    }

    /* Find drive from fp for device error reset */
    driveP2 = 0;
    if (fp != NULL) {
        void *bpb2 = *(void **)((char *)fp + 0x8);
        if (bpb2 != NULL) {
            volP = *(void **)((char *)bpb2 + 0x34);
            if (volP != NULL) {
                if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
                    driveP2 = l_sys_handle_table_p;
                } else {
                    driveP2 = NULL;
                }
                end2 = (char *)driveP2 + l_vfsys_vol_max * 0x140;
                for (; (u32)driveP2 != (u32)end2; driveP2 = (char *)driveP2 + 0x140) {
                    if (*(s32 *)driveP2 != 0 && (s8)*(char *)((char *)driveP2 + 0x14) == (s8)*(char *)((char *)volP + 0x1862)) {
                        break;
                    }
                }
                if ((u32)driveP2 == (u32)end2) {
                    driveP2 = NULL;
                }
            }
        }
    }

    if (driveP2 != NULL && *(s32 *)((char *)driveP2 + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)driveP2 + 0x8), 0);
    }

    if ((u32)driveIdx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + driveIdx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        devP = *(void **)handleP;
        if (devP != NULL) {
            savedWriteBack = *(u32 *)((char *)devP + 0xC);
        } else {
            savedWriteBack = 0;
        }
    } else {
        savedWriteBack = 0;
    }

    if ((u32)driveIdx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + driveIdx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        devP = *(void **)handleP;
        if (devP != NULL) {
            *(s32 *)((char *)devP + 0xC) = 1;
        }
    }

    if (VFipf2_fsync(fp) == 0) {
        if ((u32)driveIdx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
            handleP = (char *)l_sys_handle_table_p + driveIdx * 0x140;
        } else {
            handleP = NULL;
        }
        if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
            devP = *(void **)handleP;
            if (devP != NULL) {
                *(s32 *)((char *)devP + 0xC) = 0;
            }
        }

        if (dCommon_FlushFromHandleIdx(driveIdx, 1) == 0) {
            if ((u32)driveIdx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
                handleP = (char *)l_sys_handle_table_p + driveIdx * 0x140;
            } else {
                handleP = NULL;
            }
            if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
                devP = *(void **)handleP;
                if (devP != NULL && savedWriteBack <= 1U) {
                    *(u32 *)((char *)devP + 0xC) = savedWriteBack;
                }
            }
            return 0;
        }

        if ((u32)driveIdx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
            handleP = (char *)l_sys_handle_table_p + driveIdx * 0x140;
        } else {
            handleP = NULL;
        }
        if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
            devP = *(void **)handleP;
            if (devP != NULL && savedWriteBack <= 1U) {
                *(u32 *)((char *)devP + 0xC) = savedWriteBack;
            }
        }
        return 5;
    }

    if ((u32)driveIdx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + driveIdx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        devP = *(void **)handleP;
        if (devP != NULL) {
            *(s32 *)((char *)devP + 0xC) = 0;
        }
    }

    dCommon_FlushFromHandleIdx(driveIdx, 1);

    if ((u32)driveIdx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + driveIdx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        devP = *(void **)handleP;
        if (devP != NULL && savedWriteBack <= 1U) {
            *(u32 *)((char *)devP + 0xC) = savedWriteBack;
        }
    }
    return VFipf2_errnum();
}

s32 VFSysMountDrv(u32 idx, u8 *path, s32 pf2arg) {
    void *handleP;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP == NULL || *(void **)handleP == NULL) {
        return 0xB003;
    }
    if (*(s32 *)((char *)handleP + 0x8) == 0) {
        return 0xB003;
    }
    if (*(void **)((char *)handleP + 0x10) != NULL) {
        return 0xB005;
    }

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }

    *(s32 *)((char *)handleP + 0x2C) = *(s32 *)((char *)handleP + 0x24);
    *(s32 *)((char *)handleP + 0x30) = *(s32 *)((char *)handleP + 0x28);
    *(s16 *)((char *)handleP + 0x34) = *(s32 *)((char *)handleP + 0x20);
    *(s16 *)((char *)handleP + 0x36) = *(s32 *)((char *)handleP + 0x20);
    *(s32 *)((char *)handleP + 0x38) = 1;

    if (VFipdm_open_partition(*(s32 *)((char *)handleP + 0x8), 0, (char *)handleP + 0x18) == 0) {
        u32 devType;
        u8 *src;
        u8 *dst;
        *(s32 *)((char *)handleP + 0xC) = *(s32 *)((char *)handleP + 0x18);
        *(void **)((char *)handleP + 0x10) = (char *)handleP + 0x2C;

        devType = *(u32 *)((char *)(*(void **)handleP) + 0x8);
        src = path;

        switch (devType) {
        case 3:
            break;
        case 1:
            *(s32 *)((char *)handleP + 0x4) = pf2arg;
            break;
        case 0:
            dst = (u8 *)((char *)handleP + 0x40);
            *(s32 *)((char *)handleP + 0x4) = (s32)((char *)l_vfsys_dev_table[idx] + 0x10);
            VFipf_memset(dst, 0, 0xFF);
            {
                s32 i = 0;
                for (; i < 0xFF && (s8)*src != 0; i++) {
                    if ((s8)*src == '\\') {
                        *dst = '/';
                    } else {
                        *dst = *src;
                    }
                    src++;
                    dst++;
                }
            }
            break;
        case 2:
            dst = (u8 *)((char *)handleP + 0x40);
            *(s32 *)((char *)handleP + 0x4) = (s32)((char *)l_vfsys_dev_table[idx] + 0x10);
            VFipf_memset(dst, 0, 0xFF);
            {
                s32 i = 0;
                for (; i < 0xFF && (s8)*src != 0; i++) {
                    if ((s8)*src == '/') {
                        *dst = '\\';
                    } else {
                        *dst = *src;
                    }
                    src++;
                    dst++;
                }
            }
            break;
        default:
            dst = (u8 *)((char *)handleP + 0x40);
            VFipf_memset(dst, 0, 0xFF);
            {
                s32 i = 0;
                for (; i < 0xFF && (s8)*src != 0; i++) {
                    if ((s8)*src == '/') {
                        *dst = '\\';
                    } else {
                        *dst = *src;
                    }
                    src++;
                    dst++;
                }
            }
            break;
        }

        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
        dCommon_setFileSizeToDisk(*(s32 *)((char *)handleP + 0x8), 0x19000);
        return 0;
    }
    return VFipf2_errnum();
}

s32 VFSysUnmountDrv(u32 idx, s32 force) {
    void *handleP;
    s32 ret;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP == NULL || *(s32 *)handleP == 0) {
        return 0xB003;
    }
    if (*(s32 *)((char *)handleP + 0x8) == 0) {
        return 0xB003;
    }

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }

    ret = VFipf2_unmount((s8)*(char *)((char *)handleP + 0x14), force);
    if ((force == 0 && ret != 0) || ((u32)force == 1U && ret != 0 && ret != 1)) {
        return VFipf2_errnum();
    }

    if (VFipf2_detach((s8)*(char *)((char *)handleP + 0x14)) != 0) {
        return VFipf2_errnum();
    }

    if (VFipdm_close_partition(*(s32 *)((char *)handleP + 0x18)) == 0) {
        dCommon_setFileSizeToDisk(*(s32 *)((char *)handleP + 0x8), 0);
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
        *(s32 *)((char *)handleP + 0xC) = 0;
        *(s32 *)((char *)handleP + 0x10) = 0;
        *(s32 *)((char *)handleP + 0x4) = 0;
        VFipf_memset((char *)handleP + 0x40, 0, 0xFF);
        return 0;
    }
    return VFipf2_errnum();
}

void *VFSysGetHandleP(u32 idx) {
    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        return (char *)l_sys_handle_table_p + idx * 0x140;
    }
    return NULL;
}

s32 VFSysHandleP2Idx(void *handleP) {
    void *end;
    void *p;
    s32 i;

    if (handleP == NULL) goto notfound;

    if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
        p = l_sys_handle_table_p;
    } else {
        p = NULL;
    }

    i = 0;
    end = (char *)p + l_vfsys_vol_max * 0x140;
    for (; (u32)p != (u32)end; p = (char *)p + 0x140) {
        if ((u32)handleP == (u32)p) {
            return i;
        }
        i++;
    }
notfound:
    return -1;
}

s32 VFSysPDMDisk2HandleIdx(s32 disk) {
    void *p;
    s32 i;
    s32 result;

    result = -1;
    if (disk == 0) goto done;

    if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
        p = l_sys_handle_table_p;
    } else {
        p = NULL;
    }

    i = 0;
    for (; (u32)i < (u32)l_vfsys_vol_max; p = (char *)p + 0x140, i++) {
        if ((u32)disk == *(u32 *)((char *)p + 0x8)) {
            result = i;
            goto done;
        }
    }
done:
    return result;
}

s32 VFSysSetDeviceNANDFlash(u32 *idxOut, s32 heapStart, u32 heapSize, s32 numCacheSectors) {
    void *p;
    void *handleP;
    void *devEntryP;
    void *pend;
    u32 idx;
    MEMHeapHandle expHeap;
    s32 numSectors;
    MEMHeapHandle newHeap;

    /* Find free slot */
    if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
        p = l_sys_handle_table_p;
    } else {
        p = NULL;
    }

    idx = 0;
    pend = (char *)p + l_vfsys_vol_max * 0x140;
    for (; (u32)p != (u32)pend; p = (char *)p + 0x140) {
        if (*(s32 *)p == 0) {
            break;
        }
        idx++;
    }
    if ((u32)p == (u32)pend) {
        idx = (u32)-1;
    }

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }
    *idxOut = (u32)-1;

    if (handleP == NULL) {
        return 0xB002;
    }

    VFDevInitInfo *initInfo = &l_dev_init_info_table[idx];
    VFipf_memcpy(initInfo, &l_dev_nandflash_init_info, 8);
    initInfo->index = idx;

    if (VFipdm_open_disk(initInfo, (char *)handleP + 8) == 0) {
        devEntryP = l_vfsys_dev_table[idx];
        VFipf_memset(devEntryP, 0, 0x10);
        *(s32 *)devEntryP = 0;
        *(s32 *)((char *)devEntryP + 0xC) = 0;
        *(s32 *)((char *)devEntryP + 0x8) = 0;
        *(void **)handleP = devEntryP;
        *idxOut = idx;

        if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
            dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
        }

        if (handleP != NULL) {
            expHeap = l_vfsys_exp_heap_handle;
            numSectors = 8;
            if (heapStart != 0 && heapSize > 0x2800U) {
                newHeap = MEMCreateExpHeapEx((void *)heapStart, heapSize, 0);
                if (newHeap != NULL) {
                    expHeap = newHeap;
                    numSectors = (s32)(heapSize / 1280);
                }
            }
            VFiSysCreateCache(handleP, numSectors, expHeap);
        }
        return 0;
    }

    VFipf_memset(handleP, 0, 0x140);
    if ((char *)handleP + 0x1C != NULL) {
        *(s32 *)((char *)handleP + 0x1C) = 0;
        *(s32 *)((char *)handleP + 0x20) = 0;
        *(s32 *)((char *)handleP + 0x24) = 0;
        *(s32 *)((char *)handleP + 0x28) = 0;
    }
    return VFipf2_errnum();
}

s32 VFSysUnsetDevice(u32 idx) {
    void *handleP;
    void *cacheP;
    MEMHeapHandle cacheHeap;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP == NULL || *(s32 *)((char *)handleP + 0x8) == 0) {
        return 0xB003;
    }

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }

    if (VFipdm_close_disk(*(s32 *)((char *)handleP + 0x8)) != 0) {
        return VFipf2_errnum();
    }

    if (handleP != NULL) {
        cacheP = (char *)handleP + 0x1C;
        cacheHeap = *(MEMHeapHandle *)cacheP;
        if (cacheP != NULL) {
            if (*(void **)((char *)cacheP + 0x8) != NULL && (s32)cacheHeap != 0) {
                MEMFreeToExpHeap(l_vfsys_exp_heap_handle, *(void **)((char *)cacheP + 0x8));
            }
            if (*(void **)((char *)cacheP + 0xC) != NULL && *(MEMHeapHandle *)cacheP != NULL) {
                MEMFreeToExpHeap(l_vfsys_exp_heap_handle, *(void **)((char *)cacheP + 0xC));
            }
            if (cacheP != NULL) {
                *(s32 *)cacheP = 0;
                *(s32 *)((char *)cacheP + 0x4) = 0;
                *(s32 *)((char *)cacheP + 0x8) = 0;
                *(s32 *)((char *)cacheP + 0xC) = 0;
            }
        }
        if ((u32)cacheHeap != (u32)l_vfsys_exp_heap_handle && (s32)cacheHeap != 0) {
            MEMDestroyExpHeap(cacheHeap);
        }
    }

    VFipf_memset(handleP, 0, 0x140);
    if ((char *)handleP + 0x1C != NULL) {
        *(s32 *)((char *)handleP + 0x1C) = 0;
        *(s32 *)((char *)handleP + 0x20) = 0;
        *(s32 *)((char *)handleP + 0x24) = 0;
        *(s32 *)((char *)handleP + 0x28) = 0;
    }
    return 0;
}

void *VFSysGetDriveP(u32 idx) {
    void *handleP;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP != NULL) {
        return (char *)handleP + 4;
    }
    return NULL;
}

void *VFSysPDMDisk2DriveP(s32 disk) {
    void *handleP;
    u32 idx;
    void *result;

    if (disk != 0) {
        idx = dCommon_getHandleIdxFromDisk(disk);
        if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
            handleP = (char *)l_sys_handle_table_p + idx * 0x140;
        } else {
            handleP = NULL;
        }
        if (handleP != NULL) {
            result = (char *)handleP + 4;
        } else {
            result = NULL;
        }
    } else {
        result = NULL;
    }
    return result;
}

s32 VFSysFormatDrive(u32 idx) {
    void *handleP;
    void *devP;
    u32 savedWriteBack;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP == NULL || *(s32 *)handleP == 0) {
        return 0xB003;
    }
    if (*(s32 *)((char *)handleP + 0x8) == 0) {
        return 0xB003;
    }

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }

    if ((u32)idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        devP = *(void **)handleP;
        if (devP != NULL) {
            savedWriteBack = *(u32 *)((char *)devP + 0xC);
        } else {
            savedWriteBack = 0;
        }
    } else {
        savedWriteBack = 0;
    }

    if ((u32)idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        devP = *(void **)handleP;
        if (devP != NULL) {
            *(s32 *)((char *)devP + 0xC) = 1;
        }
    }

    if ((u32)idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }
    if (VFipf2_format((s8)*(char *)((char *)handleP + 0x14), 0) == 0) {
        if ((u32)idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
            handleP = (char *)l_sys_handle_table_p + idx * 0x140;
        } else {
            handleP = NULL;
        }
        if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
            devP = *(void **)handleP;
            if (devP != NULL) {
                *(s32 *)((char *)devP + 0xC) = 0;
            }
        }

        if (dCommon_FlushFromHandleIdx(idx, 1) == 0) {
            if ((u32)idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
                handleP = (char *)l_sys_handle_table_p + idx * 0x140;
            } else {
                handleP = NULL;
            }
            if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
                devP = *(void **)handleP;
                if (devP != NULL && savedWriteBack <= 1U) {
                    *(u32 *)((char *)devP + 0xC) = savedWriteBack;
                }
            }
            return 0;
        }

        if ((u32)idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
            handleP = (char *)l_sys_handle_table_p + idx * 0x140;
        } else {
            handleP = NULL;
        }
        if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
            devP = *(void **)handleP;
            if (devP != NULL && savedWriteBack <= 1U) {
                *(u32 *)((char *)devP + 0xC) = savedWriteBack;
            }
        }
        return 5;
    }

    if ((u32)idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        devP = *(void **)handleP;
        if (devP != NULL) {
            *(s32 *)((char *)devP + 0xC) = 0;
        }
    }

    dCommon_FlushFromHandleIdx(idx, 1);

    if ((u32)idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        devP = *(void **)handleP;
        if (devP != NULL && savedWriteBack <= 1U) {
            *(u32 *)((char *)devP + 0xC) = savedWriteBack;
        }
    }
    return VFipf2_errnum();
}

s32 VFSysCreateFile(u32 idx, const char *path) {
    void *handleP;
    s32 result;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }

    if (handleP != NULL) {
        void *vol = VFiPFVOL_GetVolumeFromDrvChar((s8)*(char *)((char *)handleP + 0x14));
        if (vol != NULL) {
            VFiPFVOL_SetCurrentVolume();
            result = 0;
            goto createfile_done;
        } else {
            result = VFipf2_errnum();
            if (result != 0) goto createfile_done;
            result = -1;
            goto createfile_done;
        }
    } else {
        result = 0xB003;
    }

createfile_done:
    if (result == 0) {
        s32 ret;
        if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
            handleP = (char *)l_sys_handle_table_p + idx * 0x140;
        } else {
            handleP = NULL;
        }
        if (handleP != NULL) {
            if (*(s32 *)((char *)handleP + 0x8) != 0) {
                dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
            }
        }
        ret = VFipf2_create(path, 0);
        if (ret == 0) {
            s32 errNum = VFipf2_errnum();
            if (errNum != 0) {
                l_vfsys_last_err = errNum;
            }
        }
        return ret;
    }
    l_vfsys_last_err = result;
    return 0;
}

s32 VFSysCreateFile_current(const char *path) {
    void *curVol;
    void *p;
    void *end;
    void *handleP;
    s32 ret;

    curVol = VFiPFVOL_GetCurrentVolume();
    if (curVol != NULL) {
        if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
            p = l_sys_handle_table_p;
        } else {
            p = NULL;
        }
        end = (char *)p + l_vfsys_vol_max * 0x140;
        for (; (u32)p != (u32)end; p = (char *)p + 0x140) {
            if (*(s32 *)p == 0 || (s8)*(char *)((char *)p + 0x14) != (s8)*(char *)((char *)curVol + 0x1862))
                continue;
            goto crfile_cur_found;
        }
    }
    handleP = NULL;
    goto crfile_cur_done;
crfile_cur_found:
    handleP = p;
crfile_cur_done:

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }
    ret = VFipf2_create(path, 0);
    if (ret == 0) {
        s32 errNum = VFipf2_errnum();
        if (errNum != 0) {
            l_vfsys_last_err = errNum;
        }
    }
    return ret;
}

s32 VFSysOpenFile(u32 idx, const char *path, const char *mode) {
    void *handleP;
    s32 result;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }

    if (handleP != NULL) {
        void *vol = VFiPFVOL_GetVolumeFromDrvChar((s8)*(char *)((char *)handleP + 0x14));
        if (vol != NULL) {
            VFiPFVOL_SetCurrentVolume();
            result = 0;
            goto openfile_done;
        } else {
            result = VFipf2_errnum();
            if (result != 0) goto openfile_done;
            result = -1;
            goto openfile_done;
        }
    } else {
        result = 0xB003;
    }

openfile_done:
    if (result == 0) {
        s32 ret;
        if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
            handleP = (char *)l_sys_handle_table_p + idx * 0x140;
        } else {
            handleP = NULL;
        }
        if (handleP != NULL) {
            if (*(s32 *)((char *)handleP + 0x8) != 0) {
                dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
            }
        }
        ret = VFipf2_fopen(path, mode);
        if (ret == 0) {
            s32 errNum = VFipf2_errnum();
            if (errNum != 0) {
                l_vfsys_last_err = errNum;
            }
        }
        return ret;
    }
    l_vfsys_last_err = result;
    return 0;
}

s32 VFSysOpenFile_current(const char *path, const char *mode) {
    void *curVol;
    void *p;
    void *end;
    void *handleP;
    s32 ret;

    curVol = VFiPFVOL_GetCurrentVolume();
    if (curVol != NULL) {
        if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
            p = l_sys_handle_table_p;
        } else {
            p = NULL;
        }
        end = (char *)p + l_vfsys_vol_max * 0x140;
        for (; (u32)p != (u32)end; p = (char *)p + 0x140) {
            if (*(s32 *)p == 0) continue;
            if ((s8)*(char *)((char *)p + 0x14) != (s8)*(char *)((char *)curVol + 0x1862)) continue;
            goto openfile_cur_found;
        }
    }
    handleP = NULL;
    goto openfile_cur_done;
openfile_cur_found:
    handleP = p;
openfile_cur_done:

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }
    ret = VFipf2_fopen(path, mode);
    if (ret == 0) {
        s32 errNum = VFipf2_errnum();
        if (errNum != 0) {
            l_vfsys_last_err = errNum;
        }
    }
    return ret;
}

s32 VFSysCloseFile(void *fp) {
    void *curVol;
    void *p;
    void *end;
    void *handleP;

    handleP = 0;
    if (fp != NULL) {
        void *bpb = *(void **)((char *)fp + 0x8);
        if (bpb != NULL) {
            curVol = *(void **)((char *)bpb + 0x34);
            if (curVol != NULL) {
                if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
                    p = l_sys_handle_table_p;
                } else {
                    p = NULL;
                }
                end = (char *)p + l_vfsys_vol_max * 0x140;
                for (; (u32)p != (u32)end; p = (char *)p + 0x140) {
                    if (*(s32 *)p != 0 && (s8)*(char *)((char *)p + 0x14) == (s8)*(char *)((char *)curVol + 0x1862)) {
                        handleP = p;
                        break;
                    }
                }
            }
        }
    }

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }
    if (VFipf2_fclose(fp) == 0) {
        return 0;
    }
    return VFipf2_errnum();
}

s32 VFSysSeekFile(void *fp, s32 offset, s32 whence) {
    void *curVol;
    void *p;
    void *end;
    void *handleP;

    handleP = 0;
    if (fp != NULL) {
        void *bpb = *(void **)((char *)fp + 0x8);
        if (bpb != NULL) {
            curVol = *(void **)((char *)bpb + 0x34);
            if (curVol != NULL) {
                if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
                    p = l_sys_handle_table_p;
                } else {
                    p = NULL;
                }
                end = (char *)p + l_vfsys_vol_max * 0x140;
                for (; (u32)p != (u32)end; p = (char *)p + 0x140) {
                    if (*(s32 *)p == 0 || (s8)*(char *)((char *)p + 0x14) != (s8)*(char *)((char *)curVol + 0x1862))
                        continue;
                    handleP = p;
                    break;
                }
            }
        }
    }

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }
    if (VFipf2_fseek(fp, offset, whence) == 0) {
        return 0;
    }
    return VFipf2_errnum();
}

s32 VFSysReadFile(u32 *bytesRead, void *buf, u32 size, void *fp) {
    u32 readSize = size;
    s32 fileSize;
    void *curVol;
    void *p;
    void *end;
    void *handleP;

    if (bytesRead != NULL) {
        *bytesRead = 0U;
    }

    if (VFipf2_finfo(fp, &fileSize) == 0) {
        u32 remaining = (u32)fileSize - *(u32 *)((char *)fp + 0x20);
        if (readSize > remaining) {
            VFipf_memset(buf, 0, (s32)readSize);
            readSize = remaining;
        }

        handleP = 0;
        if (fp != NULL) {
            void *bpb = *(void **)((char *)fp + 0x8);
            if (bpb != NULL) {
                curVol = *(void **)((char *)bpb + 0x34);
                if (curVol != NULL) {
                    if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
                        p = l_sys_handle_table_p;
                    } else {
                        p = NULL;
                    }
                    end = (char *)p + l_vfsys_vol_max * 0x140;
                    for (; (u32)p != (u32)end; p = (char *)p + 0x140) {
                        if (*(s32 *)p != 0 && (s8)*(char *)((char *)p + 0x14) == (s8)*(char *)((char *)curVol + 0x1862)) {
                            handleP = p;
                            break;
                        }
                    }
                }
            }
        }
        if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
            dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
        }

        if ((s32)readSize != 0) {
            if (VFipf2_fread((s32)buf, (s32)readSize, 1, fp) == 1U) {
                if (bytesRead != NULL) {
                    *bytesRead = readSize;
                }
                return 0;
            }
            return VFipf2_errnum();
        }
        return 0;
    }
    return VFipf2_errnum();
}

s32 VFSysWriteFile(const void *buf, u32 size, void *fp) {
    void *curVol;
    void *p;
    void *end;
    void *handleP;

    handleP = 0;
    if (fp != NULL) {
        void *bpb = *(void **)((char *)fp + 0x8);
        if (bpb != NULL) {
            curVol = *(void **)((char *)bpb + 0x34);
            if (curVol != NULL) {
                if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
                    p = l_sys_handle_table_p;
                } else {
                    p = NULL;
                }
                end = (char *)p + l_vfsys_vol_max * 0x140;
                for (; (u32)p != (u32)end; p = (char *)p + 0x140) {
                    if (*(s32 *)p != 0 && (s8)*(char *)((char *)p + 0x14) == (s8)*(char *)((char *)curVol + 0x1862)) {
                        handleP = p;
                        break;
                    }
                }
            }
        }
    }

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }
    if (VFipf2_fwrite(buf, (s32)size, 1, fp) == 1U) {
        return 0;
    }
    return VFipf2_errnum();
}

s32 VFSysDeleteFile(u32 idx, const char *path) {
    void *handleP;
    s32 result;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }

    if (handleP != NULL) {
        void *vol = VFiPFVOL_GetVolumeFromDrvChar((s8)*(char *)((char *)handleP + 0x14));
        if (vol != NULL) {
            VFiPFVOL_SetCurrentVolume();
            result = 0;
            goto deletefile_done;
        } else {
            result = VFipf2_errnum();
            if (result != 0) goto deletefile_done;
            result = -1;
            goto deletefile_done;
        }
    } else {
        result = 0xB003;
    }

deletefile_done:
    if (result == 0) {
        if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
            handleP = (char *)l_sys_handle_table_p + idx * 0x140;
        } else {
            handleP = NULL;
        }
        if (handleP != NULL) {
            if (*(s32 *)((char *)handleP + 0x8) != 0) {
                dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
            }
        }
        if (VFipf2_remove(path) == 0) {
            return 0;
        }
        return VFipf2_errnum();
    }
    return result;
}

s32 VFSysDeleteFile_current(const char *path) {
    void *curVol;
    void *p;
    void *end;
    void *handleP;

    curVol = VFiPFVOL_GetCurrentVolume();
    if (curVol != NULL) {
        if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
            p = l_sys_handle_table_p;
        } else {
            p = NULL;
        }
        end = (char *)p + l_vfsys_vol_max * 0x140;
        for (; (u32)p != (u32)end; p = (char *)p + 0x140) {
            if (*(s32 *)p == 0) continue;
            if ((s8)*(char *)((char *)p + 0x14) != (s8)*(char *)((char *)curVol + 0x1862)) continue;
            goto delfile_cur_found;
        }
    }
    handleP = NULL;
    goto delfile_cur_done;
delfile_cur_found:
    handleP = p;
delfile_cur_done:

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }
    if (VFipf2_remove(path) == 0) {
        return 0;
    }
    return VFipf2_errnum();
}

s32 VFSysCreateDir(u32 idx, const char *path) {
    void *handleP;
    s32 result;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }

    if (handleP != NULL) {
        void *vol = VFiPFVOL_GetVolumeFromDrvChar((s8)*(char *)((char *)handleP + 0x14));
        if (vol != NULL) {
            VFiPFVOL_SetCurrentVolume();
            result = 0;
            goto createdir_done;
        } else {
            result = VFipf2_errnum();
            if (result != 0) goto createdir_done;
            result = -1;
            goto createdir_done;
        }
    } else {
        result = 0xB003;
    }

createdir_done:
    if (result == 0) {
        if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
            handleP = (char *)l_sys_handle_table_p + idx * 0x140;
        } else {
            handleP = NULL;
        }
        if (handleP != NULL) {
            if (*(s32 *)((char *)handleP + 0x8) != 0) {
                dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
            }
        }
        if (VFipf2_mkdir(path) == 0) {
            return 0;
        }
        return VFipf2_errnum();
    }
    return result;
}

s32 VFSysCreateDir_current(const char *path) {
    void *curVol;
    void *p;
    void *end;
    void *handleP;

    curVol = VFiPFVOL_GetCurrentVolume();
    if (curVol != NULL) {
        if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
            p = l_sys_handle_table_p;
        } else {
            p = NULL;
        }
        end = (char *)p + l_vfsys_vol_max * 0x140;
        for (; (u32)p != (u32)end; p = (char *)p + 0x140) {
            if (*(s32 *)p == 0) continue;
            if ((s8)*(char *)((char *)p + 0x14) != (s8)*(char *)((char *)curVol + 0x1862)) continue;
            goto crdir_cur_found;
        }
    }
    handleP = NULL;
    goto crdir_cur_done;
crdir_cur_found:
    handleP = p;
crdir_cur_done:

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }
    if (VFipf2_mkdir(path) == 0) {
        return 0;
    }
    return VFipf2_errnum();
}

s32 VFSysChangeDir(u32 idx, const char *path) {
    void *handleP;
    s32 result;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }

    if (handleP != NULL) {
        void *vol = VFiPFVOL_GetVolumeFromDrvChar((s8)*(char *)((char *)handleP + 0x14));
        if (vol != NULL) {
            VFiPFVOL_SetCurrentVolume();
            result = 0;
            goto changedir_done;
        } else {
            result = VFipf2_errnum();
            if (result != 0) goto changedir_done;
            result = -1;
            goto changedir_done;
        }
    } else {
        result = 0xB003;
    }

changedir_done:
    if (result == 0) {
        if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
            handleP = (char *)l_sys_handle_table_p + idx * 0x140;
        } else {
            handleP = NULL;
        }
        if (handleP != NULL) {
            if (*(s32 *)((char *)handleP + 0x8) != 0) {
                dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
            }
        }
        if (VFipf2_chdir(path) == 0) {
            return 0;
        }
        return VFipf2_errnum();
    }
    return result;
}

s32 VFSysChangeDir_current(const char *path) {
    void *curVol;
    void *p;
    void *end;
    void *handleP;

    curVol = VFiPFVOL_GetCurrentVolume();
    if (curVol != NULL) {
        if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
            p = l_sys_handle_table_p;
        } else {
            p = NULL;
        }
        end = (char *)p + l_vfsys_vol_max * 0x140;
        for (; (u32)p != (u32)end; p = (char *)p + 0x140) {
            if (*(s32 *)p == 0) continue;
            if ((s8)*(char *)((char *)p + 0x14) != (s8)*(char *)((char *)curVol + 0x1862)) continue;
            goto chdir_cur_found;
        }
    }
    handleP = NULL;
    goto chdir_cur_done;
chdir_cur_found:
    handleP = p;
chdir_cur_done:

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }
    if (VFipf2_chdir(path) == 0) {
        return 0;
    }
    return VFipf2_errnum();
}

s32 VFSysGetFileSizeByFd(s32 *sizeOut, void *fp) {
    s32 fileSize;

    *sizeOut = 0;
    if (fp == NULL) {
        return -1;
    }
    if (VFipf2_finfo(fp, &fileSize) != 0) {
        return VFipf2_errnum();
    }
    *sizeOut = fileSize;
    return 0;
}

s32 VFSysGetFileState(void *stat, u32 idx, const char *path) {
    void *handleP;
    s32 result;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }

    if (handleP != NULL) {
        void *vol = VFiPFVOL_GetVolumeFromDrvChar((s8)*(char *)((char *)handleP + 0x14));
        if (vol != NULL) {
            VFiPFVOL_SetCurrentVolume();
            result = 0;
            goto getfilestate_done;
        } else {
            result = VFipf2_errnum();
            if (result != 0) goto getfilestate_done;
            result = -1;
            goto getfilestate_done;
        }
    } else {
        result = 0xB003;
    }

getfilestate_done:
    if (result == 0) {
        if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
            handleP = (char *)l_sys_handle_table_p + idx * 0x140;
        } else {
            handleP = NULL;
        }
        if (handleP != NULL) {
            if (*(s32 *)((char *)handleP + 0x8) != 0) {
                dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
            }
        }
        if (VFipf2_fstat(path, stat) == 0) {
            return 0;
        }
        return VFipf2_errnum();
    }
    return result;
}

s32 VFSysGetFileState_current(void *stat, const char *path) {
    void *curVol;
    void *p;
    void *end;
    void *handleP;

    curVol = VFiPFVOL_GetCurrentVolume();
    if (curVol != NULL) {
        if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
            p = l_sys_handle_table_p;
        } else {
            p = NULL;
        }
        end = (char *)p + l_vfsys_vol_max * 0x140;
        for (; (u32)p != (u32)end; p = (char *)p + 0x140) {
            if (*(s32 *)p == 0) continue;
            if ((s8)*(char *)((char *)p + 0x14) != (s8)*(char *)((char *)curVol + 0x1862)) continue;
            goto getfilestate_cur_found;
        }
    }
    handleP = NULL;
    goto getfilestate_cur_done;
getfilestate_cur_found:
    handleP = p;
getfilestate_cur_done:

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }
    if (VFipf2_fstat(path, stat) == 0) {
        return 0;
    }
    return VFipf2_errnum();
}

typedef struct {
    s32 unk0;
    s32 freeBlocks;
    s32 totalBlocks;
    s32 blockSize;
} VFDevInfo;

s32 VFSysGetDriveFreeSize(u32 idx, s32 *freeBlocks, s32 *totalBlocks, s32 *blockSize) {
    void *handleP;
    VFDevInfo devInfo;
    s32 err;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP == NULL || *(s32 *)handleP == 0) {
        l_vfsys_last_err = 0xB003;
        return 0xB003;
    }
    if (*(s32 *)((char *)handleP + 0x8) == 0) {
        l_vfsys_last_err = 0xB003;
        return 0xB003;
    }

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }

    if (VFipf2_devinf((s8)*(char *)((char *)handleP + 0x14), &devInfo) != 0) {
        err = VFipf2_errnum();
        if (err != 0) {
            l_vfsys_last_err = err;
        }
        return err;
    }
    *freeBlocks = devInfo.freeBlocks;
    *totalBlocks = devInfo.blockSize;
    *blockSize = devInfo.totalBlocks;
    return 0;
}

s32 VFSysFileSearchFirst(void *finddata, u32 idx, const char *path, const char *pattern) {
    void *handleP;
    s32 result;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }

    if (handleP != NULL) {
        void *vol = VFiPFVOL_GetVolumeFromDrvChar((s8)*(char *)((char *)handleP + 0x14));
        if (vol != NULL) {
            VFiPFVOL_SetCurrentVolume();
            result = 0;
            goto filesearchfirst_done;
        } else {
            result = VFipf2_errnum();
            if (result != 0) goto filesearchfirst_done;
            result = -1;
            goto filesearchfirst_done;
        }
    } else {
        result = 0xB003;
    }

filesearchfirst_done:
    if (result == 0) {
        if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
            handleP = (char *)l_sys_handle_table_p + idx * 0x140;
        } else {
            handleP = NULL;
        }
        if (handleP != NULL) {
            if (*(s32 *)((char *)handleP + 0x8) != 0) {
                dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
            }
        }
        if (VFipf2_fsfirst(path, pattern, finddata) == 0) {
            return 0;
        }
        return VFipf2_errnum();
    }
    return result;
}

s32 VFSysFileSearchFirst_current(void *finddata, const char *path, const char *pattern) {
    void *curVol;
    void *p;
    void *end;
    void *handleP;

    curVol = VFiPFVOL_GetCurrentVolume();
    if (curVol != NULL) {
        if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
            p = l_sys_handle_table_p;
        } else {
            p = NULL;
        }
        end = (char *)p + l_vfsys_vol_max * 0x140;
        for (; (u32)p != (u32)end; p = (char *)p + 0x140) {
            if (*(s32 *)p == 0) continue;
            if ((s8)*(char *)((char *)p + 0x14) != (s8)*(char *)((char *)curVol + 0x1862)) continue;
            goto filesearchfirst_cur_found;
        }
    }
    handleP = NULL;
    goto filesearchfirst_cur_done;
filesearchfirst_cur_found:
    handleP = p;
filesearchfirst_cur_done:

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }
    if (VFipf2_fsfirst(path, pattern, finddata) == 0) {
        return 0;
    }
    return VFipf2_errnum();
}

s32 VFSysFileSearchNext(void *finddata) {
    void *p;
    void *end;
    void *handleP;

    if (finddata == NULL) {
        goto searchnext_done;
    }
    {
        void *bpb = *(void **)((char *)finddata + 0x8);
        if (bpb == NULL) {
            goto searchnext_done;
        }
        if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
            p = l_sys_handle_table_p;
        } else {
            p = NULL;
        }
        end = (char *)p + l_vfsys_vol_max * 0x140;
        for (; (u32)p != (u32)end; p = (char *)p + 0x140) {
            if (*(s32 *)p != 0 && (s8)*(char *)((char *)p + 0x14) == (s8)*(char *)((char *)bpb + 0x1862)) {
                handleP = p;
                goto searchnext_found;
            }
        }
    }
searchnext_done:
    handleP = NULL;
searchnext_found:
    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        dCommon_setLastDeviceErrorToDisk(*(s32 *)((char *)handleP + 0x8), 0);
    }
    if (VFipf2_fsnext(finddata) == 0) {
        return 0;
    }
    return VFipf2_errnum();
}

void VFSysSetLastError(s32 err) {
    if (err != 0) {
        l_vfsys_last_err = err;
    }
}

s32 VFSysGetLastError(void) {
    return l_vfsys_last_err;
}

s32 VFSysGetLastDeviceError(u32 idx) {
    void *handleP;

    if (idx < (u32)l_vfsys_vol_max && l_sys_handle_table_p != NULL) {
        handleP = (char *)l_sys_handle_table_p + idx * 0x140;
    } else {
        handleP = NULL;
    }
    if (handleP != NULL) {
        if (*(s32 *)((char *)handleP + 0x8) != 0) {
            return dCommon_getLastDeviceErrorFromDisk(*(s32 *)((char *)handleP + 0x8));
        }
    }
    return -1;
}

s32 VFSysGetLastDeviceError_current(void) {
    void *curVol;
    void *p;
    void *end;
    void *handleP;

    curVol = VFiPFVOL_GetCurrentVolume();
    if (curVol != NULL) {
        if (l_vfsys_vol_max != 0 && l_sys_handle_table_p != NULL) {
            p = l_sys_handle_table_p;
        } else {
            p = NULL;
        }
        end = (char *)p + l_vfsys_vol_max * 0x140;
        for (; (u32)p != (u32)end; p = (char *)p + 0x140) {
            if (*(s32 *)p == 0) continue;
            if ((s8)*(char *)((char *)p + 0x14) != (s8)*(char *)((char *)curVol + 0x1862)) continue;
            goto getlastdeviceerror_cur_found;
        }
    }
    handleP = NULL;
    goto getlastdeviceerror_cur_done;
getlastdeviceerror_cur_found:
    handleP = p;
getlastdeviceerror_cur_done:

    if (handleP != NULL && *(s32 *)((char *)handleP + 0x8) != 0) {
        return dCommon_getLastDeviceErrorFromDisk(*(s32 *)((char *)handleP + 0x8));
    }
    return -1;
}

s32 VFSysSetTimeStampCallback(s32 callback) {
    s32 old = l_timeStampCallback;
    l_timeStampCallback = callback;
    return old;
}

s32 VFSysGetTimeStampCallback(void) {
    return l_timeStampCallback;
}
