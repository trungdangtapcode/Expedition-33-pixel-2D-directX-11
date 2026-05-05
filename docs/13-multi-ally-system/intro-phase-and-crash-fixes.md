# Intro Phase Refactor and Crash Fixes

## Overview
This document covers the structural fixes made to resolve an animation state bug during the battle intro sequence, as well as a dangling listener crash encountered when repeatedly fleeing and re-entering battles.

## 1. Intro Animation Bug ("Walk" forced to "FightState")

### The Problem
During the battle initialization (`INIT` phase), characters are assigned a `MoveAction` configured to play the `CombatantAnim::Walk` animation as they enter the screen. 

However, after assigning the actions, the `BattleManager` immediately transitioned to the `BattlePhase::RESOLVING` state to drain the action queue. Because the engine was technically in the `RESOLVING` phase, `BattleState::UpdateLogic` evaluated `inStance = true` for the first active player (e.g., Verso). This caused `CombatantStanceState` to forcefully override the animation to `CombatantAnim::FightState` (which looks like a crouched battle-move), overriding the requested walk animation.

### The Solution
A dedicated **`BattlePhase::INTRO`** was introduced:
- `BattleManager` now transitions to `INTRO` instead of `RESOLVING` after setting up the walk-in actions.
- `HandleIntro(float dt)` was created to exclusively drain the walk-in action queue.
- Because `INTRO` is separate from `RESOLVING`, `BattleState::UpdateLogic` no longer flags the characters as being in an active combat stance (`inStance = false`), allowing the `MoveAction` to play the `Walk` animation unimpeded.
- Once the queue empties, `HandleIntro()` calls `AdvanceTurn()` to properly calculate Action Values and enter the first `PLAYER_TURN` or `ENEMY_TURN`.

## 2. Re-entry Crash (Dangling Listener)

### The Problem
When the player fled a battle and engaged a new enemy, the game would crash specifically when a Skeleton enemy attempted to launch a Bullet Hell attack. 

This was caused by a dangling Event Manager subscription. The `BattleState` subscribed `mBulletHellStateListener` to the `"verso_bullet_hell_state"` event upon entry. However, when the player fled (popping the `BattleState`), the state was destroyed but the listener was **never unsubscribed**. When the next battle fired the event, the Event Manager invoked a callback on a destroyed object, leading to an access violation.

### The Solution
The `mBulletHellStateListener` is now explicitly unsubscribed in `BattleState::OnExit()`.

```cpp
void BattleState::OnExit()
{
    // ...
    EventManager::Get().Unsubscribe("verso_bullet_hell_state", mBulletHellStateListener);
    // ...
}
```

## 3. Supplementary Fixes
- **DeltaTime Clamping:** Updated `GameTimer.cpp` to explicitly enforce an `mDeltaTime` ceiling of `0.1s` (alongside the existing `0.0s` floor). This prevents extreme frame drops from causing physics tunneling or instantly completing queued actions.
- **Asset Integrity:** Added missing clip mappings to `maelle.json` to ensure the character has a complete animation suite parity with Verso.
