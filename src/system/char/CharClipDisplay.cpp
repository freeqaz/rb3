#include "char/CharClipDisplay.h"
#include "char/CharBones.h"
#include "char/CharIKFoot.h"
#include "math/Geo.h"
#include "rndobj/Rnd.h"
#include "obj/Msg.h"
#include "os/Debug.h"
#include <cmath>
// GetData() is CharIKFoot::mData accessor (method exists in target binary)
#define GetData() mData

float CharClipDisplay::sZoom;
float CharClipDisplay::sEm;
ObjectDir *CharClipDisplay::sDir;

void CharClipDisplay::Init(ObjectDir *dir) {
    sDir = dir;
    sEm = TheRnd->DrawString("", Vector2(0, 0), Hmx::Color(1.0f, 0.0f, 0.0f), false).y;
}

MsgSource *CharClipDisplay::FindSource(Hmx::Object *o) {
    for (ObjDirItr<MsgSource> it(ObjectDir::Main(), false); it != nullptr; ++it) {
        for (std::list<MsgSource::Sink>::iterator lit = it->mSinks.begin();
             lit != it->mSinks.end();
             ++lit) {
            if ((*lit).obj == o)
                return it;
        }
    }
    return 0;
}

void CharClipDisplay::SetClip(CharClip *clip, bool b) {
    mClip = clip;
    SetText(clip->Name());
    SetStartEnd(clip->StartBeat(), clip->EndBeat(), b);
}

void CharClipDisplay::SetText(const char *text) {
    strcpy(mText, text);
    float drawWidth = TheRnd->DrawString(text, Vector2(0, 0), Hmx::Color(1.0f, 0.0f, 0.0f), false).x;
    mTextWidth = drawWidth + sEm;
}

float CharClipDisplay::LineSpacing() { return sEm * 2.0f; }

float CharClipDisplay::GetX(float beat) const {
    float startBeat = mViewStartBeat;
    float endBeat = mViewEndBeat;
    float beatRange = (endBeat > startBeat) ? (endBeat - startBeat) : 1.0f;
    float leftMargin = sEm * 3.0f;
    float paddingPlusText = mX + mTextWidth;
    float textWidth = paddingPlusText + leftMargin;
    return (beat - startBeat) * ((TheRnd->Width() - leftMargin) - textWidth) / beatRange + textWidth;
}

void CharClipDisplay::GetXY(Vector2 &out, float beat) const {
    float drawY = mY;
    out.x = GetX(beat);
    out.y = drawY;
}

void CharClipDisplay::DrawBeatString(const char *c, float f1, const Hmx::Color &color) {
    Vector2 v;
    GetXY(v, f1);
    float posX = v.x - 4.0f;
    float posY = v.y - 18.0f;
    TheRnd->DrawString(c, Vector2(posX, posY), color, true);
}

#pragma push
#pragma fp_contract off
__declspec(noinline) void
CharClipDisplay::SetStartEnd(float start, float end, bool resetZoom) {
    mStartBeat = start;
    mEndBeat = end;
    mViewStartBeat = start;
    mViewEndBeat = end;
    float zoomRange = 16.0f / sZoom;
    if (resetZoom) {
        float margin = sEm * 3.0f;
        int width = TheRnd->Width();
        float textOffset = mX + mTextWidth + margin;
        mViewStartBeat = mBeat - (((float)width * 0.5f - textOffset) * zoomRange) / (float)width;
        mViewEndBeat = ((((float)width - margin) - textOffset) * zoomRange)
                / (float)TheRnd->Width() + mViewStartBeat;
        GetX(mBeat);
    } else {
        // (float)(double)x casts force CW to emit `frsp` (single-precision round)
        // here, matching the original binary's else-branch instruction sequence.
        float fstart = (float)(double)start;
        float fend = (float)(double)end;
        if (fend - fstart > zoomRange) {
            float cursor = mBeat;
            float halfZoom = zoomRange * 0.5f;
            if (cursor < halfZoom + start) {
                mViewEndBeat = zoomRange + start;
            } else {
                if (cursor > end - halfZoom) {
                    mViewStartBeat = end - zoomRange;
                    return;
                }
                mViewStartBeat = cursor - halfZoom;
                mViewEndBeat = halfZoom + cursor;
            }
        } else {
            if (end != start) {
                return;
            }
            mViewStartBeat = start - zoomRange * 0.5f;
            mViewEndBeat = zoomRange * 0.5f + end;
        }
    }
}
#pragma pop

