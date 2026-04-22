// ============================================================
// File: DamageSteps.h
// Responsibility: Concrete IDamageStep classes used by the default
//                 damage pipeline.
//
// Each step is intentionally tiny — composing them in
// DefaultDamageCalculator's constructor is the formula.
// ============================================================
#pragma once
#include "IDamageStep.h"

// ------------------------------------------------------------
// BaseFormulaStep
//   Reads ATK/MATK from the attacker through StatResolver, applies
//   the skill multiplier, subtracts the defender's matching resistance
//   (DEF/MDEF), and writes raw + effective damage into the result.
//   This is the seed every later step folds onto.
// ------------------------------------------------------------
class BaseFormulaStep : public IDamageStep
{
public:
    void Apply(const DamageRequest& request,
                DamageResult& result,
                const BattleContext& ctx) const override;
};

// ------------------------------------------------------------
// StatusBonusStep
//   +20% damage if the defender currently has any status effect
//   attached.  Cheap proxy for "exploit afflicted enemies" until
//   per-status keys (Burn, Stun…) exist.
// ------------------------------------------------------------
class StatusBonusStep : public IDamageStep
{
public:
    void Apply(const DamageRequest& request,
                DamageResult& result,
                const BattleContext& ctx) const override;
};

// ------------------------------------------------------------
// CritRollStep
//   10% chance.  On hit, doubles effective damage and sets
//   result.isCritical.  Uses a fast file-local LCG so the
//   calculator stays stateless from the caller's perspective.
// ------------------------------------------------------------
class CritRollStep : public IDamageStep
{
public:
    void Apply(const DamageRequest& request,
                DamageResult& result,
                const BattleContext& ctx) const override;
};

// ------------------------------------------------------------
// FinalClampStep
//   Floors effective damage at 1 so every successful hit registers.
//   Always last in the pipeline so earlier negative folds can't
//   accidentally produce a healing attack.
// ------------------------------------------------------------
class FinalClampStep : public IDamageStep
{
public:
    void Apply(const DamageRequest& request,
                DamageResult& result,
                const BattleContext& ctx) const override;
};
