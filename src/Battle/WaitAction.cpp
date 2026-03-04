// ============================================================
// File: WaitAction.cpp
// Responsibility: Implement WaitAction — delta-time accumulator.
// ============================================================
#include "WaitAction.h"

// ------------------------------------------------------------
// Constructor: store the requested duration and zero the elapsed counter.
// Passing duration <= 0 is technically valid but immediately completes —
// use it only if you need a no-op placeholder in the queue.
// ------------------------------------------------------------
WaitAction::WaitAction(float duration)
    : mDuration(duration)
    , mElapsed(0.f)
{
}

// ------------------------------------------------------------
// Execute: add dt to the running total.
//   Returns true the first frame that elapsed >= duration.
//   Never returns true before at least one dt has been added, so even a
//   very short duration (e.g. 0.016 s) waits at least one frame.
// ------------------------------------------------------------
bool WaitAction::Execute(float dt)
{
    mElapsed += dt;
    return mElapsed >= mDuration;
}
