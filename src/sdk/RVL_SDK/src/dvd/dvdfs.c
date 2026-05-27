#include "revolution/dvd/dvdfs.h"
#include "revolution/OS.h"
#include "revolution/os/OSThread.h"
#include <string.h>

// External symbols defined in dvd.c
extern u32 __DVDLayoutFormat;
BOOL DVDCancel(DVDCommandBlock* block);
BOOL DVDReadAbsAsyncPrio(DVDCommandBlock* block, void* addr, s32 length, s32 diskOffset, DVDCBCallback callback, s32 prio);

// FST entry struct (12 bytes)
typedef struct {
    u32 flags_nameOffset; // top byte = type (0=file, non-0=dir), bottom 3 bytes = name offset into string table
    u32 data;             // file: partition offset; dir: parent entry number
    u32 size;             // file: byte count; dir: last entry number of subtree
} FSTEntry;

// Filesystem globals (sbss - uninitialized; sbss order must match ASM)
u32 currentDirectory;
OSThreadQueue __DVDThreadQueue;
u32 MaxEntryNum;
u8* FstStringStart;
u8* FstStart;
static u32* BootInfo;

// Long filename flag (sdata - initialized to 1)
BOOL __DVDLongFileNameFlag = 1;

// Internal callbacks
static void cbForReadAsync(s32 result, DVDCommandBlock* block);
static void cbForReadSync(s32 result, DVDCommandBlock* block);

// Internal recursive helper
static s32 entryToPath(s32 entryNum, char* path, u32 maxlen);

void __DVDFSInit(void) {
    BootInfo = (u32*)0x80000000;
    FstStart = (u8*)BootInfo[0x38 / 4];
    if (FstStart == NULL) {
        return;
    }
    MaxEntryNum = ((u32*)FstStart)[0x8 / 4];
    FstStringStart = FstStart + MaxEntryNum * 0xc;
}

