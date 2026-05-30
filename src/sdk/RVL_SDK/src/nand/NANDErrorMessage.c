#include "types.h"

void *NANDErrorFunc;

void __NANDShowErrorMessage(s32 result) {
    (void)result;
}

void NANDSetAutoErrorMessaging(s32 enable) {
    (void)enable;
}

void __NANDPrintErrorMessage(s32 result) {
    (void)result;
}
