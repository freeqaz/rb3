#include "MSL_Common/FILE_POS.h"

int _ftell(FILE *f) {
    fpos_t pos;

    if (f->mode.file_kind == __closed_file) return -1;

    if (f->state.io_state == __writing) {
        pos = (fpos_t)(f->position + f->buffer_len);
    } else if (f->state.io_state == __reading || f->state.io_state == __rereading) {
        pos = (fpos_t)(f->position - f->buffer_len);
    } else {
        pos = (fpos_t)f->position;
    }

    return (int)pos;
}

int ftell(FILE *f) {
    return _ftell(f);
}

int _fseek(FILE *f, unsigned int offset, int whence) {
    fpos_t new_pos;
    int result;

    if (f->mode.file_kind == __closed_file) return -1;

    if (f->state.io_state == __writing && f->buffer_len > 0) {
        if (f->write_proc != NULL) {
            size_t n = f->buffer_len;
            (*f->write_proc)(f->handle, f->buffer, &n, f->ref_con);
        }
        f->buffer_len = 0;
        f->buffer_ptr = f->buffer;
    }

    if (whence == SEEK_SET) {
        new_pos = (fpos_t)offset;
    } else if (whence == SEEK_CUR) {
        new_pos = (fpos_t)((unsigned int)f->position + offset);
    } else {
        new_pos = (fpos_t)offset;
    }

    if (f->position_proc != NULL) {
        result = (*f->position_proc)(f->handle, &new_pos, whence, f->ref_con);
        if (result != 0) return -1;
    }

    f->position = (unsigned int)new_pos;
    f->state.io_state = __neutral;
    f->state.eof = 0;
    f->buffer_len = 0;
    f->buffer_ptr = f->buffer;
    return 0;
}
