// ============================================================
// File: BattleResourceRules.cpp
// Responsibility: Parse and apply battle resource tuning data.
// ============================================================
#define NOMINMAX
#include "BattleResourceRules.h"
#include "IBattler.h"
#include "IDamageCalculator.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    constexpr const char* kDefaultRulesPath = "data/battle_resource_rules.json";

    std::filesystem::path ResolveReadablePath(const std::string& path)
    {
        std::filesystem::path resolved(path);
        if (std::filesystem::exists(resolved)) return resolved;
        return std::filesystem::path("..") / path;
    }

    std::string ReadWholeFile(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file.is_open()) return std::string();

        std::ostringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    int ClampNonNegative(int value)
    {
        return std::max(0, value);
    }

    NamedResourceAmount ParseNamedAmount(const std::string& objectSrc, const char* idKey)
    {
        NamedResourceAmount rule;
        rule.id = JsonLoader::detail::CleanString(JsonLoader::detail::ValueOf(objectSrc, idKey));
        rule.amount = ClampNonNegative(JsonLoader::detail::ParseInt(
            JsonLoader::detail::ValueOf(objectSrc, "amount"), 0));
        return rule;
    }

    std::vector<NamedResourceAmount> ParseNamedAmounts(
        const std::string& src,
        const char* arrayKey,
        const char* idKey)
    {
        std::vector<NamedResourceAmount> rules;
        for (const std::string& objectSrc : JsonLoader::detail::ExtractObjectsFromArray(src, arrayKey))
        {
            NamedResourceAmount rule = ParseNamedAmount(objectSrc, idKey);
            if (!rule.id.empty() && rule.amount > 0)
            {
                rules.push_back(rule);
            }
        }
        return rules;
    }

    int FindNamedAmount(const std::vector<NamedResourceAmount>& rules, const std::string& id)
    {
        for (const NamedResourceAmount& rule : rules)
        {
            if (rule.id == id) return rule.amount;
        }
        return 0;
    }
}

BattleResourceRules& BattleResourceRules::Get()
{
    static BattleResourceRules instance;
    return instance;
}

void BattleResourceRules::EnsureLoaded()
{
    if (mLoaded) return;
    Load(kDefaultRulesPath);
}

bool BattleResourceRules::Load(const std::string& path)
{
    const std::filesystem::path resolved = ResolveReadablePath(path);
    const std::string src = ReadWholeFile(resolved);
    if (src.empty())
    {
        LOG("[BattleResourceRules] WARNING: Could not read '%s'. Defaults remain active.",
            path.c_str());
        mLoaded = true;
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, path);

    mRage.max = ClampNonNegative(JsonLoader::detail::ParseInt(
        JsonLoader::detail::ValueOf(src, "max"), mRage.max));
    const std::string resetPolicy = JsonLoader::detail::CleanString(
        JsonLoader::detail::ValueOf(src, "resetPolicy"));
    if (!resetPolicy.empty()) mRage.resetPolicy = resetPolicy;

    mRage.basicAttack = ClampNonNegative(JsonLoader::detail::ParseInt(
        JsonLoader::detail::ValueOf(src, "basicAttack"), mRage.basicAttack));
    mRage.damageDealtMin = ClampNonNegative(JsonLoader::detail::ParseInt(
        JsonLoader::detail::ValueOf(src, "damageDealtMin"), mRage.damageDealtMin));
    mRage.damageDealtPercent = std::max(0.0f, JsonLoader::detail::ParseFloat(
        JsonLoader::detail::ValueOf(src, "damageDealtPercent"), mRage.damageDealtPercent));
    mRage.damageTakenMin = ClampNonNegative(JsonLoader::detail::ParseInt(
        JsonLoader::detail::ValueOf(src, "damageTakenMin"), mRage.damageTakenMin));
    mRage.damageTakenPercentOfMaxHp = std::max(0.0f, JsonLoader::detail::ParseFloat(
        JsonLoader::detail::ValueOf(src, "damageTakenPercentOfMaxHp"), mRage.damageTakenPercentOfMaxHp));
    mRage.qteGood = ClampNonNegative(JsonLoader::detail::ParseInt(
        JsonLoader::detail::ValueOf(src, "qteGood"), mRage.qteGood));
    mRage.qtePerfect = ClampNonNegative(JsonLoader::detail::ParseInt(
        JsonLoader::detail::ValueOf(src, "qtePerfect"), mRage.qtePerfect));
    mRage.killBonus = ClampNonNegative(JsonLoader::detail::ParseInt(
        JsonLoader::detail::ValueOf(src, "killBonus"), mRage.killBonus));
    mRage.rageBurstCost = ClampNonNegative(JsonLoader::detail::ParseInt(
        JsonLoader::detail::ValueOf(src, "rageBurst"), mRage.rageBurstCost));
    mRage.gainRules = ParseNamedAmounts(src, "rules", "id");
    mRage.skillCosts = ParseNamedAmounts(src, "skillCosts", "skillId");

    mLoaded = true;
    LOG("[BattleResourceRules] Loaded rage rules from '%s'.", resolved.string().c_str());
    return true;
}

