#ifndef BEATMATCH_GEMINFO_H
#define BEATMATCH_GEMINFO_H

enum NoStrumState {
    kStrumForceOn,
    kStrumForceOff,
    kStrumDefault
};

struct MultiGemInfo {
    int track;
    int slots;
    float ms;
    float duration_ms;
    int tick;
    int duration_ticks;
    bool ignore_duration;
    bool is_cymbal;
    int players;
    NoStrumState no_strum;
};

// 4 bits max
enum RGNoteType {
    kRGNormal,
    kRGGhost,
    kRGBend,
    kRGMuted,
    kRGTap,
    kRGPalmMute,
    kRGNaturalHarmonic,
    kRGPinchHarmonic,
    kRGSlide,
    kRGPickSlide,
    kRGWhammyUp,
    kRGWhammyDown,
    kRGBassPop,
    kRGBassSlap
};

enum RGStrumType {
    kRGNoStrum,
    kRGStrum,
    kRGStrumHigh,
    kRGStrumLow
};

struct RGGemInfo {
    int track;
    float ms;
    float duration_ms;
    bool ignore_duration;
    int tick;
    int duration_ticks;
    NoStrumState no_strum;
    bool show_chord_names;
    bool show_slashes;
    bool loose;
    bool show_chord_nums;
    bool left_hand_slide;
    bool reverse_slide;
    bool enharmonic;
    char frets[6];
    RGNoteType note_types[6];
    RGStrumType strum_type;
    unsigned char hand_position;
    unsigned char root_note;
#if defined(HX_NATIVE)
    // SongParser::HandleRGGemStop does `strcpy(&chord_name, info.mRGChordText)`
    // where mRGChordText is char[64]. The Wii decomp captured this trailing field
    // as a single `char` (the struct's tail inline buffer is declared as its first
    // byte), so on native the strcpy smashes the stack frame -> __stack_chk_fail
    // the instant a song with a real_guitar chord-name event loads (every RB3 song
    // with a Pro Guitar part). Give it the full 64-byte buffer it always was on
    // disc. GameGem reads it as a C string (Symbol(&info.chord_name)); &chord_name
    // is unchanged, so all consumers are unaffected.
    char chord_name[64];
#else
    char chord_name;
#endif
};

#endif