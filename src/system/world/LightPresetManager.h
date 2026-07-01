#pragma once
#include "obj/Data.h"
#include "world/LightPreset.h"
#include <map>

class WorldDir;

class LightPresetManager {
public:
    LightPresetManager(WorldDir *);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~LightPresetManager();

    void Enter();
    void Reset();
    void Poll();
    void SyncObjects();
    void SendLightingMessage(Symbol);
    void UpdateOverlay();
    void StartPreset(LightPreset *, bool);
    void SetPresetsEquivalent(bool);
    void GetPresets(LightPreset *&, LightPreset *&);
    void ReportError();
    void SchedulePstKey(LightPreset::KeyframeCmd);
    void Interp(Symbol, Symbol, float);
    void ForcePreset(LightPreset *, float);
    void ForcePresets(LightPreset *, LightPreset *, float);
    void StompPresets(LightPreset *, LightPreset *);
    void SelectPreset(LightPreset *, bool);
    void SetLighting(Symbol, bool);
    LightPreset *PickRandomPreset(Symbol);

    NEW_POOL_OVERLOAD(LightPresetManager)
    DELETE_POOL_OVERLOAD(LightPresetManager)

    DataNode OnToggleLightingEvents(DataArray *);
    DataNode OnForcePreset(DataArray *);
    DataNode OnForceTwoPresets(DataArray *);

    std::map<Symbol, std::vector<LightPreset *> > mPresets; // 0x4
    Symbol mLastCategory; // 0x1c
    WorldDir *mParent; // 0x20
    LightPreset *mPresetOverride; // 0x24
    LightPreset *mPresetNew; // 0x28
    LightPreset *mPresetPrev; // 0x2c
    float mPresetNewStartTime; // 0x30 - Bank 5 DWARF: new preset start time
    float mPresetPrevStartTime; // 0x34 - previous preset start time
    float unk38; // 0x38 - override preset start time (Bank 8 addition)
    bool mLastFrameSame; // 0x3c - Bank 5 DWARF: last-frame-same flag
    float mBlend; // 0x40
    float mOverrideFadeInTime; // 0x44 - Bank 5 DWARF: override fade-in duration
    int unk48; // 0x48 - override state flag (Bank 8 addition)
    bool mIgnoreLightingEvents; // 0x4c
};
