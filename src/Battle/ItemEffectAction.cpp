// ============================================================
// File: ItemEffectAction.cpp
// Responsibility: Dispatch one ItemEffectKind to one target.
//                 Completes instantaneously.
// ============================================================
// NOMINMAX MUST come before any header that may transitively include
// Windows.h (Log.h does, via OutputDebugStringA).  Without it, Win32
// macros for min/max shadow std::max and produce a parser error here.
#define NOMINMAX
#include <algorithm>
#include <memory>

#include "ItemEffectAction.h"
#include "TimedStatBuffEffect.h"
#include "BattlerStats.h"
#include "IDamageCalculator.h"
#include "../Utils/Log.h"

ItemEffectAction::ItemEffectAction(IBattler* target, ItemData effect)
    : mTarget(target)
    , mEffect(std::move(effect))
{}

// ------------------------------------------------------------
// Execute: switch on the effect kind.  Every branch is a handful
// of lines — no per-kind subclass needed because the behaviors are
// all simple stat mutations.
//
// Returns true unconditionally: items never span multiple frames.
// ------------------------------------------------------------
bool ItemEffectAction::Execute(float /*dt*/)
{
    if (!mTarget)
    {
        LOG("[ItemEffectAction] WARNING: null target for '%s'.", mEffect.id.c_str());
        return true;   // drop the action so the queue advances
    }

    BattlerStats& s = mTarget->GetStats();

    switch (mEffect.effect)
    {
    case ItemEffectKind::HealHp:
    {
        // Heal alive targets only — HealHp on a KO'd ally is a bug the
        // player should be told about (Revive has its own effect kind).
        if (!mTarget->IsAlive())
        {
            LOG("[Item] '%s' cannot heal a fallen ally.", mEffect.id.c_str());
            break;
        }
        const int before = s.hp;
        s.hp += mEffect.amount;
        s.ClampHp();
        LOG("[Item] %s restores %d HP to %s (%d -> %d).",
            mEffect.name.c_str(),
            s.hp - before,
            mTarget->GetName().c_str(),
            before, s.hp);
        break;
    }

    case ItemEffectKind::HealMp:
    {
        const int before = s.mp;
        s.mp += mEffect.amount;
        if (s.mp > s.maxMp) s.mp = s.maxMp;
        LOG("[Item] %s restores %d MP to %s (%d -> %d).",
            mEffect.name.c_str(),
            s.mp - before,
            mTarget->GetName().c_str(),
            before, s.mp);
        break;
    }

    case ItemEffectKind::FullHeal:
    {
        s.hp = s.maxHp;
        s.mp = s.maxMp;
        LOG("[Item] %s fully restores %s.",
            mEffect.name.c_str(), mTarget->GetName().c_str());
        break;
    }

    case ItemEffectKind::Revive:
    {
        if (mTarget->IsAlive())
        {
            LOG("[Item] '%s' does nothing — %s is already conscious.",
                mEffect.id.c_str(), mTarget->GetName().c_str());
            break;
        }
        // Start from 0 and add the revive amount so ClampHp handles caps.
        s.hp = mEffect.amount;
        s.ClampHp();
        LOG("[Item] %s revives %s at %d HP.",
            mEffect.name.c_str(), mTarget->GetName().c_str(), s.hp);
        break;
    }

    case ItemEffectKind::RestoreRage:
    {
        // AddRage is a no-op on combatants with maxRage == 0 (enemies),
        // so using a rage item on an enemy silently wastes the item.
        s.AddRage(mEffect.amount);
        LOG("[Item] %s restores %d rage to %s (now %d/%d).",
            mEffect.name.c_str(), mEffect.amount,
            mTarget->GetName().c_str(), s.rage, s.maxRage);
        break;
    }

    case ItemEffectKind::DealDamage:
    {
        if (!mTarget->IsAlive()) break;
        // Build a DamageResult by hand — item damage bypasses ATK/DEF
        // scaling by convention.  rawDamage == effectiveDamage == amount
        // so the UI shows the full value the JSON author typed.
        DamageResult result;
        result.rawDamage       = mEffect.amount;
        result.defenseUsed     = 0;
        result.effectiveDamage = std::max(1, mEffect.amount);
        result.isCritical      = false;
        // source is nullptr — items don't grant rage on hit.
        mTarget->TakeDamage(result, nullptr);
        LOG("[Item] %s hits %s for %d.",
            mEffect.name.c_str(), mTarget->GetName().c_str(),
            result.effectiveDamage);
        break;
    }

    case ItemEffectKind::StatBuff:
    {
        if (!mTarget->IsAlive()) break;
        // Attach a TimedStatBuffEffect.  AddEffect runs Apply() which
        // pushes the StatModifier immediately.
        mTarget->AddEffect(std::make_unique<TimedStatBuffEffect>(
            mEffect.buffStat,
            mEffect.buffOp,
            mEffect.buffValue,
            mEffect.durationTurns));
        LOG("[Item] %s buffs %s for %d turns.",
            mEffect.name.c_str(), mTarget->GetName().c_str(),
            mEffect.durationTurns);
        break;
    }

    case ItemEffectKind::Cleanse:
    {
        mTarget->ClearAllStatusEffects();
        LOG("[Item] %s cleanses %s.",
            mEffect.name.c_str(), mTarget->GetName().c_str());
        break;
    }
    }

    return true;   // items never span multiple frames
}
