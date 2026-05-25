// ============================================================
// File: StatusEffectRegistry.h
// Responsibility: Load and cache data/status_effects/*.json definitions.
// ============================================================
#pragma once
#include "StatusEffectData.h"
#include <string>
#include <vector>

class StatusEffectRegistry
{
public:
    static StatusEffectRegistry& Get();

    void EnsureLoaded();
    const StatusEffectData* Find(const std::string& id) const;
    const std::vector<StatusEffectData>& All() const { return mEffects; }

private:
    StatusEffectRegistry() = default;
    StatusEffectRegistry(const StatusEffectRegistry&) = delete;
    StatusEffectRegistry& operator=(const StatusEffectRegistry&) = delete;

    bool LoadFile(const std::string& path);

    std::vector<StatusEffectData> mEffects;
    bool mLoaded = false;
};
