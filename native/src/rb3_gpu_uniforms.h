// CPU-side uniform structs for the RB3 native WebGPU backend.
//
// These mirror milo-native-engine's src/platform/Rnd_Wgpu.h structs EXACTLY —
// they are the WGSL-layout contract for the engine's standard shader
// (src/gfx/standard_wgsl.inc). They are decomp-agnostic (plain floats), so we
// copy them here instead of including Rnd_Wgpu.h, which drags in DC3-shaped
// rndobj/Rnd_NG.h that does not exist in RB3.
//
// Backs the RB3-Wii GPU rendering backend (Strategy B): rb3_render_tri.cpp
// (milestone ii) and BandRnd / the mesh+material backends (milestone iii+).

#pragma once

#include <cstdint>

struct SceneUniforms {
    float viewProj[16];       // mat4x4f
    float view[16];           // mat4x4f
    float cameraPos[3];       // vec3f
    float _pad0;
    float fogColor[3];        // vec3f
    float fogStart;
    float fogEnd;
    float fogEnabled;
    float _pad1[2];
    float lightDirs[4][4];    // array<vec4f, 4>
    float lightColors[4][4];  // array<vec4f, 4>
    float ambientColor[4];    // vec4f
    float numLights;          // f32
    float _padN[3];
    float pointLightPos[4][4];
    float pointLightColors[4][4];
    float pointLightRanges[4];
    float numPointLights;
    float _padPL[3];
    float lightViewProj[16];
    float shadowEnabled;
    float shadowBias;
    float shadowMapSize;
    float shadowStrength;
    float projLightDir[4];
    float projLightColor[4];
    float projLightProjRow0[4];
    float projLightProjRow1[4];
    float numProjLights;
    float _padProj[3];
};
static_assert(sizeof(SceneUniforms) == 656, "SceneUniforms must match WGSL layout");

struct MaterialUniforms {
    float color[4];
    float alphaThreshold;
    float useTexture;
    float specularPower;
    float emissiveMultiplier;
    float specularColor[4];
    float rimColor[4];
    float intensify;
    float shaderVariation;
    float rimLightUnder;
    float deNormal;
    float specular2Color[4];
    float anisotropy;
    float hasNormalMap;
    float materialFogEnabled;
    float prelit;
    float environMapStrength;
    float environMapFalloff;
    float environMapSpecMask;
    float texGenMode;
    float texXfmRow0[4];
    float texXfmRow1[4];
    float normDetailTiling;
    float normDetailStrength;
    float hasNormDetailMap;
    float useAlphaAsRGB;
    float hasSpecularMap;
    float _padMat[3];
};
static_assert(sizeof(MaterialUniforms) == 192, "MaterialUniforms must match WGSL layout");

struct ObjectUniforms {
    float world[16];
    float worldInvTranspose[16];
};
static_assert(sizeof(ObjectUniforms) == 128, "ObjectUniforms must match WGSL layout");

static constexpr int kMaxBones = 40;

struct BoneUniforms {
    float bones[kMaxBones][16];
};
static_assert(sizeof(BoneUniforms) == 2560, "BoneUniforms must match WGSL layout");

// GPU vertex layout after unpacking — must match the engine's static vertex
// layout (gfx/VertexFormats.h GpuVertex / VertexFormats.cpp StaticLayout).
struct GpuVertexRB3 {
    float pos[3];
    float norm[3];
    float color[4];
    float uv[2];
    float tangent[4];
};
static_assert(sizeof(GpuVertexRB3) == 64, "GpuVertexRB3 must be 64 bytes");
