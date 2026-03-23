// ============================================================
// File: DefaultDamageCalculator.h
// Responsibility: Concrete implementation of IDamageCalculator.
// Applies standard JRPG math handling physical (ATK vs DEF) and 
// magical (MATK vs MDEF) attack variants.
// ============================================================
#pragma once
#include "IDamageCalculator.h"

class DefaultDamageCalculator : public IDamageCalculator
{
public:
    DamageResult Calculate(const DamageRequest& request) const override;
};
