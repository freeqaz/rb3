// native_file.cpp — a plain stdio-backed File for the native (clang LP64) build.
// The RB3-Wii File backends (ArkFile / AsyncFile / FileCache / CDReader) target
// the disc/ARK and aren't on the DTA-parse path. NewFile() (os/File.cpp, under
// HX_NATIVE) routes here for synchronous reads from the host filesystem.
#ifdef HX_NATIVE

#include "os/File.h"
#include <cstdio>
#include <cstring>

#ifdef __EMSCRIPTEN__
#include "platform/WebAssets.h"
#include <string>
#endif

namespace {
class NativeStdioFile : public File {
public:
    NativeStdioFile(const char *path, int mode) : mFp(nullptr), mFail(true), mLastReadBytes(0) {
        // mode bit 2 (0x2) = read on Wii; otherwise treat as write/append.
        bool readMode = (mode & 2) != 0;
        const char *m = readMode ? "rb" : ((mode & 0x800) ? "ab" : "wb");
        mFp = std::fopen(path, m);
#ifdef __EMSCRIPTEN__
        // On-demand asset fetch: under emcc the assets live on the dev server,
        // not in MEMFS. If a READ open misses (the milo dependency graph pulls
        // sibling .milo_xbox / texture files lazily as DirLoader::LoadObjects
        // walks it), fetch the file into MEMFS via a synchronous XHR and retry.
        // RB3 excludes the engine's AsyncFile_Native.cpp (which normally hosts
        // this hook for the DC3 build), so it must live here. Pass `path`
        // unchanged to both calls — WebAssetsFetchSync writes to the same path
        // it opens (DC3's AsyncFileNative::_OpenAsync uses the same contract).
        if (!mFp && readMode) {
            // WebAssetsFetchSync writes the fetched bytes into MEMFS using the
            // path it is handed: it runs `FS.mkdir` for each ABSOLUTE parent dir
            // (`/ui`, `/ui/resource`, …) then `FS.writeFile(path)`. If `path` is
            // RELATIVE (FileRoot() is "." on web, so most resource milos —
            // fonts, icons — arrive as `ui/resource/.../foo.milo_xbox`), the
            // mkdir loop creates `/ui/...` while writeFile resolves the relative
            // path against the FS cwd (`/data`), i.e. `/data/ui/...`, whose
            // parent dirs were never made → `ErrnoError: No such file or
            // directory` and a failed fetch (e.g. pentatonic_regularsmall.milo
            // for the HUD BandLabels → UILabel.cpp:522 ResourceDir() abort).
            // Anchor a relative path under /data (the MEMFS asset root, == cwd)
            // so the fetch's mkdir/writeFile and this fopen agree on one
            // absolute location. The subsequent fopen still uses the original
            // (relative) `path`, which resolves to the same `/data/...` file.
            // Guarded out of the matched Wii/native asm (no __EMSCRIPTEN__).
            std::string fetchPath = path;
            if (!fetchPath.empty() && fetchPath[0] != '/')
                fetchPath = "/data/" + fetchPath;
            if (WebAssetsFetchSync(fetchPath.c_str())) {
                mFp = std::fopen(path, m);
            }
        }
#endif
        mFail = (mFp == nullptr);
    }
    ~NativeStdioFile() override {
        if (mFp)
            std::fclose(mFp);
    }

    int Read(void *buf, int n) override {
        if (!mFp || n < 0)
            return -1;
        return (int)std::fread(buf, 1, (size_t)n, mFp);
    }
    bool ReadAsync(void *buf, int n) override {
        mLastReadBytes = Read(buf, n);
        return mLastReadBytes == n;
    }
    int Write(const void *buf, int n) override {
        if (!mFp || n < 0)
            return -1;
        return (int)std::fwrite(buf, 1, (size_t)n, mFp);
    }
    int Seek(int offset, int whence) override {
        // Wii: 0=set, 1=cur, 2=end (matches SEEK_SET/CUR/END).
        if (!mFp)
            return -1;
        if (std::fseek(mFp, offset, whence) != 0)
            return -1;
        return (int)std::ftell(mFp);
    }
    int Tell() override { return mFp ? (int)std::ftell(mFp) : -1; }
    void Flush() override {
        if (mFp)
            std::fflush(mFp);
    }
    bool Eof() override {
        // The DTA lexer (YY_INPUT) reads 1 byte at a time and checks Eof()
        // BEFORE each read. feof() only trips AFTER a read past end, so use the
        // position-vs-size test to report EOF when no bytes remain — otherwise
        // the final 0-byte read sets the stream's fail flag and DataInput
        // asserts (DataFile.cpp:566).
        if (!mFp)
            return true;
        if (std::feof(mFp))
            return true;
        long cur = std::ftell(mFp);
        return cur >= 0 && cur >= Size();
    }
    bool Fail() override { return mFail; }
    int Size() override {
        if (!mFp)
            return 0;
        long cur = std::ftell(mFp);
        std::fseek(mFp, 0, SEEK_END);
        long end = std::ftell(mFp);
        std::fseek(mFp, cur, SEEK_SET);
        return (int)end;
    }
    int UncompressedSize() override { return Size(); }
    bool ReadDone(int &result) override {
        result = mLastReadBytes;
        mLastReadBytes = 0;
        return true;
    }
    int GetFileHandle(DVDFileInfo *&info) override {
        info = nullptr;
        return 0;
    }

private:
    std::FILE *mFp;
    bool mFail;
    int mLastReadBytes;
};
} // namespace

File *HmxNativeOpenFile(const char *path, int mode) {
    NativeStdioFile *f = new NativeStdioFile(path, mode);
    return f; // NewFile callers check ->Fail() and release on failure
}

#endif // HX_NATIVE
