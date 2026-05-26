#include "Movie.h"
#include "macros.h"
#include "math/Utl.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "rndobj/Tex.h"
#include "rndobj/Utl.h"
#include "rndwii/Tex.h"
#include "utl/BufStream.h"
#include "utl/FileStream.h"
#include "utl/Loader.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"

WiiMovie::WiiMovie() : unk_0x40(0), unk_0x48(0) {}

WiiMovie::~WiiMovie() {
    RELEASE(unk_0x48);
    if (unk_0x40) {
        _MemFree(unk_0x40);
        unk_0x40 = nullptr;
    }
}

void WiiMovie::SetFile(const FilePath &fp, bool b) {
    RELEASE(unk_0x48);
    if (unk_0x40) {
        _MemFree(unk_0x40);
        unk_0x40 = nullptr;
    }
    mVideoData.Reset();
    mFile = fp;
    mStream = b;
    if (!mFile.empty()) {
        if (b) {
            unk_0x48 = NewFile(CacheResource(mFile.c_str(), this), 2);
            if (unk_0x48 == nullptr) {
                TheDebug.Notify(MakeString("%s: %s not found", PathName(this), mFile));
                return;
            }
            FileStream fs(unk_0x48, true);
            mVideoData.Load(fs, true);
            unk_0x40 = _MemAlloc(mVideoData.FrameSize() * 2, 0);
            unk_0x54 = unk_0x40;
            unk_0x50 = 0;
            unk_0x4c = fs.Tell();
        } else {
            TheDebug << FormatString("Loading RndMovie into RAM\n").Str();
            TheDebug << MakeString("Opening file - %s\n", mFile.c_str());
            FileLoader *fl = dynamic_cast<FileLoader *>(TheLoadMgr.GetLoader(fp));
            const char *buf;
            int bufSize;
            if (fl) {
                buf = fl->GetBuffer(&bufSize);
                delete fl;
            } else {
                buf = nullptr;
            }
            if (buf == nullptr)
                return;
            BufStream bs((void *)buf, bufSize, true);
            mVideoData.Load(bs, false);
            if (buf)
                _MemFree((void *)buf);
        }
        unk_0x44 = mVideoData.mHeight;
        SetTex(mTex);
        SetFrame(1.0f, 0.0f);
    }
}

void WiiMovie::SetTex(RndTex *tex) {
    RndMovie::SetTex(tex);
    if (mTex == nullptr)
        return;
    int x = mVideoData.mMagic, y;
    if (x == 0 || (y = mVideoData.mWidth) == 0 || mVideoData.mHeight == 0) {
        mTex->SetBitmap(16, 16, 32, RndTex::kRegular, 0, NULL);
    } else {
        mTex->SetBitmap(
            x, y, const_cast<const SIVideo &>(mVideoData).Bpp(), RndTex::kMovie, 0, NULL
        );
    }
}

void WiiMovie::SetFrame(float frame, float blend) {
    START_AUTO_TIMER("movie");
    mFrame = frame;
    if ((mStream && unk_0x40 == 0) || mTex == nullptr || mVideoData.mHeight == 0)
        return;
    int newFrame = (int)frame % (int)mVideoData.mHeight;
    if (newFrame < 0)
        newFrame += (int)mVideoData.mHeight;
    int delta = newFrame - (int)unk_0x44;
    if (delta == 0)
        return;
    unk_0x44 = newFrame;
    if (mStream) {
        if ((unsigned int)delta > 1) {
            StreamRestart(newFrame);
        } else {
            StreamReadFinish();
            unk_0x54 = (char *)unk_0x40 + unk_0x50;
            StreamNextBuffer();
        }
    } else {
        unk_0x54 = mVideoData.Frame(newFrame);
    }
    Update();
}

void WiiMovie::Update() {
    WiiTex *tex = (WiiTex *)(RndTex *)mTex;
    int frameSize = mVideoData.FrameSize();
    memcpy(tex->GetMovieLoadingFramePtr(), unk_0x54, frameSize);
    tex->MovieSwapFrames();
}

void WiiMovie::StreamReadFinish() {
    int bytesRead;
    while (!unk_0x48->ReadDone(bytesRead)) {
    }
}

void WiiMovie::StreamNextBuffer() {
    StreamReadFinish();
    unk_0x50 = (unk_0x50 != 0) ? 0 : mVideoData.FrameSize();
    int remaining = unk_0x48->Size() - unk_0x48->Tell();
    int frameSize = mVideoData.FrameSize();
    unk_0x48->ReadAsync(
        (char *)unk_0x40 + unk_0x50, Min(frameSize, remaining)
    );
}

void WiiMovie::StreamRestart(int i) {
    StreamReadFinish();
    unk_0x48->Seek(mVideoData.FrameSize() * i + unk_0x4c, 0);
    int remaining = unk_0x48->Size() - unk_0x48->Tell();
    unk_0x48->Read(unk_0x40, Min(mVideoData.FrameSize(), remaining));
    unk_0x50 = 0;
    unk_0x54 = unk_0x40;
    StreamNextBuffer();
}
