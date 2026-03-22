#include "char/ClipDistMap.h"
#include "char/CharBoneDir.h"
#include "char/CharBonesMeshes.h"
#include "char/CharUtl.h"
#include "decomp.h"
#include "math/Utl.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "rndobj/Trans.h"

void FindWeights(
    std::vector<RndTransformable *> &transes,
    std::vector<float> &floats,
    const DataArray *arr
) {
    floats.resize(transes.size());
    float f1 = 0;
    for (int i = 0; i < transes.size(); i++) {
        float len = Length(transes[i]->mLocalXfm.v);
        if (arr) {
            float f84 = 1;
            arr->FindData(transes[i]->Name(), f84, false);
            len *= f84;
        }
        floats[i] = len;
        f1 += floats[i];
    }
    for (int i = 0; i < floats.size(); i++) {
        floats[i] *= floats.size() / f1;
    }
}

ClipDistMap::ClipDistMap(
    CharClip *clip1, CharClip *clip2, float f1, float f2, int i, const DataArray *a
)
    : mClipA(clip1), mClipB(clip2), mWeightData(a), mSamplesPerBeat(8),
      mLastMinErr(kHugeFloat), mBeatAlign(f1), mBeatAlignOffset(0), mBlendWidth(f2),
      mNumSamples(i) {
    int height = CalcHeight();
    int width = CalcWidth();
    mDists.mWidth = 0;
    mDists.mHeight = 0;
    mDists.mData = 0;
    delete[] mDists.mData;
    mDists.mWidth = width;
    mDists.mHeight = height;
    mDists.mData = (float *)new uint[width * height];

    mBeatAlignPeriod = (int)((double)((float)mSamplesPerBeat * mBeatAlign) + 0.5);

    if (mBeatAlignPeriod != 0) {
        float zero = 0.0f;
        float negB = zero - mBStart;
        float negA = zero - mAStart;
        int diff = (int)(negB * mSamplesPerBeat) - (int)(negA * mSamplesPerBeat);
        if (diff != 0) {
            diff = diff % mBeatAlignPeriod;
            if (diff < 0)
                diff += mBeatAlignPeriod;
        }
        mBeatAlignOffset = diff;
    }
}

void ClipDistMap::Array2d::Resize(int w, int h) {
    delete mData;
    mWidth = w;
    mHeight = h;
    mData = (float *)new uint[h * w];
}

DECOMP_FORCEACTIVE(ClipDistMap, "bone_facing.rotz")

