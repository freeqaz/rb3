#include "MSL_Common/file_def.h"
#include "MSL_Common/size_def.h"

size_t __fwrite(const void *buf, size_t size, size_t nmemb, FILE *f) {
    const unsigned char *src = (const unsigned char *)buf;
    size_t total = size * nmemb;
    size_t written = 0;

    if (f->mode.file_kind == __closed_file || total == 0) return 0;

    if (f->state.io_state == __reading) {
        f->state.io_state = __neutral;
        f->buffer_len = 0;
        f->buffer_ptr = f->buffer;
    }

    f->state.io_state = __writing;

    if (f->buffer_size > 0) {
        while (written < total) {
            size_t space = f->buffer_size - f->buffer_len;
            size_t chunk = total - written;
            size_t i;
            if (chunk > space) chunk = space;
            for (i = 0; i < chunk; i++) {
                *f->buffer_ptr++ = *src++;
            }
            f->buffer_len += chunk;
            written += chunk;
            if (f->buffer_len >= f->buffer_size) {
                if (f->write_proc != NULL) {
                    size_t n = f->buffer_len;
                    (*f->write_proc)(f->handle, f->buffer, &n, f->ref_con);
                    f->buffer_ptr = f->buffer;
                    f->buffer_len = 0;
                }
            }
        }
    } else {
        if (f->write_proc != NULL) {
            (*f->write_proc)(f->handle, (unsigned char *)buf, &total, f->ref_con);
        }
        written = total;
    }

    return (size > 0) ? written / size : 0;
}
