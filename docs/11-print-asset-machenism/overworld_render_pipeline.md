# Overworld Render Pipeline

## Goal

The overworld renderer now separates map rendering into explicit passes. This
fixes a design problem where every tile layer was drawn before every entity,
which made the world feel flat: roofs, canopies, gates, and foreground details
could never appear in front of the player.

The change does not require new assets or a new map format. It improves the
code path so better authored maps can create depth with ordinary Tiled layers.

## Runtime Order

`OverworldState::Render` now draws the world in this order:

1. Background tile-map pass.
2. World-space debug/landmark draws.
3. `SceneGraph` entities sorted by layer and world Y.
4. Foreground tile-map pass.
5. Post-process transition output.
6. Screen-space story and currency UI.

This keeps the existing entity Y-sort while giving the tile map a clean place
for above-player art.

## Layer Naming Contract

`TileMapRenderer` routes tile layers by name. A layer is foreground when its
name contains one of these case-insensitive tokens:

```text
foreground
front
above
canopy
roof
overlay
```

Examples:

```text
ForegroundCanopy
RoofOverlay
AbovePlayerVines
FrontGate
```

All other visible tile layers render in the background pass. Existing maps keep
working because layers such as `Ground`, `Roads`, `Objects`, and `Details` are
treated as background by default.

## Camera Culling

The renderer now computes visible tile bounds from:

- Camera world position.
- Camera zoom.
- Screen size.
- Map origin and tile size.

Only the visible tile rectangle plus a two-tile safety pad is submitted to
`SpriteBatch`. If the camera is rotated, the renderer uses the viewport half
diagonal as a conservative culling bound so rotated transitions do not clip
tiles at the corners.

## Editing Guidance

- Put ground, roads, low props, and walk-behind objects in normal background
  layers.
- Put treetops, roof overhangs, tall gate tops, hanging banners, and canopy
  shadows in foreground layers.
- Do not put collision intent into the layer name. Collision remains controlled
  by the Tiled object group and `JsonLoader::LoadTileMapData`.
- For objects that need true per-object Y-sorting against the player, create an
  `IGameObject` entity instead of a foreground tile. Foreground layers are for
  always-above art.

## Validation

After changing the render path or Tiled layer names:

```bat
.\build_src_static.bat 2>&1
```

The successful build tail must be:

```text
[OK] Build succeeded > bin\game.exe  [Debug]
```

In game, verify:

- Existing background map layers still appear.
- Player, campfires, and enemies still render above normal map art.
- A test layer named `ForegroundCanopy` renders above the player.
- Camera movement does not reveal missing tiles at screen edges.
