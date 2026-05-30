#include "MSL_Common/scanf.h"
#include "MSL_Common/file_def.h"
#include "MSL_Common/va_list_def.h"

static void parse_format(void);

int __sformatter(int (*ReadProc)(void *, int, int), void *ReadProcArg,
                 const char *format_str, va_list arg) {
    (void)ReadProc;
    (void)ReadProcArg;
    (void)format_str;
    (void)arg;
    return 0;
}

void __StringRead(void *state, const char *buf, int n) {
    (void)state;
    (void)buf;
    (void)n;
}

int sscanf(const char *buffer, const char *format, ...) {
    int r;
    va_list ap;
    va_start(ap, format);
    r = vsscanf(buffer, format, ap);
    va_end(ap);
    return r;
}

int vsscanf(const char *buffer, const char *format, va_list vlist) {
    (void)buffer;
    (void)format;
    (void)vlist;
    return 0;
}

int vscanf(const char *format, va_list vlist) {
    (void)format;
    (void)vlist;
    return 0;
}

int vfscanf(FILE *stream, const char *format, va_list vlist) {
    (void)stream;
    (void)format;
    (void)vlist;
    return 0;
}

int scanf(const char *format, ...) {
    int r;
    va_list ap;
    va_start(ap, format);
    r = vscanf(format, ap);
    va_end(ap);
    return r;
}

int fscanf(FILE *stream, const char *format, ...) {
    int r;
    va_list ap;
    va_start(ap, format);
    r = vfscanf(stream, format, ap);
    va_end(ap);
    return r;
}

static void parse_format(void) {}
