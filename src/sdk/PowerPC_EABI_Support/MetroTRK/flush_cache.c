asm void TRK_flush_cache(register void *start, register unsigned int size) {
    // clang-format off
    nofralloc

    cmplwi size, 0
    blelr

    clrlwi r5, start, 27
    add size, size, r5
    addi size, size, 31
    srwi size, size, 5
    mtctr size

cache_loop:
    dcbst 0, start
    icbi 0, start
    addi start, start, 32
    bdnz cache_loop

    sync
    isync

    blr
    // clang-format on
}
