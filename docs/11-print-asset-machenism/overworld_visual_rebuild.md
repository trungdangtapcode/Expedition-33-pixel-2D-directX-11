# Overworld Visual Rebuild

## Goal

This rebuild fixes the overworld as a visual system, not only as an asset swap.
The current foundation is:

- Top-down 64x64 map tiles instead of mixed perspective cutouts.
- Background tile pass, Y-sorted `SceneGraph` entities, then foreground tile pass.
- Data-driven static props for large objects that must sort against the player.
- Region themes that blend subtle world-only color grading as the player moves.
- Screen-space UI rendered after world filters so text and coin HUD stay readable.

## Generated Asset Pipeline

The generated v2 art lives in:

```text
assets/environments/overworld_tiles_v2.png
assets/environments/overworld_objects_v2.png
```

Regenerate the atlases:

```bat
python patches\compile_assets.py
```

Regenerate the map and prop placement data:

```bat
python patches\generate_map.py
```

The map generator writes:

```text
assets/environments/overworld_map.json
data/overworld_props.json
```

The repository ignores PNG files by default, so new generated atlas PNGs must
be force-added when committing:

```bat
git add -f assets\environments\overworld_tiles_v2.png assets\environments\overworld_objects_v2.png
```

## Tiled Layer Rules

Use tile layers for art that does not need individual Y-sorting:

- Ground, grass variants, roads, cracked stone, shrine floor, and decals.
- Low props that are always behind entities or are too small to need sorting.
- Foreground canopies, roof lips, gate tops, and other always-above overlays.

`TileMapRenderer` routes layer names containing these tokens to the foreground
pass:

```text
foreground
front
above
canopy
roof
overlay
```

All other tile layers render before `SceneGraph` entities.

## Static Prop System

Large objects that can overlap the player belong in `data/overworld_props.json`
and render through `OverworldStaticProp`.

Example:

```json
{
  "id": "glass_shrine_north",
  "texturePath": "assets/environments/overworld_objects_v2.png",
  "sourceX": 0,
  "sourceY": 256,
  "sourceWidth": 128,
  "sourceHeight": 128,
  "worldX": 448.0,
  "worldY": 1784.0,
  "pivotX": 64.0,
  "pivotY": 120,
  "scale": 1.0,
  "layer": 50,
  "sortYOffset": 0.0
}
```

`pivotY` should usually point near the object's feet or base. `GetSortY()`
uses `worldY + sortYOffset`, so changing that value changes when the prop
draws in front of or behind the player.

Collision for static props still lives in the Tiled `Collisions` object layer.
The prop renderer owns visuals only.

## Region Themes

Theme data lives in:

```text
data/overworld_themes.json
```

Story regions opt into a theme through `themeId`:

```json
{
  "id": "mirror_gate",
  "themeId": "mirror_gate",
  "minX": 2840.0,
  "minY": -2100.0,
  "maxX": 3960.0,
  "maxY": -940.0
}
```

`OverworldThemeManager` blends from the current theme to the next one using
the theme's `blendSeconds`. `ColorGradeFilter` receives the blended
`ColorGradeSettings` and applies it to the world before story and currency UI.

Keep theme values restrained:

- `tintStrength` should usually stay below `0.15`.
- `saturation` should avoid crushing color identity below `0.85`.
- `contrast` should avoid strong values above `1.12`.
- `vignetteStrength` should support mood, not hide map edges.

## Render Order

`OverworldState::Render` now follows this order:

1. Transition controller begins any needed effect setup.
2. Background tile layers render.
3. `SceneGraph` renders entities and static props by layer and Y.
4. Foreground tile layers render.
5. `ColorGradeFilter` applies world-only theme mood.
6. Battle transition filter applies if active.
7. Story and currency UI render in screen space.

The old demo blue circle and raw debug texture viewer are no longer drawn by
`OverworldState`.

## Validation

After changing the generators, map, props, or theme data:

```bat
python patches\compile_assets.py
python patches\generate_map.py
.\build_src_static.bat 2>&1
```

The successful build tail must be:

```text
[OK] Build succeeded > bin\game.exe  [Debug]
```

In game, verify:

- No old isometric houses or magenta outlines appear.
- Player, enemies, campfires, and static props sort correctly.
- Foreground layers appear above the player.
- Region theme transitions are subtle and do not tint UI text.
- Colliders block props and walls without trapping the player.
