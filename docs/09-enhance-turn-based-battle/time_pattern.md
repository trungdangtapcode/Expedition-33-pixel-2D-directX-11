## Time Pattern Used In This Project

This document explains the clock pattern that supports slow motion without breaking UI timing.

### Problem This Solves

If all systems use one global `dt`, then slow motion also slows menus, transitions, and UI effects. That feels wrong.

We need:

- Gameplay systems to slow down.
- UI and transition timers to stay real-time.

### Architecture

Implemented in:

- `src/Core/Clock.h`
- `src/Core/TimeSystem.h`
- `src/Core/TimeSystem.cpp`

Clock tree:

- `Root` clock (raw frame delta)
- `UI` child clock (always scale `1.0`)
- `Gameplay` child clock (scaled, can be paused/slow-mo)

### Frame Update Order

1. `GameTimer::Tick()` computes raw frame delta.
2. `TimeSystem::Tick(rawDt)` updates clock tree.
3. Game updates consume the appropriate clock:
	- Gameplay code uses gameplay dt.
	- UI/transition code uses UI dt.

### API Summary

- `TimeSystem::Tick(float rawDt)`
- `TimeSystem::GetUIClock().GetDeltaTime()`
- `TimeSystem::GetGameplayClock().GetDeltaTime()`
- `TimeSystem::SetSlowMotion(float scale)`
- `TimeSystem::SetGameplayPaused(bool paused)`

### Pattern In Overworld -> Battle Transition

Relevant file:

- `src/States/OverworldState.cpp`

How it is used:

1. On battle trigger, set gameplay slow motion (`0.25f`).
2. Distortion timer still advances using UI clock dt.
3. Distortion duration remains consistent in real seconds.
4. Before pushing battle, restore gameplay to `1.0f`.

This avoids the common bug where a `0.6s` effect unexpectedly lasts `2.4s` because it was measured with slowed gameplay dt.

### Pause Behavior

`SetGameplayPaused(true)` stores previous gameplay scale and sets gameplay scale to `0.0`.

`SetGameplayPaused(false)` restores the previous scale instead of forcing `1.0`.

This preserves ongoing effects if the player paused during slow motion.

### Common Mistakes To Avoid

1. Calling `TimeSystem::Tick` more than once per frame.
2. Using root dt for gameplay systems.
3. Using gameplay dt for UI transitions that must remain real-time.
4. Forgetting to restore slow motion scale after temporary effects.

