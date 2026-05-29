#ifndef RVL_EC_ASYNCOP_H
#define RVL_EC_ASYNCOP_H

#include <ec/result.h>
#include <ec/string.h>
#include <ec/vector.h>
#include <ec/object.h>
#include <ec/mem.h>
#include <ec/internal/shr.h>

// ECNamValStr: a name=value string pair used for HTTP parameters
namespace ec {
    struct ECNamValStr {
        ECString name;   // offset 0x0, size 0xC
        ECString value;  // offset 0xC, size 0xC
    };
}

// ECTransactionInfoObj — 0x20 bytes, holds one ECString at offset 0x10
struct ECTransactionInfoObj {
    char _pad00[0x10];        // 0x00..0x0F (unknown fields)
    ECString titleString;     // 0x10, size 0x0C
    char _pad1C[0x04];        // 0x1C..0x1F (padding to 0x20)
};

// Forward declarations
class ECAsyncOpArg;
class ECAsyncOp;

// ECAsyncOpArg — abstract base class for operation arguments (pure virtual interface)
class ECAsyncOpArg {
public:
    virtual ~ECAsyncOpArg() {}
    virtual void onError(ECAsyncOp *op) = 0;
    virtual ECResult init(ECAsyncOp *op, long seqNum, ECAsyncOpArg *arg) = 0;
    virtual ECResult execute() = 0;
};

// ECAsyncOp — wrapper for a single async e-commerce operation
// MWCC IPA (-ipa file) devirtualizes this class within ec_asyncOp.cpp:
//   the vtable pointer is placed at 0xC0 (after all data fields),
//   so seqNum starts at 0x00, not 0x04.
// vtable: RTTI(0), 0(1), dump(2), init(3), 0(4), ~ECAsyncOp(5)
class ECAsyncOp : public ECObj {
public:
    // seqNum starts at 0x00 (vtable moved to 0xC0 by IPA)
    int seqNum;                // 0x00
    ECString name;             // 0x04 (size 0xC)
    int state;                 // 0x10: 1=starting, 2=dispatched, 3=done
    int pad14;                 // 0x14
    unsigned long totalSizeMirror; // 0x18
    // ECProgress data:
    ECResult status;           // 0x1C
    int operation;             // 0x20
    int phase;                 // 0x24
    long isCancelRequested;    // 0x28
    unsigned long progTotalSize;   // 0x2C
    unsigned long downloadedSize;  // 0x30
    long errCode;              // 0x34
    char errInfo[128];         // 0x38 (0x80 bytes)
    int errType;               // 0xB8: 0=none, 1=has total
    ECAsyncOpArg *arg;         // 0xBC

    virtual void dump();
    virtual ECResult init(long seqNum, ECAsyncOpArg *arg);
    virtual ~ECAsyncOp();

    void setErrMsg(const char *msg);
    void setErrInfo(const char *code, const char *msg);
    void setTotalSize(unsigned long size);
    void incDownloadedSize(unsigned long delta);
    void setFinalStatus(long status);
};

