// ============================================================
// File: BattlerStats.h
// Responsibility: Plain-data struct for all numeric combat attributes.
//
// This is a VALUE TYPE — no GPU resources, no behavior, no virtual methods.
// Every combatant (player character or enemy) owns one BattlerStats by value.
//
// Data-driven note:
//   All default values are 0.  Concrete combatant constructors (or a future
//   JsonLoader::LoadCombatantData) fill them from data files.
//   Zero in source code = intentional uninitialized signal, not a game value.
//
// Rage mechanic:
//   Rage is a special resource unique to player characters.
//   It builds when the character deals OR receives damage.
//   RageSkill consumes maxRage to execute a powerful ability.
//   Enemies do not use rage (their maxRage = 0 signals this).
// ============================================================
#pragma once

struct BattlerStats
{
    // ---- Vital resources ----
    int hp    = 0;    // Current HP.  Combat ends when this reaches 0.
    int maxHp = 0;    // Maximum HP.  Set from data; never changes in battle.

    int mp    = 0;    // Current MP (reserved for future skills; not used by MVP).
    int maxMp = 0;

    // ---- Offensive / defensive attributes ----
    // atk: base damage dealt before target's def reduction is applied.
    // def: flat damage reduction applied to every incoming hit.
    // spd: determines turn order; higher spd acts first in a round.
    int atk = 0;
    int def = 0;
    int spd = 0;

    // ---- Rage (player-only passive resource) ----
    // Built passively by dealing and receiving damage.
    // rage == maxRage signals the RageSkill is ready to use.
    // Enemies leave maxRage = 0 (rage system never triggers for them).
    int rage    = 0;
    int maxRage = 0;

    // ---- Computed helpers ----

    bool IsAlive()    const { return hp > 0; }
    bool IsRageFull() const { return maxRage > 0 && rage >= maxRage; }

    // Clamp HP to [0, maxHp] after any modification.
    void ClampHp()    { if (hp < 0) hp = 0; if (hp > maxHp) hp = maxHp; }

    // Add rage, capped at maxRage.  No-op if maxRage == 0 (enemy).
    void AddRage(int amount)
    {
        if (maxRage <= 0) return;
        rage += amount;
        if (rage > maxRage) rage = maxRage;
    }
};
