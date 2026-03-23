# Turn View Queue System

## Overview
The Turn View Queue System is a data-driven graphical UI component that displays the action order of combatants in battle. It translates a logical queue of `IBattler` objects into an animated, visually pleasing list of avatars.

## Architecture & Data Flow

1. **Logical Queue (Battle Engine)**
   - The battle engine maintains an array of `IBattler*` representing the upcoming turn order.
   - When the logical queue changes (e.g., end of turn, speed buffs, new combatants), the UI is fed the updated array.

2. **UI Representation (`TurnQueueUI`)**
   - Implements `IGameObject` for pure structural integration.
   - Uses `SpriteBatch` for optimized drawing of 2D GUI elements with custom transform matrices.
   - Maintains an active sequence of `QueueNode` objects that hold current vs target positions and scales.
   - Maintains `mFadingNodes` for executing visual clear effects when combatants exit the queue.

3. **Data-Driven Configuration (`TurnViewConfig`)**
   - Exposes parameters via JSON (`assets/UI/turn-view.json`), interpreted by `JsonLoader`.
   - Modifying JSON values updates the UI state dynamically at runtime via Hot-Reloading mapping.

## Configurable Properties
The layout and presentation metrics are fully customizable. 
- **Positions**: Base `x` / `y`, vertical spacing (`spacingY`).
- **Scale**: The top queue item uses `topScale`, subsequent items use `normalScale`.
- **Fading / Exiting Animation**: 
  - `popOffX`: The horizontal offset applied to the avatar exiting the queue.
  - `popScale`: The scale multiplier uniformly applied to an exiting item.
  - `fadeSpeed`: Alpha drain rate representing how quickly an exited item disappears.
  - `spawnOffsetY`: The vertical slide-in distance when a completely new combatant enters the list.

## Animation Logic

The queue utilizes a Linear Interpolation (Lerp) mechanic coupled with DeltaTime to organically fluidly animate avatar positions and scales.

### 1. Slide-Up Validation
When the Battle Engine updates the UI queue, the UI performs relative index distance matching against the older state. This ensures logic constraints where items explicitly slide upwards or stay neutral based on their `oldIdx >= i`. A node will never visibly "wrap-around" backward on the screen due to logic updates.

### 2. Fade / Pop-Off Execution
If an `oldNode` is no longer in the presented queue, it transforms into a "Fading Node":
- Its `targetX` is shifted left/right by `popOffX`.
- Its scale receives `popScale` multiplier.
- It begins rapidly losing alpha value controlled by `fadeSpeed`.
- It renders in a secondary background pass inside `Render()`, drawing behind primary elements with dynamic `Colors::White * it.alpha`.

## Extensibility Notes
By manipulating the external configuration files, future iterations can adjust sizing formulas, animation lengths, orientation, or anchor placements without touching any compile procedures.

