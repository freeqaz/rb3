#include "utl/Locale.h"
#include "decomp.h"
#include "obj/DataFile.h"
#include "obj/DataFunc.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/DataPointMgr.h"
#include "utl/MakeString.h"
#include "utl/Str.h"
#include "utl/Symbols.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *Locale::sIgnoreMissingText;
Locale TheLocale;
int gDbgLocaleNumEntries;
int gDbgLocaleStringsSize;
bool Locale::sVerboseNotify;

namespace LocaleChunkSort {
template <int N>
int FastSort(const void *a, const void *b) {
    int i = 0;
    do {
        if (*(int *)a < *(int *)b) return -1;
        if (*(int *)a > *(int *)b) return 1;
        i++;
        a = (const char *)a + 8;
        b = (const char *)b + 8;
    } while (i < N);
    return 0;
}
template int FastSort<3>(const void *, const void *);

void Sort(OrderedLocaleChunk *chunks, int count) {
    qsort(chunks, count, sizeof(OrderedLocaleChunk), FastSort<3>);
}
}

DataNode DataSetLocaleVerboseNotify(DataArray *arr) {
    SetLocaleVerboseNotify(arr->Int(1));
    return 0;
}

DataNode DataDbgPrintLocaleSize(DataArray *) {
    MILO_LOG(
        "Localization table: %d entries, %gK in strings. Total mem: %gK\n",
        gDbgLocaleNumEntries,
        (float)gDbgLocaleStringsSize * 0.0009765625f,
        (float)(unsigned int)(gDbgLocaleStringsSize + gDbgLocaleNumEntries * 4) * 0.0009765625f
    );
    return 0;
}

Locale::~Locale() {
    if (mMagnuStrings) {
        mMagnuStrings->Release();
        mMagnuStrings = 0;
    }
}

void Locale::Init() {
    MILO_ASSERT(!mStrTable, 0x5A);
    MILO_ASSERT(!mSymTable, 0x5B);
    MILO_ASSERT(!mSize, 0x5C);
    MILO_ASSERT(!mStringData, 0x5D);
    MILO_ASSERT(!mNumFilesLoaded, 0x5E);
    MILO_ASSERT(!mUploadedFlags, 0x60);

    int i13 = 0;
    int i10 = 0;
    int numChunks;
    LocaleChunkSort::OrderedLocaleChunk *chunks;
    Symbol s60;
    DataArray *cfg = SystemConfig("locale");
    {
        std::vector<DataArray *> arrVec(cfg->Size() - 1);
        mNumFilesLoaded = arrVec.size();
        for (int i = 1; i < cfg->Size(); i++) {
            const char *path = FileMakePath(FileGetPath(cfg->File(), 0), cfg->Str(i), 0);
            arrVec[i - 1] = DataReadFile(path, true);
            if (arrVec[i - 1] == nullptr) {
                MILO_FAIL("could not load language file %s", path);
            }
            i10 += arrVec[i - 1]->Size();
        }
        chunks = new LocaleChunkSort::OrderedLocaleChunk[i10];
        numChunks = 0;
        for (int j = cfg->Size() - 2; j >= 0; j--) {
            DataArray *curArr = arrVec[j];
            for (int k = curArr->Size() - 1; k >= 0; k--, numChunks++) {
                DataArray *chunkArr = curArr->Node(k).LiteralArray(curArr);
                int size = chunkArr->Size();
                if (size != 2) {
                    MILO_FAIL(
                        "%s line %d should only have 2 entries, has %d, mismatched quotes?",
                        chunkArr->File(),
                        chunkArr->Line(),
                        size
                    );
                }
                chunks[numChunks].node1 = chunkArr->LiteralSym(0);
                chunks[numChunks].node2 = numChunks;
                chunks[numChunks].node3 = chunkArr->LiteralStr(1);
            }
            curArr->Release();
        }

        if (cfg->Size() > 1)
            LocaleChunkSort::Sort(chunks, numChunks);
        mSize = 0;
        for (int i = 0; i < numChunks; i++) {
            Symbol curSym = chunks[i].node1.LiteralSym();
            if (curSym != s60) {
                i13 += strlen(chunks[i].node3.LiteralStr());
                s60 = curSym;
                mSize++;
            }
        }
    }
    mSymTable = new Symbol[mSize];
    mStringData = new StringTable(i13);
    mStrTable = new const char *[mSize];
    int chunkIdx = 0;
    s60 = Symbol();
    for (int i = 0; i < numChunks; i++) {
        Symbol curSym = chunks[i].node1.LiteralSym();
        if (curSym != s60) {
            mSymTable[chunkIdx] = curSym;
            mStrTable[chunkIdx] = mStringData->Add(chunks[i].node3.LiteralStr());
            s60 = curSym;
            chunkIdx++;
        } else
            MILO_WARN("Locale symbol '%s' redefined", curSym.mStr);
    }
    delete[] chunks;
    if (cfg->Size() > 1) {
        mFile = cfg->Str(1);
    }
    gDbgLocaleNumEntries = mSize;
    gDbgLocaleStringsSize = mStringData->UsedSize();
    DataRegisterFunc("dbg_print_locale_size", DataDbgPrintLocaleSize);
    DataRegisterFunc("set_locale_verbose_notify", DataSetLocaleVerboseNotify);
}

