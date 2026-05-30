#include "types.h"

#define SIGABRT 6
#define SIG_DFL ((void (*)(int))0)
#define SIG_IGN ((void (*)(int))1)
#define NSIG 7

void (*signal_funcs[NSIG])(int);

int raise(int sig) {
    void (*handler)(int);

    if ((unsigned int)sig >= NSIG) {
        return -1;
    }

    handler = signal_funcs[sig];

    if (handler == SIG_DFL) {
        signal_funcs[sig] = SIG_DFL;
        if (sig == SIGABRT) {
            exit(1);
        }
    } else if (handler != SIG_IGN) {
        signal_funcs[sig] = SIG_DFL;
        handler(sig);
    }

    return 0;
}
