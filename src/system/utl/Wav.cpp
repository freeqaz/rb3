#include "utl/Wav.h"
#include "decomp.h"
#include "milo_types.h"
#include "os/Debug.h"
#include "os/Endian.h"
#include "os/File.h"
#include <cstring>

namespace {
    const char *riffID = "RIFF";
    const char *waveID = "WAVE";
    const char *formatID = "fmt ";
    const char *dataID = "data";
}

#pragma push
#pragma dont_inline on
DECOMP_FORCEBLOCK(Wav, (void), uint x = 2; uint& ui = x; EndianSwapEq(ui);)
DECOMP_FORCEBLOCK(Wav, (void), ushort x = 2; ushort& us = x; EndianSwapEq(us);)
DECOMP_FORCEBLOCK(Wav, (void), short x = 2; short& ui = x; EndianSwapEq(ui);)
#pragma pop

#pragma push
#pragma dont_inline on
void WriteWav(const char *fname, int sampleRate, const void *samples, int numBytes) {
    int buf[7];
    short fmtChunk[8];
    short swapped;
    int fd = FileOpen(fname, 0xA04);
    if (fd < 0) {
        TheDebug.Fail(MakeString(kAssertStr, __FILE__, 87, "fd >= 0"));
    }
    // Manually inlined EndianSwap so MWCC emits stwbrx despite dont_inline on.
    {
        unsigned int v = (unsigned int)(numBytes + 0x24);
        *(unsigned int *)&buf[6] = v >> 0x18 | v << 0x18 | v >> 8 & 0xFF00 | (v & 0xFF00) << 8;
    }
    memcpy(&buf[5], ::riffID, 4);
    FileWrite(fd, &buf[5], 8);
    memcpy(&buf[0], ::waveID, 4);
    FileWrite(fd, &buf[0], 4);
    buf[4] = 0x10000000;
    memcpy(&buf[3], ::formatID, 4);
    FileWrite(fd, &buf[3], 8);
    fmtChunk[0] = 1;
    fmtChunk[1] = 1;
    *(int *)&fmtChunk[2] = sampleRate;
    *(int *)&fmtChunk[4] = sampleRate * 2;
    fmtChunk[6] = 2;
    fmtChunk[7] = 0x10;
    EndianSwapEq(fmtChunk[0]);
    EndianSwapEq((unsigned short &)fmtChunk[1]);
    EndianSwapEq(*(unsigned int *)&fmtChunk[2]);
    EndianSwapEq(*(unsigned int *)&fmtChunk[4]);
    EndianSwapEq((unsigned short &)fmtChunk[6]);
    EndianSwapEq((unsigned short &)fmtChunk[7]);
    FileWrite(fd, fmtChunk, 0x10);
    {
        unsigned int v = (unsigned int)numBytes;
        *(unsigned int *)&buf[2] = v >> 0x18 | v << 0x18 | v >> 8 & 0xFF00 | (v & 0xFF00) << 8;
    }
    memcpy(&buf[1], ::dataID, 4);
    FileWrite(fd, &buf[1], 8);
    const short *src = (const short *)samples;
    int limit = numBytes / 2;
    for (int i = 0; i < limit; i++) {
        short s = *src;
        swapped = ((unsigned short)s << 8) | ((unsigned short)s >> 8);
        FileWrite(fd, &swapped, 2);
        src++;
    }
    FileClose(fd);
}
#pragma pop
