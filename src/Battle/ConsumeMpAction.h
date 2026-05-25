// ============================================================
// File: ConsumeMpAction.h
// Responsibility: Spend MP as an action after the player commits a skill.
// ============================================================
#pragma once
#include "IAction.h"

class IBattler;

class ConsumeMpAction : public IAction
{
public:
    ConsumeMpAction(IBattler* caster, int amount);

    bool Execute(float dt) override;

private:
    IBattler* mCaster = nullptr;
    int mAmount = 0;
};
