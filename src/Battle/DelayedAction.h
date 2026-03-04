// ============================================================
// File: DelayedAction.h
// Responsibility: Decorator that runs an inner IAction, then holds the
//   ActionQueue for a configurable post-action delay before completing.
//
// Purpose:
//   Provides the visual "breathing room" between combat actions so the
//   player can read the log, see hit reactions, and feel turn weight.
//   Currently manifests as a plain pause; future work can replace the
//   OnAfter hook with an animation/VFX wait without changing callers.
//
// Usage:
//   // Wrap any IAction with a 1-second post-delay:
//   mQueue.Enqueue(std::make_unique<DelayedAction>(std::move(action)));
//
//   // Custom delay:
//   mQueue.Enqueue(std::make_unique<DelayedAction>(std::move(action), 0.5f));
//
// Extension guide:
//   To add a pre-action animation, subclass DelayedAction and override
//   OnBefore(dt) — accumulate dt until the clip finishes, return true.
//   The inner action will not start until OnBefore returns true.
//
// Ownership:
//   Takes ownership of the wrapped IAction via unique_ptr (moved in).
//   Owned by ActionQueue via unique_ptr<IAction>.
// ============================================================
#pragma once
#include "IActionDecorator.h"

class DelayedAction : public IActionDecorator
{
public:
    // Default post-delay matches kDefaultDelay — currently 1.0 seconds.
    // Pass an explicit delay to override (e.g. fast attacks = 0.4 s).
    static constexpr float kDefaultDelay = 1.0f;

    explicit DelayedAction(std::unique_ptr<IAction> inner,
                           float postDelay = kDefaultDelay);

protected:
    // ------------------------------------------------------------
    // OnAfter: block the queue for mPostDelay seconds after the inner
    //   action completes. Returns true when the timer expires.
    //
    // Future customization points:
    //   - Play a hit-reaction animation and wait for it here.
    //   - Trigger a screen shake and wait for it to settle.
    //   - Show a floating damage number and wait for it to fade.
    //   When those features exist, override this method in a subclass
    //   and call the base WaitAction timer as a fallback minimum.
    // ------------------------------------------------------------
    bool OnAfter(float dt) override;

private:
    float mPostDelay;   // total wait after inner action (seconds)
    float mElapsed;     // time accumulated since inner completed
};
