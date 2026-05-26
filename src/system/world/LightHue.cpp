/* ===== PERMUTER LOCK — DO NOT EDIT =====
 * The source permuter is actively working on: LightHue::Sync
 * Started: 2026-05-26 17:22 (stale after 5 minutes)
 * This banner is temporary and will be removed automatically.
 ===== */
#include "world/LightHue.h"
#include "math/Color.h"
#include "rndobj/Bitmap.h"
#include "utl/BufStream.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/Symbols.h"

INIT_REVS(LightHue)

LightHue::LightHue() : mLoader(0), mPath(), mKeys() {}

LightHue::~LightHue() { delete mLoader; }

BEGIN_COPYS(LightHue)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(LightHue)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mPath)
        COPY_MEMBER(mKeys)
    END_COPYING_MEMBERS
END_COPYS

SAVE_OBJ(LightHue, 0x27)

BEGIN_LOADS(LightHue)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

void LightHue::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(0, 0);
    LOAD_SUPERCLASS(Hmx::Object)
    bs >> mPath;
    if (bs.Cached()) {
        bs >> mKeys;
    } else if (!mPath.empty()) {
        mLoader = new FileLoader(mPath, mPath.c_str(), kLoadFront, 0, false, true, 0);
    }
}

void LightHue::PostLoad(BinStream &bs) {
    if (!bs.Cached())
        Sync();
}

// matches in retail
void LightHue::Sync() {
    mKeys.clear();
    if (!mPath.empty()) {
        if (!mLoader) {
            mLoader =
                new FileLoader(mPath, mPath.c_str(), kLoadFront, 0, false, true, nullptr);
        }
        TheLoadMgr.PollUntilLoaded(mLoader, nullptr);
        int ibuf;
        void *buffer = (void *)mLoader->GetBuffer(&ibuf);
        RELEASE(mLoader);
        if (buffer) {
            RndBitmap bmap;
            BufStream bs(buffer, ibuf, true);
            if (bmap.LoadBmp(&bs)) {
                mKeys.resize(bmap.Width(), Key<Vector3>());
                for (int i = 0; i < bmap.Width(); i++) {
                    unsigned char r, g, b, a;
                    bmap.PixelColor(i, 0, r, g, b, a);
                    float h, s, l;
                    MakeHSL(
                        Hmx::Color(
                            (float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f
                        ),
                        h,
                        s,
                        l
                    );
                    mKeys[i].frame = (float)i / (float)bmap.Width();
                    mKeys[i].value.x = h;
                    mKeys[i].value.y = s;
                    mKeys[i].value.z = l;
                }
            }
            _MemFree(buffer);
        }
    }
}

void LightHue::TranslateColor(const Hmx::Color &col, Hmx::Color &res) {
    if (!mKeys.empty()) {
        float maxcol = Max(1.0f, Max(col.red, col.green, col.blue));
        Hmx::Color col30;
        float inv = 1.0f / maxcol;
        float ca = col.alpha;
        float cb = col.blue;
        float cg = col.green;
        float cr = col.red;
        col30.alpha = ca * inv;
        col30.blue = cb * inv;
        col30.green = cg * inv;
        col30.red = cr * inv;
        float h, s, l;
        float vecx, vecy, vecz;
        Vector3 vec;
        MakeHSL(col30, h, s, l);
        mKeys.AtFrame(h, vec);
        vecx = vec.x;
        vecy = vec.y;
        vecz = vec.z;
        float svy = s * vecy;
        float clamped = Clamp(0.0f, 1.0f, l * vecz * 2.0f);
        MakeColor(vecx, svy, clamped, res);
        res.alpha = res.alpha * maxcol;
        res.blue = res.blue * maxcol;
        res.green = res.green * maxcol;
        res.red = res.red * maxcol;
    } else
        res = col;
}

BEGIN_HANDLERS(LightHue)
    HANDLE(save_default, OnSaveDefault)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x97)
END_HANDLERS

DataNode LightHue::OnSaveDefault(DataArray *da) {
    RndBitmap bmap;
    bmap.Create(0x100, 8, 0, 0x18, 0, 0, 0, 0);
    for (int i = 0; i < 0x100; i++) {
        Hmx::Color color;
        MakeColor((float)i / 255.0f, 1.0f, 0.5f, color);
        unsigned char red = color.red * 255.0f;
        unsigned char green = color.green * 255.0f;
        unsigned char blue = color.blue * 255.0f;
        int j = 0;
        for (; j < 8; j++) {
            bmap.SetPixelColor(i, j, red, green, blue, 0xff);
        }
    }
    bmap.SaveBmp(da->Str(2));
    return 0;
}

BEGIN_PROPSYNCS(LightHue)
    SYNC_PROP_MODIFY_ALT(path, mPath, Sync())
END_PROPSYNCS