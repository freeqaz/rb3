/* Nintendo Wii e-Commerce SDK - async operation queue implementation
 * FILE: ec_asyncOp.cpp
 * COMPILED: Feb  8 2010  20:10:17
 */
#include <ec/asyncOp.h>
#include <ec/mem.h>
#include <ec/log.h>
#include <ec/string.h>
#include <ec/md5.h>
#include <ec/base64.h>
#include <revolution/esp/esp.h>
#include <string.h>

// ESP functions not declared in esp.h
extern "C" {
    s32 ESP_GetDeviceId(u64 *deviceId);
    s32 ESP_GetDeviceCert(unsigned char *cert);
}
#include <stdlib.h>
#include <stdio.h>
#include <new>

// Helper macros for byte-offset access into ECAsyncOpEnv members
#define ENV_BYTE(env, off)  (*((char *)((char*)(env) + (off))))
#define ENV_BOOL(env, off)  (*((bool *)((char*)(env) + (off))))
#define ENV_INT(env, off)   (*((int *)((char*)(env) + (off))))
#define ENV_UINT(env, off)  (*((unsigned long *)((char*)(env) + (off))))
#define ENV_LL(env, off)    (*((long long *)((char*)(env) + (off))))
#define ENV_PTR(env, off)   (*((void **)((char*)(env) + (off))))
#define ENV_STR(env, off)   (*((ECString *)((char*)(env) + (off))))

// ECNandUsage struct - from getFileSystemStatus asm analysis
struct ECNandUsage {
    unsigned long f00;
    unsigned long f04;
    unsigned long f08;
    unsigned long f0C;
    unsigned long f10;
    unsigned long f14;
    unsigned long f18;
    unsigned long f1C;
    unsigned long f20;
    unsigned long f24;
    unsigned long f28;
    unsigned long f2C;
    unsigned long f30;
};

// ECParentalControlInfo struct - from getParentalControlInfo asm analysis
// (0x1C-byte region at env+0x204, cleared with memset(...,0,0x1C) on failure)
struct ECParentalControlInfo {
    unsigned long f00;
    unsigned long f04;
    unsigned long f08;
    unsigned long f0C;
    unsigned long f10;
    unsigned long f14;
    unsigned long f18;
};

struct ECSerialNumber {
    char value[36];
};

// Forward declarations for functions in other ec modules
namespace ec {
    // from ec_file_bw
    ECResult loadConfigString(ECString &dest);
    ECResult saveConfigString(const ECString &src);
    ECResult checkDeviceKeyPair();

    // from ec_sysconfig_bw
    ECResult getSysParentalControlInfo(ECParentalControlInfo *out, bool *enabled);
    ECResult getSysNetContentRestrictions(unsigned long *out);
    const char *getSysCountry();
    const char *getSysRegion();
    const char *getSysLanguage();
    ECResult getSysSerialNumber(ECSerialNumber *out);
    ECResult getSysDeviceType(unsigned long *out);
    ECResult getSysWifiMac(unsigned char *out);
    ECResult getSysFreeChannelAppCount(unsigned long *out);
    ECResult getNandUsage(ECNandUsage *out);

    // from ec_dk
    void getAsyncOpThreadAttr(_SHRThreadAttr &attr);

    // from ec_mem
    void *mallocAlign(unsigned long size, unsigned long align);
    void dumpMemInfo();

    template<typename T>
    ECString tostring(T value);
}

// Unnamed namespace - file-local static variables
namespace {
    _SHRThread asyncOpThread__24_unnamed_ec_asyncOp_cpp_;
    _SHRMutex envMutex__24_unnamed_ec_asyncOp_cpp_;
    _SHRMutex progressMutex__24_unnamed_ec_asyncOp_cpp_;
    _SHRMessageQueue requestQueue__24_unnamed_ec_asyncOp_cpp_;
    OSMessage requests__24_unnamed_ec_asyncOp_cpp_[3];
    unsigned char wifiMAC__24_unnamed_ec_asyncOp_cpp_[6];
    bool initialized__24_unnamed_ec_asyncOp_cpp_;

    // File-local helper: parse one name=\0value\0 record from config data
    // Returns pointer to next record, or NULL if none
    const char *getNextNameValue(
        const char *data, unsigned long *remaining,
        const char **outName, const char **outValue,
        unsigned long *outValueLen)
    {
        *outName = 0;
        *outValue = 0;
        if (outValueLen != 0) *outValueLen = 0;

        const char *end = data + *remaining - 1;
        const char *p = data;
        while (true) {
            if (end <= p) break;
            if (*p == '\0') break;
            p++;
        }
        if (p == end || p[1] != '=') return (const char *)0;

        *outName = data;
        p += 2;
        const char *valStart = p;
        while (true) {
            if (end <= p) break;
            if (*p == '\0') break;
            p++;
        }
        if (*p != '\0') {
            *outName = 0;
            return (const char *)0;
        }
        *outValue = valStart;
        if (outValueLen != 0) *outValueLen = (unsigned long)(p - valStart);
        *remaining = (unsigned long)(end - (p + 1));
        return p + 1;
    }

