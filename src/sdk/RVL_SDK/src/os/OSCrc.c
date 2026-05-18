#include <revolution/os/OSCrc.h>

u32 OSCalcCRC32(const void *buf, u32 length) {
    static u32 crc32_table[16] = { 0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC,
                                   0x76DC4190, 0x6B6B51F4, 0x4DB26158, 0x5005713C,
                                   0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
                                   0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C };
    u8 *p = (u8 *)buf;
    u32 crc = 0xFFFFFFFF;

    if (length != 0) {
        u32 words = length >> 2;
        for (; words > 0; --words) {
            u32 b;
            u32 lo;

            b = p[0];
            lo = (crc >> 4) ^ crc32_table[(crc ^ b) & 0xF];
            crc = (lo >> 4) ^ crc32_table[(lo ^ (b >> 4)) & 0xF];
            b = p[1];
            lo = (crc >> 4) ^ crc32_table[(crc ^ b) & 0xF];
            crc = (lo >> 4) ^ crc32_table[(lo ^ (b >> 4)) & 0xF];
            b = p[2];
            lo = (crc >> 4) ^ crc32_table[(crc ^ b) & 0xF];
            crc = (lo >> 4) ^ crc32_table[(lo ^ (b >> 4)) & 0xF];
            b = p[3];
            p += 4;
            lo = (crc >> 4) ^ crc32_table[(crc ^ b) & 0xF];
            crc = (lo >> 4) ^ crc32_table[(lo ^ (b >> 4)) & 0xF];
        }

        length &= 3;
        if (length != 0) {
            do {
                u32 b = *p++;
                u32 lo = (crc >> 4) ^ crc32_table[(crc ^ b) & 0xF];
                crc = (lo >> 4) ^ crc32_table[(lo ^ (b >> 4)) & 0xF];
            } while (--length != 0);
        }
    }

    return ~crc;
}
