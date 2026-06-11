? Fail__5DebugFPCc(Debug *this, s8 *arg0);          /* extern */
s8 *Str__12FormatStringFv(FormatString *this);      /* extern */
void *__ct__12FormatStringFPCc(FormatString *this, s8 *arg0); /* extern */
extern Debug TheDebug;
static ? *@15533[0xD] = {
    &.L_80171CFC,
    &.L_80171D18,
    &.L_80171D10,
    &.L_80171D20,
    &.L_80171D34,
    &.L_80171D4C,
    &.L_80171D3C,
    &.L_80171D5C,
    &.L_80171D44,
    &.L_80171D5C,
    &.L_80171D54,
    &.L_80171D54,
    &.L_80171D54,
};
static ? @stringBase0;                              /* unable to generate initializer: unknown type */

/* TrackTypeToScoreType (TrackType, bool, bool) */
s32 TrackTypeToScoreType__F9TrackTypebb(? arg0, s32 arg1, s32 arg2) {
    FormatString sp8;

    if ((u32) arg0 <= 0xCU) {
        return (s32) arg0;
    }
    __ct__12FormatStringFPCc(&sp8, "Defines.cpp\0( 0) <= (controllerType) && (controllerType) <= ( kNumControllerTypes)\0CHAR_INSTRUMENT_SYMBOLS\0false\0unrecognized TrackType!\0no TrackType for this ScoreType!\0SCORE_TYPE_SYMBOLS\0DIFF_SYMBOLS\0default_difficulty\0tour\0DIFF_SHORT_SYMBOLS\0No tracks playable by controller %i\0No tracks representative of part %i\0No priority tracks for controller %i" + 0x71);
    Fail__5DebugFPCc(&TheDebug, Str__12FormatStringFv(&sp8));
    return 0xB;
}