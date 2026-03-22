#pragma once

#include "rndobj/PostProc.h"

class WiiPostProc : public RndPostProc {
public:
    WiiPostProc();
    virtual ~WiiPostProc();

    static void PreInit();
    void PrepareFinalTEV();
    void ClearFinalTEV();
    void DrawKaleidoscope();
};
