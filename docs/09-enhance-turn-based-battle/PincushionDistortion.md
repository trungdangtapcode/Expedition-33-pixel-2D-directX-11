## Pincushion Transition (Overworld -> Battle)

This document explains the current battle-entry transition and why the earlier version made characters disappear.

### Goal

- Press `B` near an overworld enemy.
- Apply a short pincushion distortion + slow motion in overworld.
- Push `BattleState` directly after distortion finishes.
- No iris-close in overworld. Battle handles its own iris-open.

### Where The Flow Lives

- Trigger logic: `src/States/OverworldState.cpp`
- Filter implementation: `src/Renderer/PincushionDistortionFilter.cpp`
- Filter interface: `src/Renderer/IScreenFilter.h`

### Runtime Flow

1. In `OverworldState::Update`, when `B` is pressed near a living enemy:
	- Cache `EnemyEncounterData`.
	- Enter `PINCUSHION` phase.
	- Call `TimeSystem::Get().SetSlowMotion(0.25f)`.
2. While in `PINCUSHION`:
	- Use UI clock delta time (not gameplay delta time).
	- Ramp `intensity` from `0` to `1` over `kPincushionDuration`.
	- Feed intensity to `mPincushionFilter->Update(...)`.
3. When timer completes:
	- Restore speed with `SetSlowMotion(1.0f)`.
	- Push `BattleState` immediately.
	- Reset transition state and filter intensity.

### Current Render Pipeline (Final Version)

The filter is now pure post-process in `Render()`:

1. Overworld scene renders normally to back buffer.
2. `PincushionDistortionFilter::Render` copies back buffer to `mSceneTexture` using `CopyResource`.
3. A full-screen quad samples `mSceneTexture` with radial pincushion UV warp and draws to back buffer.

Important notes:

- `BeginCapture` and `EndCapture` are intentionally no-ops in the current version.
- Distortion strength is `k = intensity * kMaxCoefficient`.
- Alpha blending is disabled during the fullscreen pass so warped image fully overwrites previous frame color.

### Why Characters Disappeared Before

Earlier capture-based flow switched render targets during scene rendering. A debug draw path (`DebugTextureViewer`) restored to the back buffer when capture expected an offscreen target, causing scene content mismatch:

- Offscreen texture did not receive expected character draws.
- Final fullscreen filter pass then overlaid incomplete/black capture.

Related file:

- `src/Debug/DebugTextureViewer.cpp`

The debug viewer now saves and restores the currently bound RTV/DSV via `OMGetRenderTargets`, instead of hardcoding `D3DContext::GetRTV()`.

### Quick Verification Checklist

1. Enter overworld and ensure player/enemies are visible with filter inactive.
2. Press `B` near enemy:
	- World slows down.
	- Distortion ramps in.
3. After duration, battle starts directly.
4. Return from battle and confirm overworld is normal (filter reset).

