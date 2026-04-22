# Battle Action & Animation Pipeline

This document outlines the architecture, design patterns, and historical refactoring of the turn-based combat animation and movement system. It serves as a guide for understanding the decoupling of game logic from presentation, and provides standards for scaling the system.

## 1. Architectural Overview

The battle system strictly separates **Logical Data** (`IBattler`, `ISkill`, Math) from **Visual Presentation** (`BattleRenderer`, Screen space offsets, Sprite Clips). They never hold references to each other.

To achieve this, the architecture relies on three core patterns:
1. **Command Pattern (Action Queue):** Skills are dynamically translated into a sequential array of atomic `IAction` operations.
2. **Observer Pattern (Event Manager):** The logic layer fires `EventData` payloads. The presentation layer (`BattleState`) listens, translates the concepts, and executes visual updates.
3. **Composite Pattern:** Complex, repeating visual clusters (like moving, stopping, and returning) are encapsulated into self-contained Actions (e.g., the upgraded `MoveAction`).

---

## 2. Core Components

### `IAction` and `ActionQueue`
All combat maneuvers flow through `ActionQueue::Update(float dt)`. 
- An action evaluates `Execute(dt)` every frame.
- If it returns `false`, it **blocks** the queue (e.g., waiting for movement to finish).
- If it returns `true`, it finishes, and the queue advances on the next frame.
- *Rule:* Avoid arbitrary delay wrappers. Actions should progress dynamically based on their animation or duration parameters.

### `BattleEvents.h`
The standardized contract between Logic and Renderer. It contains POD (Plain Old Data) structs like `PlayAnimPayload`, `IsAnimDonePayload`, and `MoveOffsetPayload`.
- Logic constructs the payload and calls `EventManager::Get().Broadcast()`.
- The UI layer (`BattleState`) catches it, extracts the payload via `static_cast`, finds the physical screen slot via `GetBattlerSlot()`, and alters the `BattleRenderer`.

### The Composite `MoveAction`
The `MoveAction` is responsible for visual displacement using smoothstep interpolation (`3t^2 - 2t^3`). 
Rather than forcing developers to queue manual animation steps (Start Move -> Lerp -> Stop Move), `MoveAction` accepts `movingAnim` and `stopAnim` states via its constructor.
* **On Start:** It automatically broadcasts a request to play `BattleMove`.
* **On Update:** It broadcasts the interpolated draw offsets.
* **On Finish:** It broadcasts a request to play `BattleUnmove` and returns `true`.

---

## 3. Data-Driven Design (JSON)

**ZERO HARDCODING.** No magic numbers (`0.5f`) belong in C++ logic files. 
Skills use the `JsonLoader` to populate their exact behavior:

```json
// data/skills/attack.json
{
    "moveDuration": 0.15,
    "returnDuration": 0.20
}
```
This data is loaded into `JsonLoader::SkillData` and injected into the constructor of `AttackSkill`. Designers can iterate on animation pacing without recompiling the game.

---

## 4. Historical Mistakes & Lessons Learned

### Mistake 1: The "Infinite Queue Freeze"
* **Symptom:** Combat would completely lock up when attacking.
* **Cause:** `PlayAnimationAction` relied on an `IsAnimDonePayload` event to evaluate readiness. `BattleState` didn't have a `.Subscribe` block mapping that event to `BattleRenderer`. The Action received a perpetual `false` and hung the queue forever.
* **Fix/Lesson:** Whenever an `IAction` relies on an Event for logic progression, you **must** ensure a listener is registered in `BattleState::OnEnter` and properly cleaned up in `BattleState::OnExit`. 

### Mistake 2: Artificial Framework Delays
* **Symptom:** Combat felt robotic, slow, and staggered.
* **Cause:** A wrapper class named `DelayedAction` was forcefully wrapping every single action enqueued in `BattleManager` to impose an artificial `1.0s` wait time, intended originally for UI log readability.
* **Fix/Lesson:** Purged `DelayedAction` entirely. Game loops must run at engine speed. Pacing should be governed exclusively by explicit animation lengths or mathematically defined duration variables (Data-Driven).

### Mistake 3: Duplicated Action Pipelines
* **Symptom:** `AttackSkill.cpp` had highly repetitive blocks for running towards the target and returning to the origin, manually queueing `PlayAnimationAction` before and after `MoveAction` steps.
* **Cause:** Overly granular procedural code logic. Wait flags (`wait = true`) were unnecessarily stalling animations before movement even began.
* **Fix/Lesson:** Assembled the Composite Pattern in `MoveAction` to handle implicit animation states. Converted visual state triggers to run concurrently (`wait = false`) with physics interactions.

---

## 5. Maintenance & Scaling Guide

When adding a **New Skill** or **Complex Combat Sequence**, strictly follow this pipeline:

1. **Create the Data:** Define any timing, hit-counts, or specific offsets in `data/skills/your_skill.json`.
2. **Create the Class:** Inherit from `ISkill`. Add `SkillData` as a constructor dependency.
3. **Build the Queue:** Inside `ISkill::Execute`:
   - Enqueue a `LogAction` to announce the intent.
   - Enqueue `PlayAnimationAction` explicitly if the combatant needs to charge up (e.g., `CombatantAnim::Cast`).
   - Enqueue a `MoveAction` passing your data-driven durations. It will handle the walking/sliding visuals automatically.
   - Enqueue `DamageAction` / `StatusEffectAction` exactly when the strike visually lands.
   - Enqueue a returning `MoveAction` back to origin.
4. **Extend the Events (Only if needed):** If the skill requires an entirely new visual mechanic (e.g., screen shaking, camera zooms), define a new struct in `BattleEvents.h`. Write the trigger in an `IAction`, and implement the listener inside `BattleState.cpp` to execute the camera logic. Never pass a `Camera*` to an `IAction`!
