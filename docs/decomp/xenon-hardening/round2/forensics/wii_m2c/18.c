CharData *GetDefaultPrefab__9PrefabMgrCFi(PrefabMgr *this, s32 arg0); /* extern */
PrefabMgr *GetPrefabMgr__9PrefabMgrFv(PrefabMgr *this); /* extern */
? SetChar__8BandUserFP8CharData(BandUser *this, CharData *arg0); /* static */

/* BandUser::SetLoadedPrefabChar (int) */
void SetLoadedPrefabChar__8BandUserFi(BandUser *this, s32 arg0) {
    SetChar__8BandUserFP8CharData(this, GetDefaultPrefab__9PrefabMgrCFi(GetPrefabMgr__9PrefabMgrFv((PrefabMgr *) this), arg0));
}