#include "char/CharClipDisplay.h"
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