    // Thread function for processing async operations
    void *asyncOpThreadFunc(void *ctx) {
        ECAsyncOpEnv *env = (ECAsyncOpEnv *)ctx;
        OSMessage request;
        while (true) {
            _SHR_message_receive(&requestQueue__24_unnamed_ec_asyncOp_cpp_, &request, 1);
            if (ENV_BOOL(env, 0x0C)) break;
            ECAsyncOp *op = (ECAsyncOp *)request;
            ec::logmsg(ECLogLevel_Fine, "async op thread before process %s\n", op->name.data());
            _SHR_mutex_lock(&envMutex__24_unnamed_ec_asyncOp_cpp_);
            env->dispatchOp(op);
            _SHR_mutex_unlock(&envMutex__24_unnamed_ec_asyncOp_cpp_);
            ec::logmsg(ECLogLevel_Fine, "async op thread after process %s\n", op->name.data());
        }
        ec::logmsg(ECLogLevel_Info, "async op thread exiting on request\n");
        ec::logmsg(ECLogLevel_Fine, "async op thread exiting with return value 0\n");
        return 0;
    }
} // namespace

// Define the global ec::op pointer
namespace ec {
    ECAsyncOpEnv *op;
}

// =====================================================================
// ECAsyncOpEnv implementation
// =====================================================================

ECResult ECAsyncOpEnv::init() {
    if (initialized__24_unnamed_ec_asyncOp_cpp_) {
        return ECResult_AlreadyInitialized;
    }
    initialized__24_unnamed_ec_asyncOp_cpp_ = true;
    _SHR_mutex_lock(&envMutex__24_unnamed_ec_asyncOp_cpp_);

    ECResult result = ECResult_Success;
    try {
        getDeviceInfo();

        // Log running application TitleID and device ID
        ec::logmsg(ECLogLevel_Info, "Running Application is %016llX\n",
            ENV_LL(this, 0x1a0));
        ec::logmsg(ECLogLevel_Fine, "Initial ecsUrl %s\n",
            ENV_STR(this, 0x10).data());
        ec::logmsg(ECLogLevel_Fine, "Initial iasUrl %s\n",
            ENV_STR(this, 0x1C).data());
        ec::logmsg(ECLogLevel_Fine, "Initial casUrl %s\n",
            ENV_STR(this, 0x28).data());
        ec::logmsg(ECLogLevel_Fine, "Initial ccsUrl %s\n",
            ENV_STR(this, 0x34).data());
        ec::logmsg(ECLogLevel_Fine, "Initial ucsUrl %s\n",
            ENV_STR(this, 0x40).data());

        _SHR_message_queue_init(&requestQueue__24_unnamed_ec_asyncOp_cpp_,
            requests__24_unnamed_ec_asyncOp_cpp_, 3);

        _SHRThreadAttr threadAttr;
        ec::getAsyncOpThreadAttr(threadAttr);

        _SHRThread *thr = (_SHRThread *)ENV_PTR(this, 0x8);
        int rv = _SHR_thread_create(thr, &threadAttr,
            (OSThreadFunc)asyncOpThreadFunc, this);
        result = (ECResult)rv;
        if (rv != 0) {
            ec::logmsg(ECLogLevel_Error,
                "EC_Init Error: _SHR_thread_create failed, rv=%d\n", rv);
            result = ECResult_ECFail;
        }
    } catch (std::bad_alloc &) {
        ec::logmsg(ECLogLevel_Error,
            "Caught bad_alloc exception at  FILE: ec_asyncOp.cpp  LINE: %d  COMPILED: Feb  8 2010  20:10:17 \n",
            205);
        result = ECResult_NoMemory;
    } catch (...) {
        ec::logmsg(ECLogLevel_Error,
            "Caught exception at  FILE: ec_asyncOp.cpp  LINE: %d  COMPILED: Feb  8 2010  20:10:17 \n",
            205);
        result = ECResult_ECFail;
    }

    _SHR_mutex_unlock(&envMutex__24_unnamed_ec_asyncOp_cpp_);
    return result;
}

