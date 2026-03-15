# Battle Transition Architecture

## Overview

When the player encounters an enemy in the `OverworldState`, the game smoothly transitions into the `BattleState`. Previously, the logic for the transition effect (such as the pincushion distortion shader) was tightly coupled directly within `OverworldState`. As the transition became more complex—adding a dynamic zoom and a 15-degree camera rotation—it became clear that keeping all this math and state in `OverworldState` violated the Single Responsibility Principle (SRP).

To solve this, we introduced the **Battle Transition Controller** architecture.

## Architecture

We defined a clean interface, `IBattleTransitionController`, that entirely encapsulates the lifecycle, rendering, and logic of moving from the Overworld to a Battle screen.

### 1. `IBattleTransitionController` (Interface)

Located in `src/Systems/IBattleTransitionController.h`.

This interface abstracts away how the transition is performed. The `OverworldState` simply holds a `std::unique_ptr<IBattleTransitionController>` and delegates to it.

**Key Methods:**
- `StartTransition(encounter, enemySource)`: Saves the target enemy data and kicks off the timer.
- `Update(uiDt, camera)`: Progresses the transition. It uses `uiDt` (UI delta time) which is immune to the 0.25x slow-motion applied to the game world during the transition. It receives a pointer to the `Camera2D` to allow complex camera manipulations (like zooming and rotation).
- `BeginCapture(ctx)` / `EndCaptureAndRender(ctx)`: Provides hooks for screen-space shader effects (like capturing the scene to an off-screen render target before applying a distortion pass).
- `IsFinished()`: Tells the `OverworldState` when to actually push the `BattleState` onto the stack.

### 2. `ZoomPincushionTransitionController` (Implementation)

Located in `src/Systems/ZoomPincushionTransitionController.h/cpp`.

This is the concrete implementation of our complex effect:
1. **Screen distortion:** Uses the existing `PincushionDistortionFilter`.
2. **Camera transformation:** Uses an ease-in-out curve to smoothly scale the camera's zoom from `1.0` to `1.8` and rotate the camera from `0` to `15` degrees.
3. **Execution:** Takes exactly 1 second (perceived time) to ramp to 100% intensity.

### Data Flow

1. Player presses `B` near an enemy.
2. `OverworldState` changes its phase to `PINCUSHION`, applies slow motion to the game clock, and calls `mTransitionController->StartTransition(...)`.
3. Over the next few frames, `OverworldState::Update` calls `mTransitionController->Update(...)`, which manipulates the `Camera2D` and the inner pincushion filter intensity.
4. During rendering, `OverworldState::Render` calls `BeginCapture()` to grab the scene, draws the unrotated/unfiltered SceneGraph, and then calls `EndCaptureAndRender()` to draw the distorted final image to the screen.
5. Once `mTransitionController->IsFinished()` returns `true`:
   - Normal time scale is restored.
   - `BattleState` is pushed onto the State Manager.
   - The camera is reset to original state (`zoom = 1.0`, `rotation = 0.0`), and the transition controller is reset so it's ready for the next battle.

## Benefits

- **Clean Code (SRP):** `OverworldState.cpp` is drastically simpler. It no longer manages timers, ease-in formulas, or raw pointers to distortion filters.
- **Extensibility:** If we want a different transition in the future (e.g. a pixelated wipe, a shatter effect, or a simple fade), we just create a new class implementing `IBattleTransitionController` and swap it in without touching the overworld logic.
- **Robustness:** Handles screen resizing and GPU resource management gracefully inside the controller's own lifecycle (`Initialize`, `Shutdown`, `OnResize`).
