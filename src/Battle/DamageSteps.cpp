// ============================================================
// File: DamageSteps.cpp
// Responsibility: Implement every concrete IDamageStep used by the
//                 default damage pipeline.
// ============================================================
#define NOMINMAX
#include <algorithm>

#include "DamageSteps.h"
#include "IDamageCalculator.h"   // DamageRequest / DamageResult
#include "IBattler.h"
#include "BattleContext.h"
#include "StatResolver.h"
#include "StatId.h"

namespace
{
    // ------------------------------------------------------------
    // NextRandom: tiny LCG used by CritRollStep.  Hidden behind an
    // anonymous namespace so the calculator stays stateless from
    // the caller's perspective.  Not cryptographic — and that's
    // intentional: combat math should never block on a real RNG.
    //
    // Returns an int in [0, 99].
    // ------------------------------------------------------------
    int NextRandom()
    {
        static unsigned int sState = 0xC0FFEE13u;
        sState = sState * 1664525u + 1013904223u;
        return static_cast<int>((sState >> 16) % 100u);
    }
}

// ============================================================
// BaseFormulaStep
// ============================================================

void BaseFormulaStep::Apply(const DamageRequest& request,
                              DamageResult& result,
                              const BattleContext& ctx) const
{
    if (!request.attacker || !request.defender) return;

    int power      = 0;
    int resistance = 0;

    // Stat selection mirrors the pre-pipeline version of
    // DefaultDamageCalculator.  Reads go through StatResolver so every
    // active StatModifier — flat, percent, conditional — is applied.
    switch (request.type)
    {
    case DamageType::Physical:
        power      = StatResolver::Get(*request.attacker, ctx, StatId::ATK);
        resistance = StatResolver::Get(*request.defender, ctx, StatId::DEF);
        break;
    case DamageType::Magical:
        power      = StatResolver::Get(*request.attacker, ctx, StatId::MATK);
        resistance = StatResolver::Get(*request.defender, ctx, StatId::MDEF);
        break;
    case DamageType::TrueDamage:
        // TrueDamage scales off ATK by convention but ignores all
        // resistance — the only attack type that bypasses defense.
        power      = StatResolver::Get(*request.attacker, ctx, StatId::ATK);
        resistance = 0;
        break;
    }

    const float scaled = static_cast<float>(power) * request.skillMultiplier * request.qteMultiplier;

    result.rawDamage       = static_cast<int>(scaled) + request.flatBonus;
    result.defenseUsed     = resistance;
    // Effective damage is provisional here — later steps may multiply it
    // (crit) or floor it (FinalClampStep).  Compute it now so steps that
    // run between Base and Clamp see a consistent starting value.
    result.effectiveDamage = result.rawDamage - result.defenseUsed;
}

// ============================================================
// StatusBonusStep
//   +20% effective damage if the defender currently has any status
//   effect attached.  Cheap proxy until per-status keys exist.
// ============================================================

void StatusBonusStep::Apply(const DamageRequest& request,
                              DamageResult& result,
                              const BattleContext& /*ctx*/) const
{
    if (!request.defender) return;
    if (!request.defender->HasAnyStatusEffect()) return;

    // 1.20x is intentionally modest — designers tune by replacing this
    // step rather than threading a parameter, since damage steps are
    // configured by composition not config files (yet).
    result.effectiveDamage =
        static_cast<int>(static_cast<float>(result.effectiveDamage) * 1.20f);
}

// ============================================================
// CritRollStep
//   10% chance.  On hit, doubles effective damage and sets isCritical.
// ============================================================

void CritRollStep::Apply(const DamageRequest& /*request*/,
                          DamageResult& result,
                          const BattleContext& /*ctx*/) const
{
    if (NextRandom() < 10)   // 10% crit chance
    {
        result.isCritical = true;
        result.effectiveDamage *= 2;
    }
}

// ============================================================
// FinalClampStep
//   Floors effective damage at 1.  Always last in the chain.
// ============================================================

void FinalClampStep::Apply(const DamageRequest& /*request*/,
                            DamageResult& result,
                            const BattleContext& /*ctx*/) const
{
    if (result.effectiveDamage < 1) result.effectiveDamage = 1;
}
