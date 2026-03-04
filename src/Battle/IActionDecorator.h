// ============================================================
// File: IActionDecorator.h
// Responsibility: Pure virtual interface for wrapping an IAction with
//   additional before/after behaviour.
//
// Design rationale:
//   The ActionQueue drives combat through IAction::Execute(dt).
//   Any cross-cutting concern — post-action delay, screen shake, animation
//   wait, sound cue, particle burst — should NOT be baked into the concrete
//   action class itself (DamageAction should only deal damage).
//
//   IActionDecorator follows the Decorator pattern:
//     1. It IS an IAction  (queue doesn't know it's a wrapper)
//     2. It HAS an IAction (the real work to perform)
//     3. Subclasses override OnBefore/OnAfter hooks to inject behaviour
//
// Extension examples:
//   DelayedAction    — run inner action, then wait N seconds
//   AnimationAction  — play a clip, wait for it to finish, then run inner
//   ScreenFlashAction— flash the screen for one frame, then run inner
//
// Lifetime:
//   Owned by ActionQueue via unique_ptr<IAction>.
//   Takes ownership of the wrapped action via unique_ptr<IAction>.
// ============================================================
#pragma once
#include "IAction.h"
#include <memory>

class IActionDecorator : public IAction
{
public:
    // inner — the action being decorated. Must not be null.
    explicit IActionDecorator(std::unique_ptr<IAction> inner);

    virtual ~IActionDecorator() = default;

    // ------------------------------------------------------------
    // Execute: three-phase template method.
    //   Phase 1 — OnBefore(dt): called once before the inner action starts.
    //             Return false to keep repeating OnBefore (e.g. play intro anim).
    //             Return true when ready to hand off to the inner action.
    //   Phase 2 — inner->Execute(dt): runs until it returns true.
    //   Phase 3 — OnAfter(dt): called each frame after inner completes.
    //             Return false to keep waiting (e.g. delay, outro anim).
    //             Return true to signal that the whole decorator is done.
    //
    //   Default implementations: OnBefore returns true immediately (no-op),
    //   OnAfter returns true immediately (no-op).
    //   Override only the hooks you need.
    // ------------------------------------------------------------
    bool Execute(float dt) final;

protected:
    // Override to inject behaviour BEFORE the inner action runs.
    // Default: no-op (returns true immediately on first call).
    virtual bool OnBefore(float dt);

    // Override to inject behaviour AFTER the inner action completes.
    // Default: no-op (returns true immediately on first call).
    virtual bool OnAfter(float dt);

    // Access the wrapped action (for subclasses that need to read its state).
    IAction* GetInner() const { return mInner.get(); }

private:
    std::unique_ptr<IAction> mInner;

    // Execution phase tracker — avoids branching on bool flags in Execute.
    enum class Phase { Before, Inner, After };
    Phase mPhase = Phase::Before;
};
