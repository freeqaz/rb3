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
// It validates the EXACT bytes the linked GPU flavor hands CreateShaderModule,
// read from the engine's module table (gfx/ShippedWgsl.h): under the rb3
// flavor the five .wgsl.inc modules plus standard_wgsl.inc, under dc3 the
// standard shader and every WgpuRnd pass (bloom, depth of field, 2D rects,
// post-process, RB3 retail post, display ramp, shadow, particles). A bad edit
// to any of them turns this test red. A fail-red self-test (HarnessCatchesBadShader)
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

#include "rb3_rnd_backend.h"  // RB3RndBackend::InitGpu / Gpu() (either GPU flavor; pulls webgpu_cpp.h)
#include "gfx/ShippedWgsl.h"   // ShippedWgslModules: the linked flavor's WGSL sources

#include <cstring>
#include <string>

namespace {

// One-time headless GPU bring-up, mirroring test_texsharpen.cpp:38-100. The
// engine GpuDevice is a process-global; bring it up once (InitGpu is the same
// call main_native makes) and leave it up. Returns false if no device could be
// created (then the cases SKIP rather than fail — e.g. a host with no Vulkan).
bool EnsureGpu() {
    static int sState = -1;  // -1 untried, 0 failed, 1 ready
    if (sState >= 0) return sState == 1;
    bool ok = RB3RndBackend::InitGpu(/*width=*/64, /*height=*/64, /*headless=*/true);
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

    wgpu::ShaderModule module = RB3RndBackend::Gpu().Device().CreateShaderModule(&desc);

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
    RB3RndBackend::Gpu().Instance().WaitAny(future, UINT64_MAX);

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

// Every shader module the linked GPU flavor ships must compile with zero WGSL
// Errors against the real Dawn front-end.
TEST_F(WgslValidation, AllShippedShadersCompile) {
    // Document which backend path ran (both exercise Tint WGSL validation).
    printf("[WgslValidation] GPU backend: %s\n",
           RB3RndBackend::Gpu().IsNullBackend() ? "null (Tint front-end validation still runs)"
                                          : "real (native Dawn)");

    printf("[WgslValidation] GPU flavor: %s\n", RB3RndBackend::FlavorName());

    int count = 0;
    const ShippedWgslModule* modules = ShippedWgslModules(&count);
    // Both flavors ship the standard shader plus at least five pass modules; a
    // short table means a module fell out of the registry.
    ASSERT_GE(count, 6) << "module table is shorter than any flavor ships";
    for (int i = 0; i < count; i++) {
        const ShippedWgslModule& m = modules[i];
        ASSERT_NE(m.code, nullptr) << m.name;
        EXPECT_GT(std::strlen(m.code), 32u) << m.name << ": empty source";
        std::string err;
        bool ok = CompileOk(m.code, err);
        EXPECT_TRUE(ok) << m.name << ": " << err;
        if (ok) printf("[WgslValidation] %-36s OK\n", m.name);
    }
}

// Fail-red self-test: a deliberately broken shader (calls an undefined function)
// MUST be reported as a compile failure. Proves CompileOk can fail red rather
// than silently passing — without this, AllShippedShadersCompile could be a no-op.
TEST_F(WgslValidation, HarnessCatchesBadShader) {
    static const char* kBad =
        "@fragment fn f() -> @location(0) vec4f { return nonexistent_fn(); }";
    std::string err;
    bool ok = CompileOk(kBad, err);
    EXPECT_FALSE(ok) << "harness did not detect a known-bad shader";
    if (!ok) printf("[WgslValidation] harness caught bad shader: %s\n", err.c_str());
}