ECResult ECAsyncOpEnv::shutDown() {
    // Set isShuttingDown flag at offset 0x0C
    ENV_BOOL(this, 0x0C) = true;

    // Signal current op to cancel (ECAsyncOp::isCancelRequested at offset 0x2C)
    ECAsyncOp *cur = (ECAsyncOp *)ENV_PTR(this, 0x04);
    if (cur != 0) {
        cur->isCancelRequested = 1;
    }

    _SHRThread *thr = (_SHRThread *)ENV_PTR(this, 0x8);
    if (thr != 0) {
        int rv = _SHR_message_send(&requestQueue__24_unnamed_ec_asyncOp_cpp_, 0, 0);
        if (rv != 0) {
            ec::logmsg(ECLogLevel_Error, "ERROR: EC requestQueue was full");
            return ECResult_ECFail;
        }

        void *joinResult = 0;
        rv = _SHR_thread_join(thr, &joinResult);
        if (rv != 0) {
            ec::logmsg(ECLogLevel_Error,
                "ERROR: EC _SHR_thread_join failed, rv=%d\n", rv);
            return ECResult_ECFail;
        }
    }

    // Free cached data buffers at offsets 0x8C, 0x98, 0xA4
    if (ENV_PTR(this, 0x8C) != 0) {
        ENV_PTR(this, 0x90) = 0;
        ec::free(ENV_PTR(this, 0x8C));
    }
    if (ENV_PTR(this, 0x98) != 0) {
        ENV_PTR(this, 0x9C) = 0;
        ec::free(ENV_PTR(this, 0x98));
    }
    if (ENV_PTR(this, 0xA4) != 0) {
        ENV_PTR(this, 0xA8) = 0;
        ec::free(ENV_PTR(this, 0xA4));
    }

    clearOpCaches();
    initialized__24_unnamed_ec_asyncOp_cpp_ = false;
    return ECResult_Success;
}

ECResult ECAsyncOpEnv::lock() {
    _SHR_mutex_lock(&envMutex__24_unnamed_ec_asyncOp_cpp_);
    return ECResult_Success;
}

ECResult ECAsyncOpEnv::unlock() {
    _SHR_mutex_unlock(&envMutex__24_unnamed_ec_asyncOp_cpp_);
    return ECResult_Success;
}

ECResult ECAsyncOpEnv::lockProgress() {
    _SHR_mutex_lock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
    return ECResult_Success;
}

ECResult ECAsyncOpEnv::unlockProgress() {
    _SHR_mutex_unlock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
    return ECResult_Success;
}

ECResult ECAsyncOpEnv::dispatchOp(ECAsyncOp *op) {
    void *cur = ENV_PTR(this, 0x04);
    if (cur != op) {
        return ECResult_NotFound;
    }
    ENV_INT((ECAsyncOp *)ENV_PTR(this, 0x04), 0x14) = 2;
    _SHR_mutex_lock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
    {
        void *cur2 = ENV_PTR(this, 0x04);
        ENV_INT(cur2, 0x28) = ENV_INT(cur2, 0x1C);
    }
    {
        void *cur3 = ENV_PTR(this, 0x04);
        ENV_INT(cur3, 0x24) = ENV_INT(cur3, 0x18);
    }
    _SHR_mutex_unlock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
    ECResult result;
    try {
        ECAsyncOpArg *arg = (ECAsyncOpArg *)ENV_PTR(ENV_PTR(this, 0x04), 0xC0);
        result = arg->execute();
    } catch (std::bad_alloc &) {
        ec::logmsg(ECLogLevel_Error,
            "Caught bad_alloc exception at  FILE: ec_asyncOp.cpp  LINE: %d  COMPILED: Feb  8 2010  20:10:17 \n",
            205);
        result = ECResult_NoMemory;
    } catch (...) {
        ec::logmsg(ECLogLevel_Error,
            "Caught exception at  FILE: ec_asyncOp.cpp  LINE: %d  COMPILED: Feb  8 2010  20:10:17 \n",
            205);
        result = ECResult_ECFail;
    }
    op->setFinalStatus(result);
    return ECResult_Success;
}

ECResult ECAsyncOpEnv::post(ECAsyncOp *op) {
    int rv = _SHR_message_send(&requestQueue__24_unnamed_ec_asyncOp_cpp_,
        (OSMessage)op, 0);
    if (rv == 0) return ECResult_Success;
    ec::logmsg(ECLogLevel_Error, "ERROR: EC requestQueue was full");
    return ECResult_ECFail;
}