// ECAsyncOpEnv — manages the e-commerce async operation thread
// ECObj is empty (only operator new/delete), so members start at offset 0
// Offsets verified against ec_asyncOp.cpp ENV_* macros and ec_api.cpp named-field accesses
class ECAsyncOpEnv : public ECObj {
public:
    int seqCounter;            // 0x00
    ECAsyncOp *currentOp;      // 0x04
    _SHRThread *opThread;      // 0x08
    bool isShuttingDown;       // 0x0C
    char _pad0D[3];            // 0x0D..0x0F
    ECString serviceUrl1;      // 0x10 (ecsUrl)
    ECString serviceUrl2;      // 0x1C (iasUrl)
    ECString serviceUrl3;      // 0x28 (casUrl)
    ECString contentUrl1;      // 0x34 (ccsUrl)
    ECString contentUrl2;      // 0x40 (ucsUrl)
    int unk0x4c;               // 0x4C (referenced by ec_api.cpp)
    bool unk0x50;              // 0x50
    char _pad51[0x17];         // 0x51..0x67
    ECString unk0x68;          // 0x68
    ECString unk0x74;          // 0x74
    std::vector<ECTransactionInfoObj, ECAllocator<ECTransactionInfoObj> > transactions; // 0x80 (size 0xC)
    // 0x8C..0xAF: cache buffer triples (pointer+size+pointer) — accessed via ENV_* macros
    char _pad8C[0x24];         // 0x8C..0xAF
    void *cacheData;           // 0xB0
    char _padB4[4];            // 0xB4..0xB7
    bool isLoaded;             // 0xB8
    char _padB9[7];            // 0xB9..0xBF
    ECString unk0xc0;          // 0xC0 (accountId)
    ECString deviceToken;      // 0xCC
    ECString tokenPrefix;      // 0xD8
    ECString tokenMD5;         // 0xE4
    ECString tokenExt;         // 0xF0
    ECString unk0xFC;          // 0xFC
    ECString unk0x108;         // 0x108
    ECString extAccountId;     // 0x114
    ECString country;          // 0x120
    ECString accountCountry;   // 0x12C
    ECString accountDeviceCode;// 0x138
    ECString language;         // 0x144
    // 0x150: long long fields (language ends at 0x150 = 0x144 + 0xC)
    long long forceSyncTimeLL; // 0x150
    long long extTicketTimeLL; // 0x158
    long long syncTimeLL;      // 0x160
    long long lastTicketSyncTime; // 0x168
    bool isNeedTicketSync;     // 0x170
    bool isNeedTicketSyncImportAll; // 0x171
    char _pad172[2];           // 0x172..0x173
    bool unk0x174;             // 0x174 (referenced by ec_api.cpp: unk0x174 == false)
    char _pad175[3];           // 0x175..0x177
    ECString m_TIN;            // 0x178
    ECString m_AppId;          // 0x184
    int unk0x190;              // 0x190 (referenced by ec_api.cpp)
    std::vector<ec::ECNamValStr, ECAllocator<ec::ECNamValStr> > namedValues; // 0x194 (size 0xC) — parsed config name=value pairs
    // 0x1A0..0x26B: remaining fields
    long long titleId;         // 0x1A0 — running application TitleID
    bool hasKeyPair;           // 0x1A8 — device has valid key pair
    char _pad1A9[7];           // 0x1A9..0x1AF
    ECString unk0x1b0;         // 0x1B0
    ECString unk0x1bc;         // 0x1BC
    char _pad1c8[8];           // 0x1C8..0x1CF
    ECString unk0x1d0;         // 0x1D0
    char _pad1dc[0x48];        // 0x1DC..0x223
    ECString devCert;          // 0x224 — device certificate (checked in getDeviceInfo)
    ECString unk0x230;         // 0x230
    ECString unk0x23c;         // 0x23C
    char _pad248[0x18];        // 0x248..0x25F
    ECString unk0x260;         // 0x260

    ECResult init();
    ~ECAsyncOpEnv();

    ECResult checkParentalControlPassword(const char *pin, long *result);
    ECResult lock();
    ECResult unlock();
    ECResult lockProgress();
    ECResult unlockProgress();
    ECResult shutDown();
    ECResult dispatchOp(ECAsyncOp *op);
    ECResult post(ECAsyncOp *op);
    ECResult start(ECAsyncOp *op, ECAsyncOpArg *arg);
    ECResult clearOpCaches();
    ECResult getDeviceInfo();
    void getKeyPairStatus();
    ECResult setDeviceToken(const char *token);
    ECResult setDeviceToken(ECString &token);
    ECResult setNamedValue(const char *name, const char *value);
    ECResult saveConfig();
    ECResult getConfig();
    ECResult getDevId();
    ECResult getDevCert();
    ECResult getCRL(bool force);
    ECResult getParentalControlInfo();
    ECResult getCountry();
    ECResult getRegion();
    ECResult getLanguage();
    ECResult getSerialNumber();
    void getFileSystemStatus();
    ECResult getWifiMac(char *out);
    ECResult getBluetoothMac(char *out);
    ECResult getFreeChannelAppCount();
    ECResult setWebSvcUrls(const char *ecsUrl, const char *iasUrl, const char *casUrl);
    ECResult setContentUrls(const char *ccsUrl, const char *ucsUrl);
};

namespace ec {
    extern ECAsyncOpEnv *op;
}

#endif
