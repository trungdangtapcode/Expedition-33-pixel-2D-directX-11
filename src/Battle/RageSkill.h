// ============================================================
// File: RageSkill.h
// Responsibility: Rage burst — consume full rage bar for 2× ATK damage.
//
// Requires: caster.IsRageFull() == true
// Effect:   rawDamage = caster.atk * 2; caster.rage reset to 0
// ============================================================
#pragma once
#include "ISkill.h"

class RageSkill : public ISkill
{
public:
    const char* GetName()        const override { return "Rage Burst"; }
    const char* GetDescription() const override { return "Unleash full rage for massive damage."; }

    // Only usable when rage bar is completely full.
    bool CanUse(const IBattler& caster) const override;

    // Produces: LogAction + DamageAction + RageResetAction (inline lambda action)
    std::vector<std::unique_ptr<IAction>> Execute(
        IBattler& caster,
        std::vector<IBattler*>& targets) const override;
};
