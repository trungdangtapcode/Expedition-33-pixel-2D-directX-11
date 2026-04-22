// ============================================================
// File: IDamageCalculator.h
// Responsibility: Pure virtual interface for complex damage formulas.
// Allows different systems (or future expansions with elements, critical hits,
// and status multiplier effects) to calculate final damage cleanly without
// bloating Combatant or specific Actions.
// ============================================================
#pragma once

class IBattler;
struct BattleContext;

enum class DamageType
{
    Physical,
    Magical,
    TrueDamage // Ignores all defense (optional future-proofing)
};

// Request object encapsulates all inputs for calculating a hit
struct DamageRequest
{
    IBattler* attacker = nullptr;
    IBattler* defender = nullptr;
    DamageType type = DamageType::Physical;
    
    // The base power coming from the skill/attack (e.g. basic attack is 1.0f)
    float skillMultiplier = 1.0f;
    
    // QTE result multiplier (1.5x Perfect, 1.2x Good, 0.8x Miss, 1.0x Default)
    float qteMultiplier = 1.0f;
    
    // Flat bonus damage added after multiplier but before defense
    int flatBonus = 0; 
};

// Result object encapsulates the exact outcomes
struct DamageResult
{
    int effectiveDamage = 0;
    int rawDamage = 0;     // For logging/UI
    int defenseUsed = 0;   // For logging/UI
    bool isCritical = false;
    
    // Future additions: 
    // bool missed = false;
    // ElementType weaknessTriggered = ElementType::None;
};

class IDamageCalculator
{
public:
    virtual ~IDamageCalculator() = default;

    // Evaluate the combat math using the given request + live battle context.
    //
    // ctx is required so the calculator (and any stat lookup it performs
    // via StatResolver::Get) can evaluate conditional modifiers that
    // depend on roster composition, turn count, or environment.
    virtual DamageResult Calculate(const DamageRequest& request,
                                    const BattleContext& ctx) const = 0;
};
