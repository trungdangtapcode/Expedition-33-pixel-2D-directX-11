// ============================================================
// File: Combatant.cpp
// Responsibility: Implement shared IBattler logic for all battle participants.
// ============================================================
#include "Combatant.h"
#define NOMINMAX           // prevent Windows.h from defining min/max macros
#include <algorithm>    // std::max
#include "../Utils/Log.h"
#include "../Events/EventManager.h"

// Convenience wrapper: LOG() only accepts printf-style args, so convert        
// std::string messages to C-strings before passing to the macro.
static inline void LogStr(const std::string& msg)
{
    LOG("%s", msg.c_str());
}

Combatant::Combatant(std::string name, std::wstring turnViewPath, BattlerStats stats)
    : mName(std::move(name))
    , mTurnViewPath(std::move(turnViewPath))
    , mStats(stats)
{}

const std::string& Combatant::GetName() const { return mName; }
const std::wstring& Combatant::GetTurnViewPath() const { return mTurnViewPath; }
      BattlerStats& Combatant::GetStats()       { return mStats; }
const BattlerStats& Combatant::GetStats() const { return mStats; }

// ------------------------------------------------------------
// TakeDamage
//   Damage reduction formula: effective = max(1, raw - def)
//   Minimum 1 ensures a hit always registers, preventing immortal turtles.
//   Rage is split: attacker gains more (rewarding aggression), defender gains
//   less (rewarding survival + triggering RageSkill after taking punishment).
// ------------------------------------------------------------
void Combatant::TakeDamage(int rawDamage, IBattler* source)
{
    // Clamp to at least 1 so every hit feels impactful.
    const int effective = std::max(1, rawDamage - mStats.def);
    mStats.hp -= effective;
    mStats.ClampHp();

    LOG("%s takes %d damage (raw=%d def=%d) HP=%d/%d",
        mName.c_str(), effective, rawDamage, mStats.def, mStats.hp, mStats.maxHp);

    // Receiver gains rage from the pain of being hit.
    mStats.AddRage(effective / 8);

    // Attacker gains more rage from landing the blow.
    if (source)
    {
        source->GetStats().AddRage(effective / 4);
    }

    // Broadcast HP change immediately so UI reacts precisely when damage occurs.
    const std::string eventName = (mName == "Verso") ? "verso_hp_changed" : mName + "_hp_changed";
    EventData data;
    data.name = eventName;
    data.value = static_cast<float>(mStats.hp);
    EventManager::Get().Broadcast(eventName, data);
}

void Combatant::AddEffect(std::unique_ptr<IStatusEffect> effect)
{
    // Apply the effect immediately to modify live stats, then store it.
    effect->Apply(mStats);
    LOG("%s afflicted with: %s", mName.c_str(), effect->GetName());
    mEffects.push_back(std::move(effect));
}

void Combatant::OnTurnStart()
{
    // Base no-op — subclasses may override for regen, burn, etc.
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
        effect->OnTurnEnd(mStats);
    }
    PurgeExpiredEffects();
}

bool Combatant::IsAlive() const
{
    return mStats.IsAlive();
}

// ------------------------------------------------------------
// PurgeExpiredEffects: revert expired effects and erase from the list.
// Using erase-remove idiom to avoid iterator invalidation.
// ------------------------------------------------------------
void Combatant::PurgeExpiredEffects()
{
    // Walk backwards so we can erase safely without shifting valid indices.
    for (int i = static_cast<int>(mEffects.size()) - 1; i >= 0; --i)
    {
        if (mEffects[i]->IsExpired())
        {
            LOG("%s effect expired: %s", mName.c_str(), mEffects[i]->GetName());
            mEffects[i]->Revert(mStats);
            mEffects.erase(mEffects.begin() + i);
        }
    }
}
