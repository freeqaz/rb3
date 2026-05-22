#include "tour/Tour.h"
#include "tour/TourDesc.h"
#include "tour/TourProgress.h"
#include "meta_band/Campaign.h"
#include "meta_band/CampaignLevel.h"
#include "meta_band/MetaPanel.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/TexLoadPanel.h"
#include "meta_band/BandMachineMgr.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "rndobj/Mesh.h"
#include "stl/_algo.h"
#include "ui/UIList.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "ui/UIListProvider.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "utl/VectorSizeDefs.h"
#include <vector>

namespace stlpmtx_std {
template <>
inline void _Temporary_buffer<Symbol *, Symbol>::_M_allocate_buffer() {
    _M_original_len = _M_len;
    _M_buffer = 0;
    if (_M_len > (ptrdiff_t)(INT_MAX / sizeof(Symbol)))
        _M_len = INT_MAX / sizeof(Symbol);
    while (_M_len > 0) {
        _M_buffer = (Symbol *)_MemAlloc(_M_len * sizeof(Symbol), 0);
        if (_M_buffer)
            break;
        _M_len /= 2;
    }
}

template <>
inline _Temporary_buffer<Symbol *, Symbol>::~_Temporary_buffer() {
    _STLP_STD::_Destroy_Range(_M_buffer, _M_buffer + _M_len);
    _MemFree(_M_buffer);
}
} // namespace stlpmtx_std

class TourDescCmp {
public:
    TourDescCmp(const Tour *tour) : mTour(tour) {}
    bool operator()(Symbol s1, Symbol s2) const {
        TourDesc *pLHS = mTour->GetTourDesc(s1);
        MILO_ASSERT(pLHS, 0x2D);
        TourDesc *pRHS = mTour->GetTourDesc(s2);
        MILO_ASSERT(pRHS, 0x30);
        return pLHS->GetIndex() < pRHS->GetIndex();
    }
    const Tour *mTour; // 0x0
};

class TourDescProvider : public UIListProvider, public Hmx::Object {
public:
    TourDescProvider();
    virtual ~TourDescProvider();
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual void UpdateExtendedText(int, int, UILabel *) const;
    virtual void UpdateExtendedMesh(int, int, RndMesh *) const;
    virtual void UpdateExtendedCustom(int, int, Hmx::Object *) const;
    virtual Symbol DataSymbol(int) const;
    virtual int NumData() const;
    virtual void InitData(RndDir *);
    virtual UIComponent::State
    ComponentStateOverride(int, int, UIComponent::State) const;

    inline bool IsTourDescAvailable(Symbol) const;
    void UpdateList();

    std::vector<DynamicTex *> *mTexs; // 0x20
    RndMat *mUnearnedMat; // 0x24
    RndMat *mEarnedMat; // 0x28
    std::vector<Symbol> mTours; // 0x2c
};

inline bool TourDescProvider::IsTourDescAvailable(Symbol s) const {
    TourDesc *pTourDesc = TheTour->GetTourDesc(s);
    MILO_ASSERT(pTourDesc, 0x226);
    MILO_ASSERT(TheTour->GetTourProgress(), 0x229);
    if (pTourDesc->HasRequiredCampaignLevel()) {
        Symbol level = pTourDesc->GetRequiredCampaignLevel();
        BandMachineMgr *pMachineMgr = TheSessionMgr->GetMachineMgr();
        MILO_ASSERT(pMachineMgr, 0x230);
        if (!TheCampaign->HasScoreReachedCampaignLevel(
                pMachineMgr->GetLeaderPrimaryMetaScore(), level
            ))
            return false;
    }
    return true;
}

class TourDescProvider;

