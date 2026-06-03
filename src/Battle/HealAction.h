// ============================================================
// File: HealAction.h
// Responsibility: Restore HP to one living battle participant.
//
// Owns:
//   No owned resources. The target pointer is non-owning and owned by
//   BattleManager for the full lifetime of the queued action.
//
// Lifetime:
//   Created in  -> DataDrivenSkill::Execute()
//   Destroyed in -> ActionQueue after Execute() returns true
//
// Important:
//   - Healing is a combat mutation, so it must happen inside IAction.
//   - Reviving is intentionally a separate action with different rules.
// ============================================================
#pragma once
#include "IAction.h"

class IBattler;

class HealAction final : public IAction
{
public:
    HealAction(IBattler* target, int amount);

    bool Execute(float dt) override;

private:
    IBattler* mTarget = nullptr;
    int mAmount = 0;
};
