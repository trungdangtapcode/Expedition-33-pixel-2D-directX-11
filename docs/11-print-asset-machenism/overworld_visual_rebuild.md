# Overworld Visual Rebuild

## Goal

This rebuild fixes the overworld as a visual system, not only as an asset swap.
The current foundation is:

- Top-down 64x64 map tiles instead of mixed perspective cutouts.
- Background tile pass, Y-sorted `SceneGraph` entities, then foreground tile pass.
- Data-driven static props for large objects that must sort against the player.
- Region themes that blend subtle world-only color grading as the player moves.
- Screen-space UI rendered after world filters so text and coin HUD stay readable.

## Asset Pipeline

The generated v2 art lives in:

```text
assets/environments/overworld_tiles_v2.png
assets/environments/overworld_objects_v3.png
assets/environments/overworld_route_props.png
```

Regenerate deterministic ground and road tiles:

```bat
python patches\compile_assets.py
```

`patches\compile_assets.py` intentionally does not write
`overworld_objects_v3.png`. The object atlas is imagegen-sourced art, then
locally chroma-keyed and resized into the existing 8x8 64px atlas contract.
Do not reintroduce Python-drawn rectangle/circle props for that atlas.

The current `overworld_objects_v3.png` source was generated with the imagegen
skill as a 512px-style top-down pixel-art atlas on a green chroma-key
background. Local processing only removed the key color and resized the result
to 512x512; no script drew the prop shapes.

`overworld_route_props.png` is also imagegen-sourced. It contains larger 128px
transparent cutout landmarks for route readability: barricades, lamps, signal
posts, statue fragments, carts, glass monuments, tents, and mirror shards.

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
git add -f assets\environments\overworld_tiles_v2.png assets\environments\overworld_objects_v3.png
git add -f assets\environments\overworld_route_props.png
```

## Dirt Road Style

Road tiles are alpha overlays on top of grass. They should read as worn soil,
not as hard-edged wooden planks or stamped rectangles.

`patches/compile_assets.py` generates each road tile from the road connection
mask with:

- Full-opacity dirt in connected directions.
- Noisy alpha falloff on unconnected edges.
- Darker pixels only near the fade edge.
- Opaque ground speckles so grass tiles never darken toward black.

If a future pass makes roads look tiled again, fix the generator first and then
regenerate the atlas. Do not hand-paint only the committed PNG, because the next
generator run would reintroduce the artifact.

## Object Atlas Rules

Small tile-layer props come from `assets/environments/overworld_objects_v3.png`.
This atlas should use:

- A readable silhouette at 64x64.
- A dark one-pixel outline only where it separates the prop from terrain.
- Small cast shadows to anchor the prop to the ground.
- Plank seams, highlights, and nail marks instead of flat rectangles.
- Top-down perspective only; no isometric cutouts.
- Real painted forms from image generation or artist source, not procedural
  primitive geometry.

The active tile ID contract in `patches/generate_map.py` expects this atlas to
stay at 512x512 with 8 columns and 8 rows of 64px cells. Only 64x64 cells from
this atlas should be placed. Do not use larger source rectangles from this
atlas, because generated composite cells often include baked terrain that
renders as square ground patches when placed on a different tile background.
The low-object cells currently used by the map are:

- Row 0: stone wall horizontal, stone wall vertical, cracked block, signpost,
  market bench/table, crate, barrel, bush.
- Row 1: cobblestone patch, glass shard cluster, flowers, planks, loose
  stones, short lantern, iron fence, rubble.

They should not carry interaction logic. If an object becomes interactive or
needs Y-sorting against the player, move it into `data/overworld_props.json`
and render it through `OverworldStaticProp` using a transparent cutout source.

Route landmark props come from `assets/environments/overworld_route_props.png`
and are placed only through `data/overworld_props.json`. Keep them off the main
walk line unless the matching collision rectangle is meant to block the path.

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
  "texturePath": "assets/environments/overworld_route_props.png",
  "sourceX": 256,
  "sourceY": 128,
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
