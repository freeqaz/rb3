#pragma once
#include "obj/Object.h"
#include "char/CharClip.h"
#include "math/Color.h"
#include "math/Vec.h"

class MsgSource;

class CharClipDisplay { // size 0x68
public:
    CharClipDisplay()
        : unk0(0), unk4(0), unk8(0), unkc(0), unk10(0), unk14(0), unk18(0), unk1c(0),
          unk20(0), unk64(0) {
        unk24[0] = 0;
    }

    static MsgSource *FindSource(Hmx::Object *);
    void SetClip(CharClip *, bool);
    void SetText(const char *);
    void SetStartEnd(float, float, bool);
    float GetX(float) const;
    void GetXY(Vector2 &, float) const;
    void DrawBeatString(const char *, float, const Hmx::Color &);
    void DrawBeatString(float, const Hmx::Color &);
    void DrawTrack();
    void DrawCursor();
    void DrawBlend(float, float);

    static void Init(ObjectDir *);
    static float LineSpacing();

    static float sZoom;
    static float sEm;
    static ObjectDir *sDir;

    CharClip *unk0;
    float unk4;
    float unk8;
    float unkc;
    float unk10;
    float unk14;
    float unk18;
    float unk1c;
    float unk20;
    char unk24[64];
    float unk64;
};
