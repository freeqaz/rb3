typedef struct Game {
    /* 0x000 */ char pad0[0xAC];
    /* 0x0AC */ f32 unkAC;                          /* inferred */
    /* 0x0B0 */ char padB0[0x74];                   /* maybe part of unkAC[0x1E]? */
    /* 0x124 */ f32 unk124;                         /* inferred */
} Game;                                             /* size >= 0x128 */

? EndGame__10NetSessionFibf(NetSession *this, s32 arg0, s32 arg1, f32 arg2); /* extern */
? SetCollectStats__9AutoTimerFbb(AutoTimer *this, u8 arg0, s32 arg1); /* extern */
s32 GetResult__4GameFb(Game *this, s32 arg0);       /* static */
extern void *TheGamePanel;
extern NetSession *TheNetSession;
extern void *TheRnd;

/* Game::SetGameOver (bool) */
void SetGameOver__4GameFb(Game *this, s32 arg0) {
    if ((s32) TheGamePanel->unk90 != 3) {
        if (arg0 == 0) {
            this->unk124 = this->unkAC;
        }
        SetCollectStats__9AutoTimerFbb(NULL, TheRnd->unkEC, (s32) TheGamePanel);
        EndGame__10NetSessionFibf(TheNetSession, GetResult__4GameFb(this, arg0), 0, this->unk124);
    }
}