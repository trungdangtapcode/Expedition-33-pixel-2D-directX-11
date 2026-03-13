// ============================================================
// File: TimeSystem.cpp
// Responsibility: Build the Clock tree and expose per-domain delta-times.
//
// Owns: Root Clock (value), UI Clock (child), Gameplay Clock (child).
//
// Lifetime:
//   Created on first TimeSystem::Get() call.
//   Destroyed at program exit (Meyers' Singleton).
//   All Clock children are destroyed automatically via unique_ptr in Clock.
//
// Important:
//   - Call Tick(rawDt) ONCE per frame in GameApp::Run() before any Update().
//   - SetGameplayPaused(true) saves current timeScale so unpausing restores
//     whatever slow-motion was already active.
// ============================================================
#include "TimeSystem.h"
#include "../Utils/Log.h"

// ------------------------------------------------------------
// Function: Get
// Purpose:
//   Return the single TimeSystem instance (Meyers' Singleton).
//   Thread-safe in C++11 due to guaranteed static local initialisation.
// ------------------------------------------------------------
TimeSystem& TimeSystem::Get()
{
    static TimeSystem instance;
    return instance;
}

// ------------------------------------------------------------
// Constructor
// Purpose:
//   Build the Clock tree topology:
//     Root (1.0)
//       ├── UI (1.0)      — menus, HUD, dialogue
//       └── Gameplay (1.0) — world, battle, AI, animation
//
// Why this topology:
//   Setting GameplayClock.SetTimeScale(0.2f) slows everything in gameplay
//   while UI keeps ticking at full speed — menus and health bars remain
//   responsive during bullet-time effects.
// ------------------------------------------------------------
TimeSystem::TimeSystem()
    : mRootClock("Root", 1.0f)
    , mPrePauseScale(1.0f)
    , mGameplayPaused(false)
{
    // Attach UI Clock as a child of Root.
    // Non-owning pointer stored so GetUIClock() has O(1) access.
    mpUIClock = mRootClock.AddChild(
        std::make_unique<Clock>("UI", 1.0f)
    );

    // Attach Gameplay Clock as a child of Root.
    // This is the primary target for slow-motion: SetTimeScale(0.2f) here
    // affects all combat, AI, physics, animation without touching the UI.
    mpGameplayClock = mRootClock.AddChild(
        std::make_unique<Clock>("Gameplay", 1.0f)
    );

    LOG("[TimeSystem] Clock tree built: Root -> UI, Gameplay");
}

// ------------------------------------------------------------
// Function: Tick
// Purpose:
//   Advance the entire clock tree from the raw wall-clock delta-time.
//   The Root clock's timeScale is always 1.0, so it simply passes rawDt
//   down to its children without modification.
//
// Why rawDt is the input (not a timer reference):
//   TimeSystem has no direct dependency on GameTimer — it only needs a
//   float.  This makes the system testable (mock any dt in unit tests)
//   and keeps GameTimer's responsibilities separate.
//
// Must be called once per frame before any state's Update() runs.
// ------------------------------------------------------------
void TimeSystem::Tick(float rawDt)
{
    // Root clock timeScale = 1.0 always, so mRootClock.GetDeltaTime() == rawDt.
    // Children (UI, Gameplay) multiply rawDt by their own timeScale.
    mRootClock.Tick(rawDt);
}

// ------------------------------------------------------------
// Function: SetSlowMotion
// Purpose:
//   Change the Gameplay Clock's timeScale to trigger or cancel slow-motion.
//
// Parameters:
//   scale — new multiplier:
//     1.0 = real time  (cancel slow-mo)
//     0.5 = half speed
//     0.2 = bullet-time
//     0.0 = complete freeze of gameplay
//
// Side effects:
//   If the game is currently paused (SetGameplayPaused(true) was called),
//   this updates mPrePauseScale so the correct speed restores on unpause.
// ------------------------------------------------------------
void TimeSystem::SetSlowMotion(float scale)
{
    if (mGameplayPaused)
    {
        // Game is paused — store the desired scale for when it unpauses.
        // The clock itself stays at 0.0 until unpaused.
        mPrePauseScale = scale;
    }
    else
    {
        mpGameplayClock->SetTimeScale(scale);
        mPrePauseScale = scale;  // keep in sync for future pause/unpause cycles
    }
}

// ------------------------------------------------------------
// Function: SetGameplayPaused
// Purpose:
//   Freeze or resume the entire gameplay subtree.
//   UI clock is unaffected, so menus and HUD remain interactive.
//
// On pause:   saves current timeScale into mPrePauseScale, sets clock to 0.
// On unpause: restores mPrePauseScale (which may be < 1.0 if slow-mo was
//             active at the time of the pause — e.g., player paused during
//             a QTE slow-mo window).
//
// Why save/restore instead of always going back to 1.0:
//   A slow-motion effect might be running (scale = 0.5) when the player
//   opens the pause menu.  Restoring to 1.0 would silently cancel the
//   slow-mo, which is a subtle and hard-to-reproduce bug.
// ------------------------------------------------------------
void TimeSystem::SetGameplayPaused(bool paused)
{
    if (paused == mGameplayPaused) return;  // no-op if already in the requested state
    mGameplayPaused = paused;

    if (paused)
    {
        // Save the current timeScale before zeroing the clock.
        mPrePauseScale = mpGameplayClock->GetTimeScale();
        mpGameplayClock->SetTimeScale(0.0f);
        LOG("[TimeSystem] Gameplay paused (saved scale = %.2f).", mPrePauseScale);
    }
    else
    {
        // Restore the timeScale that was active before the pause.
        mpGameplayClock->SetTimeScale(mPrePauseScale);
        LOG("[TimeSystem] Gameplay resumed (restored scale = %.2f).", mPrePauseScale);
    }
}

// ------------------------------------------------------------
// Function: IsGameplayPaused
// Purpose: Query whether gameplay is currently frozen via SetGameplayPaused.
// Note: A timeScale of 0.0 set via SetSlowMotion() does NOT count as
//       "paused" for the purposes of this flag — only SetGameplayPaused()
//       controls the mGameplayPaused boolean.
// ------------------------------------------------------------
bool TimeSystem::IsGameplayPaused() const
{
    return mGameplayPaused;
}
