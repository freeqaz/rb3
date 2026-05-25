#include "char/CharClipDisplay.h"
#include "char/CharBones.h"
#include "char/CharIKFoot.h"
#include "math/Geo.h"
#include "rndobj/Rnd.h"
#include "obj/Msg.h"
#include "os/Debug.h"
#include <cmath>

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
    unk0 = clip;
    SetText(clip->Name());
    SetStartEnd(clip->StartBeat(), clip->EndBeat(), b);
}

void CharClipDisplay::SetText(const char *text) {
    strcpy(unk24, text);
    float drawWidth = TheRnd->DrawString(text, Vector2(0, 0), Hmx::Color(1.0f, 0.0f, 0.0f), false).x;
    unk14 = drawWidth + sEm;
}

float CharClipDisplay::LineSpacing() { return sEm * 2.0f; }

float CharClipDisplay::GetX(float beat) const {
    float startBeat = unkc;
    float endBeat = unk10;
    float beatRange = (endBeat > startBeat) ? (endBeat - startBeat) : 1.0f;
    float leftMargin = sEm * 3.0f;
    float paddingPlusText = unk64 + unk14;
    float textWidth = paddingPlusText + leftMargin;
    return (beat - startBeat) * ((TheRnd->Width() - leftMargin) - textWidth) / beatRange + textWidth;
}

