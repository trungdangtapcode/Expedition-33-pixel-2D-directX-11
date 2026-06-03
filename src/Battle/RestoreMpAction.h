// ============================================================
// File: RestoreMpAction.h
// Responsibility: Restore MP to one battle participant.
//
// Owns:
//   No owned resources. The target pointer is non-owning.
//
// Lifetime:
//   Created in  -> DataDrivenSkill::Execute()
//   Destroyed in -> ActionQueue after Execute() returns true
//
// Important:
//   - MP changes are gameplay state and must remain inside IAction.
// ============================================================
#pragma once
#include "IAction.h"

class IBattler;

class RestoreMpAction final : public IAction
{
public:
    RestoreMpAction(IBattler* target, int amount);

    bool Execute(float dt) override;

private:
    IBattler* mTarget = nullptr;
    int mAmount = 0;
};