class TourDescPanel : public TexLoadPanel {
public:
    TourDescPanel();
    OBJ_CLASSNAME(TourDescPanel);
    OBJ_SET_TYPE(TourDescPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~TourDescPanel();
    virtual void Load();
    virtual void FinishLoad();
    virtual void Enter();
    virtual void Unload();

    Symbol GetSelectedTourDesc(class UIComponent *);
    void LoadIcons();
    void Refresh();
    bool IsTourAvailable();
    Symbol GetInitiallySelectedTour() const;
    void ClearInitiallySelectedTour();
    void SelectDefaultTour();
    void SelectTour(Symbol);
    void CheatWinTour();
    TourDescProvider *mProvider; // 0x4c
};

void TourDescPanel::Refresh() {
    std::vector<Symbol> descs;
    std::stable_sort(descs.begin(), descs.end(), TourDescCmp(TheTour));
}

void TourDescPanel::LoadIcons() {
    AddTex(
        "ui/accomplishments/accomplishment_art/acc_unearned_award_bronze_keep.bmp",
        "tourprize_bronze",
        true,
        false
    );
    AddTex(
        "ui/accomplishments/accomplishment_art/acc_unearned_award_silver_keep.bmp",
        "tourprize_silver",
        true,
        false
    );
    AddTex(
        "ui/accomplishments/accomplishment_art/acc_unearned_award_gold_keep.bmp",
        "tourprize_gold",
        true,
        false
    );
    for (std::map<Symbol, TourDesc *>::const_iterator it =
             TheTour->m_mapTourDesc.begin();
         it != TheTour->m_mapTourDesc.end();
         ++it) {
        TourDesc *pTourDesc = (*it).second;
        Symbol s = (*it).first;
        MILO_ASSERT(pTourDesc, 0x2A7);
        AddTex(pTourDesc->GetArt(), s.Str(), true, false);
        AddTex(pTourDesc->GetGrayArt(), MakeString("%s_gray", s.Str()), true, false);
    }
    for (std::map<Symbol, CampaignLevel *>::const_iterator it =
             TheCampaign->m_mapCampaignLevels.begin();
         it != TheCampaign->m_mapCampaignLevels.end();
         ++it) {
        CampaignLevel *pLevel = (*it).second;
        Symbol s = (*it).first;
        MILO_ASSERT(pLevel, 0x2B9);
        String iconArt = pLevel->GetIconArt();
        if (pLevel->IsMajorLevel())
            AddTex(iconArt.c_str(), s.Str(), true, false);
    }
}

void TourDescProvider::UpdateExtendedMesh(int, int iData, RndMesh *i_pMesh) const {
    MILO_ASSERT(iData < NumData(), 0x14C);
    Symbol s = DataSymbol(iData);
    TourDesc *pTourDesc = TheTour->GetTourDesc(s);
    MILO_ASSERT(pTourDesc, 0x150);
    bool bAvailable = MetaPanel::sUnlockAll ? true : IsTourDescAvailable(s);
    if (!strcmp(i_pMesh->Name(), "tour_art.mesh")) {
        String texName(bAvailable ? pTourDesc->GetName()
                                  : MakeString("%s_gray", pTourDesc->GetName()));
        std::vector<DynamicTex *>::iterator it =
            std::find(mTexs->begin(), mTexs->end(), texName);
        if (it != mTexs->end())
            i_pMesh->SetMat((*it)->mMat);
        else
            i_pMesh->SetMat(0);
    }
}

void TourDescProvider::Text(
    int, int iData, UIListLabel *i_pSlot, UILabel *i_pLabel
) const {
    MILO_ASSERT(iData < NumData(), 0x5C);
    Symbol s = DataSymbol(iData);
    TourDesc *pTourDesc = TheTour->GetTourDesc(s);
    MILO_ASSERT(pTourDesc, 0x60);
    bool bSelected = TheTour->GetTourProgress()->GetTourDesc() == s;
    bool bAvailable = MetaPanel::sUnlockAll ? true : IsTourDescAvailable(s);
    if (i_pSlot->Matches("name")) {
        i_pLabel->SetTextToken(bAvailable ? s : Symbol(gNullStr));
    } else if (i_pSlot->Matches("inprogress")) {
        i_pLabel->SetTextToken(
            (bAvailable && bSelected) ? tour_inprogress : Symbol(gNullStr)
        );
    } else if (i_pSlot->Matches("locked")) {
        i_pLabel->SetTextToken(!bAvailable ? tour_locked : Symbol(gNullStr));
    } else if (i_pSlot->Matches("numsongs")) {
        if (bAvailable && !bSelected) {
            i_pLabel->SetTokenFmt(tour_desc_songcount, pTourDesc->GetNumSongs());
        } else {
            i_pLabel->SetTextToken(Symbol(gNullStr));
        }
    } else {
        i_pLabel->SetTextToken(Symbol(i_pSlot->GetDefaultText()));
    }
}

void TourDescPanel::SelectTour(Symbol s) {
    int index = 0;
    if (s != "") {
        index = 0;
        std::vector<Symbol> &tours = mProvider->mTours;
        std::vector<Symbol>::iterator it = tours.begin();
        for (; it != tours.end(); ++it, ++index) {
            if (s == *it)
                break;
        }
        if (it == tours.end())
            index = 0;
    }
    UIList *pList = Dir()->Find<UIList>("pTourList", true);
    MILO_ASSERT(pList, 0x30B);
    pList->SetSelected(index, -1);
}

BEGIN_HANDLERS(TourDescPanel)
    HANDLE_ACTION(refresh, Refresh())
    HANDLE_EXPR(get_selected_tour, GetSelectedTourDesc(0))
    HANDLE_EXPR(is_tour_available, IsTourAvailable())
    HANDLE_ACTION(cheat_win_tour, CheatWinTour())
    HANDLE_SUPERCLASS(TexLoadPanel)
    HANDLE_CHECK(0x345)
END_HANDLERS
