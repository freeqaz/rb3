#include "types.h"

#define SIGABRT 6

extern int raise(int sig);
extern void exit(int status);

int __aborting;
void (*__stdio_exit)(void);

void abort(void) {
    if (!__aborting) {
        __aborting = 1;
        raise(SIGABRT);
    }
    exit(-1);
}