void CharClipDisplay::DrawBlend(float beat, float weight) {
    Hmx::Rect rect(0, mY + 1.0f, 0.0f, 2.0f);
    rect.x = GetX(beat);
    rect.w = GetX(beat + weight) - rect.x;
    Hmx::Color blendColor(0.0f, 0.0f, 1.0f, 0.4f);
    TheRnd->DrawRect(rect, blendColor, NULL, NULL, NULL);
    rect.y = mY - 1.0f;
    rect.h = 4.0f;
    rect.w = 3.0f;
    rect.x = GetX(weight * 0.5f + beat) - 1.0f;
    Hmx::Color markerColor(0.0f, 0.0f, 1.0f, 1.0f);
    TheRnd->DrawRect(rect, markerColor, NULL, NULL, NULL);
}

void CharClipDisplay::DrawTrack() {
    Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
    Hmx::Color green(0.0f, 1.0f, 0.0f, 1.0f);
    Hmx::Color black(0.0f, 0.0f, 0.0f, 1.0f);
    Hmx::Color ikColor(1.0f, 0.0f, 0.0f, 1.0f);

    float drawY = mY;
    float startBeat = mStartBeat;
    float nameY = -(sEm * 0.5f - drawY);
    if (mViewStartBeat >= mStartBeat) startBeat = mViewStartBeat;
    float endBeat = mEndBeat;
    if (mViewEndBeat < mEndBeat) endBeat = mViewEndBeat;

    Hmx::Rect trackRect;
    trackRect.x = GetX(startBeat);
    trackRect.y = drawY;
    trackRect.w = GetX(endBeat) - trackRect.x;
    trackRect.h = 3.0f;
    TheRnd->DrawRect(trackRect, white, NULL, NULL, NULL);

    float beatStep = 1.0f;
    float firstBeat = beatStep * (float)std::ceil(startBeat / beatStep);
    float lastBeat = beatStep * (float)std::floor(endBeat / beatStep);
    float markerY = drawY - 3.0f;
    float markerH = 9.0f;
    float beat = firstBeat;
    while (beat <= lastBeat) {
        Hmx::Rect markerRect;
        markerRect.y = markerY;
        markerRect.h = markerH;
        markerRect.x = GetX(beat);
        markerRect.w = beatStep;
        TheRnd->DrawRect(markerRect, green, NULL, NULL, NULL);
        beat += beatStep;
    }

    if (mClip != NULL) {
        {
        float halfEmConst = 0.5f;
        bool firstEvent = true;
        float rectHeight = 1.0f;
        int idx = 0;
        float eventAlpha = 0.2f;
        int byteOffset = 0;
        float eventLabelOffset = 10.0f;
        while ((unsigned int)idx < (unsigned int)mClip->NumBeatEvents()) {
        const CharClip::BeatEvent &ev = mClip->mBeatEvents[idx];
        float eventX = GetX(ev.beat);
        Vector2 labelPos(eventX, drawY);
        float halfEmVal = sEm * halfEmConst;
        Hmx::Rect eventRect(eventX, drawY - halfEmVal, rectHeight, halfEmVal);
        Hmx::Color eventColor(eventAlpha, eventAlpha, 1.0f, 1.0f);
        TheRnd->DrawRect(eventRect, eventColor, NULL, NULL, NULL);

        if (firstEvent
        && (ev.beat > mBeat
        || (idx == 0
        && mBeat > mClip->mBeatEvents.back().beat))) {
        Hmx::Color eventLabelColor(eventAlpha, eventAlpha, 1.0f, 1.0f);
        firstEvent = false;
        labelPos.y -= (halfEmVal + eventLabelOffset);
        TheRnd->DrawString(
        ev.event.Str(),
        labelPos,
        eventLabelColor,
        true
        );
        }
        idx += 1;
        byteOffset += 8;
        }
        }

        {
        CharIKFoot *leftIk = sDir->Find<CharIKFoot>("left.ikfoot", false);
        CharIKFoot *rightIk = sDir->Find<CharIKFoot>("right.ikfoot", false);

        if (leftIk == NULL) {
        if (rightIk == NULL)
        goto sampleDraw;
        }
        MILO_ASSERT(
        !rightIk || !leftIk || (rightIk->GetData() == leftIk->GetData()), 0xCB
        );
        {
        RndTransformable *data;
        if (leftIk != NULL) data = leftIk->mData.mPtr;
        else data = rightIk->mData.mPtr;
        if (data != NULL) {
        Symbol channelName
        = CharBones::ChannelName(data->Name(), CharBones::TYPE_POS);
        void *channel = mClip->GetChannel(channelName);
        Vector3 channelData;
        Hmx::Rect ikRect;
        ikRect.y = drawY;
        ikRect.w = 1.0f;
        ikRect.h = 1.0f;
        float firstFrameF = (float)std::ceil(mClip->BeatToFrame(startBeat));
        int lastFrame = (int)(float)std::floor(mClip->BeatToFrame(endBeat));
        int firstFrame = (int)firstFrameF;
        for (int frame = firstFrame; frame <= lastFrame; frame++) {
        float frameBeat = mClip->FrameToBeat((float)frame);
        mClip->EvaluateChannel(&channelData, channel, frameBeat);
        if (leftIk != NULL
        && channelData[leftIk->mDataIndex] > 0.0f) {
        ikRect.x = GetX(frameBeat);
        TheRnd->DrawRect(ikRect, ikColor, NULL, NULL, NULL);
        }
        if (rightIk != NULL
        && channelData[rightIk->mDataIndex] > 0.0f) {
        ikRect.x = GetX(frameBeat);
        ikRect.y += 2.0f;
        TheRnd->DrawRect(ikRect, ikColor, NULL, NULL, NULL);
        ikRect.y -= 2.0f;
        }
        }
        }
        goto afterSampleDraw;
        }
        sampleDraw:
        {
        Hmx::Rect sampleRect(0.0f, drawY + 1.0f, 1.0f, 1.0f);
        float frac;
        int startSample = mClip->BeatToSample(startBeat, &frac);
        int endSample = mClip->BeatToSample(endBeat, &frac);
        for (; startSample <= endSample; startSample++) {
        float sampleBeat = mClip->SampleToBeat(startSample);
        sampleRect.x = GetX(sampleBeat);
        TheRnd->DrawRect(sampleRect, black, NULL, NULL, NULL);
        }
        }
        afterSampleDraw:

        DrawBeatString(firstBeat, green);
        DrawBeatString(lastBeat, green);

        {
        float labelX = -((sEm * 2.0f) - (sEm * 3.0f + mX + mTextWidth));
        Vector2 startPos(labelX, nameY);
        TheRnd->DrawString(MakeString("%.1f", mStartBeat), startPos, white, true);
        }

        {
        float screenWidth = (float)TheRnd->Width();
        float s2em = sEm * 2.0f;
        float labelX = screenWidth - s2em;
        Vector2 endPos(labelX, nameY);
        TheRnd->DrawString(MakeString("%.1f", mEndBeat), endPos, white, true);
        }
        }
    }
    {
        Hmx::Color nameColor(1.0f, 1.0f, 1.0f, 1.0f);
        Vector2 namePos(mX + sEm, nameY);
        TheRnd->DrawString(mText, namePos, nameColor, true);
    }
}

void CharClipDisplay::DrawBeatString(float beat, const Hmx::Color &color) {
    const char *text;
    if (beat == (float)std::floor(beat)) {
        text = MakeString("%d", (int)beat);
    } else {
        text = MakeString("%.1f", beat);
    }
    DrawBeatString(text, beat, color);
}

void CharClipDisplay::DrawCursor() {
    Hmx::Color yellow(1.0f, 1.0f, 0.0f, 1.0f);
    Vector2 v;
    GetXY(v, mBeat);
    Hmx::Rect rect(v.x, v.y - 3.0f, 1.0f, 9.0f);
    TheRnd->DrawRect(rect, yellow, NULL, NULL, NULL);
    const char *text;
    if (mBlendFrac < 1.0f) {
        text = MakeString("%.1f (%.2f)", mBeat, mBlendFrac);
    } else {
        text = MakeString("%.1f", mBeat);
    }
    DrawBeatString(text, mBeat, yellow);
}
