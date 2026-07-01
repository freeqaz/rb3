#pragma once
#include "obj/Object.h"
#include "obj/ObjMacros.h"
#include "obj/ObjPtr_p.h"
#include "os/System.h"
#include "rndobj/Bitmap.h"
#include "rndobj/Tex.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include <string.h>
#include <map>

class RndMat;
class KerningTable;

struct MatChar {
    float width;
    float height;
};

/**
 * @brief Implements fonts for use with RndText objects.
 *  Additionally, RndFont can dynamically update the font atlas, cell sizes, and similar.
 *  Original _objects description:
 *   "Font objects determine the appearance for Text objects."
 */

class RndFont : public Hmx::Object {
public:
    struct CharInfo {
        float normX; // 0x0 - Bank 5 DWARF: glyph UV origin x (normalized)
        float normY; // 0x4 - glyph UV origin y (normalized)
        float charWidth; // 0x8
        float charAdvance; // 0xc - horizontal advance
    };

    struct KernInfo {
        unsigned short unk0;
        unsigned short unk2;
        float kerning; // 0x4
    };

    RndFont();
    virtual ~RndFont();
    virtual void Replace(Hmx::Object *, Hmx::Object *);
    OBJ_CLASSNAME(Font);
    OBJ_SET_TYPE(Font);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void Print();

    void GetTexCoords(unsigned short, Vector2 &, Vector2 &) const;
    void GetKerning(std::vector<KernInfo> &) const;
    void SetKerning(const std::vector<KernInfo> &);
    void SetBaseKerning(float);
    void SetCellSize(float, float);
    void SetBitmapSize(const Vector2 &, unsigned int, unsigned int);
    RndTex *ValidTexture() const;
    void UpdateChars();
    void BleedTest();
    float CharAdvance(unsigned short, unsigned short) const;
    float CharAdvance(unsigned short) const;
    float Kerning(unsigned short, unsigned short) const;
    float CharWidth(unsigned short) const;
    bool CharDefined(unsigned short) const;
    void SetCharInfo(RndFont::CharInfo *, RndBitmap &, const Vector2 &);
    String GetASCIIChars();
    void SetASCIIChars(String);

    RndMat *GetMat() const { return mMat; }
    void SetNextFont(RndFont *font) { mNextFont = font; }
    RndFont *NextFont() const { return mNextFont; }
    bool IsMonospace() const { return mMonospace; }
    bool IsPacked() const { return mPacked; }
#ifdef HX_NATIVE
    // CellDiff is the glyph cell height/width aspect, used to size text glyph
    // geometry (glyphH = textSize * CellDiff). The native port runs the Xbox
    // assets (TheLoadMgr.mPlatform = kPlatformXBox), whose higher-res font
    // atlases are sometimes non-square — notably the hub menu font
    // Pentatonic_Regular_(9_00)4x, packed into a 512x1024 atlas (2x TALLER than
    // the Wii's square 512x512). RndFont::Load bakes mCellSize = trueCell *
    // atlasDim, so mCellSize.y/mCellSize.x carries a spurious atlasH/atlasW
    // factor that is 1.0 only for SQUARE atlases. On the Wii (square) the matched
    // formula is exact; on a 2:1-TALL Xbox atlas it DOUBLES the glyph height, so
    // the big-font main-hub menu labels (PLAY NOW / CAREER / ...) render ~2x too
    // tall and overlap into an unreadable pile. Recover the true,
    // platform-independent aspect from the UV cell (mTexCellSize * atlasDim).
    //
    // Scoped to NON-SQUARE atlases (atlasH != atlasW): square atlases (the Wii
    // path, and any square Xbox atlas) keep the matched formula untouched, so
    // only fonts whose Xbox atlas is non-square get the corrected aspect. Both
    // the over-TALL (hub, 512x1024) and over-WIDE (overshell slot text /
    // instrument-icon, 512x256) cases are now handled — see below.
    float CellDiff() const {
        if (mTexCellSize.x > 0.f && mTexCellSize.y > 0.f) {
            RndTex *t = ValidTexture();
            if (t) {
                int w = t->Width(), h = t->Height();
                // The matched-fork CellDiff (mCellSize.y/mCellSize.x) carries a
                // spurious (atlasH/atlasW) factor, exact (==1) only on a SQUARE
                // atlas. Recover the true, platform-independent glyph cell
                // aspect from the normalized UV cell scaled by the atlas pixel
                // dims: (texCell.y*h)/(texCell.x*w). This is correct for ANY
                // non-square atlas. Two real Xbox cases:
                //   * TALLER-than-wide (512x1024) — the hub font
                //     Pentatonic_Regular_(9_00)4x, ~2x too TALL with the matched
                //     formula (commit 3cb8a41a).
                //   * WIDER-than-tall (512x256) — the overshell bottom-bar slot
                //     fonts Pentatonic_Bold_(3_00)4x ("CONNECT CONTROLLER" /
                //     "MENU") and the instrument-icon glyph font
                //     instrument_icons_small*, whose CellDiff comes out HALF the
                //     true aspect -> glyphs render condensed/short (the squished
                //     bottom-bar slot text + collapsed icon). Apply the UV-cell
                //     recovery for both, leaving SQUARE atlases (the Wii path)
                //     on the byte-matched formula.
                if (w > 0 && h > 0 && h != w)
                    return (mTexCellSize.y * (float)h) / (mTexCellSize.x * (float)w);
            }
        }
        return mCellSize.y / mCellSize.x;
    }
    // The byte-matched (Wii) glyph aspect, i.e. CellDiff() WITHOUT the non-square
    // atlas correction. The wide-atlas correction grows the corrected glyph
    // height relative to this; SetupCharVerts uses the delta to re-center the
    // grown glyph quad on its (milo-authored) anchor for the icon font, so the
    // instrument-icon glyph centers on the difficulty-dot rows instead of
    // hanging below them. Square atlases (the Wii path) return CellDiff()==this,
    // so the delta is 0 and nothing shifts.
    float RawCellDiff() const { return mCellSize.y / mCellSize.x; }
#else
    float CellDiff() const { return mCellSize.y / mCellSize.x; }
#endif
    RndFont *TextureOwner() const { return mTextureOwner; }
    float BaseKerning() const { return mBaseKerning; }
    float DeprecatedSize() const { return mDeprecatedSize; }
    bool HasChar(unsigned short c) const { return mCharInfoMap.count(c) != 0; }

