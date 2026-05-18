#include "types.h"
#include <revolution/rvl/so.h>
#include <revolution/ipc/ipcclt.h>

const char* __SOCKETVersion = "<< RVL_SDK - SOCKET \trelease build: Jun  9 2009 12:00:01 (0x4199_60831) >>";


so_fd_t SOSocket(so_pf_t protocol_family, so_type_t type, so_prot_t protocol) {

}

void* SOiAlloc(s32 size, s32 align);
void SOiFree(s32 size, void* ptr, s32 align);

so_ret_t SOClose(so_fd_t socket) {
    s32 x;
    so_ret_t ret;
    s32* buf;
    if (SOiPrepare(NULL, &x) == 0) {
        buf = (s32*)SOiAlloc(0xC, 0x20);
        if (buf == NULL) {
            ret = SO_ENOMEM;
        } else {
            *buf = socket;
            ret = IOS_Ioctl(x, 3, buf, 4, 0, 0);
            SOiFree(0xC, buf, 0x20);
        }
        SOiConclude(NULL, ret);
    }
}
