// ============================================================
// File: ReviveAction.h
// Responsibility: Recover one defeated ally and restart their idle sprite.
//
// Owns:
//   No owned resources. The target pointer is non-owning.
//
// Lifetime:
//   Created in  -> DataDrivenSkill::Execute()
//   Destroyed in -> ActionQueue after Execute() returns true
//
// Important:
//   - Revive is separate from HealAction because KO targets need animation
//     recovery and a different targeting rule.
// ============================================================
#pragma once
#include "IAction.h"

class IBattler;

class ReviveAction final : public IAction
{
public:
    ReviveAction(IBattler* target, int hpAmount);

    bool Execute(float dt) override;

private:
    IBattler* mTarget = nullptr;
    int mHpAmount = 0;
};
