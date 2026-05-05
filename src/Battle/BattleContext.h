// ============================================================
// File: BattleContext.h
// Responsibility: Read-only snapshot of the current battle state,
//                 passed to every system that makes a combat decision
//                 (skills, damage calculator, AI, triggers, stat resolver).
//
// Owned by:
//   BattleManager holds a single BattleContext as a value member and
//   refreshes it once per frame at the top of BattleManager::Update via
//   BattleManager::RebuildContext().
//
// Lifetime:
//   Lives as long as the BattleManager.  Systems hold const pointers
//   into this member (never into a temporary), so the pointers stay
//   valid for the entire battle — the CONTENTS change frame-to-frame
//   but the address is stable.
//
// Why snapshots instead of live queries:
//   Actions that reference combatant state (e.g. DamageAction reading
//   attacker ATK) execute 1+ frames after the skill was queued.  By
//   binding to BattleManager's mContext pointer, every read inside
//   the action automatically sees the CURRENT turn-order, alive lists,
//   and turn counter — not the values at queue time.
//
// Not included (yet):
//   - environment tag (cave, rain, open)   — reserved for later
//   - global flags (story, post-mortem)    — reserved for later
//   - timeline reference                   — not needed by step-1 systems
//
// Common mistakes:
//   1. Storing a BattleContext BY VALUE inside an action — the snapshot
//      is taken at queue time and becomes stale.  Always store a raw
//      const BattleContext* pointing at BattleManager::mContext.
//   2. Mutating fields from inside a resolver / predicate.  This struct
//      is READ-ONLY once BattleManager::RebuildContext returns.
// ============================================================
#pragma once
#include <vector>

#include "../Utils/JsonLoader.h"

class IBattler;

// Plain-data snapshot.  No behaviour, no virtual methods.
struct BattleContext
{
    // -- Roster snapshots (rebuilt each frame from BattleManager teams) --
    // These are value copies of the alive lists returned by
    // BattleManager::GetAlivePlayers / GetAliveEnemies.  Cheap — both
    // vectors hold at most kMaxTeamSize raw pointers.
    std::vector<IBattler*> alivePlayers;
    std::vector<IBattler*> aliveEnemies;

    // -- Turn / time counters --
    // turnCount increments once per BattleManager::AdvanceTurn call.
    // Starts at 0; the first combatant to act sees turnCount == 0.
    int   turnCount = 0;

    // Seconds since BattleState::OnEnter.  Accumulated in BattleManager::Update.
    // Useful for "enrage after 60 seconds" style triggers.
    float battleElapsed = 0.0f;

    // -- Global Battle Configuration --
    JsonLoader::BattleSystemConfig config;
};