ECResult ECAsyncOpEnv::start(ECAsyncOp *op, ECAsyncOpArg *arg) {
    _SHR_mutex_lock(&envMutex__24_unnamed_ec_asyncOp_cpp_);
    ECResult result;
    if (op == 0) {
        result = ECResult_InvalidBufHeap;
    } else {
        ECAsyncOp *cur = (ECAsyncOp *)ENV_PTR(this, 0x04);
        if (cur == 0 || cur->state == 3) {
            int seqNum = ENV_INT(this, 0x00);
            ENV_INT(this, 0x00) = seqNum + 1;
            result = op->arg->init(op, seqNum, arg);
            if (result < 0) {
                clearOpCaches();
            } else {
                clearOpCaches();
                ENV_PTR(this, 0x04) = op;
                result = post(op);
                if (result < 0) {
                    op->setFinalStatus(result);
                } else {
                    result = (ECResult)((ECAsyncOp *)ENV_PTR(this, 0x04))->seqNum;
                }
            }
        } else {
            result = ECResult_Busy;
        }
    }
    _SHR_mutex_unlock(&envMutex__24_unnamed_ec_asyncOp_cpp_);
    return result;
}

ECResult ECAsyncOpEnv::clearOpCaches() {
    if (ENV_PTR(this, 0xB0) != 0) {
        ec::free(ENV_PTR(this, 0xB0));
        ENV_PTR(this, 0xB0) = 0;
    }
    if (ENV_PTR(this, 0xB4) != 0) {
        ENV_PTR(this, 0xB4) = 0;
    }
    return ECResult_Success;
}

ECResult ECAsyncOpEnv::getDeviceInfo() {
    getKeyPairStatus();
    getConfig();

    ECResult rv;
    if (ENV_BOOL(this, 0x1A8)) {
        rv = getDevId();
        if (rv != 0) {
            ec::logmsg(ECLogLevel_Error, "EC_Init() getDevId() returned %d\n", rv);
            goto done;
        }
        bool certEmpty = (ENV_STR(this, 0x224).size() == 0);
        if (!certEmpty) {
        } else {
            rv = getDevCert();
            if (rv != 0) {
                ec::logmsg(ECLogLevel_Error, "EC_Init() getDevCert() returned %d\n", rv);
                goto done;
            }
        }
    }

    rv = (ECResult)ESP_GetTitleId((unsigned long long *)(((char *)this) + 0x1A0));
    if (rv == 0) {
        getCountry();
        getRegion();
        getLanguage();
        getParentalControlInfo();
        getSerialNumber();
        getFreeChannelAppCount();
        getFileSystemStatus();
    }
done:
    return rv;
}

__declspec(noinline) void ECAsyncOpEnv::getKeyPairStatus() {
    int rv = ec::checkDeviceKeyPair();
    ENV_BOOL(this, 0x1A8) = (rv == 0);
}

ECResult ECAsyncOpEnv::setDeviceToken(const char *token) {
    ECResult result = ECResult_Success;
    deviceToken = token;
    tokenPrefix.clear();
    tokenMD5.clear();
    tokenExt.clear();

    bool empty = (deviceToken.size() == 0);
    if (!empty) {
        {
            ECString prefix = "ST-" + deviceToken;
            tokenPrefix = prefix;
        }

        unsigned long tokenSize = deviceToken.size();
        result = (ECResult)ec::md5_sum(deviceToken.data(),
                        tokenSize,
                        tokenMD5);
        if (result == 0) {
            ECString ext = "WT-" + tokenMD5;
            tokenExt = ext;
        } else {
            result = ECResult_ECFail;
            ec::logmsg(ECLogLevel_Error, "md5_sum of deviceToken failed %d\n", result);
            tokenMD5.clear();
        }
    }
    return result;
}

ECResult ECAsyncOpEnv::setDeviceToken(ECString &token) {
    return setDeviceToken(token.data());
}

