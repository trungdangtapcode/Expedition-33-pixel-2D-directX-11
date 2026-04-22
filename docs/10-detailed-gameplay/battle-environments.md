# Battle Environments

## Overview
The Battle Environment system provides a scalable, data-driven approach to rendering layered, dynamically sized backgrounds and foregrounds during combat. It allows different enemy encounters to specify different scenic backdrops.

## Architecture

1. **Configuration (`EnvironmentConfig`)**
   - Encounter JSONs (e.g., `enemies/wolf_group.json`) can specify an `environmentPath` (e.g., `assets/environments/battle-paris-view.json`).
   - The JSON defines the texture paths for `background` and `foreground`, alongside transform metrics: `width`, `height`, `offsetX`, `offsetY`, and an optional `targetViewHeight`.

2. **Rendering (`EnvironmentRenderer`)**
   - Managed as part of the visual phase in `BattleState`.
   - Uses `DirectX::SpriteBatch` with a custom-bound Viewport to avoid projection offset issues when mixing UI screen space with Camera world space.
   - Calculates dynamic scale by querying the current active `Camera2D` screen resolution (`camera.GetScreenH()`) or adhering to `targetViewHeight` to maintain appropriate aspect ratios regardless of window resizing.

3. **Dynamic Viewport Synchronization**
   - The renderer ensures that `SpriteBatch::Begin()` respects camera sizing by fetching dimensions dynamically (`Camera2D::GetScreenW()`, `GetScreenH()`) and overriding `RSSetViewports(1, &vp)` prior to batch operations.
   - This fixes visual alignment bugs where Direct3D automatically scales arbitrary viewports differently from standard 2D projection metrics.

## Layering
The environment renders in two distinct phases:
- `RenderBackground(camera)`: Invoked before any combatants or static scenery widgets are drawn. It places the main ambient backdrop matching the configured scale.
- `RenderForeground(camera)`: Invoked after all character sprites and regular particle effects, allowing for depth elements like mist, immediate-foreground objects, or overlay foliage.
