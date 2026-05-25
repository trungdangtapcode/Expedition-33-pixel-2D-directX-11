// ============================================================
// File: StatusEffectData.h
// Responsibility: Plain-data definition for one data-driven battle
//                 status effect.
// ============================================================
#pragma once
#include <string>
#include <vector>
#include "StatId.h"
#include "StatModifier.h"
#include "StatusEffectView.h"

enum class StatusStackPolicy
{
    Refresh,
    StackIntensity,
    ExtendDuration
};

struct StatusModifierData
{
    StatId stat = StatId::ATK;
    StatModifier::Op op = StatModifier::Op::AddFlat;
    float value = 0.0f;
};

struct StatusEffectData
{
    std::string id;
    std::string nameKey;
    std::string descriptionKey;
    std::string shortDescriptionKey;
    std::string durationLabelKey;
    std::string iconId;
    StatusEffectCategory category = StatusEffectCategory::Neutral;
    StatusStackPolicy stackPolicy = StatusStackPolicy::Refresh;
    int durationTurns = 1;
    int maxStacks = 1;
    bool dispellable = true;
    int tickDamage = 0;
    int tickDamagePerStack = 0;
    std::vector<StatusModifierData> modifiers;
};
