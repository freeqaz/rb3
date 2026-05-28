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
    CacheResourceResult &_ref0 = result;
    _ref0 = (CacheResourceResult)0;
    Platform platform = TheLoadMgr.GetPlatform();
    if (!file || *file == '\0' || platform == kPlatformNone) {
        return nullptr;
    }
    if (platform == kPlatformPC) {
        return file;
    }
    const char *localized = FileLocalize(file, nullptr);
    Symbol platformSym = PlatformSymbol(platform);
    static char cacheFile[0x100];
    const char * _tmp0 = MakeString("%s/gen/%s.%s_%s", FileGetPath(localized, nullptr), FileGetBase(localized, nullptr), FileGetExt(localized), platformSym);
    strcpy(
        cacheFile,
        _tmp0
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
    _ref0 = cacheResult;
    if ((int)cacheResult > 0) {
        return nullptr;
    }
    return cacheFile;
}
