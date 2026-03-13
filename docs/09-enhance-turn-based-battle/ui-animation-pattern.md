# UI Animation Pattern

## Purpose
The `UIEffectState` component is a shared state machine used across all UI elements (like Health Bars, Enemy HP Bars) to provide consistent visual feedback such as **Scaling (Selection)** and **Shaking (Damage/Hit)** without code duplication.

## Components
### 1. UIEffectState
Provides generic offset and scale variables.
- `mCurrentScale`: Lerps towards a target scale (e.g. 1.2x when focused, 1.0x normally).
- `mOffsetX`, `mOffsetY`: Randomly generated per-frame when a shake is active (`mShakeTimer > 0`).

### 2. Implementation in Renderers
Both `HealthBarRenderer` and `EnemyHpBarRenderer` instantiate `UIEffectState mEffectState;`.
- On damage (HP changes), they call `mEffectState.TriggerShake()`.
- On selection, a component (like `BattleState`) calls `SetTargetScale(1.2f)`.

### 3. Rendering Integration
When calling `SpriteBatch::Draw`, the renderer calculates the new bounds based on `mCurrentScale` and adds the shake offsets (`mOffsetX`, `mOffsetY`) to the drawing position. The origin is adjusted to scale smoothly from the center.

## Clean Code & Refactoring
To keep state classes like `BattleState` clean, UI scaling logic and input handling should be decoupled into their respective components (e.g. `BattleInputController`) and rendering logic separated from core game loop updates.
