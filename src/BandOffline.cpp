#include "BandOffline.h"
#include "decomp.h"
#include "obj/DataFunc.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "obj/Msg.h"
#include "os/System.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "os/ContentMgr.h"
#include "synth/Synth.h"
#include "rndobj/Rnd.h"
#include "ui/UI.h"
#include "obj/Task.h"
#include "net/Net.h"
#include "net_band/RockCentral.h"
#include "bandobj/BandCharacter.h"
#include "bandobj/BandDirector.h"
#include "utl/StlAlloc.h"
#include "utl/MakeString.h"
#include "utl/Option.h"
#include "utl/TextFileStream.h"
#include <algorithm>
#include <vector>
#include <map>

// Forward declaration
extern const char *BandIntensityString(int);

// ─── STL template specializations for pair<String,float> + SortGroupPolls ───
// Manual specializations avoid bool materialization (mfcr/extrwi/beq) that the
// generic template produces. Direct float comparisons generate fcmpo+bgt/ble.

namespace stlpmtx_std {

typedef std::pair<String, float> PairSF;

template <>
void __push_heap<PairSF *, long, PairSF, SortGroupPolls>(
    PairSF *__first, long __holeIndex, long __topIndex,
    PairSF __val, SortGroupPolls) {
    long __parent = (__holeIndex - 1) / 2;
    while (__holeIndex > __topIndex &&
           (__first + __parent)->second < __val.second) {
        *(__first + __holeIndex) = *(__first + __parent);
        __holeIndex = __parent;
        __parent = (__holeIndex - 1) / 2;
    }
    *(__first + __holeIndex) = __val;
}

template <>
void __adjust_heap<PairSF *, long, PairSF, SortGroupPolls>(
    PairSF *__first, long __holeIndex, long __len,
    PairSF __val, SortGroupPolls __comp) {
    long __topIndex = __holeIndex;
    long __secondChild = 2 * __holeIndex + 2;
    while (__secondChild < __len) {
        if ((__first + __secondChild)->second > (__first + (__secondChild - 1))->second)
            __secondChild--;
        *(__first + __holeIndex) = *(__first + __secondChild);
        __holeIndex = __secondChild;
        __secondChild = 2 * (__secondChild + 1);
    }
    if (__secondChild == __len) {
        *(__first + __holeIndex) = *(__first + (__secondChild - 1));
        __holeIndex = __secondChild - 1;
    }
    __push_heap(__first, __holeIndex, __topIndex, __val, __comp);
}

template <>
void __unguarded_linear_insert<PairSF *, PairSF, SortGroupPolls>(
    PairSF *__last, PairSF __val, SortGroupPolls __comp) {
    PairSF *__next = __last;
    --__next;
    while (__val.second > __next->second) {
        *__last = *__next;
        __last = __next;
        --__next;
    }
    *__last = __val;
}

template <>
void __linear_insert<PairSF *, PairSF, SortGroupPolls>(
    PairSF *__first, PairSF *__last, PairSF __val, SortGroupPolls __comp) {
    if (__val.second > __first->second) {
        copy_backward(__first, __last, __last + 1);
        *__first = __val;
    } else {
        __unguarded_linear_insert(__last, __val, __comp);
    }
}

template <>
PairSF *
__unguarded_partition<PairSF *, PairSF, SortGroupPolls>(
    PairSF *__first, PairSF *__last, PairSF __pivot, SortGroupPolls) {
    for (;;) {
        while (__first->second > __pivot.second)
            ++__first;
        --__last;
        while (__pivot.second > __last->second)
            --__last;
        if (!(__first < __last))
            return __first;
        iter_swap(__first, __last);
        ++__first;
    }
}

template <>
void __introsort_loop<PairSF *, PairSF, long, SortGroupPolls>(
    PairSF *__first, PairSF *__last, PairSF *,
    long __depth_limit, SortGroupPolls __comp) {
    while (__last - __first > 16) {
        if (__depth_limit == 0) {
            partial_sort(__first, __last, __last, __comp);
            return;
        }
        ptrdiff_t __len = __last - __first;
        float __a = __first->second;
        float __c = (__last - 1)->second;
        --__depth_limit;
        PairSF *__mid = __first + __len / 2;
        float __b = __mid->second;
        PairSF *__pivot_ptr;
        if (__a > __b) {
            if (__b > __c)
                __pivot_ptr = __mid;
            else if (__a > __c)
                __pivot_ptr = __last - 1;
            else
                __pivot_ptr = __first;
        } else if (__a > __c) {
            __pivot_ptr = __first;
        } else if (__b > __c) {
            __pivot_ptr = __last - 1;
        } else {
            __pivot_ptr = __mid;
        }
        PairSF *__cut = __unguarded_partition(
            __first, __last, *__pivot_ptr, __comp);
        __introsort_loop(__cut, __last, (PairSF *)0, __depth_limit, __comp);
        __last = __cut;
    }
}

} // namespace stlpmtx_std

