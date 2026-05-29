#pragma once

/**
 * @brief Base class for all Milo engine executables.
 */
class App {
public:
    App(int, char **);
    ~App();

    void Run();
    void RunWithoutDebugging();
    void Draw();
    void DrawRegular();
    void CaptureHiRes();
#ifdef HX_NATIVE
    // Per-frame core poll + draw, extracted from the HX_NATIVE frame-loop body.
    // Shared by the native desktop loop (RunWithoutDebugging) and the web boot
    // machine (main_web.cpp BOOT_RUNNING). Declared only under HX_NATIVE so the
    // PPC asm build never sees it (zero decomp-match impact). `frame` is the
    // running frame counter consumed by the synthetic/web input driver.
    void RunOneFrame(int frame);
#endif
};
