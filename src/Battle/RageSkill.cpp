// ============================================================
// File: RageSkill.cpp
// ============================================================
#include "RageSkill.h"
#include "IBattler.h"
#include "IAction.h"
#include "DamageAction.h"
#include "LogAction.h"

bool RageSkill::CanUse(const IBattler& caster) const
{
    // Gated on a full rage bar; prevents accidental activation.
    return caster.GetStats().IsRageFull();
}

std::vector<std::unique_ptr<IAction>> RageSkill::Execute(
    IBattler& caster,
    std::vector<IBattler*>& targets) const
{
    std::vector<std::unique_ptr<IAction>> actions;

    if (targets.empty()) return actions;

    IBattler* target = targets[0];

    DamageRequest req;
    req.attacker = &caster;
    req.defender = target;
    req.type = DamageType::Physical; // Or TrueDamage/Magical if needed
    req.skillMultiplier = 2.0f;      // Double damage

    actions.push_back(std::make_unique<LogAction>(
        nullptr,
        caster.GetName() + " unleashes RAGE BURST on " + target->GetName() + "!"
    ));

    actions.push_back(std::make_unique<DamageAction>(req));

    // Reset rage to 0 after the burst — captured by pointer, safe because
    // caster outlives the ActionQueue (BattleManager owns both).
    struct RageResetAction : public IAction {
        IBattler* mCaster;
        explicit RageResetAction(IBattler* c) : mCaster(c) {}
        bool Execute(float) override {
            mCaster->GetStats().rage = 0;
            return true;
        }
    };
    actions.push_back(std::make_unique<RageResetAction>(&caster));

    return actions;
}
