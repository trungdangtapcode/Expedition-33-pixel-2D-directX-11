#include "../Utils/JsonLoader.h"
// ============================================================
// File: EnemyCombatant.cpp
// ============================================================
#include "EnemyCombatant.h"
#include "IBattler.h"

static constexpr int kEnemyMaxHp  = 50;
static constexpr int kEnemyMaxMp  = 0;
static constexpr int kEnemyAtk    = 15;
static constexpr int kEnemyDef    = 5;
static constexpr int kEnemySpd    = 8;

EnemyCombatant::EnemyCombatant(std::string name, std::string attackJsonPath)
    : Combatant(std::move(name), BattlerStats{
        kEnemyMaxHp, kEnemyMaxHp,
        kEnemyMaxMp, kEnemyMaxMp,
        kEnemyAtk, kEnemyDef, kEnemySpd,
        0, 0    // rage=0, maxRage=0 — enemies do not use rage
    })
    , mAttack([&]() {
            JsonLoader::SkillData attackData;
            JsonLoader::LoadSkillData(attackJsonPath, attackData);
            return std::make_unique<AttackSkill>(attackData);
        }())
{}

// ------------------------------------------------------------
// Data-driven constructor — receives a fully populated BattlerStats
// from EnemyEncounterData::battleParty so stat values come from JSON,
// not from the hardcoded constants above.
// Called by BattleManager::Initialize(EnemyEncounterData) exclusively.
// ------------------------------------------------------------
EnemyCombatant::EnemyCombatant(std::string name, const BattlerStats& stats, std::string attackJsonPath)
    : Combatant(std::move(name), stats)
    , mAttack([&]() {
            JsonLoader::SkillData attackData;
            JsonLoader::LoadSkillData(attackJsonPath, attackData);
            return std::make_unique<AttackSkill>(attackData);
        }())
{}

// ------------------------------------------------------------
// ChooseTarget: Simple AI — first alive player.
// Extend with targeting priority (lowest HP, highest threat) as needed.
// ------------------------------------------------------------
IBattler* EnemyCombatant::ChooseTarget(const std::vector<IBattler*>& players) const
{
    for (IBattler* p : players)
    {
        if (p && p->IsAlive()) return p;
    }
    return nullptr;
}