    NEW_OVERLOAD
    DELETE_OVERLOAD
    NEW_OBJ(RndFont)
    static void Init() { REGISTER_OBJ_FACTORY(RndFont) }
    DECLARE_REVS

    ObjPtr<RndMat> mMat; // 0x1c
    ObjOwnerPtr<RndFont> mTextureOwner; // 0x28
    std::map<unsigned short, CharInfo> mCharInfoMap; // 0x34
    KerningTable *mKerningTable; // 0x4c
    float mBaseKerning; // 0x50
    Vector2 mCellSize; // 0x54 - cell width, cell height
    float mDeprecatedSize; // 0x5c
    std::vector<unsigned short> mChars; // 0x60
    bool mMonospace; // 0x68
    Vector2 mTexCellSize; // 0x6c
    bool mPacked; // 0x74
    ObjPtr<RndFont> mNextFont; // 0x78
};

class KerningTable {
public:
    struct Entry {
        Entry *next; // 0x0
        int key; // 0x4
        float kerning; // 0x8
    };

#ifdef HX_NATIVE
    // mTable is Entry*[32]. The matched-fork hardcodes 0x80 (=32*4) for the memset,
    // correct when a pointer is 4 bytes (console) but only HALF the table on LP64
    // (where 32 pointers = 256 bytes). The unzeroed upper buckets mTable[16..31]
    // then hold garbage, so Find()/Kerning() walk a bogus Entry*->next and crash.
    // Use sizeof(mTable) so the whole table is cleared at every platform width.
    KerningTable() : mNumEntries(0), mEntries(0) { memset(mTable, 0, sizeof(mTable)); }
#else
    KerningTable() : mNumEntries(0), mEntries(0) { memset(mTable, 0, 0x80); }
#endif

    ~KerningTable() { delete mEntries; }

    Entry *Find(unsigned short us1, unsigned short us2) {
        if (mNumEntries == 0)
            return nullptr;
        else {
            Entry *entry = mTable[TableIndex(us1, us2)];
            int key = Key(us1, us2);
            for (; entry != nullptr && key != entry->key; entry = entry->next)
                ;
            return entry;
        }
    }

    float Kerning(unsigned short us1, unsigned short us2) {
        Entry *kerningEntry = Find(us1, us2);
        if (kerningEntry)
            return kerningEntry->kerning;
        else
            return 0;
    }