ECResult ECAsyncOpEnv::setNamedValue(const char *name, const char *value) {
    ECResult rv = ECResult_Success;
    if (strcmp(name, "accountId") == 0) {
        ENV_STR(this, 0xC0) = value;
    } else if (strcmp(name, "deviceToken") == 0) {
        rv = setDeviceToken(value);
    } else if (strcmp(name, "country") == 0) {
        ENV_STR(this, 0x120) = value;
    } else if (strcmp(name, "accountCountry") == 0) {
        ENV_STR(this, 0x12C) = value;
    } else if (strcmp(name, "accountDeviceCode") == 0) {
        ENV_STR(this, 0x138) = value;
    } else if (strcmp(name, "lastTicketSyncTime") == 0) {
        ENV_LL(this, 0x168) = strtoll(value, (char **)0, 10);
    } else if (strcmp(name, "isNeedTicketSync") == 0) {
        int v = *value - '0';
        ENV_BYTE(this, 0x170) = (char)v - (*value - '1' + (v == 0));
    } else if (strcmp(name, "isNeedTicketSyncImportAll") == 0) {
        int v = *value - '0';
        ENV_BYTE(this, 0x171) = (char)v - (*value - '1' + (v == 0));
    } else if (strcmp(name, "extAccountId") == 0) {
        ENV_STR(this, 0x114) = value;
    } else if (strcmp(name, "forceSyncTime") == 0) {
        ENV_LL(this, 0x150) = strtoll(value, (char **)0, 10);
    } else if (strcmp(name, "extTicketTime") == 0) {
        ENV_LL(this, 0x158) = strtoll(value, (char **)0, 10);
    } else if (strcmp(name, "syncTime") == 0) {
        ENV_LL(this, 0x160) = strtoll(value, (char **)0, 10);
    } else {
        ec::ECNamValStr entry;
        entry.name = name;
        entry.value = value;
        namedValues.insert(namedValues.end(), entry);
    }
    return rv;
}

ECResult ECAsyncOpEnv::saveConfig() {
    ec::ECOstringstream os;
    os << "cfgVersion" << '\0' << "=" << "0" << '\0';
    os << "accountId" << '\0' << "=" << unk0xc0 << '\0';
    os << "deviceToken" << '\0' << "=" << deviceToken << '\0';
    os << "country" << '\0' << "=" << country << '\0';
    os << "accountCountry" << '\0' << "=" << accountCountry << '\0';
    os << "accountDeviceCode" << '\0' << "=" << accountDeviceCode << '\0';
    os << "lastTicketSyncTime" << '\0' << "=" << lastTicketSyncTime << '\0';
    os << "isNeedTicketSync" << '\0' << "=" << isNeedTicketSync << '\0';
    os << "isNeedTicketSyncImportAll" << '\0' << "=" << isNeedTicketSyncImportAll << '\0';
    os << "extAccountId" << '\0' << "=" << extAccountId << '\0';
    os << "forceSyncTime" << '\0' << "=" << forceSyncTimeLL << '\0';
    os << "extTicketTime" << '\0' << "=" << extTicketTimeLL << '\0';
    os << "syncTime" << '\0' << "=" << syncTimeLL << '\0';

    unsigned long count = 0;
    unsigned long stride = 0;
    for (; count < namedValues.size(); count++, stride += sizeof(ec::ECNamValStr)) {
        const ec::ECNamValStr *entry = (const ec::ECNamValStr *)(((char *)namedValues.data()) + stride);
        os << entry->name << '\0' << "=" << entry->value << '\0';
    }

    ECString cfg = os.str();
    ECResult rv = ec::saveConfigString(cfg);
    if (rv == 0) isLoaded = true;
    return rv;
}

ECResult ECAsyncOpEnv::getConfig() {
    ECString cfgStr;
    ECResult rv = ec::loadConfigString(cfgStr);
    if (rv != 0) {
        if (rv == ECResult_NotFound) {
            ec::logmsg(ECLogLevel_Info, "EC config file not present\n");
            ENV_BOOL(this, 0xB8) = false;
        }
        return rv;
    }
    ENV_BOOL(this, 0xB8) = true;
    const char *name;
    const char *value;
    unsigned long valueLen;
    const char *data = cfgStr.data();
    unsigned long remaining = cfgStr.size();
    const char *next = getNextNameValue(
        data, &remaining, &name, &value, &valueLen);
    if (next == 0 || strcmp(name, "cfgVersion") != 0 ||
        valueLen != 2 || memcmp(value, "0", valueLen) != 0) {
        return ECResult_ECFail;
    }
    ((std::__vec_deleter<ec::ECNamValStr, ECAllocator<ec::ECNamValStr> > *)&namedValues)->clear();
    while ((next = getNextNameValue(
                next, &remaining, &name, &value, 0)) != 0) {
        setNamedValue(name, value);
    }
    return ECResult_Success;
}

ECResult ECAsyncOpEnv::getDevId() {
    unsigned long deviceType;
    ECResult rv = (ECResult)ec::getSysDeviceType(&deviceType);
    if (rv == 0) {
        rv = (ECResult)ESP_GetDeviceId((unsigned long long *)(((char *)this) + 0x1AC));
        if (rv == 0) {
            ENV_STR(this, 0x1B0).clear();
            unsigned long long devId = *(unsigned long long *)(((char *)this) + 0x1AC);
            ECString idStr = ec::tostring<unsigned long long>(devId);
            ENV_STR(this, 0x1B0) = idStr;
        } else {
            ec::logmsg(ECLogLevel_Error, "ES_GetDeviceId returned %d\n", rv);
        }
    }
    return rv;
}

