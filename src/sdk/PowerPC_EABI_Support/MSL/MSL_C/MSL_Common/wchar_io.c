#include "types.h"
#include "MSL_Common/file_def.h"
#include "MSL_Common/wchar_def.h"
#include "MSL_Common/wint_def.h"

int __fwide(FILE *stream) {
    (void)stream;
    return 0;
}

int fwide(FILE *stream, int mode) {
    if (mode > 0) {
        if (stream->mode.file_orientation == __unoriented) {
            stream->mode.file_orientation = __wide_oriented;
        }
    } else if (mode < 0) {
        if (stream->mode.file_orientation == __unoriented) {
            stream->mode.file_orientation = __char_oriented;
        }
    }

    if (stream->mode.file_orientation == __wide_oriented) {
        return 1;
    }
    if (stream->mode.file_orientation == __char_oriented) {
        return -1;
    }
    return 0;
}
