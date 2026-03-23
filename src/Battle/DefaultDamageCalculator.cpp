// ============================================================
// File: DefaultDamageCalculator.cpp
// ============================================================
#include "DefaultDamageCalculator.h"
#include "IBattler.h"
#include <algorithm>

DamageResult DefaultDamageCalculator::Calculate(const DamageRequest& request) const
{
    DamageResult result;
    if (!request.attacker || !request.defender)
    {
        return result; // Invalid request
    }

    const auto& atkStats = request.attacker->GetStats();
    const auto& defStats = request.defender->GetStats();

    int attackerPower = 0;
    int defenderResistance = 0;

    // 1. Identify primary stats according to damage type
    switch (request.type)
    {
    case DamageType::Physical:
        attackerPower = atkStats.atk;
        defenderResistance = defStats.def;
        break;
    case DamageType::Magical:
        attackerPower = atkStats.matk;
        defenderResistance = defStats.mdef;
        break;
    case DamageType::TrueDamage:
        attackerPower = atkStats.atk; // Or could be MATK, but TrueDamage ignores defense
        defenderResistance = 0;
        break;
    }

    // 2. Apply multipliers (scaling from skill)
    // Later we can modify attackerPower or defenderResistance using status effect multipliers here.
    float scaledPower = static_cast<float>(attackerPower) * request.skillMultiplier;
    
    // 3. Compute raw damage
    result.rawDamage = static_cast<int>(scaledPower) + request.flatBonus;
    result.defenseUsed = defenderResistance;

    // 4. Calculate final effective damage: min hit is 1
    // Effective = max(1, Raw - Defense)
    result.effectiveDamage = std::max(1, result.rawDamage - result.defenseUsed);

    // TODO: Critical hit calculations would go here
    result.isCritical = false;

    return result;
}
