#include "meta/MemcardMgr_Wii.h"
#include "os/Debug.h"
#include "utl/MemMgr.h"

MemcardMgr TheMemcardMgr;

namespace {
    int TheWiiNeedSizeFSBlocks;
}

MemcardMgr::MemcardMgr()
    : unka4(0), unka8(0), unkac(0), mFlags(0), unkbc(-1), unkc0(0), unkcc(0), unkd0(0),
      unkd4(-1), unkd8(-1), unkdc(0) {}

MemcardMgr::~MemcardMgr() {}

void MemcardMgr::SetDevice(unsigned int) {
    MILO_FAIL("SetDevice not supported on the Wii.\n");
}

void MemcardMgr::SelectDevice(Profile *, bool, Hmx::Object *, int) {
    MILO_FAIL("SelectDevice not supported on the Wii.\n");
}

void MemcardMgr::OnSearchForDevice(Profile *) {
    MILO_FAIL("OnSearchForDevice not supported on the Wii.\n");
}

void MemcardMgr::OnCheckForSaveContainer(Profile *) {
    MILO_ASSERT(false, 0x1b4);
}

void MemcardMgr::UnLoadBanner() {
    if (mBanner) {
        _MemFree(mBanner);
        mBanner = NULL;
    }
    if (mBannerIcons) {
        _MemFree(mBannerIcons);
        mBannerIcons = NULL;
    }
}

int MemcardMgr::GetSizeNeeded() { return TheWiiNeedSizeFSBlocks; }

void MemcardMgr::DisableWriting(bool b) {
    if (b) {
        mFlags |= 1;
    } else {
        mFlags &= ~1;
    }
}

bool MemcardMgr::IsDisableWriting() const { return mFlags & 1; }

bool MemcardMgr::IsWriteMode() const { return (mFlags >> 1) & 1; }
