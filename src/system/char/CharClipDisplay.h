#pragma once
#include "obj/Object.h"
#include "char/CharClip.h"
#include "math/Color.h"
#include "math/Vec.h"

class MsgSource;

/** Debug timeline row for one playing CharClip (drawn by CharDriver's
    debug_draw). No Bank 5 DWARF exists for this class - all member names are
    semantic, derived from Bank 8 usage. */
class CharClipDisplay { // size 0x68
public:
    CharClipDisplay()
        : mClip(0), mStartBeat(0), mEndBeat(0), mViewStartBeat(0), mViewEndBeat(0),
          mTextWidth(0), mY(0), mBeat(0), mBlendFrac(0), mX(0) {
        mText[0] = 0;
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

    /** the clip this row displays (SetClip) */
    CharClip *mClip; // 0x0
    float mStartBeat; // 0x4 - clip's StartBeat
    float mEndBeat; // 0x8 - clip's EndBeat
    /** first beat of the zoomed view window (SetStartEnd) */
    float mViewStartBeat; // 0xc
    /** last beat of the zoomed view window */
    float mViewEndBeat; // 0x10
    /** pixel width of the name label (+1 em), set by SetText */
    float mTextWidth; // 0x14
    float mY; // 0x18 - screen Y of this row (set by CharDriver)
    /** current playback beat, drawn as the cursor (CharClipDriver::mBeat) */
    float mBeat; // 0x1c
    /** blend fraction shown next to the cursor (CharClipDriver::mBlendFrac) */
    float mBlendFrac; // 0x20
    char mText[64]; // 0x24 - name label
    float mX; // 0x64 - left origin X of the row
};
