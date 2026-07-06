#pragma once
#ifdef HX_NATIVE
#include <map>
#include <set>
#include <string>
#include <vector>
#endif
#include "char/Character.h"
#include "char/CharCollide.h"
#include "char/CharCuff.h"
#include "char/CharDriver.h"
#include "char/CharDriverMidi.h"
#include "char/CharEyes.h"
#include "char/CharHair.h"
#include "char/CharLipSyncDriver.h"
#include "char/CharLookAt.h"
#include "char/CharMeshHide.h"
#include "char/FileMerger.h"
#include "char/Waypoint.h"
#include "char/CharIKScale.h"
#include "char/CharIKHand.h"
#include "char/CharIKMidi.h"
#include "char/CharWeightSetter.h"
#include "char/CharBoneOffset.h"
#include "bandobj/BandCharDesc.h"
#include "bandobj/BandPatchMesh.h"
#include "bandobj/OutfitConfig.h"
#include "bandobj/CharKeyHandMidi.h"
#include "obj/Utl.h"
#include "rndobj/Rnd.h"
#include "rndobj/MeshDeform.h"

#ifdef HX_NATIVE
// render-polish 2026-06-11 (char-render): StartLoad caller-attribution tag for the
// RELOAD_PROBE diagnostics. Every BandCharacter::StartLoad call site sets this just
// before the call; StartLoad prints + resets it. Diagnostics only (probe-gated
// output); harmless single pointer write when probes are off.
extern const char *gNativeStartLoadTag;
#endif

