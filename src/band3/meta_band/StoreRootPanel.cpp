#include "StoreRootPanel.h"
#include "meta/StorePackedMetadata.h"
#include "os/CommerceMgr_Wii.h"
#include "os/ContentMgr.h"
#include "os/ContentMgr_Wii.h"
#include "utl/Str.h"
#include "utl/TextStream.h"
StoreRootPanel::StoreRootPanel() {}

StoreRootPanel::~StoreRootPanel() {
    
}

void StoreRootPanel::Enter() {
    UIPanel::Enter();
}

void StoreRootPanel::Exit() {
    UIPanel::Exit();
}

void StoreRootPanel::Unload() {
    UIPanel::Unload();
}

DataNode StoreRootPanel::OnMsg(const MetadataLoadedMsg &msg) { return DataNode(kDataFloat, 6); }

BEGIN_HANDLERS(StoreRootPanel)
    HANDLE_MESSAGE(MetadataLoadedMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(149)
END_HANDLERS

void InitStoreOverlay() { 
    gStoreUIOverlay = RndOverlay::Find(store, false);
}

void UpdateStoreOverlay() {
    if (gStoreUIOverlay && gStoreUIOverlay->mShowing != false) {
        gStoreUIOverlay->Clear();
        *gStoreUIOverlay << "store flags: " << TheStoreMetadata.mFlags;
        int loadState = TheStoreMetadata.mLoadingState;
        if (loadState != 0) {
            *gStoreUIOverlay << " - load state: " << gStoreMetadataManagerLoadStepName[loadState];
        }
        *gStoreUIOverlay << "\n";
        int lineCount = 1;
        if (TheWiiCommerceMgr.mCommerceAsyncOpId != -1) {
            *gStoreUIOverlay << "commerce op: "
                << WiiCommerceMgr::mOpName[TheWiiCommerceMgr.mCommerceAsyncName]
                << " - " << TheWiiCommerceMgr.mProgressPercent << "%\n";
            lineCount = 2;
        }
        if (gLastErrorReturnValue != 0) {
            *gStoreUIOverlay << "commerce error: "
                << gLastErrorReturnValue << ", code: "
                << TheWiiCommerceMgr.mLastErrorCode << ", msg: "
                << gLastErrorDesc << "\n";
            lineCount++;
        }
        const char *modeName = (TheWiiContentMgr.mMode == 0) ? "SD" : "NAND";
        *gStoreUIOverlay << "content mgr: " << modeName << " mode.\n";
        int totalLines = lineCount + 1;
        for (std::list<Content *>::iterator it = TheWiiContentMgr.mContents.begin();
             it != TheWiiContentMgr.mContents.end(); ++it) {
            Content *content = *it;
            Content::State state = content->GetState();
            if (state != Content::kUnmounted && state != Content::kAlwaysMounted) {
                const char *busyStr = gCNTThreadInUse ? " busy" : "";
                *gStoreUIOverlay << "cu " << content->FileName() << " "
                    << gContentStateName[state] << busyStr << "\n";
                totalLines++;
            }
        }
        if (totalLines != 0) {
            gStoreUIOverlay->SetLines(totalLines);
        }
    }
}