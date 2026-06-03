#include "BlockMgr.h"
#include "obj/DataFunc.h"
#include "os/Archive.h"
#include "os/AsyncTask.h"
#include "os/CDReader.h"
#include "os/Debug.h"
#include "os/HDCache.h"
#include "utl/MemMgr.h"
#include "decomp.h"

#define kNumBlockBuffers 4

BlockMgr TheBlockMgr;
static int gLastBlockNum = -1;
static int gLastArkNum = -1;
static char *gBuffers;
static int gCurrBuffNum;
int Block::sCurrTimestamp = 0;
static int gNumPolls;
Timer gReadTime;
static int gSeekCount;
static float gSeekTimeMs;

namespace {
    bool gReadHD = false;
    static DataNode OnSpinUp(DataArray *) { return TheBlockMgr.SpinUp(); }
}

int GetFreeBuffer() {
    MILO_ASSERT(gCurrBuffNum < kNumBlockBuffers, 0x44);
    return gCurrBuffNum++;
}

DECOMP_FORCEACTIVE(BlockMgr, "it->Exceeds(ark, block)")

Block::Block() : mArkfileNum(-1), mBlockNum(-1), mWritten(true), mDebugName("") {
    mBuffer = &gBuffers[GetFreeBuffer() * 0x10000];
    UpdateTimestamp();
}

void Block::UpdateTimestamp() { mTimestamp = ++sCurrTimestamp; }

BlockRequest::BlockRequest(const AsyncTask &task)
    : mArkfileNum(task.mArkfileNum), mBlockNum(task.GetBlockNum()), mStr(task.GetStr()) {
    mTasks.push_back(task);
}

void BlockMgr::Init() {
    gBuffers = (char *)_MemAlloc(0x40000, 0x40);
    gCurrBuffNum = 0;
    mBlockCache.resize(4);
    mReadingBlock = nullptr;
    for (int i = 0; i < mBlockCache.size(); i++) {
        mBlockCache[i] = new Block();
    }
    TheHDCache.Init();
    DataRegisterFunc("disc_spin_up", OnSpinUp);
}

const char *BlockMgr::GetBlockData(int ark, int blk) {
    Block *blokc = FindBlock(ark, blk);
    if (blokc != nullptr && blokc != mReadingBlock) {
        blokc->UpdateTimestamp();
        return blokc->mBuffer;
    }
    return nullptr;
}

void BlockMgr::WriteBlock() {
    MILO_ASSERT(!mWritingBlock, 345);
    bool ret;
    Block *blk;
    do {
        blk = FindLRUBlock(true);
        if (blk == nullptr)
            return;
        blk->mWritten = true;
        ret = TheHDCache.WriteAsync(blk->mArkfileNum, blk->mBlockNum, blk->mBuffer);
    } while (!ret);
    mWritingBlock = blk;
}

void BlockMgr::ReadBlock() {
    MILO_ASSERT(mReadingBlock, 364);

    bool x;
    void *buf = (void *)mReadingBlock->mBuffer;
    u32 arknum = mReadingBlock->mArkfileNum;
    u32 blknum = mReadingBlock->mBlockNum;
    if (TheHDCache.ReadAsync(arknum, blknum, buf)) {
        gReadHD = true;
        x = false;
    } else {
        gReadHD = false;
        x = CDRead(arknum, blknum * 32, 32, buf);
    }
    if (!x) {
        mReadingBlock->UpdateTimestamp();
    } else {
        MILO_LOG("CD READING ERROR: %x\n", x);
        mReadingBlock = nullptr;
    }
}

Block *BlockMgr::FindBlock(int i1, int i2) {
    for (int i = 0; i < mBlockCache.size(); i++) {
        if (mBlockCache[i]->CheckMetadata(i1, i2))
            return mBlockCache[i];
    }
    return nullptr;
}

Block *BlockMgr::FindLRUBlock(bool b) {
    Block *ret = 0;
    int time = Block::sCurrTimestamp;
    for (int i = 0; i < mBlockCache.size(); i++) {
        if (mBlockCache[i] != mWritingBlock && mBlockCache[i] != mReadingBlock) {
            if (b) {
                if (mBlockCache[i]->mWritten) continue;
            }
            if (mBlockCache[i]->mTimestamp < time) {
                ret = mBlockCache[i];
                time = mBlockCache[i]->mTimestamp;
            }
        }
    }
    return ret;
}

Block *BlockMgr::FindMRUBlock() {
    int time = -1;
    Block *ret = nullptr;
    for (int i = 0; i < mBlockCache.size(); i++) {
        if (mBlockCache[i]->mTimestamp > time) {
            ret = mBlockCache[i];
            time = mBlockCache[i]->mTimestamp;
        }
    }
    return ret;
}

