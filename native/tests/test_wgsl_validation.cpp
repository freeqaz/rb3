// WGSL shader-validation gate (W1.1.S2).
//
// After W1.1.S1 externalized the 5 inline WGSL modules in
// milo-native-engine/src/platform/Rnd_Wgpu_RB3.cpp out to
// src/gfx/Shaders/*.wgsl.inc, a WGSL syntax error in any of them would only be
// caught at runtime CreateShaderModule (a black frame or a broken pass), not at
// build/test time. This test closes that gap: it compiles every shipped shader
// through the REAL engine GPU device (headless Dawn) and asserts each produces
// zero WGSL compilation Errors.
//
// It validates the EXACT bytes shipped by #include-ing the same .inc files the
// engine embeds (so a bad edit to a .wgsl.inc turns this test red), plus the
// engine's own standard_wgsl.inc. A fail-red self-test (HarnessCatchesBadShader)
// proves the harness can actually fail on a broken shader — a validator that
// only ever passes is worthless.
//
// CompileOk() reproduces the Dawn error-detection dance from
// milo-native-engine/src/gfx/PipelineManager.cpp:261-296 — Dawn returns a
// non-null module even on error, so you MUST call GetCompilationInfo(...) and
// scan for CompilationMessageType::Error, then Instance().WaitAny(...).
//
// Null-backend note: even on Dawn's null backend, Tint still runs WGSL
// front-end validation on CreateShaderModule, so GetCompilationInfo still
// reports real syntax errors — the test stays meaningful. Only a total
// device-init failure GTEST_SKIPs.

// test_helpers.h first: it neutralizes glibc's st_atime/st_mtime/st_ctime
// macros (pulled by <gtest/gtest.h> via <sys/stat.h>) before any decomp header,
// which os/File.h (reached transitively below) uses as struct member names.
#include "test_helpers.h"

#include "platform/Rnd_Wgpu_RB3.h"  // gBandRnd, BandRnd::InitGpu, GpuDevice (pulls webgpu_cpp.h)

#include <string>

namespace {

// ---------------------------------------------------------------------------
// The exact shipped shader bytes, embedded the same compile-time way the engine
// embeds them (PipelineManager.cpp:37-40 pattern). Any WGSL edit to a .wgsl.inc
// changes these strings, so this test guards the real artifacts.
// ---------------------------------------------------------------------------
static const char* kHaloBlit =
#include "gfx/Shaders/rb3_halo_blit.wgsl.inc"
;
static const char* kPostProc =
#include "gfx/Shaders/rb3_postproc.wgsl.inc"
;
static const char* kQuad =
#include "gfx/Shaders/rb3_quad.wgsl.inc"
;
static const char* kCompose =
#include "gfx/Shaders/rb3_compose.wgsl.inc"
;
static const char* kParticle =
#include "gfx/Shaders/rb3_particle.wgsl.inc"
;
static const char* kStandard =
#include "gfx/standard_wgsl.inc"
;

struct NamedShader {
    const char* name;
    const char* code;
};

// One-time headless GPU bring-up, mirroring test_texsharpen.cpp:38-100. The
// engine GpuDevice is a process-global; bring it up once (InitGpu is the same
// call main_native makes) and leave it up. Returns false if no device could be
// created (then the cases SKIP rather than fail — e.g. a host with no Vulkan).
bool EnsureGpu() {
    static int sState = -1;  // -1 untried, 0 failed, 1 ready
    if (sState >= 0) return sState == 1;
    bool ok = gBandRnd.InitGpu(/*width=*/64, /*height=*/64, /*headless=*/true);
    sState = ok ? 1 : 0;
    return ok;
}

// Reproduces PipelineManager.cpp:261-296: Dawn always returns a non-null module
// even on a bad shader, so compile then poll GetCompilationInfo for Errors.
// Returns true iff the shader compiled with zero Errors; on failure, firstError
// holds the first Error message text.
bool CompileOk(const char* wgsl, std::string& firstError) {
    firstError.clear();

    wgpu::ShaderSourceWGSL wgslSource;
    wgslSource.code = wgsl;
    wgpu::ShaderModuleDescriptor desc{};
    desc.label = "WgslValidationTest";
    desc.nextInChain = &wgslSource;

    wgpu::ShaderModule module = gBandRnd.Gpu().Device().CreateShaderModule(&desc);

    bool hasError = false;
    wgpu::Future future = module.GetCompilationInfo(
        wgpu::CallbackMode::WaitAnyOnly,
        [&hasError, &firstError](wgpu::CompilationInfoRequestStatus status,
                                 wgpu::CompilationInfo const* info) {
            (void)status;
            if (!info) return;
            for (size_t i = 0; i < info->messageCount; i++) {
                auto& msg = info->messages[i];
                if (msg.type == wgpu::CompilationMessageType::Error) {
                    hasError = true;
                    if (firstError.empty())
                        firstError.assign(msg.message.data, msg.message.length);
                }
            }
        });
    gBandRnd.Gpu().Instance().WaitAny(future, UINT64_MAX);

    return !hasError;
}

class WgslValidation : public ::testing::Test {
protected:
    void SetUp() override {
        if (!EnsureGpu())
            GTEST_SKIP() << "headless GPU device unavailable on this host";
    }
};

}  // namespace

// Every shipped shader (5 externalized RB3 modules + the engine's standard
// shader) must compile with zero WGSL Errors against the real Dawn front-end.
TEST_F(WgslValidation, AllRB3ShadersCompile) {
    // Document which backend path ran (both exercise Tint WGSL validation).
    printf("[WgslValidation] GPU backend: %s\n",
           gBandRnd.Gpu().IsNullBackend() ? "null (Tint front-end validation still runs)"
                                          : "real (native Dawn)");

    const NamedShader shaders[] = {
        {"rb3_halo_blit.wgsl.inc", kHaloBlit},
        {"rb3_postproc.wgsl.inc", kPostProc},
        {"rb3_quad.wgsl.inc", kQuad},
        {"rb3_compose.wgsl.inc", kCompose},
        {"rb3_particle.wgsl.inc", kParticle},
        {"standard_wgsl.inc", kStandard},
    };

    for (const auto& s : shaders) {
        std::string err;
        bool ok = CompileOk(s.code, err);
        EXPECT_TRUE(ok) << s.name << ": " << err;
        if (ok) printf("[WgslValidation] %-24s OK\n", s.name);
    }
}

// Fail-red self-test: a deliberately broken shader (calls an undefined function)
// MUST be reported as a compile failure. Proves CompileOk can fail red rather
// than silently passing — without this, AllRB3ShadersCompile could be a no-op.
TEST_F(WgslValidation, HarnessCatchesBadShader) {
    static const char* kBad =
        "@fragment fn f() -> @location(0) vec4f { return nonexistent_fn(); }";
    std::string err;
    bool ok = CompileOk(kBad, err);
    EXPECT_FALSE(ok) << "harness did not detect a known-bad shader";
    if (!ok) printf("[WgslValidation] harness caught bad shader: %s\n", err.c_str());
}
