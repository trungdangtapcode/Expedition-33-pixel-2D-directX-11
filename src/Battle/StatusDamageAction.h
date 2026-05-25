// ============================================================
// File: StatusDamageAction.h
// Responsibility: Apply one damage-over-time status tick through the
//                 action queue.
// ============================================================
#pragma once
#include "IAction.h"
#include <string>

class IBattler;

class StatusDamageAction : public IAction
{
public:
    StatusDamageAction(IBattler* target, int damage, std::string effectId);

    bool Execute(float dt) override;

private:
    IBattler* mTarget = nullptr;
    int mDamage = 0;
    std::string mEffectId;
};
