#include "types.h"

s32 uhf_ker_get_memory_block(s32 size, void **ptr) {
    (void)size; (void)ptr;
    return 0;
}

s32 uhf_ker_release_memory_block(void *ptr) {
    (void)ptr;
    return 0;
}

s32 uhf_ker_check_cacheable(void *ptr) {
    (void)ptr;
    return 0;
}
