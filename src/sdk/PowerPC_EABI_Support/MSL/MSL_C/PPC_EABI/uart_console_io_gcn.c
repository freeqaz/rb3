#include "MSL_Common/file_def.h"
#include "MSL_Common/console_io.h"
#include "MSL_Common_Embedded/UART.h"

static void __init_uart_console(void) {
    static int initialized = 0;
    if (!initialized) {
        initialized = 1;
        InitializeUART(kBaud115200);
    }
}

int __write_console(__file_handle handle, unsigned char *buffer, size_t *count, __ref_con ref_con) {
    (void)handle;
    (void)ref_con;
    __init_uart_console();
    if (WriteUARTN(buffer, *count) != kUARTNoError) {
        return -1;
    }
    return 0;
}

int __close_console(__file_handle handle) {
    (void)handle;
    return 0;
}