// ─── BandOffline ─────────────────────────────────────────────────────────────

void BandOffline::Init() {
    DataRegisterFunc("make_charbudget", MakeCharClipBudget);
}

void BandOffline::Poll() {
    SystemPoll(false);
    TheSynth->Poll();
    ThePlatformMgr.Poll();
    TheNet.Poll();
    TheRockCentral.Poll();
    TheUI.Poll();
    TheTaskMgr.Poll();
    TheRnd->DoWorldBegin();
    TheUI.Draw();
    TheRnd->DoWorldEnd();
}

// ─── BandCharacter accessors (defined in this TU per the linker map) ─────────

const char *BandCharacter::GetGroupName() const {
    return mGroupName;
}

int BandCharacter::GetPlayFlags() const {
    return mPlayFlags;
}

// ─── GetStatKeeperIndex ──────────────────────────────────────────────────────

int GetStatKeeperIndex(const char *section) {
    static const char prefixes[] = "gjpr";
    return strchr(prefixes, (int)*section) - prefixes;
}

// ─── CharStatKeeper ──────────────────────────────────────────────────────────

CharStatKeeper::CharStatKeeper() : mByIntensity(), mByGroup() {}

CharStatKeeper::~CharStatKeeper() {}

void CharStatKeeper::OnPoll(int intensity, String groupName, float dt) {
    int key = intensity & 0x7F000;
    mByIntensity[key] += dt;
    mByGroup[groupName] += dt;
}

typedef std::map<int, float, std::less<int>,
        STLPORT::StlNodeAlloc<std::pair<const int, float> > > IntFloatMap;
typedef std::map<String, float, std::less<String>,
        STLPORT::StlNodeAlloc<std::pair<const String, float> > > StringFloatMap;

void CharStatKeeper::AddEq(const CharStatKeeper &c) {
    for (IntFloatMap::const_iterator it = c.mByIntensity.begin();
         it != c.mByIntensity.end(); ++it) {
        mByIntensity[it->first] += it->second;
    }
    for (StringFloatMap::const_iterator it = c.mByGroup.begin();
         it != c.mByGroup.end(); ++it) {
        mByGroup[it->first] += it->second;
    }
}

void CharStatKeeper::MaxEq(const CharStatKeeper &c) {
    for (IntFloatMap::const_iterator it = c.mByIntensity.begin();
         it != c.mByIntensity.end(); ++it) {
        float &cur = mByIntensity[it->first];
        if (it->second > cur)
            cur = it->second;
    }
    for (StringFloatMap::const_iterator it = c.mByGroup.begin();
         it != c.mByGroup.end(); ++it) {
        float &cur = mByGroup[it->first];
        if (it->second > cur)
            cur = it->second;
    }
}

void CharStatKeeper::ScaleEq(float scale) {
    for (IntFloatMap::iterator it = mByIntensity.begin();
         it != mByIntensity.end(); ++it) {
        it->second *= scale;
    }
    for (StringFloatMap::iterator it = mByGroup.begin();
         it != mByGroup.end(); ++it) {
        it->second *= scale;
    }
}

// ─── MakeCharClipBudget ──────────────────────────────────────────────────────

