// ============================================================
// File: WaitAction.h
// Responsibility: Time-based action that blocks the ActionQueue for a
//   fixed duration, then completes.
//
// Primary use case:
//   Post-action pause — gives the player time to read log text, see a
//   hit animation land, or simply feel the weight of the turn before the
//   next action begins.
//
// Future extension:
//   Replace a WaitAction with an AnimationAction that holds the queue until
//   a sprite clip finishes, then optionally idles for a remaining delta.
//
// Ownership:
//   Owned by ActionQueue via unique_ptr<IAction>.
//   Holds no pointers to other objects — safe to cancel/clear at any time.
// ============================================================
#pragma once
#include "IAction.h"

class WaitAction : public IAction
{
public:
    // duration — seconds to block the queue. Must be > 0.
    explicit WaitAction(float duration);

    // ------------------------------------------------------------
    // Execute: accumulate dt each frame.
    //   Returns true when total elapsed time >= mDuration.
    //   The queue will not advance to the next action until this returns true.
    // ------------------------------------------------------------
    bool Execute(float dt) override;

    // Returns the configured wait duration (useful for debug logging).
    float GetDuration() const { return mDuration; }

private:
    float mDuration;    // total pause length in seconds
    float mElapsed;     // time accumulated so far this wait
};
