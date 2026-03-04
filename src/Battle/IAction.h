// ============================================================
// File: IAction.h
// Responsibility: Pure virtual interface for one atomic combat step.
//
// Implemented by:
//   DamageAction       — apply damage + rage to combatants
//   StatusEffectAction — attach an IStatusEffect to a target
//   WaitAction         — pause the queue for N seconds (animation time)
//   LogAction          — write a message to the battle log
//
// Contract:
//   Execute(dt) is called every frame by ActionQueue.
//   Return true when the action is fully complete and the queue may advance.
//   Instantaneous actions (DamageAction, StatusEffectAction) return true
//   on the very first call. Time-based actions (WaitAction) return false
//   until their timer expires.
//
// Ownership:
//   ActionQueue owns all actions via unique_ptr<IAction>.
// ============================================================
#pragma once

class IAction
{
public:
    virtual ~IAction() = default;

    // ------------------------------------------------------------
    // Execute: perform one frame of work.
    //   dt   — frame delta-time in seconds (for animated / timed actions)
    //   Returns true when this action is complete and may be dequeued.
    // ------------------------------------------------------------
    virtual bool Execute(float dt) = 0;
};
