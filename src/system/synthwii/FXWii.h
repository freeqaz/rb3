#pragma once

class WiiFX {
public:
    int unk0;
    int unk4;
    int unk8;

    void SetFX(int, int);
    bool IsReverb(int);
    void SetReverb(int, bool);
};

extern WiiFX TheWiiFX;