#pragma push
#pragma dont_inline on
void ClipDistMap::FindDists(float f1, DataArray *arr) {
    CharBoneDir *rsrcA = mClipA->GetResource();
    CharUtlBoneSaver saver(rsrcA);
    CharBonesMeshes meshes;
    meshes.SetName("tmp_bones", rsrcA);
    rsrcA->StuffBones(meshes, mClipA->GetContext());
    std::vector<RndTransformable *> transes;
    for (ObjDirItr<RndTransformable> it(rsrcA, true); it != nullptr; ++it) {
        if (strnicmp(it->Name(), "bone_", 4) == 0) {
            transes.push_back(&*it);
        }
    }
    mClipA->GetChannel("bone_facing.rotz");

    DataNode &dataVarABeat = DataVariable("a_beat");
    float varABeat = dataVarABeat.Float();
    DataNode &dataVarBBeat = DataVariable("b_beat");
    float varBBeat = dataVarBBeat.Float();
    DataNode &dataVarAStart = DataVariable("a_start");
    float varAStart = dataVarAStart.Float();
    DataNode &dataVarAEnd = DataVariable("a_end");
    float varAEnd = dataVarAEnd.Float();
    DataNode &dataVarBStart = DataVariable("b_start");
    float varBStart = dataVarBStart.Float();
    DataNode &dataVarBEnd = DataVariable("b_end");
    float varBEnd = dataVarBEnd.Float();
    DataNode &dataVarAMiddle = DataVariable("a_middle");
    float varAMiddle = dataVarAMiddle.Float();
    DataNode &dataVarBMiddle = DataVariable("b_middle");
    float varBMiddle = dataVarBMiddle.Float();
    DataNode &dataVarDelta = DataVariable("delta");
    float varDelta = dataVarDelta.Float();

    std::vector<DistEntry> distEntries;
    distEntries.resize(mDists.Height());
    std::vector<float> floatVec;
    float interpA = Interp(mClipA->StartBeat(), mClipA->EndBeat(), 0.5f);
    float interpB = Interp(mClipB->StartBeat(), mClipB->EndBeat(), 0.5f);
    mWorstErr = 0;

    for (int i = 0; i < mDists.Width(); i++) {
        float beatA = BeatA(i);
        DistEntry newDistEntry;
        for (int j = 0; j < mDists.Height(); j++) {
            mDists(i, j) = kHugeFloat;
            float beatB = BeatB(j);
            if (mBeatAlign == 0 || BeatAligned(i, j)) {
                if (arr) {
                    dataVarABeat = beatA;
                    dataVarBBeat = beatB;
                    dataVarAStart = beatA - mClipA->StartBeat();
                    dataVarAEnd = mClipA->EndBeat() - beatA;
                    dataVarBStart = beatB - mClipB->StartBeat();
                    dataVarBEnd = mClipB->EndBeat() - beatB;
                    dataVarAMiddle = beatA - interpA;
                    dataVarBMiddle = beatB - interpB;
                    dataVarDelta = beatA - beatB;
                    if (arr->Evaluate(1).Int() == 0)
                        continue;
                }

                //     DistEntry::DistEntry(aDStack_240);
                //         pDVar11 =
                //         stlpmtx_std::vector<><>::operator_[](distEntries,uVar9); if
                //         (param_1 > 0.0) {
                //           pvVar17 = avStack_23c; the vector in the distentry
                //           fVar1 = 0.3333333;
                //           iVar15 = 1;
                //           fVar2 = local_234[0]; start of facing in distentry
                //           pDVar3 = pDVar11;
                //           do {
                //             dVar28 = fVar1;
                //             dVar26 = LimitAng(*(pvVar17 + 0xc) - *(pvVar17 + 8));
                //             dVar27 = LimitAng(*(pDVar3 + 0x10) - *(pDVar3 + 0xc));
                //             iVar15 = iVar15 + 1;
                //             fVar1 = dVar28 + 0.3333333432674408;
                //             pvVar17 = pvVar17 + 4;
                //             fVar2 = fVar2 + (1.0 - dVar28) * dVar26 + dVar28 * dVar27;
                //             pDVar3 = pDVar3 + 4;
                //           } while (iVar15 < 4);
                //           this_11 = LimitAng(fVar2 - *(pDVar11 + 0x18));
                //           dVar26 = std::fabs(this_11);
                //           if (param_1 < dVar26) {
                //             pfVar10 = Array2d::operator_()(this + 0x38,iVar4,uVar9);
                //             *pfVar10 = 1e+30;
                //             goto LAB_8072e548;
                //           }
                //         }
                DistEntry &curDistEntry = distEntries[j];
                GenerateDistEntry(meshes, curDistEntry, BeatB(j), mClipB, transes);
                GenerateDistEntry(meshes, newDistEntry, BeatA(i), mClipA, transes);
                if (f1 > 0) {
                    float fvar1 = 0.33333334f;
                    float fvar2 = newDistEntry.facing[0];
                    // some more stuff happens here
                }
                if (floatVec.empty()) {
                    FindWeights(transes, floatVec, mWeightData);
                }
                float f314 = 0;
                for (int k = 0; k < newDistEntry.bones.size(); k++) {
                    float curFloat = floatVec[k % floatVec.size()];
                    f314 += DistanceSquared(newDistEntry.bones[k], curDistEntry.bones[k])
                        * curFloat;
                }
                f314 = std::sqrt(f314 / (float)newDistEntry.bones.size());
                MaxEq(mWorstErr, f314);
                mDists(i, j) = f314;
            }
        }
    }
    dataVarABeat = varABeat;
    dataVarBBeat = varBBeat;
    dataVarAStart = varAStart;
    dataVarAEnd = varAEnd;
    dataVarBStart = varBStart;
    dataVarBEnd = varBEnd;
    dataVarAMiddle = varAMiddle;
    dataVarBMiddle = varBMiddle;
    dataVarDelta = varDelta;
}
#pragma pop

bool ClipDistMap::LocalMin(int col, int row) {
    int width = mDists.mWidth;
    float val = mDists(col, row);
    if (val == kHugeFloat) {
        return false;
    }
    if (mBeatAlign == 0.0f || !BeatAligned(col, row)) {
        goto nested_loop;
    }
    if (col - 1 >= 0 && row - 1 >= 0 && mDists(col - 1, row - 1) < val) {
        return false;
    }
    if (col + 1 < width && row + 1 < mDists.mHeight && mDists(col + 1, row + 1) < val) {
        return false;
    }
    return true;
nested_loop:
    for (int c = col - 1; c < col + 2; c++) {
        for (int r = row - 1; r < row + 2; r++) {
            if ((c != col || r != row) && c >= 0 && c < width && r >= 0 &&
                r < mDists.mHeight) {
                float neighbor = mDists(c, r);
                if (neighbor != kHugeFloat && neighbor < val)
                    return false;
            }
        }
    }
    return true;
}

bool ClipDistMap::FindBestNode(float maxError, float startBeat, float endBeat, Node &node) {
    if (startBeat >= endBeat) {
        return false;
    }
    node.err = maxError;
    float clipAStart = mAStart;
    int startCol = (int)((startBeat - mAStart) * mSamplesPerBeat);
    startCol = startCol & ~(startCol >> 31);
    int endCol = (int)((endBeat - mAStart) * mSamplesPerBeat);
    int maxCol = endCol;
    if (mDists.mWidth < endCol) {
        maxCol = mDists.mWidth;
    }
    while (startCol < maxCol) {
        float curBeat = clipAStart + (float)startCol / (float)mSamplesPerBeat;
        int rowIdx = mDists.mHeight - 1;
        if (rowIdx >= 0) {
            int rowCount = rowIdx + 1;
            do {
                float cellError = mDists(startCol, rowIdx);
                bool foundBetter = cellError < node.err;
                if (foundBetter) node.err = cellError;
                if (foundBetter) {
                    node.curBeat = curBeat;
                    node.nextBeat = mBStart + (float)rowIdx / (float)mSamplesPerBeat;
                }
                rowIdx--;
                rowCount--;
            } while (rowCount != 0);
        }
        startCol++;
    }
    return node.err < maxError;
}

