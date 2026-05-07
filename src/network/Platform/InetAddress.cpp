#include "Platform/InetAddress.h"

#include "Platform/StringConversion.h"
#include "Platform/BadEvents.h"
#include <vector>
#include "revolution/rvl/so.h"

// uncompiled
extern "C" u32 SOGetHostID(void);
extern "C" s32 SOInetPtoN(s32, char *, s32 *);
extern "C" void SOInetNtoP(s32, const s32 *, char *, s32);
extern u16 htons(u16 arg0);
extern u16 ntohs(u16 arg0);
extern s32 htonl(unsigned int arg0);
extern s32 ntohl(unsigned int arg0);

namespace Quazal {
    InetAddress::InetAddress() {
        memset(this, 0, 8);
        this->unk1 = 2;
    }

    InetAddress::InetAddress(const InetAddress &orig) { memcpy(this, &orig, 0x20); }

    InetAddress::InetAddress(const char *str, u16 arg1) {
        char sp110[0x100];
        char sp10[0x100];
        s32 spC;
        s32 sp8;
        u32 var_r3;
        const char *temp_r28;
        so_host_t *temp_r3;

        memset(this, 0, 8);
        this->unk1 = 2;
        if (strcmp(str, "255.255.255.255") == 0) {
            this->address = -1;
        } else if (strcmp(str, "localhost") == 0) {
            temp_r28 = "127.0.0.1";
            if (strcmp(temp_r28, "255.255.255.255") == 0) {
                this->address = -1;
            } else if (strcmp(temp_r28, "localhost") == 0) {
                SetAddress(temp_r28);
            } else {
                StringConversion::T2Char8(temp_r28, sp110, 0x100);
                if (SOInetPtoN(2, sp110, &sp8) > 0) {
                    var_r3 = sp8;
                } else {
                    var_r3 = -1;
                }
                this->address = var_r3;
                if (var_r3 == -1) {
                    BadEvents::Signal((BadEvents::_ID)7);
                    temp_r3 = SOGetHostByName(sp110);
                    if (temp_r3 != NULL) {
                        this->address = *(uint *)temp_r3->h_addr_list[0];
                    }
                }
            }
        } else {
            StringConversion::T2Char8(temp_r28, sp10, 0x100);
            if (SOInetPtoN(2, sp10, &spC) > 0) {
                var_r3 = spC;
            } else {
                var_r3 = -1;
            }
            this->address = var_r3;
            if (var_r3 == -1) {
                BadEvents::Signal((BadEvents::_ID)7);
                temp_r3 = SOGetHostByName(sp10);
                if (temp_r3 != NULL) {
                    this->address = *(uint *)temp_r3->h_addr_list[0];
                }
            }
        }
        this->port = htons(arg1);
    }

    InetAddress::~InetAddress() {}

    void InetAddress::Init() {
        memset(this, 0, 8);
        this->unk1 = 2;
    }

    s32 InetAddress::SetAddress(const char *arg0) {
        char sp110[0x100];
        char sp10[0x100];
        s32 spC;
        s32 sp8;
        u32 var_r3;
        char *temp_r30;
        so_host_t *temp_r3;

        if (strcmp(arg0, "255.255.255.255") == 0) {
            this->address = -1;
            return 1;
        }
        if (strcmp(arg0, "localhost") == 0) {
            temp_r30 = "127.0.0.1";
            if (strcmp(temp_r30, "255.255.255.255") == 0) {
                this->address = -1;
                return 1;
            }
            if (strcmp(temp_r30, "localhost") == 0) {
                return this->SetAddress(temp_r30);
            }
            StringConversion::T2Char8(temp_r30, sp10, 0x100);
            if (SOInetPtoN(2, sp10, &spC) > 0) {
                var_r3 = spC;
            } else {
                var_r3 = -1;
            }
            this->address = var_r3;
            if (var_r3 == -1) {
                BadEvents::Signal((Quazal::BadEvents::_ID)7);
                temp_r3 = SOGetHostByName(sp10);
                if (temp_r3 != NULL) {
                    this->address = *(uint *)temp_r3->h_addr_list[0];
                } else {
                    return 0;
                }
            }
            return 1;
        }
        StringConversion::T2Char8(arg0, sp110, 0x100);
        if (SOInetPtoN(2, sp110, &sp8) > 0) {
            var_r3 = sp8;
        } else {
            var_r3 = -1;
        }
        this->address = var_r3;
        if (var_r3 == -1) {
            BadEvents::Signal((Quazal::BadEvents::_ID)7);
            temp_r3 = SOGetHostByName(sp110);
            if (temp_r3 != NULL) {
                this->address = *(uint *)temp_r3->h_addr_list[0];
            } else {
                return 0;
            }
        }
        return 1;
    }

    void InetAddress::SetAddress(unsigned int addr) { this->address = htonl(addr); }

    void InetAddress::SetNetworkAddress(unsigned int addr) { this->address = addr; }

    s32 InetAddress::GetAddress() const { return ntohl(this->address); }

    s32 InetAddress::GetAddress(char *arg0, unsigned int arg1) const {
        char sp8[0x20];

        if (arg1 < 0x10) {
            return 0;
        }
        SOInetNtoP(2, &this->address, &sp8[0], 0x14);
        StringConversion::Char8_2T(&sp8[0], arg0, arg1);
        return 1;
    }

    void InetAddress::SetPortNumber(u16 port) { this->port = htons(port); }

    u16 InetAddress::GetPortNumber() const { return ntohs(this->port); }

    // 90% match. Remaining 10% is register allocation (curr_port held in r5 vs r3,
    // this->address loaded into r29 vs r31) — CW schedules the b.address load too
    // early and keeps curr_port in a non-callee register because of the |OR with
    // the high half of the u64.
    bool InetAddress::operator<(const Quazal::InetAddress &b) const {
        u32 check_addr = b.address;
        u32 check_port = ntohs(b.port);
        u32 curr_addr = this->address;
        u32 curr_port = (u16)ntohs(this->port);

        u64 lhs = ((u64)(curr_addr ^ 0x80000000) << 32) | curr_port;
        u64 rhs = ((u64)(check_addr ^ 0x80000000) << 32) | check_port;
        return lhs < rhs;
    }

    bool InetAddress::operator==(const InetAddress &b) const {
        s32 temp_r31;
        s32 temp_r30;
        s32 temp_r29;

        s32 temp;
        s32 temp2;

        temp_r29 = b.address;
        temp_r30 = ntohs(b.port);
        temp_r31 = this->address;
        temp = ((u16)ntohs(this->port));

        return ((temp ^ temp_r30) | (temp_r31 ^ temp_r29)) == 0;
    }

    InetAddress *InetAddress::operator=(const InetAddress &b) {
        memcpy(this, &b, 0x20);
        return this;
    }
}

DECOMP_FORCEACTIVE(str, "%d", ":")