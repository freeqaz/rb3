#include "char/CharClipDisplay.h"
#include "char/CharBones.h"
#include "char/CharIKFoot.h"
#include "math/Geo.h"
#include "rndobj/Rnd.h"
#include "obj/Msg.h"
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

__declspec(noinline) void
CharClipDisplay::SetStartEnd(float start, float end, bool resetZoom) {
    unk4 = start;
    unk8 = end;
    unkc = start;
    unk10 = end;
    float zoomRange = 16.0f / sZoom;
    if (resetZoom) {
        float margin = sEm * 3.0f;
        float screenWidth = (float)TheRnd->Width();
        float textOffset = unk64 + unk14 + margin;
        unkc = unk1c - ((screenWidth * 0.5f - textOffset) * zoomRange) / screenWidth;
        unk10 = (((screenWidth - margin) - textOffset) * zoomRange)
                / (float)TheRnd->Width() + unkc;
        GetX(unk1c);
    } else {
        if (end - start > zoomRange) {
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

void CharClipDisplay::DrawCursor() {
    Hmx::Color yellow(1.0f, 1.0f, 0.0f, 1.0f);
    Vector2 v;
    GetXY(v, unk1c);
    Hmx::Rect rect(v.x, v.y - 3.0f, 1.0f, 9.0f);
    TheRnd->DrawRect(rect, yellow, NULL, NULL, NULL);
    const char *text;
    if (unk20 >= 1.0f) {
        text = MakeString("%.1f (%.2f)", unk1c, unk20);
    } else {
        text = MakeString("%.1f", unk1c);
    }
    DrawBeatString(text, unk1c, yellow);
}