bool BattleResourceRules::ResetRageAtBattleStart() const
{
    return mRage.resetPolicy == "battle_start";
}

int BattleResourceRules::RageCostForSkill(const std::string& skillId) const
{
    return FindNamedAmount(mRage.skillCosts, skillId);
}

int BattleResourceRules::RageGainForRule(const std::string& ruleId) const
{
    return FindNamedAmount(mRage.gainRules, ruleId);
}

void BattleResourceRules::GrantRage(IBattler* target, int amount, const char* reason) const
{
    if (!target || amount <= 0) return;
    BattlerStats& stats = target->GetStats();
    if (stats.maxRage <= 0) return;

    const int before = stats.rage;
    stats.AddRage(amount);
    const int gained = stats.rage - before;
    if (gained <= 0) return;

    LOG("[BattleResourceRules] %s gains %d rage from %s (%d -> %d).",
        target->GetDebugName().c_str(),
        gained,
        reason ? reason : "resource rule",
        before,
        stats.rage);
}

void BattleResourceRules::GrantDamageRage(IBattler* attacker,
                                          IBattler* defender,
                                          const DamageResult& result,
                                          bool defenderWasKilled) const
{
    if (result.effectiveDamage <= 0) return;

    GrantRage(attacker, ComputeDamageDealtGain(result.effectiveDamage), "damage dealt");
    GrantRage(defender, ComputeDamageTakenGain(defender, result.effectiveDamage), "damage taken");
    if (defenderWasKilled)
    {
        GrantRage(attacker, mRage.killBonus, "defeat bonus");
    }
}

void BattleResourceRules::GrantQteRage(IBattler* actor, int perfectCount, int goodCount) const
{
    const int amount =
        ClampNonNegative(perfectCount) * mRage.qtePerfect +
        ClampNonNegative(goodCount) * mRage.qteGood;
    GrantRage(actor, amount, "QTE timing");
}

int BattleResourceRules::ComputeDamageDealtGain(int damage) const
{
    if (damage <= 0) return 0;
    const int scaled = static_cast<int>(std::ceil(static_cast<float>(damage) * mRage.damageDealtPercent));
    return std::max(mRage.damageDealtMin, scaled);
}

int BattleResourceRules::ComputeDamageTakenGain(IBattler* defender, int damage) const
{
    if (!defender || damage <= 0) return 0;
    const int maxHp = defender->GetStats().maxHp;
    if (maxHp <= 0) return mRage.damageTakenMin;

    const float hpRatio = static_cast<float>(damage) / static_cast<float>(maxHp);
    const int scaled = static_cast<int>(std::ceil(hpRatio * mRage.damageTakenPercentOfMaxHp));
    return std::max(mRage.damageTakenMin, scaled);
}