    void GetKerning(std::vector<RndFont::KernInfo> &info) const {
        info.resize(mNumEntries);
        for (int i = 0; i < mNumEntries; i++) {
            unsigned short &info0 = info[i].unk0;
            info0 = mEntries[i].key;
            unsigned short &info2 = info[i].unk2;
            info2 = (unsigned int)(mEntries[i].key) >> 16;
            float &info4 = info[i].kerning;
            info4 = mEntries[i].kerning;
        }
    }

    void SetKerning(const std::vector<RndFont::KernInfo> &info, RndFont *font) {
        int validcount = 0;
        for (int i = 0; i < info.size(); i++) {
            if (Valid(info[i], font)) {
                validcount++;
            }
        }
        if (validcount != mNumEntries) {
            mNumEntries = validcount;
            delete[] mEntries;
            mEntries = new Entry[mNumEntries];
        }
#ifdef HX_NATIVE
        memset(mTable, 0, sizeof(mTable)); // LP64: full Entry*[32] (see ctor note)
#else
        memset(mTable, 0, 0x80);
#endif
        for (int i = 0; i < info.size(); i++) {
            const RndFont::KernInfo &curInfo = info[i];
            if (Valid(curInfo, font)) {
                Entry &curEntry = mEntries[i];
                curEntry.key = Key(curInfo.unk0, curInfo.unk2);
                curEntry.kerning = curInfo.kerning;
                int index = TableIndex(curInfo.unk0, curInfo.unk2);
                curEntry.next = mTable[index];
                mTable[index] = &curEntry;
            }
        }
    }

    bool Valid(const RndFont::KernInfo &info, RndFont *font) {
        return !font || (font->CharDefined(info.unk0) && font->CharDefined(info.unk2));
    }

    int Key(unsigned short us0, unsigned short us2) {
        return us0 | (us2 << 0x10);
    }

    int TableIndex(unsigned short us0, unsigned short us2) { return (us0 ^ us2) & 0x1F; }

    int Size() const { return mNumEntries * 0xC + 0x88; }

    void Load(BinStream &bs, RndFont *font) {
        if (RndFont::gRev < 7) {
            std::vector<RndFont::KernInfo> kernInfos;
            bs >> kernInfos;
            SetKerning(kernInfos, font);
        } else {
            int size;
            bs >> size;
            if (size != mNumEntries) {
                mNumEntries = size;
                delete[] mEntries;
                mEntries = new Entry[mNumEntries];
            }
#ifdef HX_NATIVE
            memset(mTable, 0, sizeof(mTable)); // LP64: full Entry*[32] (see ctor note)
#else
            memset(mTable, 0, 0x80);
#endif
            for (int i = 0; i < mNumEntries; i++) {
                Entry &curEntry = mEntries[i];
                bs >> curEntry.key;
                bs >> curEntry.kerning;
                unsigned short us4, us3;
                if (RndFont::gRev < 0x11) {
                    us4 = curEntry.key & 0xFF;
                    us3 = curEntry.key >> 8 & 0xFF;
                    curEntry.key = Key(us4, us3);
                } else {
                    us4 = curEntry.key;
                    us3 = curEntry.key >> 16;
                }
                int idx = TableIndex(us4, us3);
                curEntry.next = mTable[idx];
                mTable[idx] = &curEntry;
            }
        }
    }

    int mNumEntries; // 0x0
    Entry *mEntries; // 0x4
    Entry *mTable[32]; // 0x8
};

class BitmapLocker {
public:
    BitmapLocker(RndFont *f) : mTexture(0), mPbm(0) {
        mTexture = f->ValidTexture();
        if (mTexture) {
            const char *filestr = mTexture->mFilepath.c_str();
            int fplen = strlen(filestr);
#ifdef MILO_DEBUG
            if (UsingCD() || fplen < 4 || stricmp(filestr + fplen - 4, ".bmp") != 0) {
#endif
                mTexture->LockBitmap(mBm, 3);
                if (mBm.Pixels()) {
                    mPbm = &mBm;
                }
#ifdef MILO_DEBUG
            } else {
                mBm.LoadBmp(filestr, false, false);
                if (mBm.Pixels()) {
                    mPbm = &mBm;
                }
                mTexture = nullptr;
            }
#endif
        }
    }

    ~BitmapLocker() {
        if (mTexture)
            mTexture->UnlockBitmap();
    }

    RndBitmap *PtrToBitmap() const { return mPbm; }

    RndTex *mTexture; // 0x0
    RndBitmap *mPbm; // 0x4
    RndBitmap mBm; // 0x8
};

BinStream &operator>>(BinStream &, MatChar &);
BinStream &operator>>(BinStream &, RndFont::KernInfo &);
