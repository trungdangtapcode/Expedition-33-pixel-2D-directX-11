# Combatant Stance State Machine

## 1. Problem Statement
Combatants need to smoothly transition their animation states based on the battle logic phases. Specifically, when a player is selecting a command/skill, or actively attacking, they should step forward and enter a combat ready stance. 

The desired animation flow is:
`idle` -> `ready` (transitional) -> `fight-state` (looping) -> `unready` (transitional) -> `idle`

Managing this flow directly in `BattleState` or `BattleRenderer` using boolean flags (e.g., `isReadying`, `isFighting`) and clip completion checks creates a monolithic block of conditionals ("spaghetti code"). 

## 2. Solution: GoF State Pattern
To keep the presentation layer completely decoupled and scalable, we implemented the **State Pattern** (Gang of Four) across the combatant sprite rendering pipeline.

The `BattleRenderer` tracks an instance of `ICombatantStanceState` for each active slot. The `BattleState` merely provides the "intent" (e.g., "this character should be in a fighting stance now"). The states themselves dictate when and how the clips transition.

## 3. Architecture

### 3.1. Interface
A pure virtual interface governs all transitions. Each frame, the current state's `Update` returns either `nullptr` (stay in current state) or a pointer to a new state.

```cpp
class ICombatantStanceState
{
public:
    virtual ~ICombatantStanceState() = default;
    virtual void Enter(BattleRenderer* renderer, int slot, bool isPlayer) = 0;
    virtual ICombatantStanceState* Update(BattleRenderer* renderer, int slot, bool isPlayer, bool requestedFightStance) = 0;
};
```

### 3.2. Singletons for States
Because the transitional states (`StanceStateIdle`, `StanceStateReady`, etc.) contain zero instance data (all context is passed cleanly via `Update()`), they are implemented as **Meyers' Singletons**. This entirely eliminates the performance overhead of allocating and freeing states via `new` / `delete`.

### 3.3. State Transition Flow

1. **`StanceStateIdle`**: Looping. If `requestedFightStance == true`, transitions to `StanceStateReady`.
2. **`StanceStateReady`**: Plays the `ready` transition clip. Once `IsPlayerClipDone(slot)` is true, transitions to `StanceStateFight`.
3. **`StanceStateFight`**: Looping active state. If `requestedFightStance == false`, transitions safely to `StanceStateUnready`.
4. **`StanceStateUnready`**: Plays the `unready` transition clip. Once `IsPlayerClipDone(slot)` returns true, completes the circuit back to `StanceStateIdle`.

## 4. Integration with BattleRenderer
`BattleRenderer` now exposes two specific APIs:
* `void SetPlayerFightStance(int slot, bool active);` -> Allows the battle loop to update the `mPlayerStanceRequested` intent.
* `void SetPlayerStanceEnabled(int slot, bool enabled);` -> Provides a kill-switch lock. 

### Death Animation Edge Case
If a character dies, the `BattleState` forces `SetPlayerStanceEnabled(i, false)`. This cleanly halts the Stance State Machine from running and inadvertently overwriting the `CombatantAnim::Die` animation clip back to `Idle`.

## 5. Clean BattleState Logic
Because the State pattern handles the "how" and "when", `BattleState::UpdateLogic` only has to define the "what". 

```cpp
bool inStance = false;
if (phaseAfter == BattlePhase::PLAYER_TURN) {
    if (inputPhase == SKILL_SELECT || inputPhase == TARGET_SELECT) {
        inStance = true;
    }
}
else if (phaseAfter == BattlePhase::RESOLVING) {
    // Action is playing out
    inStance = true;
}

mBattleRenderer.SetPlayerFightStance(i, inStance);
```
With this architecture, future additions (like a `Guard` stance or `Stunned` layout) can be cleanly slipped into the enum and chained into the State classes without breaking presentation mechanics.