// ============================================================
// File: CleanseAction.cpp
// Responsibility: Implement status cleanup as an action-queue mutation.
// ============================================================
#include "CleanseAction.h"
#include "IBattler.h"
#include "../Utils/Log.h"

CleanseAction::CleanseAction(IBattler* target)
    : mTarget(target)
{
}

// ------------------------------------------------------------
// Function: Execute
// Purpose:
//   Remove every active status effect from one target.
// Why:
//   Status effects own stat modifiers, and clearing them through the
//   combatant guarantees Revert() runs before the objects are released.
// Parameters:
//   dt - Unused because this action completes immediately.
// ------------------------------------------------------------
bool CleanseAction::Execute(float /*dt*/)
{
    if (!mTarget) return true;
    mTarget->ClearAllStatusEffects();
    LOG("[CleanseAction] Cleansed status effects from %s.",
        mTarget->GetDebugName().c_str());
    return true;
}
