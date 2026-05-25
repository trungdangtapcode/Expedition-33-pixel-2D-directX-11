// ============================================================
// File: StatusEffectView.h
// Responsibility: Small immutable snapshot of one active status effect
//                 for HUD renderers and debug UI.
// ============================================================
#pragma once
#include <string>

enum class StatusEffectCategory
{
    Buff,
    Debuff,
    Neutral
};

struct StatusEffectView
{
    std::string id;
    std::string iconId;
    std::string nameKey;
    std::string descriptionKey;
    StatusEffectCategory category = StatusEffectCategory::Neutral;
    int remainingTurns = 0;
    int stackCount = 1;
    int maxStacks = 1;
    bool dispellable = true;
};
