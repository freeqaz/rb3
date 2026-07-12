#include "char/CharDriverMidi.h"
#ifdef HX_NATIVE
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <set>
#endif
#include "obj/Msg.h"
#include "char/CharClip.h"
#include "char/CharClipGroup.h"
#include "char/CharClipDriver.h"
#include "utl/TimeConversion.h"
#include "obj/Task.h"
#include "utl/Symbols.h"

INIT_REVS(CharDriverMidi)

CharDriverMidi::CharDriverMidi()
    : mParser(), mFlagParser(), mClipFlags(0), mBlendOverridePct(1.0f) {
#ifdef HX_NATIVE
    { static const char *p = getenv("RB3_MIDIDRV_PROBE");
      if (p) fprintf(stderr, "[MIDIDRV_CTOR] CharDriverMidi created %p\n", (void *)this); }
#endif
}

void CharDriverMidi::Enter() {
    unk89 = true;
    CharDriver::Enter();
    MsgSource *msgParser =
        dynamic_cast<MsgSource *>(Dir()->FindObject(mParser.Str(), true));
    if (msgParser)
        msgParser->AddSink(this);
    MsgSource *msgFlagParser =
        dynamic_cast<MsgSource *>(Dir()->FindObject(mFlagParser.Str(), true));
    if (msgFlagParser)
        msgFlagParser->AddSink(this);
#ifdef HX_NATIVE
    // W32-PROP-FAN read-only discriminator (default-OFF RB3_MIDIDRV_PROBE):
    // is this instrument-MIDI driver BOUND (Enter ran) and did it find its
    // parser MsgSource to sink onto? Answers branches (a0) not-bound vs
    // (a)/(b) bound. Wii object byte-identical (#ifdef HX_NATIVE).
    { static const char *p = getenv("RB3_MIDIDRV_PROBE");
      if (p) fprintf(stderr,
          "[MIDIDRV_ENTER] drv='%s' parser='%s' found=%d flagParser='%s' ffound=%d\n",
          PathName(this), mParser.Str(), msgParser ? 1 : 0,
          mFlagParser.Str(), msgFlagParser ? 1 : 0); }
#endif
}

void CharDriverMidi::Exit() {
    CharDriver::Exit();
    MsgSource *msgParser = ObjectDir::Main()->Find<MsgSource>(mParser.Str(), false);
    if (msgParser)
        msgParser->RemoveSink(this);
    MsgSource *msgFlagParser =
        ObjectDir::Main()->Find<MsgSource>(mFlagParser.Str(), false);
    if (msgFlagParser)
        msgFlagParser->RemoveSink(this);
}

#ifdef HX_NATIVE
// W32-PROP-FAN branch-(b) STARVED fix (default-OFF flag RB3_MIDIDRV_ENTER_FIX):
// the instrument-MIDI prop drivers (drum-hit / strum / fret .dmidi) are created
// and per-frame POLLED (BandCharacter::Poll -> Character::Poll -> RndDir::Poll)
// but their Enter() is never invoked natively (a char load-order gap: the drivers
// are added to the char dir's mPolls AFTER the one-time Character::Enter ran, so
// RndDir::Enter's mPolls loop misses them while RndDir::Poll picks them up every
// frame). CharDriverMidi::Enter is what AddSink()s the driver onto its MIDI
// parser MsgSource; with Enter skipped the driver never subscribes, OnMidiParser
// never fires, the per-note hit/strum clip never plays, and the arm holds its
// idle pose -> CharIKHand over-reaches the far drum/fret target -> prop-tip fan.
// FIX: lazily run the missing Enter exactly once, on the first native Poll, for
// any driver that was polled without being entered. Whole thing is #ifdef
// HX_NATIVE + default-OFF -> Wii object byte-identical.
static int sMidiEnterFix() {
    static int v = -1;
    if (v < 0) { const char *e = getenv("RB3_MIDIDRV_ENTER_FIX");
        v = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return v;
}
// Native-only guard set (no struct member — Wii layout untouched).
static std::set<const void *> &sMidiEnteredSet() {
    static std::set<const void *> s;
    return s;
}
#endif

void CharDriverMidi::Poll() {
#ifdef HX_NATIVE
    if (sMidiEnterFix() && sMidiEnteredSet().insert(this).second) {
        Enter();
    }
    { static const char *p = getenv("RB3_MIDIDRV_PROBE");
      if (p) { static char sSeen[64][96]; static int sN = 0;
        const char *pn = PathName(this); int found = 0;
        for (int i = 0; i < sN; ++i) if (!std::strcmp(sSeen[i], pn)) { found = 1; break; }
        if (!found && sN < 64) { std::strncpy(sSeen[sN], pn, 95); sSeen[sN][95] = 0; sN++;
          fprintf(stderr, "[MIDIDRV_POLL] drv='%s' dir='%s'\n", pn,
                  Dir() && Dir()->Name() ? Dir()->Name() : "(nodir)"); } } }
#endif
    CharDriver::Poll();
}

void CharDriverMidi::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    CharDriver::PollDeps(changedBy, change);
}

