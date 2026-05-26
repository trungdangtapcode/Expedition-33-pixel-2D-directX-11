// ============================================================
// File: CleanseAction.h
// Responsibility: Remove active status effects from one combatant.
//
// Owns:
//   No owned resources. The target pointer is non-owning.
//
// Lifetime:
//   Created in  -> DataDrivenSkill::Execute()
//   Destroyed in -> ActionQueue after Execute() returns true
//
// Important:
//   - Combatant::ClearAllStatusEffects() owns modifier cleanup, so this
//     action stays small and does not touch stat modifiers directly.
// ============================================================
#pragma once
#include "IAction.h"

class IBattler;

class CleanseAction final : public IAction
{
public:
    explicit CleanseAction(IBattler* target);

    bool Execute(float dt) override;

private:
    IBattler* mTarget = nullptr;
};