s32 DVDConvertPathToEntrynum(const char* pathPtr) {
    s32 currentDir;
    const char* ptr;
    s32 i;
    FSTEntry* fstEntry;
    const char* compStart;
    const char* compEnd;
    const char* fstName;
    u32 rawWord;
    s32 nameEnd;
    u32 isFile;
    s32 isDirEntry;

    currentDir = currentDirectory;
    ptr = pathPtr;

    while (1) {
        char c = (signed char)*ptr;

        if (c == '\0') {
            return currentDir;
        }

        if (c == '/') {
            currentDir = 0;
            ptr++;
            continue;
        }

        if (c == '.') {
            char c2 = (signed char)ptr[1];
            if (c2 == '.') {
                char c3 = (signed char)ptr[2];
                if (c3 == '/') {
                    // go up: get parent of currentDir
                    FSTEntry* dirEnt = (FSTEntry*)(FstStart + currentDir * 0xc);
                    currentDir = dirEnt->data;
                    ptr += 3;
                    continue;
                } else if (c3 == '\0') {
                    FSTEntry* dirEnt = (FSTEntry*)(FstStart + currentDir * 0xc);
                    return dirEnt->data;
                }
            } else if (c2 == '/') {
                ptr += 2;
                continue;
            } else if (c2 == '\0') {
                return currentDir;
            }
        }

        // Find end of this path component
        if (!__DVDLongFileNameFlag) {
            // Short filename (8.3) mode: validate
            const char* scan = ptr;
            s32 hasDot = 0;
            s32 hasLong = 0;
            const char* dotAfter = ptr;

            while (1) {
                char ch = (signed char)*scan;
                if (ch == '\0' || ch == '/') {
                    break;
                }
                if (ch == '.') {
                    if (scan - ptr > 8) {
                        hasLong = 1;
                        break;
                    }
                    if (hasDot == 1) {
                        hasLong = 1;
                        break;
                    }
                    dotAfter = scan + 1;
                    hasDot = 1;
                } else if (ch == ' ') {
                    hasLong = 1;
                }
                scan++;
            }
            if (hasDot == 1) {
                if (scan - dotAfter > 3) {
                    hasLong = 1;
                }
            }
            if (hasLong) {
                OSPanic("dvdfs.c", 0x1c4,
                    "DVDConvertEntrynumToPath(possibly DVDOpen or DVDChangeDir or DVDOpenDir): specified directory or file (%s) doesn't match standard 8.3 format. This is a temporary restriction and will be removed soon\n",
                    pathPtr);
            }
            compEnd = ptr;
            while (1) {
                char ch = (signed char)*compEnd;
                if (ch == '\0' || ch == '/') break;
                compEnd++;
            }
        } else {
            compEnd = ptr;
            while (1) {
                char ch = (signed char)*compEnd;
                if (ch == '\0' || ch == '/') break;
                compEnd++;
            }
        }

        {
            // Search FST for matching component
            s32 compLen = compEnd - ptr;
            FSTEntry* dirEnt = (FSTEntry*)(FstStart + currentDir * 0xc);
            u32 endEntry = dirEnt->size;
            // isFile: true if compEnd points to '\0' (no more '/' after)
            {
                char endCh = *compEnd;
                isFile = (u32)(((-((int)(endCh == '\0'))) | (int)(endCh == '\0')) >> 31) & 1;
            }

            // Locale-based case-insensitive compare
            // _current_locale->lc_ctype->toupper_tab at offset 0x38+0x10
            // For simplicity use our own comparison loop
            i = currentDir + 1;

            while ((u32)i < endEntry) {
                s32 fstOffset = i * 0xc;
                rawWord = *(u32*)(FstStart + fstOffset);
                isDirEntry = (rawWord >> 24) != 0;

                if (isDirEntry) {
                    if (isFile) {
                        // skip entire directory subtree
                        i = (s32)((FSTEntry*)(FstStart + fstOffset))->size;
                        continue;
                    }
                }

                // compare name
                fstName = (const char*)FstStringStart + (rawWord & 0x00FFFFFF);
                {
                    const char* a = fstName;
                    const char* b = ptr;
                    s32 match = 1;
                    while (1) {
                        char fa = (signed char)*a;
                        char fb = (signed char)*b;
                        if (fa == '\0') {
                            if (fb == '/' || fb == '\0') {
                                // matched
                            } else {
                                match = 0;
                            }
                            break;
                        }
                        if (fa != fb) {
                            match = 0;
                            break;
                        }
                        a++;
                        b++;
                    }
                    if (match) {
                        if (!isFile) {
                            // found directory: descend into it
                            currentDir = i;
                            ptr = compEnd + 1;
                            goto next_component;
                        } else {
                            return i;
                        }
                    }
                }

                if (isDirEntry) {
                    i = (s32)((FSTEntry*)(FstStart + i * 0xc))->size;
                } else {
                    i++;
                }
            }

            // not found
            return -1;
        }
next_component:;
    }
}

BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* fileInfo) {
    u32 mult;
    u32 rawWord;

    if (entrynum < 0) {
        goto fail;
    }
    if ((u32)entrynum >= MaxEntryNum) {
        goto fail;
    }

    mult = entrynum * 0xc;
    rawWord = *(u32*)(FstStart + mult);
    if (!(rawWord & 0xFF000000)) {
        goto success;
    }
fail:
    return FALSE;
success:
    {
        u32 shift = __DVDLayoutFormat;
        u32 diskOff = ((u32*)(FstStart + mult))[0x4 / 4];
        fileInfo->startAddr = diskOff >> shift;
        fileInfo->length = ((u32*)(FstStart + mult))[0x8 / 4];
        fileInfo->callback = NULL;
        fileInfo->cb.state = 0;
    }
    return TRUE;
}

