// ============================================================
// File: Clock.cpp
// Responsibility: Implement the hierarchical Clock node (see Clock.h).
//
// All logic is intentionally minimal.  The Clock class is a thin data
// carrier; the interesting coordination lives in TimeSystem.
// ============================================================
#include "Clock.h"

// ------------------------------------------------------------
// Constructor
// debugName — label for debug tools and assertion messages.
// timeScale — initial scale factor (1.0 = real time).
// mScaledDt starts at 0.0 so the first call to GetDeltaTime()
// before any Tick() is safe (returns 0, no motion on frame 0).
// ------------------------------------------------------------
Clock::Clock(std::string debugName, float timeScale)
    : mDebugName(std::move(debugName))
    , mTimeScale(timeScale)
    , mScaledDt(0.0f)
{
}

// ------------------------------------------------------------
// Function: Tick
// Purpose:
//   Compute this clock's scaled delta-time from the parent's dt,
//   then propagate to every child clock in the subtree.
//
// Why propagate before children tick:
//   Children receive THIS clock's already-scaled dt as their
//   "parentDt".  Each child then multiplies it by its own timeScale.
//   This creates the cascade effect described in time-system.md §1:
//     root(1.0) → gameplay(0.2) → enemy(1.0)  ⟹  enemy dt = raw*0.2
//
// Parameters:
//   parentDt — the caller's (parent's) scaled delta-time in seconds.
//              For the root clock this is the raw GameTimer dt.
// ------------------------------------------------------------
void Clock::Tick(float parentDt)
{
    // Multiply parent's dt by this clock's scale to get our scaled dt.
    // When mTimeScale = 0.0, this clock and its entire subtree are frozen.
    mScaledDt = parentDt * mTimeScale;

    // Propagate to all children so the whole subtree updates in one call.
    // Children multiply mScaledDt by their own timeScale, chaining the effect.
    for (auto& child : mChildren)
    {
        child->Tick(mScaledDt);
    }
}

// ------------------------------------------------------------
// Function: AddChild
// Purpose:
//   Transfer ownership of a child Clock into this node's child list.
//   The child will be ticked automatically as part of this node's subtree.
//
// Returns a non-owning (observer) pointer so the caller can retain a
// direct reference without taking ownership.  The pointer remains valid
// as long as the Clock tree is alive (TimeSystem lifetime = process lifetime).
// ------------------------------------------------------------
Clock* Clock::AddChild(std::unique_ptr<Clock> child)
{
    Clock* raw = child.get();   // save observer pointer before move
    mChildren.push_back(std::move(child));
    return raw;
}
