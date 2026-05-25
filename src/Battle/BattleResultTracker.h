// ============================================================
// File: BattleResultTracker.h
// Responsibility: Accumulate battle-end statistics for result screens.
//
// Owns:
//   Scalar counters only. Durable rewards and party progress remain owned
//   by Wallet and PartyManager.
//
// Lifetime:
//   Created in  -> BattleState construction.
//   Reset in    -> BattleState::BeginBattleSession().
//   Destroyed in -> BattleState destruction.
//
// Important:
//   - The tracker records presentation metrics only; it never mutates
//     combatants, inventory, wallet, or party progress.
// ============================================================
#pragma once

#include "BattleEvents.h"
#include "BattleResultData.h"
#include "../Systems/PartyManager.h"
#include <vector>

class IBattler;

class BattleResultTracker
{
public:
    void Reset();
    void CaptureInitialParty(const std::vector<PartyMemberProgress>& progress);

    void RecordDamage(IBattler* target, bool targetIsPlayer, int damage);
    void RecordQteResult(QTEResult result);
    void RecordDodgeResult(const BattleDodgeResultPayload& payload);

    BattleResultData BuildResult(
        BattleResultOutcome outcome,
        int baseExp,
        int baseCoins,
        int noDamageBonusPercent,
        int killCount,
        float battleSeconds,
        const std::vector<PartyMemberProgress>& finalProgress,
        const std::vector<PartyMember>& finalParty) const;

private:
    std::vector<PartyMemberProgress> mInitialParty;

    int mTotalDamageDealt = 0;
    int mTotalDamageReceived = 0;
    int mHighestDamage = 0;
    int mQtePerfect = 0;
    int mQteGood = 0;
    int mQteMiss = 0;
    int mCleanDodges = 0;
    int mDodgeHits = 0;
};