void CharClipDisplay::GetXY(Vector2 &out, float beat) const {
    float drawY = unk18;
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

void CharClipDisplay::DrawBeatString(float beat, const Hmx::Color &color) {
    const char *text;
    if (beat == (float)std::floor(beat)) {
        text = MakeString("%d", (int)beat);
    } else {
        text = MakeString("%.2f", beat);
    }
    DrawBeatString(text, beat, color);
}

#pragma push
#pragma fp_contract off
__declspec(noinline) void
CharClipDisplay::SetStartEnd(float start, float end, bool resetZoom) {
    unk4 = start;
    unk8 = end;
    unkc = start;
    unk10 = end;
    float zoomRange = 16.0f / sZoom;
    if (resetZoom) {
        float margin = sEm * 3.0f;
        int width = TheRnd->Width();
        float textOffset = unk64 + unk14 + margin;
        unkc = unk1c - (((float)width * 0.5f - textOffset) * zoomRange) / (float)width;
        unk10 = ((((float)width - margin) - textOffset) * zoomRange)
                / (float)TheRnd->Width() + unkc;
        GetX(unk1c);
    } else {
        // (float)(double)x casts force CW to emit `frsp` (single-precision round)
        // here, matching the original binary's else-branch instruction sequence.
        float fstart = (float)(double)start;
        float fend = (float)(double)end;
        if (fend - fstart > zoomRange) {
            float cursor = unk1c;
            float halfZoom = zoomRange * 0.5f;
            if (cursor < halfZoom + start) {
                unk10 = zoomRange + start;
            } else {
                if (cursor > end - halfZoom) {
                    unkc = end - zoomRange;
                    return;
                }
                unkc = cursor - halfZoom;
                unk10 = halfZoom + cursor;
            }
        } else {
            if (end != start) {
                return;
            }
            unkc = start - zoomRange * 0.5f;
            unk10 = zoomRange * 0.5f + end;
        }
    }
}
#pragma pop

void CharClipDisplay::DrawBlend(float beat, float weight) {
    Hmx::Rect rect(0.0f, unk18 + 1.0f, 0.0f, 2.0f);
    rect.x = GetX(beat);
    rect.w = GetX(beat + weight) - rect.x;
    Hmx::Color blendColor(0.0f, 0.0f, 1.0f, 0.4f);
    TheRnd->DrawRect(rect, blendColor, NULL, NULL, NULL);
    rect.y = unk18 - 1.0f;
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

    float drawY = unk18;
    float startBeat = unk4;
    float nameY = -(sEm * 0.5f - drawY);
    if (unkc >= unk4) startBeat = unkc;
    float endBeat = unk8;
    if (unk10 < unk8) endBeat = unk10;

    float startX = GetX(startBeat);
    float endX = GetX(endBeat);
    Hmx::Rect trackRect(startX, drawY, endX - startX, 3.0f);
    TheRnd->DrawRect(trackRect, white, NULL, NULL, NULL);

    float firstBeat = (float)std::ceil(startBeat);
    float lastBeat = (float)std::floor(endBeat);
    if (firstBeat + 1.0f != firstBeat && firstBeat <= lastBeat) {
        float markerY = drawY - 3.0f;
        float markerH = 9.0f;
        float beat = firstBeat;
        do {
            Hmx::Rect markerRect(GetX(beat), markerY, 1.0f, markerH);
            TheRnd->DrawRect(markerRect, green, NULL, NULL, NULL);
            beat += 1.0f;
        } while (beat <= lastBeat);
    }

    if (unk0 == NULL)
        goto drawName;

    {
        bool firstEvent = true;
        int idx = 0;
        float eventLabelOffset = 10.0f;
        if (unk0->NumBeatEvents() != 0) {
            float eventAlpha = 0.2f;
            do {
                const CharClip::BeatEvent &ev = unk0->mBeatEvents[idx];
                float eventX = GetX(ev.beat);
                float halfEmVal = sEm * 0.5f;
                Hmx::Rect eventRect(eventX, drawY - halfEmVal, halfEmVal, 1.0f);
                Hmx::Color eventColor(eventAlpha, eventAlpha, 1.0f, 1.0f);
                TheRnd->DrawRect(eventRect, eventColor, NULL, NULL, NULL);

                if (firstEvent
                    && (ev.beat > unk1c
                        || (idx == 0
                            && unk1c > unk0->mBeatEvents.back().beat))) {
                    Hmx::Color eventLabelColor(eventAlpha, eventAlpha, 1.0f, 1.0f);
                    firstEvent = false;
                    float labelY = drawY - (halfEmVal + eventLabelOffset);
                    TheRnd->DrawString(
                        ev.event.Str(),
                        Vector2(eventX, labelY),
                        eventLabelColor,
                        true
                    );
                }
                idx += 1;
            } while ((unsigned int)idx < (unsigned int)unk0->NumBeatEvents());
        }
    }

    {
        CharIKFoot *leftIk = sDir->Find<CharIKFoot>("left.ikfoot", false);
        CharIKFoot *rightIk = sDir->Find<CharIKFoot>("right.ikfoot", false);

        if (leftIk == NULL) {
            if (rightIk == NULL)
                goto sampleDraw;
            else {
                RndTransformable *data = rightIk->mData;
                goto drawIKData;
            }
        } else {
            MILO_ASSERT(
                !rightIk || !leftIk || (rightIk->mData == leftIk->mData), 0xCB
            );
            RndTransformable *data = leftIk->mData;
        drawIKData:
            if (data != NULL) {
                Symbol channelName
                    = CharBones::ChannelName(data->Name(), CharBones::TYPE_POS);
                void *channel = unk0->GetChannel(channelName);
                Vector3 channelData;
                Hmx::Color ikColor(1.0f, 0.0f, 0.0f, 1.0f);
                Hmx::Rect ikRect(0.0f, drawY, 1.0f, 1.0f);
                int firstFrame = (int)(float)std::ceil(unk0->BeatToFrame(startBeat));
                int lastFrame = (int)(float)std::floor(unk0->BeatToFrame(endBeat));
                for (int frame = firstFrame; frame <= lastFrame; frame++) {
                    float frameBeat = unk0->FrameToBeat((float)frame);
                    unk0->EvaluateChannel(&channelData, channel, frameBeat);
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
            int startSample = unk0->BeatToSample(startBeat, &frac);
            int endSample = unk0->BeatToSample(endBeat, &frac);
            for (; startSample <= endSample; startSample++) {
                float sampleBeat = unk0->SampleToBeat(startSample);
                sampleRect.x = GetX(sampleBeat);
                TheRnd->DrawRect(sampleRect, black, NULL, NULL, NULL);
            }
        }
    afterSampleDraw:

        DrawBeatString(firstBeat, green);
        DrawBeatString(lastBeat, green);

        {
            float labelX = -((sEm * 2.0f) - (sEm * 3.0f + unk64 + unk14));
            Vector2 startPos(labelX, nameY);
            TheRnd->DrawString(MakeString("%.1f", unk4), startPos, white, true);
        }

        {
            float screenWidth = (float)TheRnd->Width();
            float labelX = -(sEm * 3.0f - screenWidth);
            Vector2 endPos(labelX, nameY);
            TheRnd->DrawString(MakeString("%.1f", unk8), endPos, white, true);
        }
    }

drawName:
    {
        Hmx::Color nameColor(1.0f, 1.0f, 1.0f, 1.0f);
        Vector2 namePos(unk64 + sEm, nameY);
        TheRnd->DrawString(unk24, namePos, nameColor, true);
    }
}

void CharClipDisplay::DrawCursor() {
    Hmx::Color yellow(1.0f, 1.0f, 0.0f, 1.0f);
    Vector2 v;
    GetXY(v, unk1c);
    Hmx::Rect rect(v.x, v.y - 3.0f, 1.0f, 9.0f);
    TheRnd->DrawRect(rect, yellow, NULL, NULL, NULL);
    const char *text;
    if (unk20 < 1.0f) {
        text = MakeString("%.1f", unk1c);
    } else {
        text = MakeString("%.1f (%.2f)", unk1c, unk20);
    }
    DrawBeatString(text, unk1c, yellow);
}
