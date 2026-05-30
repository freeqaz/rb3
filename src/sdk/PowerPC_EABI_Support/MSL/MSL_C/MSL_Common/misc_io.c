#include "misc_io.h"
#include "ansi_files.h"
#include <stdlib.h>

void __stdio_atexit(void) {
    atexit(__close_all);
}
