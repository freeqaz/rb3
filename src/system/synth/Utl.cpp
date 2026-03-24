#include "synth/Utl.h"
#include "os/File.h"
#include "os/HolmesClient.h"
#include "os/System.h"
#include "utl/Loader.h"
#include "utl/MakeString.h"
#include <cstring>

class AsyncFileWii {
public:
    static bool FileExistsOnCD(const char *);
};

float CalcSpeedFromTranspose(float f) { return std::pow(2.0f, f / 12.0f); }

const char *CacheWav(const char *file, CacheResourceResult &result) {
    result = (CacheResourceResult)0;
    Platform platform = TheLoadMgr.GetPlatform();
    if (!file || *file == '\0' || platform == kPlatformNone) {
        return nullptr;
    }
    if (platform == kPlatformPC) {
        return file;
    }
    const char *localized = FileLocalize(file, nullptr);
    Symbol platformSym = PlatformSymbol(platform);
    const char *ext = FileGetExt(localized);
    const char *base = FileGetBase(localized, nullptr);
    static char cacheFile[0x100];
    strcpy(
        cacheFile,
        MakeString("%s/gen/%s.%s_%s", FileGetPath(localized, nullptr), base, ext, platformSym)
    );
    bool isLocal = FileIsLocal(localized);
    bool existsOnCD = AsyncFileWii::FileExistsOnCD(cacheFile);
    bool canUseCached = (isLocal | existsOnCD) != 0;
    if (UsingCD() || canUseCached) {
        return cacheFile;
    }
    String qualifiedName;
    FileQualifiedFilename(qualifiedName, localized);
    CacheResourceResult cacheResult = HolmesClientCacheResource(qualifiedName.c_str(), cacheFile);
    result = cacheResult;
    if ((int)cacheResult > 0) {
        return nullptr;
    }
    return cacheFile;
}