class BandCharacter : public Character,
                      public BandCharDesc,
                      public MergeFilter,
                      public Rnd::CompressTextureCallback {
public:
    class BoneState {
    public:
        RndTransformable *mBone;
        Transform mXfm;
    };

    BandCharacter();
    OBJ_CLASSNAME(BandCharacter);
    OBJ_SET_TYPE(BandCharacter);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual ~BandCharacter();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SyncObjects();
    virtual bool AllowsInlineProxy() { return false; }
    virtual void AddedObject(Hmx::Object *);
    virtual void RemovingObject(Hmx::Object *);
    virtual void Replace(Hmx::Object *, Hmx::Object *);
    virtual void DrawShowing();
    virtual RndDrawable *CollideShowing(const Segment &, float &, Plane &);
    virtual void CollideList(const Segment &, std::list<Collision> &);
    virtual void Poll();
    virtual void Enter();
    virtual void Exit();
    virtual void Teleport(Waypoint *);
    virtual void CalcBoundingSphere();
    virtual float ComputeScreenSize(RndCam *);
    virtual void DrawLodOrShadow(int, DrawMode);
    virtual CharEyes *GetEyes() { return mEyes; }
    virtual bool ValidateInterest(CharInterest *, ObjectDir *);
    virtual bool SetFocusInterest(CharInterest *, int);
    virtual void SetInterestFilterFlags(int);
    virtual void ClearInterestFilterFlags();
    virtual void TextureCompressed(int);
    virtual RndTex *GetPatchTex(Patch &);
    virtual RndMesh *GetPatchMesh(Patch &);
    virtual RndTex *GetBandLogo();
    virtual void Compress(RndTex *, bool);
#ifdef HX_NATIVE
    // The Wii build leaves this empty (4-byte `blr`, matching the target): on
    // PPC it returns whatever is in r3, which the patch-pre-render path tolerates.
    // On native a non-void function that falls off the end is UB — the optimizer
    // emits a trap (x86 `ud2` → SIGILL), which crashes OutfitConfig::DrawPreClear's
    // BandPatchMesh::PreRender loop the moment a band character with patch meshes
    // composes its outfit. BandCharacter IS an ObjectDir (via Character→RndDir→
    // ObjectDir) and is the dir GetPatchMesh()/GetPatchTex() search for patch
    // meshes/textures, so the character's own dir is the correct patch dir.
    virtual ObjectDir *GetPatchDir() {
        return static_cast<ObjectDir *>(static_cast<Character *>(this));
    }
#else
    virtual ObjectDir *GetPatchDir() {}
#endif
    virtual void AddOverlays(BandPatchMesh &);
    virtual void MiloReload();
    virtual Action Filter(Hmx::Object *, Hmx::Object *, ObjectDir *);
    virtual Action FilterSubdir(ObjectDir *o1, ObjectDir *);

    void DrawLodOrShadowMode(int, DrawMode);
    void AddObject(Hmx::Object *);
    void ClearGroup();
    void StartLoad(bool, bool, bool);
    bool IsLoading();
    const char *FlagString(int);
    void SetContext(Symbol);
    void SavePrefabFromCloset();
    void SetSingalong(float);
    void GameOver();
    void ClearDircuts();
    void SetInstrumentType(Symbol);
    void SetGroupName(const char *);
    void SetHeadLookatWeight(float);
#ifdef HX_NATIVE
    // wave-08 native-only: repoint this band member's outfit skin meshes from the
    // static shared char/main/skeleton magnet onto the member's OWN animated
    // per-member skeleton bones (resolved by name via Find<RndTransformable>). Keeps
    // the authored gender-correct inverse-bind offset (SetBone calcOffset=false) so
    // the female stops flinging AND the band animates. Idempotent (runs once per
    // member, mNativeReboundOnce guard). Called from Poll once Find resolves to the
    // moving instance. No-op on Wii (HX_NATIVE only). Opt-out RB3_NO_SKEL_REBIND=1.
    void RebindOutfitBonesToOwnSkeleton();
    // native-only (C7/C8): rebind the head/hair/hands/face (+ remaining non-torso)
    // skin meshes onto the member's OWN per-member skeleton, baking an EXACT
    // inverse-bind against each bone's REST WorldXfm captured at the first Poll
    // (before Character::Poll applies any clip, so the per-member skeleton still holds
    // the SetDeformation gender-bind rest pose). The thin head/hair/finger geometry
    // SHARDS under the authored magnet offset (rotation-basis mismatch, R*sin(theta));
    // an exact rest-baked offset against the LIVE per-member bone -> coherent AND it
    // animates. Complements RebindOutfitBonesToOwnSkeleton (which owns the torso).
    // Opt-out RB3_NO_HEAD_REBIND=1. No-op on Wii (HX_NATIVE only).
    void RebindHeadHandsAtRest();
    // wave-inststrings (native-only): fix the band lead-guitar *_strings skin
    // explosion. The "brain"-class special guitars (chainsaw/guitar_brain/...) author
    // their string-bend rig on the CHARACTER skeleton (skeleton_unshared.milo) and
    // have NO own-resource neck (mInstDir->Find returns nil); on native the per-member
    // skeleton basis diverges from the authored bind so the rigid-authored strings
    // mesh skins to a ~136u world AABB (ratio ~5.0) and the V24 shard guard drops it.
    // Rigid-anchors every strings bone to the body-end bone (bone_bridge) and rebakes
    // its offset so the mesh rides that one bone rigidly (world AABB == bind AABB,
    // ratio ~1.0), matching the FINE instruments. Scoped to mInstDir *_strings.mesh
    // whose bones resolve to skeleton_unshared.milo only (never touches the FINE
    // own-resource instruments). Called from Poll AFTER mInstDir->Poll(). Opt-out
    // RB3_NO_INST_REBIND=1. No-op on Wii (HX_NATIVE only).
    void RebindInstStringsToRestBasis();
    // W2.8 BL-A1 (native-only): the hands/fingers "missing hands" fix. The head/hands
    // rest-rebind (RebindHeadHandsAtRest) binds each finger bone to its own live
    // per-member skeleton bone with a rest-baked inverse-bind, but the per-member
    // skeleton's rotation BASIS diverges from the authored magnet by a POSE-VARYING
    // angle theta, so the distal finger verts (radius R from their bone) fling by
    // R*sin(theta) into thin radiating sheets — a per-bone static rebake cannot fix a
    // pose-varying multi-bone divergence (W2.8 step-0, RB3_HANDS_BIND_FIX proven
    // inert). This collapses that divergence by rigid-anchoring: it repoints every
    // hand/finger bone (per L/R side) to that side's wrist bone (bone_L-hand /
    // bone_R-hand) and rebakes offset = meshWorld * inv(wristWorld), so each hand
    // rides its own wrist as ONE rigid body — no relative multi-bone basis error left
    // to shard. Exactly the proven RebindInstStringsToRestBasis mechanism, applied to
    // hand meshes. TRADEOFF: fingers no longer articulate (rigid hand); it trades the
    // shard for a static-but-coherent hand that tracks the arm. The faithful
    // finger-articulating fix is the CharBones pose-pipeline basis (engine, out of this
    // lane's fence) — see STATUS backlog. DEFAULT-OFF, opt-in RB3_HANDS_POSEAWARE=1;
    // scoped to hand/finger/glove skin meshes; each rebound mesh sets
    // RndMesh::mNativeBonesRebound. No-op on Wii (HX_NATIVE only).
    void NativeRepinHandsRigid();
    // render-polish 2026-06-11 (char-render): shared skinned-mesh collector used by
    // both Poll-time rebinds and the SyncObjects rest-pose seeding — hashtable
    // objects + each dir's mDraws + every LOD Group/TransGroup, recursing
    // RndDrawable::ListDrawChildren. Scope = {this, mOutfitDir}; mInstDir excluded.
    // frame-stall 2026-06-20 (TRACK A): now serves from mNativeSkinnedMeshCache
    // (rebuilt only when invalidated), so the RTTI-heavy walk runs once per (re)load
    // instead of every Poll. Appends the cached meshes to `out`.
    void NativeCollectSkinnedMeshes(std::vector<RndMesh *> &out);
    // frame-stall 2026-06-20 (TRACK A): (re)walk the dir/draw tree into the cache.
    void NativeRebuildSkinnedMeshCache();
    // frame-stall 2026-06-20 (TRACK A): drop the cache (call at StartLoad/SyncObjects,
    // where the dir tree may have been re-stuffed). Forces the next collect to rewalk.
    void NativeInvalidateSkinnedMeshCache();
    // render-polish 2026-06-11 (char-render): deterministic rest-pose seeding,
    // called from SyncObjects() right after SetDeformation() (the deform clip's
    // PoseMeshes leaves the skeleton at the weighted gender-bind REST pose). Seeds
    // mNativeRestPose for skin-mesh bones that resolve to a DISTINCT live
    // per-member instance and aren't snapshotted yet, so meshes (re)merged
    // mid-song bake against true rest instead of a mid-clip Poll pose.
    void NativeCaptureRestPoseAfterDeform();
#endif
    CharClipDriver *SetState(const char *, int, int, bool, bool);
    bool InVignetteOrCloset() const;
    void RemoveDrawAndPoll(Character *);
    void SetClipTypes(Symbol, Symbol);
    void SetTempoGenreVenue(Symbol, Symbol, const char *);
    void DeformHead(SyncMeshCB *);
    void SyncOutfitConfig(OutfitConfig *);
    void SetDeformation();
    void PlayGroup(const char *, bool, int, float, TaskUnits, Symbol);
    bool AllowOverride(const char *);
    bool SetPrefab(BandCharDesc *);
    bool AddDircut(Symbol, Symbol, int);
    bool AddDircut(const FilePath &);
    CharLipSyncDriver *GetLipSyncDriver();
    int GetShotFlags(Symbol);
    void SetVisemes();
    void RecomposePatches(BandCharDesc *, int);
    OutfitConfig *GetOutfitConfig(const char *);
    void SetLipSync(CharLipSync *);
    void SetSongOwner(CharLipSyncDriver *);
    void PlayFaceClip();
    void UpdateOverlay();
    void SetDircuts();
    void SaveBoneAndChildren(RndTransformable *);
    CharClipDriver *PlayMainClip(int, bool);
    Symbol InstrumentType() const { return mInstrumentType; }
    bool AddDriverClipDir() { return mAddDriver && mAddDriver->ClipDir(); }

    DataNode OnListDircuts();
    DataNode ListAnimGroups(int);
    DataNode OnPlayGroup(DataArray *);
    DataNode OnGroupOverride(DataArray *);
    DataNode OnChangeFaceGroup(DataArray *);
    DataNode OnSetPlay(DataArray *);
    DataNode OnCamTeleport(DataArray *);
    DataNode OnClosetTeleport(DataArray *);
    DataNode OnInstallFilter(DataArray *);
    DataNode OnPreClear(DataArray *);
    DataNode OnCopyPrefab(DataArray *);
    DataNode OnSavePrefab(DataArray *);
    DataNode OnSetFileMerger(DataArray *);
    DataNode OnLoadDircut(DataArray *);
    DataNode OnPostMerge(DataArray *);
    DataNode OnHideCategories(DataArray *);
    DataNode OnRestoreCategories(DataArray *);
    DataNode OnToggleInterestDebugOverlay(DataArray *);
    DataNode OnListDrumVenues(DataArray *);
    DataNode OnPortraitBegin(DataArray *);
    DataNode OnPortraitEnd(DataArray *);

    bool InCloset() const { return mInCloset; }
    const char *GetGroupName() const;
    int GetPlayFlags() const;

    static void MakeMRU(BandCharacter *, CharClip *);
    static void Init();
    static void Register() { REGISTER_OBJ_FACTORY(BandCharacter); }
    static void Terminate();
    DECLARE_REVS;
    NEW_OBJ(BandCharacter);
    NEW_OVERLOAD;
    DELETE_OVERLOAD;

    int mPlayFlags; // 0x450
    ObjPtr<CharDriver, ObjectDir> unk454; // 0x454
    CharDriver *mAddDriver; // 0x460
    CharDriver *mFaceDriver; // 0x464
    char mGroupName[64]; // 0x468
    char mFaceGroupName[64]; // 0x4a8
    char mOverrideGroup[64]; // 0x4e8
    bool mForceNextGroup; // 0x528
    bool mForceVertical; // 0x529
    ObjPtr<Character, ObjectDir> mOutfitDir; // 0x52c
    ObjPtr<Character, ObjectDir> mInstDir; // 0x538
    Symbol mTempo; // 0x544
    FileMerger *mFileMerger; // 0x548
    RndOverlay *mOverlay; // 0x54c
    ObjPtr<CharLookAt, ObjectDir> mHeadLookAt; // 0x550
    ObjPtr<CharLookAt, ObjectDir> mNeckLookAt; // 0x55c
    ObjPtr<CharEyes, ObjectDir> mEyes; // 0x568
    bool unk574; // 0x574
    ObjOwnerPtr<BandCharDesc, ObjectDir> mTestPrefab; // 0x578
    Symbol mGenre; // 0x584
    Symbol mDrumVenue; // 0x588
    Symbol mTestTourEndingVenue; // 0x58c
    Symbol mInstrumentType; // 0x590
    ObjPtr<Waypoint, ObjectDir> unk594; // 0x594
    bool mInCloset; // 0x5a0
    bool unk5a1; // 0x5a1
    bool unk5a2; // 0x5a2
    bool unk5a3; // 0x5a3
    ObjPtr<CharWeightSetter, ObjectDir> mSingalongWeight; // 0x5a4
    ObjPtrList<CharMeshHide, ObjectDir> unk5b0; // 0x5b0
    ObjPtrList<CharIKScale, ObjectDir> unk5c0; // 0x5c0
    ObjPtrList<CharIKHand, ObjectDir> unk5d0; // 0x5d0
    ObjPtrList<CharCollide, ObjectDir> unk5e0; // 0x5e0
    ObjPtrList<CharHair, ObjectDir> unk5f0; // 0x5f0
    ObjPtrList<CharCuff, ObjectDir> unk600; // 0x600
    ObjPtrList<RndMeshDeform, ObjectDir> unk610; // 0x610
    ObjPtrList<OutfitConfig, ObjectDir> unk620; // 0x620
    ObjPtrList<OutfitConfig, ObjectDir> unk630; // 0x630
    ObjPtrList<CharBoneOffset, ObjectDir> unk640; // 0x640
    ObjPtrList<CharIKMidi, ObjectDir> unk650; // 0x650
    ObjPtrList<CharDriverMidi, ObjectDir> unk660; // 0x660
    ObjPtrList<CharKeyHandMidi, ObjectDir> unk670; // 0x670
    ObjPtr<RndMesh, ObjectDir> unk680; // 0x680
    ObjPtr<RndMesh, ObjectDir> unk68c; // 0x68c
    ObjPtr<RndMesh, ObjectDir> unk698; // 0x698
    ObjPtr<RndMesh, ObjectDir> unk6a4; // 0x6a4
    ObjPtr<CharWeightable, ObjectDir> unk6b0; // 0x6b0
    bool mUseMicStandClips; // 0x6bc
    bool unk6bd; // 0x6bd
    ObjPtr<BandCharacter, ObjectDir> unk6c0; // 0x6c0
    std::list<String> mDircuts; // 0x6cc
    bool mInTourEnding; // 0x6d4
    float unk6d8; // 0x6d8
    std::list<int> mCompressedTextureIDs; // 0x6dc
    std::list<BoneState> unk6e4; // 0x6e4
    CharDriver *unk6ec; // 0x6ec
    int unk6f0; // 0x6f0
    char unk6f4[64]; // 0x6f4
    Waypoint *unk734; // 0x734
    unsigned int unk738; // 0x738
    ObjPtrList<RndMesh, ObjectDir> unk73c; // 0x73c
    ObjPtrList<RndMesh, ObjectDir> unk74c; // 0x74c
#ifdef HX_NATIVE
    // wave-08 native-only: rebind bookkeeping for RebindOutfitBonesToOwnSkeleton
    // (called from Poll). mNativeReboundOnce latches to 1 once the rebind is COMPLETE
    // (the body clothing + face/hands have been repointed AND a later scan finds
    // nothing new), after which Poll skips the scan entirely. mNativeReboundQuiet
    // counts consecutive no-new-rebind scans since the last rebind, so a late-loading
    // body mesh is still caught before latching. Appended after the matched layout so
    // the Wii image stays byte-identical. Default 0.
    int mNativeReboundOnce;
    int mNativeReboundQuiet;
    int mNativeReboundBody; // ever rebound a >=20-bone body/face mesh (latch gate)
    // C7/C8 head/hands rest-pose rebind bookkeeping (RebindHeadHandsAtRest).
    // mNativeRestPose snapshots each per-member bone's REST WorldXfm at the first Poll
    // (before Character::Poll), so late-streamed head meshes still rebake against the
    // true rest (not a mid-animation pose). mNativeHeadReboundOnce latches when no
    // head/hands mesh remains to rebind for a quiet window. Appended after the matched
    // layout so the Wii image stays byte-identical. Default 0/false/empty.
    int mNativeHeadReboundOnce;
    int mNativeHeadReboundQuiet;
    bool mNativeRestCaptured;
    std::map<std::string, Transform> mNativeRestPose;
    // render-polish 2026-06-11 (char-render): provenance for mNativeRestPose
    // entries. A bone name in this set was captured from a DISTINCT per-member
    // resolve (own != bound — the authoritative rest basis). Entries seeded while
    // own == bound (post-deform SyncObjects seeding, which may have resolved the
    // shared magnet) are overwritten ONCE by the first distinct resolve.
    std::set<std::string> mNativeRestDistinct;
    // wave-inststrings: rebind bookkeeping for RebindInstStringsToRestBasis (the band
    // lead-guitar *_strings rebind, called from Poll after mInstDir->Poll()).
    // mNativeInstReboundOnce latches when no in-scope strings mesh remains to rebind
    // for a quiet window; mNativeInstReboundQuiet counts consecutive no-rebind scans.
    // Appended after the matched layout so the Wii image stays byte-identical.
    // Default 0.
    int mNativeInstReboundOnce;
    int mNativeInstReboundQuiet;
    // W2.8 BL-A1 (native-only): rigid-anchor hands rebind bookkeeping
    // (NativeRepinHandsRigid, RB3_HANDS_POSEAWARE). Latches once no in-scope hand mesh
    // remains to rigid-anchor for a quiet window; re-armed on StartLoad/SyncObjects
    // like the other rebinds. Appended after the matched layout so the Wii image stays
    // byte-identical. Default 0.
    int mNativeHandsRigidOnce;
    int mNativeHandsRigidQuiet;
    // frame-stall 2026-06-20 (TRACK A): per-member skinned-mesh collection cache.
    // NativeCollectSkinnedMeshes used to re-walk the member's ObjectDir hashtable
    // (ObjDirItr RTTI dynamic_cast per entry) + the whole draw tree
    // (dynamic_cast<RndMesh*> per drawable) EVERY Poll for the ~10s rebind-latch
    // window — the #1 __dynamic_cast caller chain at song-start (~650ms). The set
    // of skinned meshes only changes when the dir tree is re-stuffed (StartLoad /
    // SyncObjects), so the walk result is cached here and invalidated at exactly
    // those points (NativeInvalidateSkinnedMeshCache). Appended after the matched
    // layout so the Wii image stays byte-identical. Default empty/false.
    std::vector<RndMesh *> mNativeSkinnedMeshCache;
    bool mNativeSkinnedCacheValid;
    // walk-on snap (docs/native/walkon-2026-07-02/SCOUT.md fix #1). Armed by
    // SetContext("venue"); while true, Poll() retries playing the default stage
    // idle each frame until a clip actually plays, so the on-stage band never
    // holds the stale loading-screen vignette pose during the count-in (the
    // body_clips group can finish loading a few frames after the band is already
    // visible). Self-clears once any clip is live. Appended after the matched
    // layout so the Wii image stays byte-identical. Default false.
    bool mNativeWalkonSnapPending;
#endif
};
