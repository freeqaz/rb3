#include "meta_band/StoreMenuPanel.h"
#include "StoreMenuPanel.h"

StoreMenuPanel* inst;

StoreMenuPanel::StoreMenuPanel(): unk38(), unk40(0xffffffff), unk44(0xffffffff), unk48(0), unk4c(0) {
   inst = this;
}
StoreMenuPanel::~StoreMenuPanel() {
   inst = nullptr;
}