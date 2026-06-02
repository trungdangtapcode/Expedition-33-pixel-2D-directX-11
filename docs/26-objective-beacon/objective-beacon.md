# Objective Beacon

## Purpose

The overworld needs more than text guidance. Expedition-style route readability
comes from a combination of objective text, clear destination language, and a
visible world-space marker that tells the player where the next authored beat
lives.

`ObjectiveBeaconRenderer` adds that missing world marker. It consumes the active
`ObjectiveView` from `ObjectiveDirector`, draws a generated objective sigil at
the objective waypoint, and hides itself when the player is close enough for the
local interaction prompt to take over.

## Runtime Flow

1. `ObjectiveDirector::Resolve()` returns objective text plus waypoint
   coordinates.
2. `OverworldState::UpdateStoryRegion()` stores the current `ObjectiveView`.
3. `ObjectiveBeaconRenderer::Update()` advances marker bobbing.
4. `OverworldState::Render()` draws the marker after foreground map layers and
   before color grading.
5. `RenderInteractionPrompt()` still owns the close-range action prompt.

This keeps navigation help and interaction help separate: the beacon guides the
route from a distance, while prompts tell the player what to press at the target.

## Data Files

`data/objective_beacon.json`

```json
{
  "enabled": true,
  "texturePath": "assets/UI/objective_beacon.png",
  "layoutPath": "assets/UI/objective-beacon-ui.json",
  "hideWithinDistanceUnits": 48.0
}
```

`assets/UI/objective-beacon-ui.json`

```json
{
  "width": 128,
  "height": 128,
  "pivot": [64, 64],
  "y_offset": -150.0,
  "bob_speed": 4.0,
  "bob_amplitude": 8.0
}
```

## Asset Pipeline

The marker asset was generated with the built-in image generation tool using a
flat chroma-key background, then processed locally into an alpha PNG.

Final committed asset:

- `assets/UI/objective_beacon.png`

Intermediate chroma-key files are not required by the game and should not be
referenced by data.

## Authoring Rules

- Every visible beacon must come from `ObjectiveDirector` waypoint data.
- Tune marker texture, layout, and hide distance through JSON.
- Do not hardcode objective coordinates in `ObjectiveBeaconRenderer`.
- Keep the marker readable at 64 to 128 pixels; it should guide, not decorate.
- Hide the marker at close range so enemies, NPCs, and campfires remain visible.

## Verification

- Build with `.\build_src_static.bat 2>&1`.
- Confirm `ObjectiveBeaconRenderer.cpp` is listed in `build_src_static.bat`.
- Confirm `assets/UI/objective_beacon.png` has alpha and loads through WIC.
- Start a route objective with a waypoint and confirm the beacon appears in the
  world, follows camera movement, and hides near the waypoint.
