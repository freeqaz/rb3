#pragma once

class PitchDetector {
public:
    PitchDetector(int);
    ~PitchDetector();
    void AnalyzeBlock(const short *, int, float &, float &);

    char pad[0x3c]; // 0x0
    bool mEnablePitchDetection; // 0x3c
};