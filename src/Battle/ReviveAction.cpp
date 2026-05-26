// ============================================================
// File: ReviveAction.cpp
// Responsibility: Implement KO recovery as an action-queue mutation.
// ============================================================
#define NOMINMAX
#include "ReviveAction.h"
#include "BattleEvents.h"
#include "CombatantAnim.h"
#include "IBattler.h"
#include "../Events/EventManager.h"
#include "../Utils/Log.h"
#include <algorithm>

ReviveAction::ReviveAction(IBattler* target, int hpAmount)
    : mTarget(target)
    , mHpAmount(hpAmount)
{
}

// ------------------------------------------------------------
// Function: Execute
// Purpose:
//   Restore a defeated target to positive HP and request idle animation.
// Why:
//   A revived combatant otherwise remains visually frozen on the death
//   frame even though its HP is above zero.
// Parameters:
//   dt - Unused because this action completes immediately.
// Caveats:
//   - Alive targets are ignored so revive cannot become a stronger heal.
// ------------------------------------------------------------
bool ReviveAction::Execute(float /*dt*/)
{
    if (!mTarget || mHpAmount <= 0) return true;
    if (mTarget->IsAlive())
    {
        LOG("[ReviveAction] Ignored revive on living target '%s'.",
            mTarget->GetDebugName().c_str());
        return true;
    }

    BattlerStats& stats = mTarget->GetStats();
    stats.hp = mHpAmount;
    stats.ClampHp();

    PlayAnimPayload animPayload{ mTarget, CombatantAnim::Idle };
    EventData animEvent;
    animEvent.payload = &animPayload;
    EventManager::Get().Broadcast("battler_play_anim", animEvent);

    LOG("[ReviveAction] %s revived at %d HP.",
        mTarget->GetDebugName().c_str(),
        stats.hp);
    return true;
}