ECResult ECAsyncOpEnv::getDevCert() {
    unsigned char certBuf[0x180];
    ECResult rv = (ECResult)ESP_GetDeviceCert(certBuf);
    if (rv == 0) {
        ec::logmsg(ECLogLevel_Fine, "ES_GetDeviceCert success\n");
        ENV_STR(this, 0x224).clear();
        ec::base64_encode(certBuf, 0x180, ENV_STR(this, 0x224));
    } else {
        ec::logmsg(ECLogLevel_Error, "ES_GetDeviceCert returned %d\n", rv);
    }
    return rv;
}

ECResult ECAsyncOpEnv::getCRL(bool force) {
    // Simplified stub - full implementation uses ec::Content<ECString> HTTP client
    if (!force && ENV_BOOL(this, 0x258)) {
        return ECResult_Success;
    }
    ENV_UINT(this, 0x254) = 0;
    ENV_PTR(this, 0x250) = 0;
    ENV_BOOL(this, 0x258) = false;
    return ECResult_Success;
}

ECResult ECAsyncOpEnv::checkParentalControlPassword(const char *pin, long *result) {
    ENV_BOOL(this, 0x221) = false;
    ECResult rv = getParentalControlInfo();
    if (rv == 0) {
        if (!ENV_BOOL(this, 0x220)) {
            return ECResult_NoPC;
        }
        if (pin != 0) {
            unsigned long pinLen = ENV_UINT(this, 0x21C);
            size_t pinStrLen = strnlen(pin, pinLen + 1);
            if (pinLen == pinStrLen) {
                int cmp = strncmp(pin, ((char *)this) + 0x20A, pinLen);
                *result = (cmp == 0);
                if (cmp == 0) ENV_BOOL(this, 0x221) = true;
                return ECResult_Success;
            }
        }
        *result = 0;
    }
    return rv;
}

ECResult ECAsyncOpEnv::getParentalControlInfo() {
    int rv = ec::getSysParentalControlInfo(
        (ECParentalControlInfo *)(((char *)this) + 0x204),
        (bool *)(((char *)this) + 0x220)
    );
    if (rv == 0) {
        ec::logmsg(ECLogLevel_Fine,
            "Parental control enabled %d, flags %02X  User Age %u\n",
            ENV_BOOL(this, 0x220),
            ENV_UINT(this, 0x204),
            (unsigned)(unsigned char)ENV_BYTE(this, 0x209)
        );
        rv = ec::getSysNetContentRestrictions((unsigned long *)(((char *)this) + 0x200));
        if (rv != 0) ENV_UINT(this, 0x200) = 0;
    } else {
        memset(((char *)this) + 0x204, 0, 0x1C);
        ENV_BOOL(this, 0x220) = false;
        ENV_UINT(this, 0x200) = 0;
    }
    return ECResult_Success;
}

ECResult ECAsyncOpEnv::getCountry() {
    ECResult result = ECResult_Success;
    if (!ENV_BOOL(this, 0x26D)) {
        const char *country = ec::getSysCountry();
        if (country != 0) {
            ENV_STR(this, 0x120) = country;
        } else {
            ENV_STR(this, 0x120) = "<none>";
            result = ECResult_NotFound;
        }
    }
    return result;
}

ECResult ECAsyncOpEnv::getRegion() {
    ECResult result = ECResult_Success;
    if (!ENV_BOOL(this, 0x26E)) {
        const char *region = ec::getSysRegion();
        if (region != 0) {
            ENV_STR(this, 0x1D0) = region;
        } else {
            ENV_STR(this, 0x1D0) = "<none>";
            result = ECResult_NotFound;
        }
    }
    return result;
}

ECResult ECAsyncOpEnv::getLanguage() {
    ECResult result = ECResult_Success;
    if (!ENV_BOOL(this, 0x26C)) {
        const char *lang = ec::getSysLanguage();
        if (lang != 0) {
            ENV_STR(this, 0x144) = lang;
        } else {
            ENV_STR(this, 0x144) = "<none>";
            result = ECResult_NotFound;
        }
    }
    return result;
}

ECResult ECAsyncOpEnv::getSerialNumber() {
    ENV_STR(this, 0x230).clear();
    ECSerialNumber sn;
    ECResult rv = (ECResult)ec::getSysSerialNumber(&sn);
    if (rv == 0) {
        ENV_STR(this, 0x230) = sn.value;
        return rv;
    }
    ECString snStr = "fakeSN" + ENV_STR(this, 0x1B0);
    ENV_STR(this, 0x230) = snStr;
    return ECResult_NotFound;
}

