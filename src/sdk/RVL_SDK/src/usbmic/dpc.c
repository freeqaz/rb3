namespace usbmic {

struct DPCContext {};
struct DPCEntry {};

void DPC_Initialize(DPCContext* ctx) {
    (void)ctx;
}

void DPC_Deinitialize(DPCContext* ctx) {
    (void)ctx;
}

void DPC_Queue(DPCContext* ctx, DPCEntry* entry) {
    (void)ctx; (void)entry;
}

void DPC_Process(DPCContext* ctx) {
    (void)ctx;
}

} // namespace usbmic
