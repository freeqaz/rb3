namespace usbmic {

struct USB_CommonDescr {};
struct UA_CommonDescr {};

long StartGetDescriptor(long dev, unsigned char type, void* buf,
    unsigned short size, void (*cb)(long, void*), void* arg) {
    (void)dev; (void)type; (void)buf; (void)size; (void)cb; (void)arg;
    return 0;
}

long StartSetInterface(long dev, unsigned char iface, unsigned char alt,
    void (*cb)(long, void*), void* arg) {
    (void)dev; (void)iface; (void)alt; (void)cb; (void)arg;
    return 0;
}

long StartGetMinMaxReq(long dev, unsigned char req, unsigned char cs,
    unsigned char unit, unsigned char iface, unsigned char chan,
    void* buf, unsigned short size, void (*cb)(long, void*), void* arg) {
    (void)dev; (void)req; (void)cs; (void)unit; (void)iface; (void)chan;
    (void)buf; (void)size; (void)cb; (void)arg;
    return 0;
}

USB_CommonDescr* ScanDescriptor(USB_CommonDescr* base, unsigned char type,
    USB_CommonDescr* start, unsigned long size) {
    (void)base; (void)type; (void)start; (void)size;
    return 0;
}

UA_CommonDescr* ScanUADescriptor(UA_CommonDescr* base, unsigned char subtype,
    unsigned char id, UA_CommonDescr* start, unsigned long size) {
    (void)base; (void)subtype; (void)id; (void)start; (void)size;
    return 0;
}

UA_CommonDescr* ScanUADescriptorById(UA_CommonDescr* base, unsigned char id,
    UA_CommonDescr* start, unsigned long size) {
    (void)base; (void)id; (void)start; (void)size;
    return 0;
}

} // namespace usbmic