BOOL DVDOpen(char* path, DVDFileInfo* fileInfo) {
    s32 entrynum;
    char localPath[0x80];
    u32 result;

    entrynum = DVDConvertPathToEntrynum(path);
    if (entrynum < 0) {
        result = entryToPath(currentDirectory, localPath, 0x80);
        if (result == 0x80) {
            localPath[0x7f] = '\0';
        } else {
            u32 rawWord = *(u32*)(FstStart + currentDirectory * 0xc);
            if ((rawWord >> 24) != 0) {
                // is a directory
                if (result == 0x7f) {
                    localPath[result] = '\0';
                } else {
                    localPath[result] = '/';
                    localPath[result + 1] = '\0';
                }
            } else {
                localPath[result] = '\0';
            }
        }
        OSReport("Warning: DVDOpen(): file '%s' was not found under %s.\n", path, localPath);
        return FALSE;
    }

    {
        u32 mult = entrynum * 0xc;
        u32 rawWord = *(u32*)(FstStart + mult);
        if ((rawWord >> 24) != 0) {
            return FALSE;
        }
        fileInfo->startAddr = ((u32*)(FstStart + mult))[0x4 / 4] >> __DVDLayoutFormat;
        fileInfo->length = ((u32*)(FstStart + mult))[0x8 / 4];
        fileInfo->callback = NULL;
        fileInfo->cb.state = 0;
    }
    return TRUE;
}

BOOL DVDClose(DVDFileInfo* fileInfo) {
    DVDCancel((DVDCommandBlock*)fileInfo);
    return TRUE;
}

static s32 entryToPath(s32 entryNum, char* path, u32 maxlen) {
    u32 mult;
    u32 rawWord;
    u32 nameOff;
    u8* name;
    s32 isDirEntry;
    s32 parentEntry;
    s32 result;
    u32 remaining;
    u8* src;
    char* dst;
    u32 count;

    if (entryNum == 0) {
        return 0;
    }

    mult = entryNum * 0xc;
    rawWord = *(u32*)(FstStart + mult);
    isDirEntry = (rawWord >> 24) != 0;
    nameOff = rawWord & 0x00FFFFFF;
    name = FstStringStart + nameOff;

    // Find parent: walk backwards from entryNum
    {
        s32 cur;
        u32 rw;
        u32 sz;

        if (isDirEntry) {
            // for directory, the parent is stored in data field
            parentEntry = (s32)((FSTEntry*)(FstStart + mult))->data;
        } else {
            parentEntry = entryNum;
        }

        // Find the directory that contains entryNum
        // by walking backwards through the FST
        cur = entryNum;
        {
            s32 ctr = entryNum;
            while (ctr > 0) {
                u32* ep = (u32*)(FstStart + ctr * 0xc);
                rw = ep[0];
                if ((rw >> 24) != 0) {
                    sz = ep[2];
                    if (sz > (u32)entryNum) {
                        break;
                    }
                }
                ctr--;
            }
            cur = ctr;
        }

        result = entryToPath(cur, path, maxlen);
    }

    if (result == (s32)maxlen) {
        return result;
    }

    remaining = maxlen - result - 1;
    path[result] = '/';

    {
        u32 written = result + 1;
        src = name;
        dst = path + written;
        count = remaining;
        while (count > 0) {
            char ch = (signed char)*src;
            if (ch == '\0') break;
            *dst++ = ch;
            src++;
            count--;
        }
        remaining -= count;
        result = written + remaining;
    }

    return result;
}

BOOL DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, DVDCallback callback, s32 prio) {
    u32 diskOffset;
    s32 adjOffset;

    if (offset < 0 || (u32)offset > fileInfo->length) {
        OSPanic("dvdfs.c", 0x34d, "DVDReadAsync(): specified area is out of the file  ");
    }

    adjOffset = offset + length;
    if (adjOffset < 0 || (u32)adjOffset >= fileInfo->length + 0x20) {
        OSPanic("dvdfs.c", 0x353, "DVDReadAsync(): specified area is out of the file  ");
    }

    fileInfo->callback = callback;
    diskOffset = fileInfo->startAddr + (offset >> 2);

    DVDReadAbsAsyncPrio((DVDCommandBlock*)fileInfo, addr, length, (s32)diskOffset, cbForReadAsync, prio);
    return TRUE;
}