#pragma push
#pragma dont_inline on
DataNode BandOffline::MakeCharClipBudget(DataArray *da) {
    // Function-local static: instrument instance names (4 chars).
    // Compiled to @LOCAL@MakeCharClipBudget__11BandOfflineFP9DataArray@insts@2
    static const char *insts[] = { "george", "john", "paul", "ringo" };

    // Open the log file
    TextFileStream log(OptionStr("budget_log", "charclipbudget_log.csv"), false);

    // Kick off content refresh; poll until complete
    TheContentMgr->StartRefresh();
    while (!TheContentMgr->RefreshDone()) {
        BandOffline::Poll();
    }

    // Create the scripting controller object, name it "cb" in the main dir.
    Hmx::Object *charsObj = Hmx::Object::New<Hmx::Object>();
    charsObj->SetName("cb", ObjectDir::Main());
    charsObj->SetType(Symbol("charbudget"));

    // Load the song list config
    DataArray *songArr = SystemConfig(Symbol("charbudget_songs"));

    // Outer loop: iterate over the 3 tempo sections.
    // Local stack array — MWCC emits a rodata pointer-table (@54615) and
    // copies the 3 entries to stack at function entry.
    const char *sSections[3] = { "slow", "medium", "fast" };
    int sectionCount = 0;
    const char **pSection = sSections;
    do {
        // ── Per-section: 12 outer accumulators (4 chars × 3 purposes) ──
        // Declared inside the loop so they're ctor/dtor'ed each iteration.
        // Target stack layout: addSk (0x3d8..0x468), maxSk (0x318..0x3a8), normSk (0x258..0x2e8)
        // Plain arrays: first-declared = LOWEST address; [0] = lowest within array.
        CharStatKeeper addSk[4];
        CharStatKeeper maxSk[4];
        CharStatKeeper normSk[4];

        // Song-timing vector (unnamed allocator temp — destroyed right after ctor)
        std::vector<std::pair<Symbol, float>, unsigned short,
                    STLPORT::StlNodeAlloc<std::pair<Symbol, float> > > songTimes(
            STLPORT::StlNodeAlloc<std::pair<Symbol, float> >());

        Symbol sectionSym(*pSection);
        float totalTime = 0.0f;

        // Inner loop: iterate over the song config array
        int songIdx = 1;
        while (songIdx < songArr->Size()) {
            Symbol songSym(songArr->Sym(songIdx));

            // Query tempo section of this song
            {
                DataNode songNode(songSym);
                Message tempoMsg(Symbol("get_song_tempo"), songNode);
                Symbol tempoSym(charsObj->HandleType(tempoMsg).Sym(NULL));
                if (tempoSym != sectionSym) {
                    ++songIdx;
                    continue;
                }
            }

            // Log progress
            TheDebug << MakeString("Loading %s\n", songSym);

            // ── 4 per-song inner stat keepers ──
            // Created after "Loading" print, at 0x198..0x228(r1)
            CharStatKeeper perSk[4];
            float songDt = 0.0f;

            // Start the session for this song
            {
                DataNode songNode2(songSym);
                Message runMsg(Symbol("run_session"), songNode2);
                charsObj->HandleType(runMsg);
            }

            // Wait until UI transition completes
            while (TheUI.InTransition()) {
                BandOffline::Poll();
            }

            // 4 BandCharacter pointers (enumerated once when session goes active)
            BandCharacter *chars[4];
            chars[0] = 0;

            // Poll until session completes
            int isDone = 0;
            do {
                BandOffline::Poll();

                // Function-local static Message: "is_active"
                static Message sIsActiveMsg(Symbol("is_active"));

                int isActive = charsObj->HandleType(sIsActiveMsg).Int(NULL);

                if (isActive) {
                    if (chars[0] == 0) {
                        // Enumerate the 4 band characters via ObjDirItr
                        static Message sCharsDirMsg(Symbol("chars_dir"));
                        DataNode charsDirResult = TheBandDirector->Handle(sCharsDirMsg, true);
                        ObjectDir *dir = dynamic_cast<ObjectDir *>(charsDirResult.GetObj(NULL));
                        int byteOff = 0;
                        ObjDirItr<BandCharacter> it(dir, true);
                        while (it) {
                            *((BandCharacter **)((char *)chars + byteOff)) = (BandCharacter *)it;
                            byteOff += 4;
                            ++it;
                        }
                    }

                    // Accumulate delta time and stats for all 4 chars
                    songDt += TheTaskMgr.DeltaSeconds();
                    for (int ci = 0; ci < 4; ci++) {
                        BandCharacter *bc = chars[ci];
                        String grp(bc->mGroupName);
                        float dt = TheTaskMgr.DeltaSeconds();
                        int flags = bc->mPlayFlags;
                        CharStatKeeper *sk = (CharStatKeeper *)((char *)&perSk[0] + GetStatKeeperIndex(bc->Name()) * 0x30);
                        sk->OnPoll(flags, grp, dt);
                    }
                }

                // Function-local static Message: "is_done"
                static Message sIsDoneMsg(Symbol("is_done"));

                isDone = charsObj->HandleType(sIsDoneMsg).Int(NULL);

            } while (!isDone);

            // Record song timing
            std::pair<Symbol, float> songEntry(songSym, songDt);
            songTimes.push_back(songEntry);

            // Fold per-song stats into section accumulators using pointer stepping
            CharStatKeeper *addSkP = &addSk[0];
            CharStatKeeper *maxSkP = &maxSk[0];
            CharStatKeeper *normSkP = &normSk[0];
            CharStatKeeper *perSkP = &perSk[0];
            for (int ci = 0; ci < 4; ci++) {
                addSkP->AddEq(*perSkP);
                maxSkP->MaxEq(*perSkP);
                float invDt = 1.0f / songDt;
                perSkP->ScaleEq(invDt);
                normSkP->MaxEq(*perSkP);
                addSkP = (CharStatKeeper *)((char *)addSkP + 0x30);
                maxSkP = (CharStatKeeper *)((char *)maxSkP + 0x30);
                normSkP = (CharStatKeeper *)((char *)normSkP + 0x30);
                perSkP = (CharStatKeeper *)((char *)perSkP + 0x30);
            }
            totalTime += songDt;

            ++songIdx;
        }

        // Compute per-song average factor
        int nSongs = (int)songTimes.size();
        float avgFactor;
        if (nSongs != 0)
            avgFactor = 1.0f / (float)nSongs;
        else
            avgFactor = 0.0f;
        float normTotal = totalTime * avgFactor;

        // ── CSV output: song duration block ──
        log << "\nTempo,Section,Song,Duration (s)\n";
        for (int si = 0; si < nSongs; si++) {
            std::pair<Symbol, float> &entry = songTimes[si];
            log << sectionSym << "," << "songs" << ","
                << entry.first.Str() << "," << entry.second << "\n";
        }

        // ── CSV output: animation stats block ──
        log << "\nTempo,Section,Category,Animation,Average (s),Max (s),Normalized Max (s)\n";

        const char **pInst = insts;
        CharStatKeeper *addSkArr = &addSk[0];
        CharStatKeeper *maxSkArr = &maxSk[0];
        CharStatKeeper *normSkArr = &normSk[0];
        for (int ci = 0; ci < 4; ci++) {
            addSkArr->ScaleEq(avgFactor);

            // Intensity rows
            for (IntFloatMap::iterator it = addSkArr->mByIntensity.begin();
                 it != addSkArr->mByIntensity.end(); ++it) {
                int intensity = it->first;
                float maxVal = maxSkArr->mByIntensity[intensity];
                float normVal = normTotal * normSkArr->mByIntensity[intensity];
                log << sectionSym << "," << *pInst
                    << "," << "intensities" << ","
                    << BandIntensityString(intensity) << ","
                    << it->second << "," << maxVal
                    << "," << normVal << "\n";
            }

            // Group rows: build vector, filter by is_valid_group, sort and output
            {
                std::vector<std::pair<String, float>, unsigned short,
                            STLPORT::StlNodeAlloc<std::pair<String, float> > > groupList(
                    STLPORT::StlNodeAlloc<std::pair<String, float> >());

                for (StringFloatMap::iterator it = addSkArr->mByGroup.begin();
                     it != addSkArr->mByGroup.end(); ++it) {
                    String grpName(it->first);
                    DataNode nameNode(grpName.c_str());
                    Message validMsg(Symbol("is_valid_group"), nameNode);
                    if (charsObj->HandleType(validMsg).Int(NULL)) {
                        std::pair<String, float> grpEntry(*it);
                        groupList.push_back(grpEntry);
                    }
                }

                SortGroupPolls cmp;
                std::sort(groupList.begin(), groupList.end(), cmp);

                for (int gi = 0; gi < (int)groupList.size(); gi++) {
                    std::pair<String, float> &grpEntry = groupList[gi];
                    String &grpName = grpEntry.first;
                    float maxVal = maxSkArr->mByGroup[grpName];
                    float normVal = normTotal * normSkArr->mByGroup[grpName];
                    log << sectionSym << "," << *pInst
                        << "," << "groups" << ","
                        << grpName.c_str() << "," << grpEntry.second
                        << "," << maxVal << "," << normVal
                        << "\n";
                }
            }

            ++pInst;
            addSkArr = (CharStatKeeper *)((char *)addSkArr + 0x30);
            maxSkArr = (CharStatKeeper *)((char *)maxSkArr + 0x30);
            normSkArr = (CharStatKeeper *)((char *)normSkArr + 0x30);
        }

        sectionCount++;
        ++pSection;
    } while (sectionCount < 3);

    log << "\nDone\n";

    if (charsObj != nullptr) {
        delete charsObj;
    }

    TheDebug.Exit(0, true);
    return DataNode(1);
}
#pragma pop
