// rb3-native live-tunable settings — implementation.
// See rb3_native_settings.h for the design + scope rationale.

#include "rb3_native_settings.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// -----------------------------------------------------------------------------
// Field table: the single source of truth tying each tunable field to its name
// (used by GetAllAsJson / Set/GetByName) and its env-var seed. Adding a knob is
// one row here + one member in the struct.
// -----------------------------------------------------------------------------
namespace {

struct FieldDesc {
    const char *name;   // JSON / SetByName identifier
    const char *envVar; // environment variable that seeds it
    float NativeSettings::*member;
};

#define FIELD(jsonName, envName, member) \
    { jsonName, envName, &NativeSettings::member }

const FieldDesc kFields[] = {
    // Camera pose
    FIELD("camRotX",          "CAM_ROTX",          camRotX),
    FIELD("camForwardOffset", "RB3_CAM_FORWARD",   camForwardOffset),
    FIELD("camHeightOffset",  "RB3_CAM_HEIGHT",    camHeightOffset),
    FIELD("camLateralOffset", "RB3_CAM_LATERAL",   camLateralOffset),
    FIELD("camPitch",         "RB3_CAM_PITCH",     camPitch),
    FIELD("camYaw",           "RB3_CAM_YAW",       camYaw),
    FIELD("camRoll",          "RB3_CAM_ROLL",      camRoll),
    FIELD("fovScale",         "RB3_CAM_FOV_SCALE", fovScale),
    // Gem / highway
    FIELD("gemScrollRateMult","RB3_GEM_SCROLL_MULT", gemScrollRateMult),
    FIELD("gemLookAheadSec",  "RB3_GEM_LOOKAHEAD",   gemLookAheadSec),
    FIELD("gemScale",         "RB3_GEM_SCALE",       gemScale),
    // Render
    FIELD("clearColorR",      "RB3_CLEAR_R",       clearColorR),
    FIELD("clearColorG",      "RB3_CLEAR_G",       clearColorG),
    FIELD("clearColorB",      "RB3_CLEAR_B",       clearColorB),
};

#undef FIELD

const int kNumFields = (int)(sizeof(kFields) / sizeof(kFields[0]));

} // namespace

void NativeSettings::InitFromEnv() {
    for (int i = 0; i < kNumFields; i++) {
        const FieldDesc &f = kFields[i];
        if (const char *v = getenv(f.envVar)) {
            this->*(f.member) = (float)atof(v);
            fprintf(stderr, "[NativeSettings] %s = %.4f (from %s)\n",
                    f.name, (double)(this->*(f.member)), f.envVar);
        }
    }
}

bool NativeSettings::SetByName(const char *name, const char *value) {
    if (!name || !value)
        return false;
    for (int i = 0; i < kNumFields; i++) {
        if (strcmp(kFields[i].name, name) == 0) {
            this->*(kFields[i].member) = (float)atof(value);
            return true;
        }
    }
    return false;
}

bool NativeSettings::GetByName(const char *name, std::string &out) const {
    if (!name)
        return false;
    for (int i = 0; i < kNumFields; i++) {
        if (strcmp(kFields[i].name, name) == 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.4f", (double)(this->*(kFields[i].member)));
            out = buf;
            return true;
        }
    }
    return false;
}

std::string NativeSettings::GetAllAsJson() const {
    std::string json = "{";
    for (int i = 0; i < kNumFields; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s\"%s\":%.4f",
                 i == 0 ? "" : ",",
                 kFields[i].name,
                 (double)(this->*(kFields[i].member)));
        json += buf;
    }
    json += "}";
    return json;
}

NativeSettings &TheNativeSettings() {
    static NativeSettings instance;
    return instance;
}
