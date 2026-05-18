#pragma once
#include "meta_band/BandSongMetadata.h"
#include "stl/pointers/_vector.h"
#include "system/ui/UIPanel.h"

class StoreMenuPanel : public UIPanel {
public:
    StoreMenuPanel();
    OBJ_CLASSNAME(StoreMenuPanel);
    OBJ_SET_TYPE(StoreMenuPanel);
    NEW_OBJ(StoreMenuPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~StoreMenuPanel();
    virtual void FinishLoad();
    virtual void Unload();
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();

    void AddMenu(DataArray*, const char*);
    void OnBack(const DataArray*);
    void OnMsg(const MetadataLoadedMsg&);
    const char* GetCrumbText() const;
    void SetPendingMenuIx(int);

    static StoreMenuPanel* sInstance() { return inst; }

    std::vector<int> unk38; // 0x38, 0x3c, 0x3e
    int (unk40); // 0x40
    int (unk44); // 0x44
    int (unk48); // 0x48
    int (unk4c); // 0x4c

    static StoreMenuPanel* inst;
};