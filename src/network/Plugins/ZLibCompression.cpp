#include "network/Plugins/ZLibCompression.h"

void *QuazalCZlibAlloc(void *, unsigned int, unsigned int);
void QuazalCZlibFree(void *, void *);

namespace Quazal {
    ZLibCompression::ZLibCompression() {
        mStreams = new (__FILE__, 0x78) ZLibStreams;
        mStreams->inflate_stream.zalloc = (alloc_func)QuazalCZlibAlloc;
        mStreams->inflate_stream.zfree = (free_func)QuazalCZlibFree;
        mStreams->inflate_stream.opaque = 0;
        inflateInit(&mStreams->inflate_stream);
        mStreams->deflate_stream.zalloc = (alloc_func)QuazalCZlibAlloc;
        mStreams->deflate_stream.zfree = (free_func)QuazalCZlibFree;
        mStreams->deflate_stream.opaque = 0;
        deflateInit(&mStreams->deflate_stream, Z_DEFAULT_COMPRESSION);
    }
}