#include "MSL_Common/wprintf.h"
#include "MSL_Common/size_def.h"
#include "MSL_Common/wchar_def.h"
#include "MSL_Common/va_list_def.h"

static void parse_format(void);
static void long2str(void);
static void longlong2str(void);
static void double2hex(void);
static void round_decimal(void);
static void float2str(void);

int __wpformatter(void (*WriteProc)(void *, const wchar_t *, size_t),
                  void *WriteProcArg,
                  const wchar_t *format_str,
                  va_list arg) {
    (void)WriteProc;
    (void)WriteProcArg;
    (void)format_str;
    (void)arg;
    return 0;
}

void __wStringWrite(void *state, const wchar_t *buf, size_t n) {
    (void)state;
    (void)buf;
    (void)n;
}

int vswprintf(wchar_t *buffer, size_t bufsz, const wchar_t *format, va_list vlist) {
    (void)buffer;
    (void)bufsz;
    (void)format;
    (void)vlist;
    return 0;
}

int swprintf(wchar_t *buffer, size_t bufsz, const wchar_t *format, ...) {
    int r;
    va_list ap;
    va_start(ap, format);
    r = vswprintf(buffer, bufsz, format, ap);
    va_end(ap);
    return r;
}

int vwprintf(const wchar_t *format, va_list vlist) {
    (void)format;
    (void)vlist;
    return 0;
}

int vfwprintf(FILE *stream, const wchar_t *format, va_list vlist) {
    (void)stream;
    (void)format;
    (void)vlist;
    return 0;
}

int wprintf(const wchar_t *format, ...) {
    int r;
    va_list ap;
    va_start(ap, format);
    r = vwprintf(format, ap);
    va_end(ap);
    return r;
}

int fwprintf(FILE *stream, const wchar_t *format, ...) {
    int r;
    va_list ap;
    va_start(ap, format);
    r = vfwprintf(stream, format, ap);
    va_end(ap);
    return r;
}

static void parse_format(void) {}
static void long2str(void) {}
static void longlong2str(void) {}
static void double2hex(void) {}
static void round_decimal(void) {}
static void float2str(void) {}
