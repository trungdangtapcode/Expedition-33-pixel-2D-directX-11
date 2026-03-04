// ============================================================
// File: IActionDecorator.cpp
// Responsibility: Drive the Before→Inner→After three-phase template method.
// ============================================================
#include "IActionDecorator.h"

IActionDecorator::IActionDecorator(std::unique_ptr<IAction> inner)
    : mInner(std::move(inner))
    , mPhase(Phase::Before)
{
}

// ------------------------------------------------------------
// Execute: advance through Before → Inner → After sequentially.
//   Each phase delegates to its hook/method and moves to the next only when
//   that phase signals completion (returns true).
//   The whole decorator is complete when OnAfter returns true.
// ------------------------------------------------------------
bool IActionDecorator::Execute(float dt)
{
    // Phase 1: run OnBefore until it signals it's done.
    if (mPhase == Phase::Before)
    {
        if (!OnBefore(dt)) return false;    // still in pre-phase
        mPhase = Phase::Inner;
    }

    // Phase 2: run the wrapped action until it completes.
    if (mPhase == Phase::Inner)
    {
        if (!mInner->Execute(dt)) return false; // inner still running
        mPhase = Phase::After;
    }

    // Phase 3: run OnAfter until it signals it's done.
    // mPhase == Phase::After is implied here.
    return OnAfter(dt);
}

// Default hooks — both complete immediately (no-op).
bool IActionDecorator::OnBefore(float /*dt*/) { return true; }
bool IActionDecorator::OnAfter (float /*dt*/) { return true; }