void BlockMgr::Poll() {
    MILO_ASSERT(MainThread(), 402);

    TheHDCache.Poll();
    mSpinDownTimer.Split();

    if (mWritingBlock && TheHDCache.WriteDone()) {
        mWritingBlock = nullptr;
        WriteBlock();
    }

    if (mReadingBlock) {
        gNumPolls++;
        int err;
        if (gReadHD) {
            err = TheHDCache.ReadFail();
        } else {
            err = CDGetError();
        }
        if (err != 0) {
            MILO_LOG(" CD READING ERROR!!!  %x\n", err);
            ReadBlock();
            return;
        }
        bool readDone;
        if (gReadHD) {
            readDone = TheHDCache.ReadDone();
        } else {
            readDone = CDReadDone();
        }
        if (readDone) {
            if (Archive::DebugArkOrder()) {
                gReadTime.Split();
                int seekDist = mReadingBlock->mBlockNum - gLastBlockNum;
                if (mReadingBlock->mArkfileNum != gLastArkNum) {
                    seekDist = 99999;
                }
                if (seekDist != 1) {
                    gSeekCount++;
                    gSeekTimeMs += Timer::CyclesToMs(gReadTime.mCycles);
                } else {
                    gSeekCount = 0;
                    gSeekTimeMs = 0.0f;
                }
                if (gSeekCount >= 1 || gSeekTimeMs >= 240.0f) {
                    char debugName[100];
                    strncpy(debugName, mReadingBlock->mDebugName, 99);
                    debugName[99] = '\0';
                    MILO_LOG(
                        "BlockMgr Seek: Ark: %2d  Dist: %5d  Seek Time: %3.0f ms  Suspect: %s\n",
                        mReadingBlock->mArkfileNum,
                        seekDist,
                        gSeekTimeMs,
                        debugName
                    );
                }
                gLastBlockNum = mReadingBlock->mBlockNum;
                gLastArkNum = mReadingBlock->mArkfileNum;
            }
            if (!gReadHD) {
                MarkDiscRead();
            }
            mReadingBlock->UpdateTimestamp();

            std::list<BlockRequest>::iterator request = mRequests.begin();
            while (request != mRequests.end()) {
                if (mReadingBlock->CheckMetadata(request->mArkfileNum, request->mBlockNum))
                    break;
                ++request;
            }
            MILO_ASSERT(request != mRequests.end(), 480);

            mReadingBlock = nullptr;
            for (std::list<AsyncTask>::iterator taskIt = request->mTasks.begin();
                 taskIt != request->mTasks.end(); ++taskIt) {
                taskIt->FillData();
            }
            mRequests.erase(request);
            if (!mWritingBlock) {
                WriteBlock();
            }
        }
    }

    if (mReadingBlock)
        return;
    if (mRequests.size() == 0)
        return;

    Block *block = FindLRUBlock(false);
    BlockRequest &nextReq = mRequests.front();
    int arkfilenum = nextReq.mArkfileNum;
    int blocknum = nextReq.mBlockNum;
    const char *str = nextReq.mStr;

    MILO_ASSERT(blocknum != -1, 516);

    mReadingBlock = block;
    mReadingBlock->mBlockNum = blocknum;
    mReadingBlock->mArkfileNum = arkfilenum;
    mReadingBlock->mWritten = false;
    mReadingBlock->mDebugName = str;

    gReadTime.Restart();
    gNumPolls = 0;

    ReadBlock();
}

bool BlockMgr::SpinUp() {
    TheBlockMgr.Poll();
    if (UsingCD()) {
        if (Timer::CyclesToMs(mSpinDownTimer.mCycles) > 120000.000f) {
            if (mReadingBlock == nullptr) {
                MILO_LOG("BlockMgr spinning up...\n");
                Block *blk = FindMRUBlock();
                mReadingBlock = blk;
                AsyncTask at(blk->mArkfileNum, blk->mBlockNum);
                AddTask(at);
                gReadHD = false;
                bool x = CDRead(
                    mReadingBlock->mArkfileNum,
                    ((mReadingBlock->mBlockNum + 1) << 5) - 1,
                    1,
                    (void *)(mReadingBlock->mBuffer + 0xF800)
                );
                if (!x) {
                    mReadingBlock->UpdateTimestamp();
                } else {
                    MILO_LOG("CD READING ERROR: %x\n", x);
                    mReadingBlock = nullptr;
                }
            }
            return false;
        }
    }
    return true;
}

void BlockMgr::MarkDiscRead() { mSpinDownTimer.Restart(); }

void BlockMgr::GetAssociatedBlocks(
    unsigned long long offset, int bytes, int &startBlock, int &numBlocks, int &blockSize
) {
    blockSize = 0x10000;
    startBlock = (int)(offset >> 16);
    int remaining = (int)(offset & 0xFFFF) + bytes - 0x10000;
    if (remaining > 0) {
        int extraBlocks = remaining / 0x10000;
        numBlocks = extraBlocks + 1;
        if (remaining % 0x10000 != 0) {
            numBlocks++;
        }
        return;
    }
    numBlocks = 1;
}

void BlockMgr::KillBlockRequests(ArkFile *arkFile) {
    std::list<BlockRequest>::iterator end = mRequests.end();
    std::list<BlockRequest>::iterator it = mRequests.begin();
    while (it != end) {
        std::list<AsyncTask>::iterator taskIt = it->mTasks.begin();
        while (taskIt != it->mTasks.end()) {
            if (taskIt->GetOwner() == arkFile) {
                taskIt = it->mTasks.erase(taskIt);
            } else {
                ++taskIt;
            }
        }
        if (it->mTasks.size() == 0
            && !(mReadingBlock
                 && mReadingBlock->CheckMetadata(it->mArkfileNum, it->mBlockNum))) {
            it = mRequests.erase(it);
        } else {
            ++it;
        }
    }
}

void BlockMgr::AddTask(const AsyncTask &task) {
    std::list<BlockRequest>::iterator it;
    int blockNum = task.GetBlockNum();
    int arkNum = task.mArkfileNum;
    for (it = mRequests.begin(); it != mRequests.end(); ++it) {
        bool match = (arkNum == it->mArkfileNum && blockNum == it->mBlockNum);
        if (match) {
            it->mTasks.push_back(task);
            break;
        }
        int itArk = it->mArkfileNum;
        bool exceeds =
            (itArk > arkNum
             || (itArk == arkNum && it->mBlockNum > blockNum));
        if (exceeds) {
            mRequests.insert(it, BlockRequest(task));
            break;
        }
    }
    if (it == mRequests.end()) {
        mRequests.push_back(BlockRequest(task));
    }
}
