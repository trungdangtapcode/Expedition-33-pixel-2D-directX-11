// ============================================================
// File: RageSkill.cpp
// ============================================================
#include "RageSkill.h"
#include "IBattler.h"
#include "IAction.h"
#include "DamageAction.h"
#include "LogAction.h"
#include "BattleContext.h"
#include "../Systems/LocalizationManager.h"

std::string RageSkill::GetName() const
{
    return LocalizationManager::Get().Text("skill.rage_burst.name");
}

std::string RageSkill::GetDescription() const
{
    return LocalizationManager::Get().Text("skill.rage_burst.description");
}

bool RageSkill::CanUse(const IBattler& caster, const BattleContext& /*ctx*/) const
{
    // Gated on a full rage bar; prevents accidental activation.
    return caster.GetStats().IsRageFull();
}

std::vector<std::unique_ptr<IAction>> RageSkill::Execute(
    IBattler& caster,
    std::vector<IBattler*>& targets,
    const BattleContext& ctx) const
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
        LocalizationManager::Get().Format("battle.log.rage_burst", {
            { "actor", caster.GetName() },
            { "target", target->GetName() }
        })
    ));

    // Pass &ctx so the calculator reads the attacker's buffed ATK at execution.
    actions.push_back(std::make_unique<DamageAction>(req, &ctx));

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
