// ============================================================
// File: RageSpendAction.h
// Responsibility: Spend rage through the action queue.
//
// Owns:
//   No owned resources. The target pointer is non-owning.
//
// Lifetime:
//   Created in  -> Skill action builders.
//   Destroyed in -> ActionQueue after Execute() returns true.
//
// Important:
//   - Spending is clamped so data mistakes cannot create negative rage.
//   - spendAll is data-driven for finisher skills that empty the bar.
// ============================================================
#pragma once

#include "IAction.h"

class IBattler;

class RageSpendAction final : public IAction
{
public:
    RageSpendAction(IBattler* target, int amount, bool spendAll);

    bool Execute(float dt) override;

private:
    IBattler* mTarget = nullptr;
    int mAmount = 0;
    bool mSpendAll = false;
};
