// ============================================================
// File: BattleResourceRules.h
// Responsibility: Load and apply data-driven battle resource rules.
//
// Owns:
//   RageResourceRules value data loaded from JSON.
//
// Lifetime:
//   Created in  -> Meyers singleton on first access.
//   Destroyed in -> Process shutdown.
//
// Important:
//   - Tuning values live in data/battle_resource_rules.json.
//   - Mutating helpers are called only from IAction::Execute paths.
//   - UI can read the same rules for availability text without owning
//     gameplay state.
// ============================================================
#pragma once

#include <string>
#include <vector>

class IBattler;
struct DamageResult;

struct NamedResourceAmount
{
    std::string id;
    int amount = 0;
};

struct RageResourceRules
{
    int max = 100;
    std::string resetPolicy = "battle_start";

    std::vector<NamedResourceAmount> gainRules;
    std::vector<NamedResourceAmount> skillCosts;

    int basicAttack = 10;
    int damageDealtMin = 2;
    float damageDealtPercent = 0.20f;
    int damageTakenMin = 4;
    float damageTakenPercentOfMaxHp = 25.0f;
    int qteGood = 4;
    int qtePerfect = 8;
    int killBonus = 10;

    int rageBurstCost = 100;
};

class BattleResourceRules final
{
public:
    static BattleResourceRules& Get();

    void EnsureLoaded();
    bool Load(const std::string& path);

    const RageResourceRules& Rage() const { return mRage; }
    bool ResetRageAtBattleStart() const;
    int RageCostForSkill(const std::string& skillId) const;
    int RageGainForRule(const std::string& ruleId) const;

    void GrantRage(IBattler* target, int amount, const char* reason) const;
    void GrantDamageRage(IBattler* attacker,
                         IBattler* defender,
                         const DamageResult& result,
                         bool defenderWasKilled) const;
    void GrantQteRage(IBattler* actor, int perfectCount, int goodCount) const;

private:
    BattleResourceRules() = default;

    int ComputeDamageDealtGain(int damage) const;
    int ComputeDamageTakenGain(IBattler* defender, int damage) const;

    bool mLoaded = false;
    RageResourceRules mRage;
};
