struct IsoTransfer {};

namespace usbmic {

struct Mic {};
struct UsbRequest {};

void urInit(UsbRequest* ur, Mic* mic, unsigned short n) {
    (void)ur; (void)mic; (void)n;
}

void urDelete(UsbRequest* ur) {
    (void)ur;
}

void urOnRecordIsochDone(long result, IsoTransfer* transfer, void* arg) {
    (void)result; (void)transfer; (void)arg;
}

long urStartRecording(UsbRequest* ur) {
    (void)ur;
    return 0;
}

} // namespace usbmic
