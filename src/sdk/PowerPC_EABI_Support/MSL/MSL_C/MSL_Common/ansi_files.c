#include "MSL_Common/file_def.h"
#include "MSL_Common/misc_io.h"

unsigned char stdin_buff[256];
unsigned char stdout_buff[256];
unsigned char stderr_buff[256];

FILE __files[_STATIC_FILES];

void __close_all(void) {
    FILE *f;
    for (f = __files; f != NULL; f = f->next_file_struct) {
        if (f->mode.file_kind != __closed_file) {
            if (f->close_proc != NULL) {
                (*f->close_proc)(f->handle);
            }
            f->mode.file_kind = __closed_file;
        }
    }
}

unsigned int __flush_all(void) {
    FILE *f;
    unsigned int result = 0;
    for (f = __files; f != NULL; f = f->next_file_struct) {
        if (f->mode.file_kind != __closed_file && f->state.io_state == __writing) {
            if (f->buffer_len > 0 && f->write_proc != NULL) {
                size_t count = f->buffer_len;
                (*f->write_proc)(f->handle, f->buffer_ptr - f->buffer_len, &count, f->ref_con);
                f->buffer_len = 0;
            }
        }
    }
    return result;
}
