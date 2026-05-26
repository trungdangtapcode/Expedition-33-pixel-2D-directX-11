// ============================================================
// File: RestoreMpPercentAction.h
// Responsibility: Restore MP from a target's Max MP percentage.
//
// Owns:
//   No owned resources. The target pointer is non-owning.
//
// Lifetime:
//   Created in  -> AttackSkill::Execute()
//   Destroyed in -> ActionQueue after Execute() returns true
//
// Important:
//   - MP changes are gameplay state and must remain inside IAction.
//   - The percentage is evaluated at execution time so buffs, equipment,
//     and level changes that alter Max MP are respected.
// ============================================================
#pragma once

#include "IAction.h"

class IBattler;

class RestoreMpPercentAction final : public IAction
{
public:
    RestoreMpPercentAction(IBattler* target, float percent);

    bool Execute(float dt) override;

private:
    IBattler* mTarget = nullptr;
    float mPercent = 0.0f;
};