void ClipDistMap::FindBestNodeRecurse(
    float minDist, float searchRadius, float minGap, float startBeat, float endBeat
) {
    MILO_ASSERT(minDist > 0, 621);
    if (!(endBeat - startBeat > searchRadius))
        return;

    float searchEnd = searchRadius + startBeat + minGap;
    if (endBeat >= searchEnd)
        searchEnd = endBeat;

    float searchStart = (endBeat - minGap) - searchRadius;
    if (startBeat >= searchStart)
        searchStart = startBeat;

    Node node;
    if (!FindBestNode(minDist, searchStart, searchEnd, node))
        return;

    float curBeat = node.curBeat;
    unsigned int count = mNodes.size();
    unsigned int i = 0;
    for (; i < count; i++) {
        if (mNodes[i].curBeat == curBeat)
            goto skip;
    }
    mNodes.push_back(node);
skip:;
    FindBestNodeRecurse(minDist, searchRadius, minGap, curBeat + searchRadius, endBeat);
    FindBestNodeRecurse(minDist, searchRadius, minGap, startBeat, curBeat - searchRadius);
}

void ClipDistMap::FindNodes(float maxError, float maxDist, float endDist) {
    mNodes.clear();
    mLastMinErr = maxError;

    float searchRadius = maxDist * 0.45f;
    if (maxDist == 0.0f) {
        searchRadius = kHugeFloat;
        endDist = searchRadius;
    } else if (endDist == 0.0f) {
        endDist = maxDist;
    }

    FindBestNodeRecurse(maxError, searchRadius, maxDist - searchRadius * 2.0f, mAStart, mAEnd);

    std::sort(mNodes.begin(), mNodes.end(), DistMapNodeSort());

    if (!mNodes.empty() && endDist > 0.0f) {
        float lastNodeDist = mAEnd - mNodes.back().curBeat;
        if (lastNodeDist > endDist) {
            Node node;
            if (FindBestNode(maxError, mAEnd - endDist, mAEnd, node)) {
                mNodes.push_back(node);
                std::sort(mNodes.begin(), mNodes.end(), DistMapNodeSort());
            }
        }
    }

    for (int i = 1; i < (int)mNodes.size() - 1; ) {
        float dist = mNodes[i + 1].curBeat - mNodes[i - 1].curBeat;
        if (dist < maxDist) {
            mNodes.erase(mNodes.begin() + i--);
        }
        i++;
    }
}

bool ClipDistMap::BeatAligned(int i1, int i2) {
    int l1 = i1 - i2;
    int l2 = mBeatAlignPeriod;
    if (l2 == 0) {
        l1 = 0;
    } else {
        l1 = l1 % l2;
        if (l1 < 0) {
            l1 += l2;
        }
    }
    return mBeatAlignOffset == l1;
}

float ClipDistMap::BeatA(int i) { return mAStart + (float)i / (float)mSamplesPerBeat; }

float ClipDistMap::BeatB(int i) { return mBStart + (float)i / (float)mSamplesPerBeat; }

int ClipDistMap::CalcWidth() {
    float inv = 1.0f / (float)mSamplesPerBeat;
    float start = mClipA->StartBeat();
    float mod = Modulo(start, inv);
    float f1 = start - mod;
    mAStart = f1;
    if (mAStart < mClipA->StartBeat()) {
        mAStart += inv;
    }
    float end = mClipA->EndBeat();
    float mod2 = Modulo(end, inv);
    mAEnd = end - mod2;
    float next = mAEnd + inv;
    if (next <= mClipA->EndBeat()) {
        mAEnd = next;
    }
    float aStart = mAStart;
    int spb = mSamplesPerBeat;
    int width = Max(0, (int)(float)floor(((mAEnd - aStart) * (float)spb) + 0.5f)) + 1;
    mAEnd = aStart + (float)(width - 1) / (float)spb;
    return width;
}

int ClipDistMap::CalcHeight() {
    float inv = 1.0f / (float)mSamplesPerBeat;
    float start = mClipB->StartBeat();
    float mod = Modulo(start, inv);
    float f1 = start - mod;
    mBStart = f1;
    if (mBStart < mClipB->StartBeat()) {
        mBStart += inv;
    }
    CharClip *clipB = mClipB;
    float mod2 = Modulo(clipB->EndBeat(), inv);
    float end = clipB->EndBeat();
    float fVar = end - mod2;
    float next = fVar + inv;
    if (next <= clipB->EndBeat()) {
        fVar = next;
    }
    int res = (int)(float)floor(((fVar - mBStart) * (float)mSamplesPerBeat) + 0.5f);
    return Max(0, res) + 1;
}