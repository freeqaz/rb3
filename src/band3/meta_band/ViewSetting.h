#pragma once
#include "obj/Object.h"
#include "ui/UIListProvider.h"
#include <vector>

class ViewSetting;

class ViewSettingsProvider : public UIListProvider, public Hmx::Object {
public:
    ViewSettingsProvider();
    virtual ~ViewSettingsProvider() {}
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual int NumData() const;
    virtual bool IsActive(int) const;
    virtual void InitData(RndDir *);
    virtual UIColor *SlotColorOverride(int, int, class UIListWidget *, UIColor *c) const;

    void BuildFilters(Symbol);

private:
    // [+0x20] 8 bytes: STL-Port vector<ViewSetting*> (ptr + 2 x unsigned short)
    std::vector<ViewSetting *> mSettings;
    // [+0x28] currently selected/active setting
    ViewSetting *mActiveSetting;
    // [+0x2c-0x38] TODO: identify when ViewSetting.cpp is decomposed
    int mUnk2c;
    int mUnk30;
    int mUnk34;
    int mUnk38;
};