void ECAsyncOpEnv::getFileSystemStatus() {
    ECNandUsage usage;
    ECResult rv = ec::getNandUsage(&usage);
    if (rv == 0) {
        ENV_UINT(this, 0x1DC) = usage.f00;
        ENV_UINT(this, 0x1E4) = usage.f04;
        ENV_UINT(this, 0x1F4) = usage.f08;
        ENV_UINT(this, 0x1E0) = usage.f04 - usage.f0C;
        ENV_UINT(this, 0x1F0) = usage.f08 - usage.f10;
        unsigned long sumEC = usage.f14 + usage.f24;
        ENV_UINT(this, 0x1EC) = sumEC;
        unsigned long sumFC = usage.f18 + usage.f28;
        ENV_UINT(this, 0x1FC) = sumFC;
        ENV_UINT(this, 0x1E8) = sumEC - usage.f1C - usage.f2C;
        ENV_UINT(this, 0x1F8) = sumFC - usage.f20 - usage.f30;
    }
}

ECResult ECAsyncOpEnv::getWifiMac(char *out) {
    unsigned char **macPtrPtr = (unsigned char **)(((char *)this) + 0x248);
    unsigned char *mac = *macPtrPtr;
    ECResult rv = ECResult_Success;
    if (mac == 0) {
        rv = (ECResult)ec::getSysWifiMac(wifiMAC__24_unnamed_ec_asyncOp_cpp_);
        mac = 0;
        if (rv == 0) {
            mac = wifiMAC__24_unnamed_ec_asyncOp_cpp_;
            *macPtrPtr = wifiMAC__24_unnamed_ec_asyncOp_cpp_;
        }
    }
    if (mac == 0) {
        rv = ECResult_NotFound;
        *out = '\0';
    } else {
        snprintf(out, 0x12, "%02x:%02x:%02x:%02x:%02x:%02x",
            (unsigned)mac[0], (unsigned)mac[1], (unsigned)mac[2],
            (unsigned)mac[3], (unsigned)mac[4], (unsigned)mac[5]);
    }
    return rv;
}

ECResult ECAsyncOpEnv::getBluetoothMac(char *out) {
    unsigned char *mac = *(unsigned char **)(((char *)this) + 0x24C);
    if (mac == 0) {
        *out = '\0';
        return ECResult_NotFound;
    }
    snprintf(out, 0x12, "%02x:%02x:%02x:%02x:%02x:%02x",
        (unsigned)mac[0], (unsigned)mac[1], (unsigned)mac[2],
        (unsigned)mac[3], (unsigned)mac[4], (unsigned)mac[5]);
    return ECResult_Success;
}

ECResult ECAsyncOpEnv::getFreeChannelAppCount() {
    unsigned long *count = (unsigned long *)(((char *)this) + 0x1CC);
    ECResult rv = ec::getSysFreeChannelAppCount(count);
    if (rv != 0) {
        *count = 0;
        return ECResult_NotFound;
    }
    return rv;
}

ECResult ECAsyncOpEnv::setWebSvcUrls(const char *ecsUrl, const char *iasUrl, const char *casUrl) {
    const char *ecs = ecsUrl ? ecsUrl : "<none>";
    const char *ias = iasUrl ? iasUrl : "<none>";
    const char *cas = casUrl ? casUrl : "<none>";

    if (serviceUrl1.compare(ecs) != 0) {
        ec::logmsg(ECLogLevel_Info, "new ecsUrl: %s\n", ecs);
        serviceUrl1 = ecs;
    }
    if (serviceUrl2.compare(ias) != 0) {
        ec::logmsg(ECLogLevel_Info, "new iasUrl: %s\n", ias);
        serviceUrl2 = ias;
    }
    if (serviceUrl3.compare(cas) != 0) {
        ec::logmsg(ECLogLevel_Info, "new casUrl: %s\n", cas);
        serviceUrl3 = cas;
    }
    return ECResult_Success;
}

ECResult ECAsyncOpEnv::setContentUrls(const char *ccsUrl, const char *ucsUrl) {
    const char *ccs = ccsUrl ? ccsUrl : "<none>";
    const char *ucs = ucsUrl ? ucsUrl : "<none>";

    if (contentUrl1.compare(ccs) != 0) {
        ec::logmsg(ECLogLevel_Info, "new ccsUrl: %s\n", ccs);
        contentUrl1 = ccs;
    }
    if (contentUrl2.compare(ucs) != 0) {
        ec::logmsg(ECLogLevel_Info, "new ucsUrl: %s\n", ucs);
        contentUrl2 = ucs;
    }
    return ECResult_Success;
}

