// ============================================================
// File: BattleResultData.h
// Responsibility: Define the immutable data shown on the battle
//                 victory and defeat result screens.
//
// Owns:
//   No resources. These are plain value types assembled by BattleState
//   after the battle simulation reaches a terminal outcome.
//
// Lifetime:
//   Created in  -> BattleState when an outcome is ready to present.
//   Destroyed in -> BattleState reset or state destruction.
//
// Important:
//   - Reward values in this snapshot are post-bonus values for display.
//   - Member rows carry before/after progression so renderers never read
//     mutable PartyManager state while drawing.
// ============================================================
#pragma once

#include "../Battle/BattlerStats.h"
#include <string>
#include <vector>

enum class BattleResultOutcome
{
    Victory,
    Defeat
};

struct BattleMemberResult
{
    std::string id;
    std::string name;
    std::wstring portraitPath;

    int levelBefore = 1;
    int levelAfter = 1;
    int expBefore = 0;
    int expAfter = 0;
    int expToNextBefore = 100;
    int expToNextAfter = 100;
    int hpAfter = 0;
    int maxHpAfter = 0;
    bool leveledUp = false;
};

struct BattleResultData
{
    BattleResultOutcome outcome = BattleResultOutcome::Victory;

    int baseExp = 0;
    int bonusExp = 0;
    int totalExp = 0;
    int baseCoins = 0;
    int bonusCoins = 0;
    int totalCoins = 0;
    int kills = 0;

    float battleSeconds = 0.0f;

    int totalDamageDealt = 0;
    int totalDamageReceived = 0;
    int highestDamage = 0;

    int qtePerfect = 0;
    int qteGood = 0;
    int qteMiss = 0;
    int cleanDodges = 0;
    int dodgeHits = 0;

    bool noDamage = false;

    std::vector<BattleMemberResult> members;
};
