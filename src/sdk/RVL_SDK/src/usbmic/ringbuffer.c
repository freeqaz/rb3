struct micOpenParam {};
struct IsoTransfer {};

namespace usbmic {

struct RingBuffer {};

void memcpy_2byte_swap(void* dst, const void* src, unsigned long n) {
    (void)dst; (void)src; (void)n;
}

void memcpy_3byte_swap(void* dst, const void* src, unsigned long n) {
    (void)dst; (void)src; (void)n;
}

void memcpy_4byte_swap(void* dst, const void* src, unsigned long n) {
    (void)dst; (void)src; (void)n;
}

void rbInit(RingBuffer* rb, micOpenParam* param) {
    (void)rb; (void)param;
}

void rbDelete(RingBuffer* rb) {
    (void)rb;
}

void rbPutIsoTransfer(RingBuffer* rb, IsoTransfer* transfer, unsigned short n) {
    (void)rb; (void)transfer; (void)n;
}

long rbGetBlockUpto(RingBuffer* rb, unsigned char* buf, unsigned long* size) {
    (void)rb; (void)buf; (void)size;
    return 0;
}

} // namespace usbmic
