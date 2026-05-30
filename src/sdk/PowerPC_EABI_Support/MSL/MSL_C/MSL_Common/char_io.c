#include "MSL_Common/file_def.h"
#include "MSL_Common/misc_io.h"
#include <stdio.h>

int __put_char(int c, FILE *f) {
    unsigned char ch = (unsigned char)c;
    size_t count = 1;

    if (f->mode.file_kind == __closed_file) return -1;

    if (f->state.io_state == __reading) {
        f->state.io_state = __neutral;
        f->buffer_len = 0;
        f->buffer_ptr = f->buffer;
    }

    f->state.io_state = __writing;

    if (f->buffer_size > 0) {
        *f->buffer_ptr++ = ch;
        f->buffer_len++;
        if (f->mode.buffer_mode == _IOLBF && ch == '\n') {
            if (f->write_proc != NULL) {
                size_t n = f->buffer_len;
                (*f->write_proc)(f->handle, f->buffer, &n, f->ref_con);
                f->buffer_ptr = f->buffer;
                f->buffer_len = 0;
            }
        } else if (f->buffer_len >= f->buffer_size) {
            if (f->write_proc != NULL) {
                size_t n = f->buffer_len;
                (*f->write_proc)(f->handle, f->buffer, &n, f->ref_con);
                f->buffer_ptr = f->buffer;
                f->buffer_len = 0;
            }
        }
    } else {
        if (f->write_proc != NULL) {
            (*f->write_proc)(f->handle, &ch, &count, f->ref_con);
        }
    }

    return (unsigned char)c;
}

int puts(const char *s) {
    FILE *f = stdout;
    const char *p = s;
    while (*p != '\0') {
        if (__put_char((unsigned char)*p, f) < 0) return -1;
        p++;
    }
    if (__put_char('\n', f) < 0) return -1;
    return 0;
}
