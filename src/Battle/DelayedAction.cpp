// ============================================================
// File: DelayedAction.cpp
// Responsibility: Post-action pause via the IActionDecorator::OnAfter hook.
// ============================================================
#include "DelayedAction.h"

// ------------------------------------------------------------
// Constructor: forward the inner action to IActionDecorator,
//   store the post-delay duration and zero the elapsed counter.
// ------------------------------------------------------------
DelayedAction::DelayedAction(std::unique_ptr<IAction> inner, float postDelay)
    : IActionDecorator(std::move(inner))
    , mPostDelay(postDelay)
    , mElapsed(0.f)
{
}

// ------------------------------------------------------------
// OnAfter: called every frame after the inner action completes.
//   Accumulates dt until mPostDelay is reached, then returns true
//   to release the ActionQueue to the next action.
//
//   This is the hook to replace with animation/VFX in the future:
//     1. Trigger the animation on the first call (mElapsed == 0 before +=).
//     2. Wait for it to finish (renderer signals back via a flag/event).
//     3. Return true when both the animation is done AND the minimum delay
//        has elapsed — whichever is longer.
// ------------------------------------------------------------
bool DelayedAction::OnAfter(float dt)
{
    mElapsed += dt;
    return mElapsed >= mPostDelay;
}
