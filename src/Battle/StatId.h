// ============================================================
// File: StatId.h
// Responsibility: Enumerate every stat that can be resolved through
//                 the StatModifier / StatResolver pipeline.
//
// HP, MP, and Rage are NOT listed here — they are mutable STATE, not
// derived stats.  They change in response to damage, healing, and skill
// costs, and have no concept of a "base" value that modifiers fold over.
//
// MaxHp / MaxMp ARE listed because equipment and buffs may change them.
// ============================================================
#pragma once

enum class StatId
{
    ATK,    // physical attack power
    DEF,    // physical flat reduction
    MATK,   // magical attack power
    MDEF,   // magical flat reduction
    SPD,    // turn-order weight; higher acts sooner
    MAX_HP, // upper bound for hp (rarely modified in current design)
    MAX_MP  // upper bound for mp (rarely modified in current design)
};
