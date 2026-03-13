// ============================================================
// File: Clock.h
// Responsibility: Hierarchical time clock for scaled delta-time delivery.
//
// Architecture — Composite Pattern:
//   Clocks form a tree. Each Clock multiplies the parent's dt by its own
//   timeScale before producing its own scaled dt.  Child clocks are owned
//   and ticked automatically by their parent, so callers only tick the
//   root.
//
//   Root Clock (always 1.0)
//     ├── UI Clock        (always 1.0 — menus unaffected by slow-mo)
//     └── Gameplay Clock  (set to 0.2 for bullet-time, etc.)
//           ├── Player Clock    (optional per-entity scaling)
//           └── Enemy Clock     (optional per-entity scaling)
//
// Lifetime:
//   Created and owned by TimeSystem (Meyers' Singleton).
//   Destroyed when TimeSystem goes out of scope at program exit.
//
// Usage:
//   // In game loop (GameApp):
//   TimeSystem::Get().Tick(rawDt);   // propagates down the whole tree
//
//   // In BattleState::Update():
//   float dt = TimeSystem::Get().GetGameplayClock().GetDeltaTime();
//
//   // Trigger bullet-time for 1 second:
//   TimeSystem::Get().GetGameplayClock().SetTimeScale(0.2f);
//
// Common mistakes:
//   1. Calling GetDeltaTime() before Tick() — returns 0.0f on first frame.
//   2. Storing a raw pointer to a child Clock; the tree is stable but
//      pointers become dangling if you call RemoveChild and destroy nodes.
//   3. Setting timeScale on the ROOT clock — this scales everything including
//      UI; set it on GameplayClock instead.
// ============================================================
#pragma once
#include <vector>
#include <memory>
#include <string>

// ------------------------------------------------------------
// Clock — one node in the hierarchical time tree.
//
// A Clock does NOT use QueryPerformanceCounter directly.  It only stores
// the *scaled* delta-time produced by its parent.  The raw wall-clock
// reading lives exclusively in GameTimer, which passes raw dt to
// TimeSystem::Tick(), which feeds the root clock, which propagates down.
// ------------------------------------------------------------
class Clock
{
public:
    // ------------------------------------------------------------
    // Constructor.
    // debugName — human-readable label used in debug HUD and assertions.
    // timeScale — initial multiplier (1.0 = real time, 0.5 = half speed).
    // ------------------------------------------------------------
    explicit Clock(std::string debugName = "unnamed", float timeScale = 1.0f);

    // Not copyable — clocks own child trees via unique_ptr.
    Clock(const Clock&) = delete;
    Clock& operator=(const Clock&) = delete;

    // ----------------------------------------------------------------
    // Tick — advance this clock and all children by parentDt * timeScale.
    //
    // Called automatically for all children when the parent is ticked.
    // External callers should only tick the ROOT clock via TimeSystem.
    //
    // parentDt — the parent's already-scaled delta-time (seconds).
    //            For the root clock, parentDt is the raw GameTimer dt.
    // ----------------------------------------------------------------
    void Tick(float parentDt);

    // ----------------------------------------------------------------
    // GetDeltaTime — returns this clock's scaled dt from the last Tick().
    //
    // Use this value everywhere time-dependent logic runs inside a system
    // that "belongs" to this clock's domain.
    // ----------------------------------------------------------------
    float GetDeltaTime() const { return mScaledDt; }

    // ----------------------------------------------------------------
    // GetTimeScale / SetTimeScale
    //
    // timeScale = 1.0  → real time
    // timeScale = 0.5  → half speed (slow-motion)
    // timeScale = 0.0  → complete pause for this subtree only
    // timeScale = 2.0  → double speed (fast-forward)
    //
    // The change takes effect on the NEXT Tick() call.
    // ----------------------------------------------------------------
    float GetTimeScale() const           { return mTimeScale; }
    void  SetTimeScale(float timeScale)  { mTimeScale = timeScale; }

    // ----------------------------------------------------------------
    // AddChild — attach a child Clock to this node.
    //
    // Returns a non-owning pointer to the child so callers can keep a
    // reference without taking ownership.  Ownership stays inside the
    // tree (unique_ptr stored in mChildren).
    // ----------------------------------------------------------------
    Clock* AddChild(std::unique_ptr<Clock> child);

    // ----------------------------------------------------------------
    // GetDebugName — used by debug HUD to label the clock.
    // ----------------------------------------------------------------
    const std::string& GetDebugName() const { return mDebugName; }

private:
    std::string                           mDebugName;
    float                                 mTimeScale;  // multiplier applied each frame
    float                                 mScaledDt;   // result after this frame's tick

    // Children are owned by this clock — they are ticked automatically
    // inside Tick() after this clock computes its own mScaledDt.
    std::vector<std::unique_ptr<Clock>>   mChildren;
};
