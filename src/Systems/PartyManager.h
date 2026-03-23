// ============================================================
// File: PartyManager.h
// Responsibility: Persist the player party's BattlerStats across
//                 multiple BattleState instances.
//
// Problem solved:
//   BattleState and BattleManager are destroyed and recreated for every
//   encounter.  Without a persistent store, Verso's HP resets to full at
//   the start of every battle.  PartyManager holds Verso's stats between
//   battles so wounds carry forward.
//
// Design:
//   - Meyers' Singleton — constructed once, destroyed at program exit.
//   - Stores BattlerStats by VALUE (plain data, no GPU resources).
//   - Rage is reset to 0 between battles (rage is a per-fight resource).
//   - BattleManager::Initialize() calls GetVersoStats() to seed the
//     PlayerCombatant with the last-known stats.
//   - BattleState::Update() calls SetVersoStats() when WIN or LOSE is
//     detected, before calling StateManager::PopState().
//
// Lifetime:
//   Created on first Get() call (before any BattleState).
//   Destroyed at program exit (static storage duration).
//
// Common mistakes:
//   1. Forgetting to call SetVersoStats() before PopState() — HP resets.
//   2. Saving rage — rage should always reset to 0 between battles.
//   3. Calling Get() after D3DContext shutdown — safe because PartyManager
//      owns no GPU resources.
// ============================================================
#pragma once
#include "../Battle/BattlerStats.h"

class PartyManager
{
public:
    // ------------------------------------------------------------
    // Get: Meyers' Singleton accessor.
    // Thread-safe in C++11+ (magic statics).
    // ------------------------------------------------------------
    static PartyManager& Get()
    {
        static PartyManager instance;
        return instance;
    }

    // ------------------------------------------------------------
    // GetVersoStats: return the last-saved stats for Verso.
    //   Called by BattleManager::Initialize() to seed the combatant.
    //   On the very first battle this returns the default full-HP stats.
    // ------------------------------------------------------------
    const BattlerStats& GetVersoStats() const { return mVersoStats; }

    // ------------------------------------------------------------
    // SetVersoStats: persist Verso's stats at the end of a battle.
    //   Called by BattleState::Update() immediately before PopState().
    //   Rage is deliberately reset to 0 — it is a per-battle resource.
    // Parameters:
    //   stats — the BattlerStats from the live PlayerCombatant at battle end.
    // ------------------------------------------------------------
    void SetVersoStats(const BattlerStats& stats)
    {
        mVersoStats = stats;
        mVersoStats.rage = 0;   // rage does not carry between battles
    }

    // ------------------------------------------------------------
    // RestoreFullHP: heal Verso to maxHp and reset rage.
    //   Use at rest sites, save points, or after the game-over screen.
    // ------------------------------------------------------------
    void RestoreFullHP()
    {
        mVersoStats.hp   = mVersoStats.maxHp;
        mVersoStats.mp   = mVersoStats.maxMp;
        mVersoStats.rage = 0;
    }

private:
    // Private constructor — only Get() may create the instance.
    PartyManager() = default;

    // Non-copyable / non-movable — singleton must not be duplicated.
    PartyManager(const PartyManager&)            = delete;
    PartyManager& operator=(const PartyManager&) = delete;

    // ------------------------------------------------------------
    // mVersoStats: Verso's current stats, persisted between battles.
    //
    // Default values match PlayerCombatant MVP constants:
    //   HP=100  ATK=25  DEF=10  SPD=10  maxRage=100
    // In the full game these will be loaded from data/characters/verso.json.
    // ------------------------------------------------------------
    BattlerStats mVersoStats {
        100, // hp
        100, // maxHp
        50,  // mp
        50,  // maxMp
        25,  // atk
        10,  // def
        25,  // matk
        10,  // mdef
        10,  // spd
        0,   // rage
        100  // maxRage
    };
};
