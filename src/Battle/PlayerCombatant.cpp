// ============================================================
// File: PlayerCombatant.cpp
// ============================================================
#include "PlayerCombatant.h"
#include "SkillFactory.h"
#include "../Utils/Log.h"
#include <utility>

// ------------------------------------------------------------
// MVP stats — hardcoded constants only; in production these come from JSON.
// ------------------------------------------------------------
static constexpr int kPlayerMaxHp   = 100;
static constexpr int kPlayerMaxMp   = 50;
static constexpr int kPlayerAtk     = 25;
static constexpr int kPlayerDef     = 10;
static constexpr int kPlayerSpd     = 10;
static constexpr int kPlayerMaxRage = 100;

PlayerCombatant::PlayerCombatant(std::string name,
                                 std::wstring turnViewPath,
                                 std::vector<std::string> skillPaths)
    : Combatant(std::move(name), std::move(turnViewPath), BattlerStats{
        kPlayerMaxHp, kPlayerMaxHp,       // hp, maxHp
        kPlayerMaxMp, kPlayerMaxMp,       // mp, maxMp
        kPlayerAtk, kPlayerDef, kPlayerSpd,
        0, kPlayerMaxRage                 // rage starts at 0
    })
{
    BuildSkills(skillPaths);
}

// ------------------------------------------------------------
// Constructor with seeded stats: used by BattleManager::Initialize()
// to restore persistent HP from PartyManager.
// The skill list is always rebuilt fresh — skills are not persisted.
// ------------------------------------------------------------
PlayerCombatant::PlayerCombatant(std::string name,
                                 std::wstring turnViewPath,
                                 const BattlerStats& seedStats,
                                 std::vector<std::string> skillPaths)
    : Combatant(std::move(name), std::move(turnViewPath), seedStats)
{
    BuildSkills(skillPaths);
}

void PlayerCombatant::BuildSkills(const std::vector<std::string>& skillPaths)
{
    std::vector<std::string> paths = skillPaths;
    if (paths.empty())
    {
        paths.push_back("data/skills/verso_attack.json");
        paths.push_back("data/skills/verso_sunder_guard.json");
        paths.push_back("data/skills/verso_rage_burst.json");
    }

    for (const std::string& path : paths)
    {
        auto skill = SkillFactory::CreateFromFile(path);
        if (!skill)
        {
            LOG("[PlayerCombatant] WARNING: failed to build skill '%s'.", path.c_str());
            continue;
        }
        mSkills.push_back(std::move(skill));
    }
}

int PlayerCombatant::GetSkillCount() const
{
    return static_cast<int>(mSkills.size());
}

ISkill* PlayerCombatant::GetSkill(int index) const
{
    if (index < 0 || index >= static_cast<int>(mSkills.size())) return nullptr;
    return mSkills[index].get();
}

void PlayerCombatant::SetPendingAction(int skillIndex, IBattler* target)
{
    mPendingSkillIndex  = skillIndex;
    mPendingItemId.clear();          // clear any prior item selection
    mPendingTarget      = target;
    mHasPendingAction   = true;
}

// ------------------------------------------------------------
// SetPendingItem: queue an item-use as the player's turn action.
// Mutually exclusive with SetPendingAction — calling either one
// overwrites the other.
// ------------------------------------------------------------
void PlayerCombatant::SetPendingItem(const std::string& itemId, IBattler* target)
{
    mPendingItemId      = itemId;
    mPendingSkillIndex  = -1;        // skill index not used for item turns
    mPendingTarget      = target;    // may be nullptr for self / AoE items
    mHasPendingAction   = true;
}

bool PlayerCombatant::HasPendingAction() const
{
    return mHasPendingAction;
}

int PlayerCombatant::GetPendingSkillIndex() const
{
    return mPendingSkillIndex;
}

const std::string& PlayerCombatant::GetPendingItemId() const
{
    return mPendingItemId;
}

IBattler* PlayerCombatant::GetPendingTarget() const
{
    return mPendingTarget;
}

void PlayerCombatant::ClearPendingAction()
{
    mHasPendingAction   = false;
    mPendingSkillIndex  = -1;
    mPendingItemId.clear();
    mPendingTarget      = nullptr;
}
