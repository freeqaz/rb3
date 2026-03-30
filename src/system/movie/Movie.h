#pragma once

#include "utl/BinStream.h"

class Movie {
public:
    class Impl;

    Movie();
    ~Movie();

    int GetFrame() const;
    bool Paused() const;
    void SetAspect(float);
    bool Ready() const;
    bool Poll();
    void End();
    void SetPaused(bool);
    void Draw();
    void SetWidthHeight(int, int);
    bool IsLoading() const;
    bool IsOpen() const;
    void CheckOpen(bool);
    float MsPerFrame() const;
    void UnlockThread();
    void LockThread();
    void Begin(const char *, float, bool, bool, bool, bool, int, BinStream *);

    static void Terminate();
    static void Validate();
    static void Init();

    Impl *mImpl;
};