void Locale::Terminate() {
    delete[] mSymTable;
    mSymTable = 0;
    delete[] mStrTable;
    mStrTable = 0;
    delete[] mUploadedFlags;
    mUploadedFlags = 0;
    RELEASE(mStringData);
    mSize = 0;
    mFile = Symbol();
    mNumFilesLoaded = 0;
}

void Locale::SetMagnuStrings(DataArray *da) {
    if (mMagnuStrings) {
        mMagnuStrings->Release();
        mMagnuStrings = 0;
    }
    mMagnuStrings = da;
}

const char *Locale::Localize(Symbol s, bool b) const {
    if (s.Null())
        return "";
    else if (!mSymTable) {
        MILO_WARN("attempting to localize %s before Locale initialized!\n", s.mStr);
        return s.mStr;
    } else {
        MILO_ASSERT(mSymTable, 0x1F0);
        if (mMagnuStrings && SystemLanguage() == eng) {
            DataArray *magnuArr = mMagnuStrings->FindArray(s, false);
            if (magnuArr) {
                return magnuArr->Str(1);
            }
        }
        int iii;
        if (FindDataIndex(s, iii, b)) {
            if (UsingCD() && !mUploadedFlags[iii]) {
                SendDataPoint("debug/locale/token", "token", s, "success", true);
                mUploadedFlags[iii] = true;
            }
            return mStrTable[iii];
        } else if (UsingCD()) {
            SendDataPoint("debug/locale/token", "token", s, "success", false);
        }
        return nullptr;
    }
}

bool Locale::FindDataIndex(Symbol s, int &idx, bool fail) const {
    int low = 0;
    int high = mSize - 1;
    const char *sStr = s.mStr;
    while (high - low >= 0) {
        int mid = (low + high) >> 1;
        if ((int)sStr > (int)mSymTable[mid].mStr) {
            low = mid + 1;
        } else if ((int)sStr < (int)mSymTable[mid].mStr) {
            high = mid - 1;
        } else {
            idx = mid;
            return true;
        }
    }
    if (fail) {
        MILO_FAIL("Couldn't find '%s' in array (file %s)", s.mStr, mFile.mStr);
    }
    return false;
}

const char *Localize(Symbol token, bool *notify) {
    bool localized;
    const char *textStr = TheLocale.Localize(token, false);
    localized = textStr != 0;
    if (!localized) {
        textStr = token.mStr;
        Locale::sIgnoreMissingText = textStr;
        if (Locale::sVerboseNotify != 0) {
            MILO_WARN("\"%s\" needs localization", token);
        }
    }
    if (notify)
        *notify = localized;
    return textStr;
}

DECOMP_FORCEACTIVE(Locale, "locale_mmsshh")

char gSepIntBufs[4][0x32];
int gNextSepIntBuf;
char gFloatBufs[4][0x32];
int gNextFloatBuf;

const char *LocalizeSeparatedInt(int num) {
    char digit[2];
    bool success = false;
    const char *sep = Localize(locale_separator, &success);
    if (!success) {
        sep = ",";
    }
    bool isNullSep = !strcmp(sep, gNullStr);
    if (isNullSep) {
        return MakeString("%i", num);
    }
    int sepLen = strlen(sep);
    char *text = gSepIntBufs[gNextSepIntBuf];
    int offset = 0x31;
    text[0x31] = '\0';
    bool less_than_zero = num < 0;
    if (less_than_zero) {
        num = abs(num);
    }
    int digitCount = 0;
    while (digitCount == 0 || num > 0) {
        if (digitCount % 3 == 0 && digitCount > 0) {
            for (int j = sepLen - 1; j >= 0; j--) {
                text[--offset] = sep[j];
            }
        }
        snprintf(digit, 2, "%d", num % 10);
        offset--;
        text[offset] = digit[0];
        digitCount++;
        num = num / 10;
    }
    if (less_than_zero) {
        offset--;
        text[offset] = '-';
    }
    const char *result = &text[offset];
    gNextSepIntBuf = (gNextSepIntBuf + 1) % 4;
    return result;
}

const char *LocalizeFloat(const char *fmt, float num) {
    const char *str = MakeString(fmt, num);
    const char *sep = TheLocale.Localize(locale_decimal_separator, false);
    if (sep == 0 || *sep == '.') {
        return str;
    }
    char *buf = gFloatBufs[gNextFloatBuf];
    strncpy(buf, str, 0x32);
    buf[0x31] = '\0';
    char *p = buf;
    while (*p != '\0') {
        if (*p == '.') {
            *p = *sep;
            break;
        }
        p++;
    }
    gNextFloatBuf = (gNextFloatBuf + 1) % 4;
    return buf;
}

void SyncReloadLocale() {
    DataArray *cfg = SystemConfig(locale);
    for (int i = 1; i < cfg->Size(); i++) {
        const char *str = cfg->Str(i);
        const char *path = FileMakePath(FileGetPath(cfg->File(), 0), str, 0);
        if (SystemExec(MakeString("p4 sync %s", path)) == 0) {
            TheDebug << MakeString("updated %s\n", path);
        } else {
            TheDebug << MakeString("failed to update %s\n", path);
        }
    }
    TheLocale.Terminate();
    TheLocale.Init();
}