static void cbForReadAsync(s32 result, DVDCommandBlock* block) {
    DVDFileInfo* fileInfo = (DVDFileInfo*)block;
    DVDCallback cb = fileInfo->callback;
    if (cb == NULL) {
        return;
    }
    cb(result, fileInfo);
}

s32 DVDReadPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, s32 prio) {
    u32 diskOffset;
    s32 adjOffset;
    BOOL intrStatus;
    s32 result;
    s32 state;

    if (offset < 0 || (u32)offset > fileInfo->length) {
        OSPanic("dvdfs.c", 0x393, "DVDRead(): specified area is out of the file  ");
    }

    adjOffset = offset + length;
    if (adjOffset < 0 || (u32)adjOffset >= fileInfo->length + 0x20) {
        OSPanic("dvdfs.c", 0x399, "DVDRead(): specified area is out of the file  ");
    }

    diskOffset = fileInfo->startAddr + (offset >> 2);

    if (!DVDReadAbsAsyncPrio((DVDCommandBlock*)fileInfo, addr, length, (s32)diskOffset, cbForReadSync, prio)) {
        return -1;
    }

    intrStatus = OSDisableInterrupts();

    while (1) {
        state = fileInfo->cb.state;
        if (state == 0) {
            result = (s32)fileInfo->cb.transferredSize;
            break;
        }
        if (state == -1) {
            result = -1;
            break;
        }
        if (state == 0xa) {
            result = -3;
            break;
        }
        OSSleepThread(&__DVDThreadQueue);
    }

    OSRestoreInterrupts(intrStatus);
    return result;
}

static void cbForReadSync(s32 result, DVDCommandBlock* block) {
    OSWakeupThread(&__DVDThreadQueue);
}

BOOL DVDFastOpenDir(int entryNum, DVDDir* dir) {
    u32 mult;
    u32 rawWord;

    if (entryNum < 0) {
        goto fail;
    }
    if ((u32)entryNum >= MaxEntryNum) {
        goto fail;
    }

    mult = entryNum * 0xc;
    rawWord = *(u32*)(FstStart + mult);
    if (rawWord & 0xFF000000) {
        goto success;
    }
fail:
    return FALSE;
success:
    dir->entryNum = entryNum;
    dir->location = entryNum + 1;
    dir->next = ((u32*)(FstStart + mult))[0x8 / 4];
    return TRUE;
}

BOOL DVDReadDir(DVDDir* dir, DVDDirEntry* entry) {
    u32 location;
    u32 mult;
    u32 rawWord;
    u32 type;

    location = dir->location;

    if (location <= dir->entryNum) {
        goto fail;
    }
    if (dir->next > location) {
        goto success;
    }
fail:
    return FALSE;
success:

    entry->entryNum = location;
    mult = location * 0xc;

    // First load: compute isDir
    rawWord = *(u32*)(FstStart + mult);
    type = rawWord & 0xFF000000;
    {
        s32 t = (s32)type;
        entry->isDir = (BOOL)(((-t) | t) >> 31) & 1;
    }

    // Second load: compute name pointer
    {
        u32 rawWord2 = *(u32*)(FstStart + mult);
        entry->name = (char*)FstStringStart + (rawWord2 & 0x00FFFFFF);
    }

    // Third load: update dir->location
    {
        u32* fstPtr = (u32*)(FstStart + mult);
        u32 rawWord3 = fstPtr[0];
        if (rawWord3 >> 24) {
            dir->location = fstPtr[0x8 / 4];
        } else {
            dir->location = location + 1;
        }
    }

    return TRUE;
}

BOOL DVDCloseDir(DVDDir* dir) {
    return TRUE;
}

void* DVDGetFSTLocation(void) {
    return (void*)BootInfo[0x38 / 4];
}
