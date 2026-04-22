# Animation-Synchronized Damage Execution

## Overview

Previously, the combat Action Queue was rigidly sequential: an action like `PlayAnimationAction` would block the queue until the animation fully completed (progress `1.0`), and only then would `DamageAction` execute. This caused a visual disconnect where the weapon hit the target visually, but the damage numbers, HP bar depletion, and screen shake occurred seconds later when the animation ended.

To solve this, we introduced an **Animation Progress Polling** mechanic and the **Composite Event-Driven Action** `AnimDamageAction`. This allows game designers to define a normalized timestamp (`damageTakenOccurMoment`) where the hit physically connects, syncing visual impact with engine logic.

---

## Architectural Changes

### 1. Sprite Progress Tracking (`WorldSpriteRenderer`)
Added a method to calculate the exact, normalized progress of the currently active animation clip:
```cpp
float GetClipProgress() const
{
    // ...
    // Calculates elapsed time out of total duration based on framerate and numFrames
    float progress = elapsed / totalDuration;
    return progress; // Normalized [0.0, 1.0]
}
```
This is bubbled up through the `BattleCombatantSprite` via the `BattleRenderer`.

### 2. Decoupling via Event System (`BattleEvents.h` & `BattleState.cpp`)
Following the strict MVC decoupling of the project, logic components (`IAction`) cannot query the renderer directly. We introduced a new payload and listener:
*   **Payload:** `GetAnimProgressPayload` (provides `target` as input, mutates `progress` as output).
*   **Event:** `"battler_get_anim_progress"` 
*   **Listener:** `BattleState::OnGetAnimProgress` intercepts the request and queries the `BattleRenderer` for the specific entity's clip progress.

### 3. Composite Action: `AnimDamageAction.cpp`
Instead of queuing `PlayAnimationAction` followed by `DamageAction`, we unified them.
`AnimDamageAction` evaluates in the `ActionQueue::Update(dt)` loop:
1. Triggers the attack animation ("battler_play_anim").
2. Each frame, fires `"battler_get_anim_progress"`.
3. If `progress >= mDamageMoment` and damage has *not* yet been dealt, it calls `mDefender->TakeDamage(...)`. This triggers logging, HP reduction, and UI reactions instantly mid-animation.
4. Continues to block the queue until `"battler_is_anim_done"` returns true.

---

## Data-Driven Configuration (Zero Hardcoding)

In alignment with the engine's core principles, the exact hit frame timing is injected via the data-driven `.json` files. Different entities can have entirely different wind-up times.

### The Skill Data Struct
```cpp
// src/Utils/JsonLoader.h
struct SkillData {
    float moveDuration = 0.5f;
    float returnDuration = 0.5f;
    float meleeOffset = 80.0f;
    float damageTakenOccurMoment = 0.8f; // The exact % of the way through the clip to apply hit
};
```

### JSON Usage Example
Each combat entity is linked to its own targeted `.json` profile (e.g., `verso_attack.json` vs `skeleton_attack.json`).

```json
// data/skills/verso_attack.json
{ 
  "moveDuration": 0.4, 
  "returnDuration": 0.35, 
  "meleeOffset": 80.0, 
  "damageTakenOccurMoment": 0.1 
}
```
*   `verso_attack` has `"damageTakenOccurMoment": 0.1` because Verso's attack clip likely strikes almost instantly within its frames.
*   `skeleton_attack` has `"damageTakenOccurMoment": 0.8` because the skeleton has a long, drawn-out wind-up before the sword falls. 

If animations change, game designers only need to edit the `.json` values without recompiling any C++ logic.

---

## Common Mistakes Avoided
1. **Relying on hardcoded absolute timers:** We did not use float timers (e.g., 0.5 seconds). We use *normalized progress* constraints (e.g. 0.8). If the `"frame_rate"` of an animation changes in `skeleton.json`, the hit will dynamically scale and still occur at 80% through the animation.
2. **Coupling Logic and Renderer:** `AnimDamageAction` does not hold a reference to `WorldSpriteRenderer`. It interacts successfully through `EventManager::Broadcast`.
3. **Double Damage:** `bool mDamageApplied` ensures `TakeDamage` is fired exactly once per execution cycle even as the animation remains in progress for several subsequent frames.