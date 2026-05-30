#include "MSL_Common/file_def.h"
#include "MSL_Common/size_def.h"
#include <stdlib.h>
#include <string.h>

int fclose(FILE *f) {
    int result = 0;
    if (f == NULL) return -1;
    if (f->mode.file_kind == __closed_file) return -1;

    if (f->state.io_state == __writing && f->buffer_len > 0) {
        if (f->write_proc != NULL) {
            size_t n = f->buffer_len;
            (*f->write_proc)(f->handle, f->buffer, &n, f->ref_con);
        }
    }

    if (f->close_proc != NULL) {
        result = (*f->close_proc)(f->handle);
    }

    if (f->state.free_buffer && f->buffer != NULL) {
        free(f->buffer);
    }

    f->mode.file_kind = __closed_file;
    return result;
}

int fflush(FILE *f) {
    if (f == NULL) {
        /* flush all */
        return 0;
    }
    if (f->mode.file_kind == __closed_file) return -1;
    if (f->state.io_state == __writing && f->buffer_len > 0) {
        if (f->write_proc != NULL) {
            size_t n = f->buffer_len;
            int r = (*f->write_proc)(f->handle, f->buffer, &n, f->ref_con);
            f->buffer_ptr = f->buffer;
            f->buffer_len = 0;
            if (r != 0) { f->state.error = 1; return -1; }
        }
    }
    return 0;
}

int __msl_strnicmp(const char *s1, const char *s2, size_t n) {
    unsigned char c1, c2;
    while (n-- > 0) {
        c1 = (unsigned char)*s1++;
        c2 = (unsigned char)*s2++;
        if (c1 >= 'A' && c1 <= 'Z') c1 += 'a' - 'A';
        if (c2 >= 'A' && c2 <= 'Z') c2 += 'a' - 'A';
        if (c1 != c2) return (int)c1 - (int)c2;
        if (c1 == '\0') return 0;
    }
    return 0;
}

char *__msl_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *p = (char *)malloc(len);
    if (p != NULL) {
        memcpy(p, s, len);
    }
    return p;
}
