#include "MSL_Common/printf.h"
#include "MSL_Common/file_def.h"
#include "MSL_Common/size_def.h"
#include "MSL_Common/va_list_def.h"
#include <string.h>

typedef struct {
    char *buf;
    size_t len;
    size_t max;
} PrintState;

static void parse_format(void);
static void long2str(void);
static void longlong2str(void);
static void double2hex(void);
static void round_decimal(void);
static void float2str(void);

int __pformatter(void (*WriteProc)(void *, const char *, size_t),
                 void *WriteProcArg,
                 const char *format_str,
                 va_list arg) {
    (void)WriteProc;
    (void)WriteProcArg;
    (void)format_str;
    (void)arg;
    return 0;
}

void __FileWrite(FILE *f, const char *buf, size_t n) {
    if (f != NULL && f->write_proc != NULL) {
        (*f->write_proc)(f->handle, (unsigned char *)buf, &n, f->ref_con);
    }
}

void __StringWrite(PrintState *state, const char *buf, size_t n) {
    if (state == NULL || buf == NULL) return;
    while (n-- > 0 && state->len < state->max) {
        state->buf[state->len++] = *buf++;
    }
}

int vprintf(const char *format, va_list vlist) {
    (void)format;
    (void)vlist;
    return 0;
}

int vfprintf(FILE *stream, const char *format, va_list vlist) {
    (void)stream;
    (void)format;
    (void)vlist;
    return 0;
}

int printf(const char *format, ...) {
    int r;
    va_list ap;
    va_start(ap, format);
    r = vprintf(format, ap);
    va_end(ap);
    return r;
}

int fprintf(FILE *stream, const char *format, ...) {
    int r;
    va_list ap;
    va_start(ap, format);
    r = vfprintf(stream, format, ap);
    va_end(ap);
    return r;
}

int vsnprintf(char *buffer, size_t bufsz, const char *format, va_list vlist) {
    (void)buffer;
    (void)bufsz;
    (void)format;
    (void)vlist;
    return 0;
}

int vsprintf(char *buffer, const char *format, va_list vlist) {
    return vsnprintf(buffer, (size_t)-1, format, vlist);
}

int snprintf(char *buffer, size_t bufsz, const char *format, ...) {
    int r;
    va_list ap;
    va_start(ap, format);
    r = vsnprintf(buffer, bufsz, format, ap);
    va_end(ap);
    return r;
}

int sprintf(char *buffer, const char *format, ...) {
    int r;
    va_list ap;
    va_start(ap, format);
    r = vsprintf(buffer, format, ap);
    va_end(ap);
    return r;
}

static void parse_format(void) {}
static void long2str(void) {}
static void longlong2str(void) {}
static void double2hex(void) {}
static void round_decimal(void) {}
static void float2str(void) {}
