#ifndef RNDWII_RND_H
#define RNDWII_RND_H

#include "math/Geo.h"
#include "os/DiscErrorMgr_Wii.h"
#include "os/HomeMenu_Wii.h"
#include "os/VirtualKeyboard.h"
#include "revolution/mtx/mtx.h"
#include "rndobj/Rnd.h"
#include "rndwii/SplitPostProc.h"
#include <vector>

class WiiOrthoProj {
public:
    WiiOrthoProj();
    ~WiiOrthoProj();
    float proj[8];
};

class WiiRnd : public Rnd,
               public HomeMenu::Callback,
               public VirtualKeyboard::Callback,
               public DiscErrorMgrWii::Callback {
public:
    enum SharedTexType {
    };

    WiiRnd();
    virtual ~WiiRnd();
    virtual DataNode Handle(DataArray *, bool);
    virtual void SetAspect(Aspect a) { mAspect = a; }
    virtual void RemovePointTest(RndFlare *);
    virtual void DoPostProcess();

    void SwapFrameBuffer();
    void SetTriFrameRendering(bool);
    void SetOrthoProj();
    void DoPointTests();
    bool GetProgressiveScan();
    void CopyBuffer();
    void DrawQuad(const Hmx::Rect &);
    void DrawQuad(int, int);
    void DrawLine(const Vector3 &, const Vector3 &, const Hmx::Color &, bool);
    void WiiPreInit();
    void SetFullScrProj();
    void PreInit();
    void ClearSwapTables();
    void DrawBlackBackground();
    void KeyboardOpen();
    void KeyboardClose();
    RndTex *GetSharedTex(SharedTexType, bool);

    // Layout: Rnd is 0x160. Three Callback bases add 4-byte vtables at
    // 0x160, 0x164, 0x168. Own fields begin at 0x16C.
    char pad_0x16C[0x4]; // 0x16C
    ushort unk_0x170; // 0x170
    ushort unk_0x172; // 0x172
    char pad_0x174[0x10]; // 0x174
    u8 unk_0x184; // 0x184
    u8 unk_0x185; // 0x185
    char pad_0x186[0x22]; // 0x186 (34 bytes -> 0x1A8)
    void *unk_0x1A8; // 0x1A8
    void *unk_0x1AC; // 0x1AC
    bool unk_0x1B0; // 0x1B0 (init=true)
    char pad_0x1B1[0x3]; // 0x1B1
    void *unk_0x1B4; // 0x1B4
    u32 unk_0x1B8; // 0x1B8
    Mtx44 unk_0x1BC; // 0x1BC
    Mtx44 unk_0x1FC; // 0x1FC
    Mtx unk_0x23C; // 0x23C (48 bytes -> 0x26C)
    int unk_0x26C; // 0x26C (init=0)
    bool unk_0x270; // 0x270 (init=true)
    char pad_0x271[0x3]; // 0x271
    WiiSplitPostProc *unk_0x274; // 0x274
    char pad_0x278[0x35]; // 0x278 (53 bytes -> 0x2AD)
    bool mProgScan; // 0x2AD
    bool unk_0x2AE; // 0x2AE
    bool unk_0x2AF; // 0x2AF (init=true)
    bool unk_0x2B0; // 0x2B0
    bool unk_0x2B1; // 0x2B1
    bool unk_0x2B2; // 0x2B2
    bool unk_0x2B3; // 0x2B3
    std::vector<Rnd::PointTest> unk_0x2B4; // 0x2B4 (8 bytes -> 0x2BC)
    bool unk_0x2BC; // 0x2BC
    int mFramesBuffered; // 0x2C0

    void PrepareRenderAlley();
    void RestoreRenderAlley();

    static bool mUseLockedCache, mShowParticle, mShowAssetName;
    static void ToggleAssetName() { mShowAssetName = !mShowAssetName; }
    static void ToggleShowParticle() { mShowParticle = !mShowParticle; }
    static void ToggleLockedCache() { mUseLockedCache = !mUseLockedCache; }
    static void SyncFree(void *);
    static void *GetCurrXFB();
};

void SetGPHangDetectEnabled(bool, const char *);
void RndGXBegin(_GXPrimitive prim, _GXVtxFmt fmt, unsigned short verts);
void RndGXEnd();
void RndGxDrawDone();
void MakeWiiMtxTex(const Transform &, bool, Mtx);
void MakeWiiMtx(const Transform &, Mtx &);
uint MakeU32Color(const Hmx::Color &);

extern WiiRnd TheWiiRnd;
extern int gSuppressPointTest;
extern bool gbDbgRequestForcedHang;
extern bool gbDbgRequestHangRecovery;
extern bool gRecoveringThisFrame;

#endif // RNDWII_RND_H