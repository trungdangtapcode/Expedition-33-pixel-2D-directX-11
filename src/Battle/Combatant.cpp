// ============================================================
// File: Combatant.cpp
// Responsibility: Implement shared IBattler logic for all battle participants.
// ============================================================
#include "Combatant.h"
#define NOMINMAX           // prevent Windows.h from defining min/max macros
#include <algorithm>    // std::max
#include "../Utils/Log.h"
#include "../Events/EventManager.h"
#include "BattleEvents.h"
#include "CombatantAnim.h"
#include "IAction.h"

// Convenience wrapper: LOG() only accepts printf-style args, so convert        
// std::string messages to C-strings before passing to the macro.
static inline void LogStr(const std::string& msg)
{
    LOG("%s", msg.c_str());
}

Combatant::Combatant(std::string name,
                     std::wstring turnViewPath,
                     BattlerStats stats,
                     std::string debugName)
    : mName(std::move(name))
    , mDebugName(debugName.empty() ? mName : std::move(debugName))
    , mTurnViewPath(std::move(turnViewPath))
    , mStats(stats)
{}

const std::string& Combatant::GetName() const { return mName; }
const std::string& Combatant::GetDebugName() const { return mDebugName; }
const std::wstring& Combatant::GetTurnViewPath() const { return mTurnViewPath; }
      BattlerStats& Combatant::GetStats()       { return mStats; }
const BattlerStats& Combatant::GetStats() const { return mStats; }

// ------------------------------------------------------------
// TakeDamage
//   Receives pre-calculated effective damage from IDamageCalculator evaluation.
//   Resource gains are applied by the calling damage action after this method
//   completes, because the action knows which resource rules should apply.
// ------------------------------------------------------------
void Combatant::TakeDamage(const DamageResult& result, IBattler* /*source*/)
{
    mStats.hp -= result.effectiveDamage;
    mStats.ClampHp();

    LOG("%s takes %d damage (raw=%d def=%d) HP=%d/%d",
        mDebugName.c_str(), result.effectiveDamage, result.rawDamage, result.defenseUsed, mStats.hp, mStats.maxHp);

    // Broadcast HP change immediately so UI reacts precisely when damage occurs.
    const std::string eventName = (mName == "Verso") ? "verso_hp_changed" : mName + "_hp_changed";
    EventData data;
    data.name = eventName;
    data.value = static_cast<float>(mStats.hp);
    EventManager::Get().Broadcast(eventName, data);

    // Broadcast death animation globally triggering fallback contracts organically
    if (mStats.hp <= 0)
    {
        PlayAnimPayload animPayload{ this, CombatantAnim::Die };
        EventData edAnim;
        edAnim.payload = &animPayload;
        EventManager::Get().Broadcast("battler_play_anim", edAnim);
    }

    // Broadcast generic damage taken for floating text
    DamageTakenPayload dmgPayload;
    dmgPayload.target = this;
    dmgPayload.damage = result.effectiveDamage;
    dmgPayload.isCrit = result.isCritical;
    // We don't have perfect qte info here, but we can set it to false and rely on isCrit mostly
    dmgPayload.isPerfectQte = false; 
    
    EventData dmgData;
    dmgData.payload = &dmgPayload;
    EventManager::Get().Broadcast("battler_damage_taken", dmgData);
}

void Combatant::AddEffect(std::unique_ptr<IStatusEffect> effect)
{
    if (!effect) return;

    for (auto& active : mEffects)
    {
        if (active && active->GetId() == effect->GetId() &&
            active->TryMergeFrom(*this, *effect))
        {
            LOG("%s refreshed effect: %s", mDebugName.c_str(), active->GetName());
            return;
        }
    }

    // Apply the effect immediately - it may push stat modifiers via
    // AddStatModifier or begin a countdown.  Then store it.
    effect->Apply(*this);
    LOG("%s afflicted with: %s", mDebugName.c_str(), effect->GetName());
    mEffects.push_back(std::move(effect));
}

