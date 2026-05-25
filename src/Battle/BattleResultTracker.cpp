// ============================================================
// File: BattleResultTracker.cpp
// Responsibility: Build stable battle-result snapshots from runtime
//                 events and party before/after progress.
//
// Common mistakes:
//   1. Reading PartyManager directly during rendering -> result rows can
//      drift after a retry or save/load event.
//   2. Granting rewards here -> the tracker becomes a gameplay mutator.
//   3. Counting QTE Pending/None states -> per-frame UI payloads would
//      inflate player performance stats.
// ============================================================
#define NOMINMAX
#include "BattleResultTracker.h"
#include "IBattler.h"
#include <algorithm>

void BattleResultTracker::Reset()
{
    mInitialParty.clear();
    mTotalDamageDealt = 0;
    mTotalDamageReceived = 0;
    mHighestDamage = 0;
    mQtePerfect = 0;
    mQteGood = 0;
    mQteMiss = 0;
    mCleanDodges = 0;
    mDodgeHits = 0;
}

void BattleResultTracker::CaptureInitialParty(const std::vector<PartyMemberProgress>& progress)
{
    mInitialParty = progress;
}

void BattleResultTracker::RecordDamage(IBattler* /*target*/, bool targetIsPlayer, int damage)
{
    if (damage <= 0) return;

    if (targetIsPlayer)
    {
        mTotalDamageReceived += damage;
        return;
    }

    mTotalDamageDealt += damage;
    mHighestDamage = (std::max)(mHighestDamage, damage);
}

void BattleResultTracker::RecordQteResult(QTEResult result)
{
    switch (result)
    {
    case QTEResult::Perfect: ++mQtePerfect; break;
    case QTEResult::Good:    ++mQteGood;    break;
    case QTEResult::Miss:    ++mQteMiss;    break;
    default: break;
    }
}

void BattleResultTracker::RecordDodgeResult(const BattleDodgeResultPayload& payload)
{
    if (payload.hitsTaken <= 0 && payload.completed)
    {
        ++mCleanDodges;
        return;
    }

    mDodgeHits += (std::max)(0, payload.hitsTaken);
}

BattleResultData BattleResultTracker::BuildResult(
    BattleResultOutcome outcome,
    int baseExp,
    int baseCoins,
    int noDamageBonusPercent,
    int killCount,
    float battleSeconds,
    const std::vector<PartyMemberProgress>& finalProgress,
    const std::vector<PartyMember>& finalParty) const
{
    BattleResultData result{};
    result.outcome = outcome;
    result.baseExp = (std::max)(0, baseExp);
    result.baseCoins = (std::max)(0, baseCoins);
    result.noDamage = (mTotalDamageReceived == 0);
    result.bonusExp = result.noDamage
        ? (result.baseExp * noDamageBonusPercent) / 100
        : 0;
    result.bonusCoins = result.noDamage
        ? (result.baseCoins * noDamageBonusPercent) / 100
        : 0;
    result.totalExp = result.baseExp + result.bonusExp;
    result.totalCoins = result.baseCoins + result.bonusCoins;
    result.kills = (std::max)(0, killCount);
    result.battleSeconds = (std::max)(0.0f, battleSeconds);
    result.totalDamageDealt = mTotalDamageDealt;
    result.totalDamageReceived = mTotalDamageReceived;
    result.highestDamage = mHighestDamage;
    result.qtePerfect = mQtePerfect;
    result.qteGood = mQteGood;
    result.qteMiss = mQteMiss;
    result.cleanDodges = mCleanDodges;
    result.dodgeHits = mDodgeHits;

    result.members.reserve(finalParty.size());
    for (size_t i = 0; i < finalParty.size(); ++i)
    {
        const PartyMember& member = finalParty[i];

        const auto beforeIt = std::find_if(
            mInitialParty.begin(),
            mInitialParty.end(),
            [&member](const PartyMemberProgress& progress)
            {
                return progress.id == member.id;
            });

        const auto afterIt = std::find_if(
            finalProgress.begin(),
            finalProgress.end(),
            [&member](const PartyMemberProgress& progress)
            {
                return progress.id == member.id;
            });

        const BattlerStats beforeStats = (beforeIt != mInitialParty.end())
            ? beforeIt->baseStats
            : member.baseStats;
        const BattlerStats afterStats = (afterIt != finalProgress.end())
            ? afterIt->baseStats
            : member.baseStats;

        BattleMemberResult row{};
        row.id = member.id;
        row.name = member.name;
        row.portraitPath = member.turnViewPath;
        row.levelBefore = beforeStats.level;
        row.levelAfter = afterStats.level;
        row.expBefore = beforeStats.exp;
        row.expAfter = afterStats.exp;
        row.expToNextBefore = PartyManager::ExpToNextLevel(beforeStats.level);
        row.expToNextAfter = PartyManager::ExpToNextLevel(afterStats.level);
        row.hpAfter = afterStats.hp;
        row.maxHpAfter = afterStats.maxHp;
        row.leveledUp = row.levelAfter > row.levelBefore;
        result.members.push_back(row);
    }

    return result;
}
