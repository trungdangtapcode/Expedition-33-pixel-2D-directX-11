// ============================================================
// File: RageGainAction.h
// Responsibility: Add rage through the action queue.
//
// Owns:
//   No owned resources. The target pointer is non-owning.
//
// Lifetime:
//   Created in  -> Skill action builders.
//   Destroyed in -> ActionQueue after Execute() returns true.
//
// Important:
//   - Use this for known, non-damage rage grants such as basic attack
//     momentum. Damage-derived rage is applied by damage actions after
//     the final damage amount is known.
// ============================================================
#pragma once

#include "IAction.h"
#include <string>

class IBattler;

class RageGainAction final : public IAction
{
public:
    RageGainAction(IBattler* target, int amount, std::string reason);

    bool Execute(float dt) override;

private:
    IBattler* mTarget = nullptr;
    int mAmount = 0;
    std::string mReason;
};
