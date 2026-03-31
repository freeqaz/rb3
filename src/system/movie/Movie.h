#pragma once

#include "utl/BinStream.h"
#include <list>

class Movie {
public:
    class Impl {
    public:
        Impl();
        ~Impl();
        static void Init();
        void End();
        bool IsOpen() const;
        bool IsLoading() const;
        bool CheckOpen(bool);
        void LockThread();
        void UnlockThread();
        int GetFrame() const;
        float MsPerFrame() const;
        int NumFrames() const;
        bool Paused() const;
        void SetPaused(bool);
        bool Ready() const;
        void Draw();
        bool Poll();
        void SetWidthHeight(int, int);
        void SetAspect(float);
        void Begin(const char *, float, bool, bool, bool, bool, int, BinStream *);
        void Terminate();
    };

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
    bool CheckOpen(bool);
    float MsPerFrame() const;
    int NumFrames() const;
    void UnlockThread();
    void LockThread();
    void Begin(const char *, float, bool, bool, bool, bool, int, BinStream *);

    static void Terminate();
    static void Validate();
    static void Init();

    static std::list<Impl *> openMovieFiles;
    Impl *mImpl;
};