// ECAsyncOpEnv destructor
ECAsyncOpEnv::~ECAsyncOpEnv() {
    _SHR_mutex_destroy(&envMutex__24_unnamed_ec_asyncOp_cpp_);
    _SHR_mutex_destroy(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
}

// =====================================================================
// ECAsyncOp implementation
// =====================================================================

void ECAsyncOp::setErrMsg(const char *msg) {
    _SHR_mutex_lock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
    if (msg == 0 || *msg == '\0') {
        errInfo[0] = '\0';
    } else {
        strncpy(errInfo, msg, 0x80);
        errInfo[0x7F] = '\0';
    }
    _SHR_mutex_unlock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
}

void ECAsyncOp::setErrInfo(const char *code, const char *msg) {
    _SHR_mutex_lock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
    if (code != 0 && *code != '\0') {
        errCode = strtol(code, (char **)0, 0);
    }
    if (msg == 0 || *msg == '\0') {
        errInfo[0] = '\0';
    } else {
        strncpy(errInfo, msg, 0x80);
        errInfo[0x7F] = '\0';
    }
    _SHR_mutex_unlock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
}

void ECAsyncOp::setTotalSize(unsigned long size) {
    _SHR_mutex_lock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
    downloadedSize = 0;
    if (errType == 1) {
        progTotalSize = size;
    } else if (errType == 0) {
        progTotalSize = 0;
    }
    _SHR_mutex_unlock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
}

void ECAsyncOp::incDownloadedSize(unsigned long delta) {
    _SHR_mutex_lock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
    downloadedSize += delta;
    _SHR_mutex_unlock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
}

#pragma dont_inline on
ECResult ECAsyncOp::init(long seq, ECAsyncOpArg *argIn) {
    seqNum = seq;
    state = 1; // Starting
    _SHR_mutex_lock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
    memset(&status, 0, 0x9C);
    status = ECResult_NotDone;
    phase = 1; // EC_PHASE_Starting
    errType = 0;
    _SHR_mutex_unlock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
    ec::dumpMemInfo();
    return ECResult_Success;
}
#pragma dont_inline reset

void ECAsyncOp::setFinalStatus(long s) {
    _SHR_mutex_lock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
    status = (ECResult)s;
    phase = 2; // EC_PHASE_Done
    if (s < 0) {
        if (s == errCode) errCode = 0;
        ec::logmsg(ECLogLevel_Info, "%s failed %d\n", name.data(), s);
        arg->onError(this);
    }
    _SHR_mutex_unlock(&progressMutex__24_unnamed_ec_asyncOp_cpp_);
    ec::dumpMemInfo();
    state = 3; // Done
}

#pragma dont_inline on
void ECAsyncOp::dump() {
    // Get the environment from the arg's context
    ec::logmsg(ECLogLevel_Info,
        "Dump of %s environment:\necsUrl %s\niasUrl %s\ncasUrl %s\nccsUrl %s\nucsUrl %s\n",
        name.data(),
        ec::op->serviceUrl1.data(),
        ec::op->serviceUrl2.data(),
        ec::op->serviceUrl3.data(),
        ec::op->contentUrl1.data(),
        ec::op->contentUrl2.data()
    );
    ec::logmsg(ECLogLevel_Info,
        "ECAsyncOp id %d, phase %d, op %d, status %d, isDone %d, errCode %d, errInfo %s\n",
        seqNum, phase, operation, status, (phase == 3), errCode, errInfo
    );
}
#pragma dont_inline reset

#pragma dont_inline on
ECAsyncOp::~ECAsyncOp() {
    if (this != 0) {
        name.~ECString();
        if (1 > 0) { // param_2 > 0 check from Ghidra
            // delete is handled by ECObj
        }
    }
}
#pragma dont_inline reset


// Out-of-line ECString member functions needed by ec_asyncOp
int ECString::compare(const char *str) const {
    unsigned long str_len = strlen(str);
    unsigned long self_len = size();
    const char *self_ptr;
    unsigned long dummy_len;
    pointer_size(self_ptr, dummy_len);
    unsigned long cmp_len = (self_len < str_len) ? self_len : str_len;
    int result = memcmp(self_ptr, str, cmp_len);
    if (result != 0) return result;
    if (self_len < str_len) return -1;
    if (self_len > str_len) return 1;
    return 0;
}

int ECString::compare(size_t pos, size_t len, const char *str, size_t n) const {
    return memcmp(data() + pos, str, (len < n) ? len : n);
}