SAVE_OBJ(CharDriverMidi, 0x58)

// fn_804C90E0
BEGIN_LOADS(CharDriverMidi)
    LOAD_REVS(bs)
    ASSERT_REVS(7, 0)
    LOAD_SUPERCLASS(CharDriver)
    if (gRev < 7) {
        mDefaultClip.Load(bs, false, mClips);
    }
    if (gRev == 2) {
        String str;
        bs >> str;
    } else if (gRev > 3)
        bs >> mParser;
    if (gRev > 4)
        bs >> mFlagParser;
    if (gRev > 5)
        bs >> mBlendOverridePct;
END_LOADS

BEGIN_COPYS(CharDriverMidi)
    COPY_SUPERCLASS(CharDriver)
    CREATE_COPY(CharDriverMidi)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(unk89)
        COPY_MEMBER(mParser)
        COPY_MEMBER(mFlagParser)
        COPY_MEMBER(mBlendOverridePct)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_HANDLERS(CharDriverMidi)
    HANDLE(midi_parser, OnMidiParser)
    HANDLE(midi_parser_group, OnMidiParserGroup)
    HANDLE(midi_parser_flags, OnMidiParserFlags)
    HANDLE_SUPERCLASS(CharDriver)
    HANDLE_CHECK(0x99)
END_HANDLERS

// fn_804C945C
DataNode CharDriverMidi::OnMidiParser(DataArray *da) {
#ifdef HX_NATIVE
    { static const char *p = getenv("RB3_MIDIDRV_PROBE");
      if (p) fprintf(stderr, "[MIDIDRV_FEED] on=parser drv='%s'\n", PathName(this)); }
#endif
    CharClip *clip;
    bool b = false;
    if (!unk89 && mDefaultClip)
        b = true;
    if (b)
        clip = dynamic_cast<CharClip *>(mDefaultClip.Ptr());
    else
        clip = FindClip(da->Node(2), false);
    if (!clip)
        return 0;
    if (clip || clip != FirstClip()) {
        float somefloat = da->Float(3);
        if (clip->PlayFlags() & 0x200) {
            float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime);
            float bts = BeatToSeconds(somefloat + TheTaskMgr.Beat()) - secs;
            somefloat = bts * clip->AverageBeatsPerSecond();
        }
        MaxEq(somefloat, 0.0f);
        Play(clip, 0, somefloat * mBlendOverridePct, -somefloat, 0.0f);
    }
    return 0;
}

DataNode CharDriverMidi::OnMidiParserFlags(DataArray *da) {
#ifdef HX_NATIVE
    { static const char *p = getenv("RB3_MIDIDRV_PROBE");
      if (p) fprintf(stderr, "[MIDIDRV_FEED] on=flags drv='%s'\n", PathName(this)); }
#endif
    mClipFlags = da->Int(2);
    return 0;
}

DataNode CharDriverMidi::OnMidiParserGroup(DataArray *da) {
#ifdef HX_NATIVE
    { static const char *p = getenv("RB3_MIDIDRV_PROBE");
      if (p) fprintf(stderr, "[MIDIDRV_FEED] on=group drv='%s'\n", PathName(this)); }
#endif
    const char *name = da->Str(2);
    CharClipGroup *grp = mClips->Find<CharClipGroup>(name, false);
    if (!grp) {
        MILO_WARN("%s could not find group %s in %s", PathName(this), name, grp->Name());
        return 0;
    } else {
        CharClip *clip;
        bool b = false;
        if (!unk89 && mDefaultClip)
            b = true;
        if (b)
            clip = dynamic_cast<CharClip *>(mDefaultClip.Ptr());
        else
            clip = grp->GetClip(mClipFlags);
        if (!clip) {
            MILO_WARN(
                "%s could not find clip with flags %d in %s",
                PathName(this),
                mClipFlags,
                PathName(grp)
            );
            return 0;
        } else {
            if (clip || clip != FirstClip()) {
                float somefloat = da->Float(3);
                if (clip->PlayFlags() & 0x200) {
                    somefloat *= clip->AverageBeatsPerSecond();
                }
                MaxEq(somefloat, 0.0f);
                Play(clip, 0, -somefloat, 1e+30f, 0.0f)->mBlendWidth =
                    somefloat * mBlendOverridePct;
            }
            return 0;
        }
    }
}

BEGIN_PROPSYNCS(CharDriverMidi)
    SYNC_PROP(parser, mParser)
    SYNC_PROP(flag_parser, mFlagParser)
    SYNC_PROP(blend_override_pct, mBlendOverridePct)
    SYNC_SUPERCLASS(CharDriver)
END_PROPSYNCS