#include "../Utils/JsonLoader.h"
// ============================================================
// File: PlayerCombatant.cpp
// ============================================================
#include "PlayerCombatant.h"
#include "AttackSkill.h"
#include "RageSkill.h"
#include "WeakenSkill.h"
#include "../Utils/Log.h"

// ------------------------------------------------------------
// MVP stats — hardcoded constants only; in production these come from JSON.
// ------------------------------------------------------------
static constexpr int kPlayerMaxHp   = 100;
static constexpr int kPlayerMaxMp   = 50;
static constexpr int kPlayerAtk     = 25;
static constexpr int kPlayerDef     = 10;
static constexpr int kPlayerSpd     = 10;
static constexpr int kPlayerMaxRage = 100;

PlayerCombatant::PlayerCombatant(std::string name, std::string attackJsonPath)
    : Combatant(std::move(name), BattlerStats{
        kPlayerMaxHp, kPlayerMaxHp,       // hp, maxHp
        kPlayerMaxMp, kPlayerMaxMp,       // mp, maxMp
        kPlayerAtk, kPlayerDef, kPlayerSpd,
        0, kPlayerMaxRage                 // rage starts at 0
    })
{
    // Register the three default player skills.
    mSkills.push_back([&]() {
            JsonLoader::SkillData attackData;
            if (!JsonLoader::LoadSkillData(attackJsonPath, attackData)) {
                LOG("[PlayerCombatant] WARNING: Failed to load attack data '%s'. Using fallback defaults.", attackJsonPath.c_str());
            }
            LOG("[PlayerCombatant] LOADED %s move=%.2f ret=%.2f dmg=%.2f", attackJsonPath.c_str(), attackData.moveDuration, attackData.returnDuration, attackData.damageTakenOccurMoment);
            return std::make_unique<AttackSkill>(attackData);
        }());
    mSkills.push_back(std::make_unique<RageSkill>());
    mSkills.push_back(std::make_unique<WeakenSkill>());
}

// ------------------------------------------------------------
// Constructor with seeded stats: used by BattleManager::Initialize()
// to restore persistent HP from PartyManager.
// The skill list is always rebuilt fresh — skills are not persisted.
// ------------------------------------------------------------
PlayerCombatant::PlayerCombatant(std::string name, const BattlerStats& seedStats, std::string attackJsonPath)
    : Combatant(std::move(name), seedStats)
{
    // Register the three default player skills.
    mSkills.push_back([&]() {
            JsonLoader::SkillData attackData;
            if (!JsonLoader::LoadSkillData(attackJsonPath, attackData)) {
                LOG("[PlayerCombatant] WARNING: Failed to load attack data '%s'. Using fallback defaults.", attackJsonPath.c_str());
            }
            LOG("[PlayerCombatant] LOADED %s move=%.2f ret=%.2f dmg=%.2f", attackJsonPath.c_str(), attackData.moveDuration, attackData.returnDuration, attackData.damageTakenOccurMoment);
            return std::make_unique<AttackSkill>(attackData);
        }());
    mSkills.push_back(std::make_unique<RageSkill>());
    mSkills.push_back(std::make_unique<WeakenSkill>());
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
    mPendingTarget      = target;
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

IBattler* PlayerCombatant::GetPendingTarget() const
{
    return mPendingTarget;
}

void PlayerCombatant::ClearPendingAction()
{
    mHasPendingAction   = false;
    mPendingSkillIndex  = -1;
    mPendingTarget      = nullptr;
}
