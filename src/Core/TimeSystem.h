// ============================================================
// File: TimeSystem.h
// Responsibility: Singleton that owns the hierarchical Clock tree and
//   exposes named clocks for each subsystem.
//
// Clock tree topology:
//
//   Root Clock  (timeScale = 1.0, always mirrors raw GameTimer dt)
//     ├── UI Clock        (timeScale = 1.0 — menus / HUD unaffected by slow-mo)
//     └── Gameplay Clock  (timeScale = 1.0 by default; set < 1 for slow-motion)
//
// Lifetime:
//   Meyers' Singleton — created on first Get(), destroyed at program exit.
//   GameTimer is NOT replaced; TimeSystem reads GameTimer::DeltaTime() via
//   GameApp and calls TimeSystem::Tick(rawDt) once per frame.
//
// Usage examples:
//
//   // GameApp game loop (once per frame):
//   TimeSystem::Get().Tick(mTimer.DeltaTime());
//
//   // BattleState (affected by slow-motion):
//   float dt = TimeSystem::Get().GetGameplayClock().GetDeltaTime();
//
//   // MenuState (never slowed down):
//   float dt = TimeSystem::Get().GetUIClock().GetDeltaTime();
//
//   // Trigger bullet-time (e.g., on QTE activation):
//   TimeSystem::Get().SetSlowMotion(0.2f);    // 5x slower
//
//   // Restore normal speed:
//   TimeSystem::Get().SetSlowMotion(1.0f);
//
//   // Pause the game world (UI/menus still run):
//   TimeSystem::Get().SetGameplayPaused(true);
//
// Common mistakes:
//   1. Calling Tick() twice per frame — dt will be doubled.
//   2. Using GetRootClock().GetDeltaTime() in gameplay code — you skip
//      the slow-motion and pause applied to the Gameplay Clock.
//   3. Forgetting to call Tick() before any state calls GetDeltaTime() —
//      all clocks return 0 until the first Tick().
// ============================================================
#pragma once
#include "Clock.h"

class TimeSystem
{
public:
    // Meyers' Singleton — thread-safe in C++11.
    static TimeSystem& Get();

    // Not copyable or movable.
    TimeSystem(const TimeSystem&) = delete;
    TimeSystem& operator=(const TimeSystem&) = delete;

    // ----------------------------------------------------------------
    // Tick — advance the entire clock tree from raw wall-clock dt.
    //
    // Must be called ONCE per frame in GameApp::Run(), immediately
    // after GameTimer::Tick(), before any state Update() call.
    //
    // rawDt — raw, unscaled delta-time from GameTimer::DeltaTime() (seconds).
    // ----------------------------------------------------------------
    void Tick(float rawDt);

    // ----------------------------------------------------------------
    // Clock accessors — return a reference to the named clock node.
    //
    // Use GetGameplayClock() in all game-logic systems (battle, movement,
    // animation, AI) so they automatically respect slow-motion and pause.
    //
    // Use GetUIClock() in menus, HUD, and dialogue text scrolling so they
    // keep running at full speed even when gameplay is paused.
    // ----------------------------------------------------------------
    Clock& GetRootClock()     { return mRootClock; }
    Clock& GetUIClock()       { return *mpUIClock; }
    Clock& GetGameplayClock() { return *mpGameplayClock; }

    // ----------------------------------------------------------------
    // Convenience helpers — these are thin wrappers over SetTimeScale()
    // on the Gameplay Clock, provided so call sites read like English.
    //
    // SetSlowMotion(scale):
    //   scale = 1.0 → real time (cancels slow-mo)
    //   scale = 0.5 → half speed (stylish dodge slow-mo)
    //   scale = 0.2 → bullet-time (Max Payne style)
    //   scale = 0.0 → freeze gameplay (boss intro cutscene pause)
    //
    // SetGameplayPaused(true/false):
    //   Equivalent to SetSlowMotion(0.0) / SetSlowMotion(mPreviousScale).
    //   Restores the previous timeScale when unpausing, so a 0.5x slow-mo
    //   correctly resumes instead of snapping back to 1.0.
    // ----------------------------------------------------------------
    void SetSlowMotion(float scale);
    void SetGameplayPaused(bool paused);
    bool IsGameplayPaused() const;

private:
    // Private constructor — only Get() may instantiate.
    TimeSystem();

    // The root clock is a value member so no allocation is needed.
    // It owns UI and Gameplay clocks as children via AddChild().
    Clock  mRootClock;

    // Non-owning observer pointers into the clock tree.
    // They are set once in the constructor and remain valid for the
    // lifetime of the process (Meyers' Singleton never dies early).
    Clock* mpUIClock       = nullptr;
    Clock* mpGameplayClock = nullptr;

    // Saved timeScale so SetGameplayPaused(false) can restore it
    // instead of unconditionally returning to 1.0.
    float  mPrePauseScale  = 1.0f;
    bool   mGameplayPaused = false;
};