std::vector<StatusEffectView> Combatant::GetStatusEffectViews() const
{
    std::vector<StatusEffectView> views;
    views.reserve(mEffects.size());
    for (const auto& effect : mEffects)
    {
        if (effect) views.push_back(effect->GetView());
    }
    return views;
}

// ------------------------------------------------------------
// ClearAllStatusEffects: Revert() every active effect then drop them.
// Used by Cleanse items and by any "reset combatant state" flow.
//
// Revert() MUST run before erasing so StatModifier entries pushed in
// Apply() are stripped from the battler - otherwise a buff's bonus
// would persist forever when an effect is removed mid-duration.
// ------------------------------------------------------------
void Combatant::ClearAllStatusEffects()
{
    if (mEffects.empty()) return;

    for (auto& effect : mEffects)
    {
        LOG("%s cleansed of: %s", mDebugName.c_str(), effect->GetName());
        effect->Revert(*this);
    }
    mEffects.clear();
}

// ------------------------------------------------------------
// AddStatModifier: append a fully-constructed modifier.
// No deduplication - an effect that wants to replace an earlier
// modifier must call RemoveStatModifiersBySource first.
// ------------------------------------------------------------
void Combatant::AddStatModifier(const StatModifier& mod)
{
    mStatModifiers.push_back(mod);
}

// ------------------------------------------------------------
// RemoveStatModifiersBySource: strip every modifier that was pushed
// by the effect instance identified by sourceId.  Called by effect
// Revert() implementations; also safe to call on already-empty lists.
// ------------------------------------------------------------
void Combatant::RemoveStatModifiersBySource(int sourceId)
{
    // Skip the work when sourceId is 0 - that is the "unassigned" sentinel
    // and would match every default-constructed modifier, which is wrong.
    if (sourceId == 0) return;

    mStatModifiers.erase(
        std::remove_if(mStatModifiers.begin(), mStatModifiers.end(),
            [sourceId](const StatModifier& m) { return m.sourceId == sourceId; }),
        mStatModifiers.end());
}

const std::vector<StatModifier>& Combatant::GetStatModifiers() const
{
    return mStatModifiers;
}

void Combatant::OnTurnStart()
{
    // Base no-op - subclasses may override for regen, burn, etc.
}

std::vector<std::unique_ptr<IAction>> Combatant::BuildTurnStartActions(const BattleContext& ctx)
{
    std::vector<std::unique_ptr<IAction>> actions;
    for (auto& effect : mEffects)
    {
        if (!effect) continue;

        auto effectActions = effect->BuildTurnStartActions(*this, ctx);
        for (auto& action : effectActions)
        {
            actions.push_back(std::move(action));
        }
    }
    return actions;
}

// ------------------------------------------------------------
// OnTurnEnd: decrement all effect durations, then purge expired ones.
// Order matters: run OnTurnEnd first so the effect's last-tick logic fires
// before Revert() strips the stat modification.
// ------------------------------------------------------------
void Combatant::OnTurnEnd()
{
    for (auto& effect : mEffects)
    {
        effect->OnTurnEnd(*this);
    }
    PurgeExpiredEffects();
}

bool Combatant::IsAlive() const
{
    return mStats.IsAlive();
}

// ------------------------------------------------------------
// PurgeExpiredEffects: revert expired effects and erase from the list.
// Walks backwards so indices stay valid after erase().
// ------------------------------------------------------------
void Combatant::PurgeExpiredEffects()
{
    for (int i = static_cast<int>(mEffects.size()) - 1; i >= 0; --i)
    {
        if (mEffects[i]->IsExpired())
        {
            LOG("%s effect expired: %s", mDebugName.c_str(), mEffects[i]->GetName());
            mEffects[i]->Revert(*this);
            mEffects.erase(mEffects.begin() + i);
        }
    }
}
