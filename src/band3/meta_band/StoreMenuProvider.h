#pragma once
#include "obj/Object.h"
#include "ui/UIListProvider.h"
#include "utl/Str.h"

class DataArray;
class StorePage;
class RndTex;

class StoreMenuProvider : public UIListProvider, public Hmx::Object {
public:
    StoreMenuProvider(DataArray *, const char *);
    virtual ~StoreMenuProvider();
    virtual DataNode Handle(DataArray *, bool);

    void SetData(DataArray *);
    virtual int NumData() const;
    virtual bool IsActive(int) const;
    virtual void Text(int, int, class UIListLabel *, class UILabel *) const;
    const char *GetTitle();

    int mIxHighlight; // 0x20
    String mPathBase; // 0x24 (String = 8 bytes)
    StorePage *mPage; // 0x2c
    RndTex *mBanner; // 0x30
    int unk34;
    int unk38;
    int unk3c;
}; // 0x44
