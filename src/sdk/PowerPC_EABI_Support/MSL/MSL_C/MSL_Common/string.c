#include "MSL_Common/size_def.h"
#include <string.h>

char *strcpy(char *dst, const char *src) {
    char *ret = dst;
    while ((*dst++ = *src++) != '\0') {}
    return ret;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *ret = dst;
    while (n > 0 && *src != '\0') {
        *dst++ = *src++;
        n--;
    }
    while (n-- > 0) {
        *dst++ = '\0';
    }
    return ret;
}

char *strcat(char *dst, const char *src) {
    char *ret = dst;
    while (*dst != '\0') dst++;
    while ((*dst++ = *src++) != '\0') {}
    return ret;
}

char *strncat(char *dst, const char *src, size_t n) {
    char *ret = dst;
    while (*dst != '\0') dst++;
    while (n-- > 0 && *src != '\0') {
        *dst++ = *src++;
    }
    *dst = '\0';
    return ret;
}

int strcmp(const char *s1, const char *s2) {
    unsigned char c1, c2;
    do {
        c1 = (unsigned char)*s1++;
        c2 = (unsigned char)*s2++;
        if (c1 == '\0') return (int)c1 - (int)c2;
    } while (c1 == c2);
    return (int)c1 - (int)c2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    unsigned char c1, c2;
    while (n-- > 0) {
        c1 = (unsigned char)*s1++;
        c2 = (unsigned char)*s2++;
        if (c1 != c2) return (int)c1 - (int)c2;
        if (c1 == '\0') return 0;
    }
    return 0;
}

char *strchr(const char *s, int c) {
    unsigned char ch = (unsigned char)c;
    while (*s != '\0') {
        if ((unsigned char)*s == ch) return (char *)s;
        s++;
    }
    return (ch == '\0') ? (char *)s : NULL;
}

char *strrchr(const char *s, int c) {
    unsigned char ch = (unsigned char)c;
    const char *last = NULL;
    do {
        if ((unsigned char)*s == ch) last = s;
    } while (*s++ != '\0');
    return (char *)last;
}

static char *strtok_ptr;

char *strtok(char *s, const char *delim) {
    char *p;
    if (s != NULL) strtok_ptr = s;
    if (strtok_ptr == NULL) return NULL;
    p = strtok_ptr;
    while (*p != '\0' && strchr(delim, (unsigned char)*p) != NULL) p++;
    if (*p == '\0') { strtok_ptr = NULL; return NULL; }
    strtok_ptr = p;
    while (*strtok_ptr != '\0' && strchr(delim, (unsigned char)*strtok_ptr) == NULL) strtok_ptr++;
    if (*strtok_ptr != '\0') *strtok_ptr++ = '\0';
    else strtok_ptr = NULL;
    return p;
}

char *strstr(const char *haystack, const char *needle) {
    size_t nlen;
    if (*needle == '\0') return (char *)haystack;
    nlen = strlen(needle);
    while (*haystack != '\0') {
        if (strncmp(haystack, needle, nlen) == 0) return (char *)haystack;
        haystack++;
    }
    return NULL;
}

char *strerror(int errnum) {
    return (char *)__strerror(errnum);
}

const char *__strerror(int errnum) {
    switch (errnum) {
        case 0: return "No error";
        case 1: return "Operation not permitted";
        case 2: return "No such file or directory";
        case 3: return "No such process";
        case 4: return "Interrupted system call";
        case 5: return "Input/output error";
        case 6: return "No such device or address";
        case 7: return "Argument list too long";
        case 8: return "Exec format error";
        case 9: return "Bad file descriptor";
        case 10: return "No child processes";
        case 11: return "Resource temporarily unavailable";
        case 12: return "Out of memory";
        case 13: return "Permission denied";
        case 14: return "Bad address";
        case 16: return "Device or resource busy";
        case 17: return "File exists";
        case 18: return "Invalid cross-device link";
        case 19: return "No such device";
        case 20: return "Not a directory";
        case 21: return "Is a directory";
        case 22: return "Invalid argument";
        case 23: return "Too many open files in system";
        case 24: return "Too many open files";
        case 25: return "Inappropriate ioctl for device";
        case 27: return "File too large";
        case 28: return "No space left on device";
        case 29: return "Illegal seek";
        case 30: return "Read-only file system";
        case 31: return "Too many links";
        case 32: return "Broken pipe";
        case 33: return "Numerical argument out of domain";
        case 34: return "Numerical result out of range";
        case 36: return "Resource deadlock avoided";
        case 38: return "Function not implemented";
        case 61: return "No data available";
        case 62: return "Timer expired";
        case 66: return "Connection timed out";
        default: return "Unknown error";
    }
}
