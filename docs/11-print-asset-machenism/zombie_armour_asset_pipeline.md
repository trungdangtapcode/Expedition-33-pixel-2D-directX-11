# Zombie Armour Enemy Asset Pipeline

## Goal

The zombie armour sheet is an enemy-class asset, similar to the existing
Skeleton enemy. The source sheet is a black-background sprite sheet with many
small pixel-art poses. The game needs a normalized atlas where every animation
clip occupies one row of fixed `128 x 128` frames.

The processor lives at:

```text
tools/process_zombie_armour_asset.py
```

Default source path:

```text
source_assets/zombie_armour_raw.png
```

Default outputs:

```text
assets/animations/zombie_armour.png
assets/animations/zombie_armour.json
assets/UI/turn-view-zombie-armour.png
```

## Why A Processor Is Needed

The raw source sheet is not already in the runtime format:

- Frames are arranged in many irregular rows.
- The background is black instead of transparent.
- Frame spacing is not guaranteed to be a fixed grid.
- The engine expects one row per animation clip.
- `WorldSpriteRenderer` expects fixed frame dimensions from JSON.

The processor solves this by detecting visible sprite pixels, grouping them
into source rows and frame boxes, then repacking selected rows into a clean
runtime atlas.

## Processing Command

Save the provided image as:

```text
source_assets/zombie_armour_raw.png
```

Then run:

```text
python tools/process_zombie_armour_asset.py
```

The script writes the animation atlas, sprite-sheet JSON, and turn-view portrait.

## Detection Strategy

The script:

1. Converts the source image to RGBA.
2. Treats pixels brighter than `--threshold` as sprite pixels.
3. Scans rows to find source animation bands.
4. Scans columns inside each row band to find individual frame boxes.
5. Crops each detected frame.
6. Scales frames with nearest-neighbor sampling.
7. Pastes each frame into a transparent `128 x 128` cell.
8. Writes project-format sprite-sheet JSON.

Default packing:

- `idle`: source row 0
- `walk`: source row 1
- `fight-state`: source row 0
- `attack-1`: source row 2
- `die`: last detected source row

The defaults are intentionally conservative. They make the enemy playable, but
an artist can refine the mapping with a recipe.

## Optional Recipe

For precise animation mapping, create a recipe JSON:

```json
{
  "animations": [
    { "name": "idle", "sourceRow": 0, "frames": 8, "frameRate": 8, "loop": true },
    { "name": "walk", "sourceRow": 1, "frames": 8, "frameRate": 10, "loop": true },
    { "name": "fight-state", "sourceRow": 0, "frames": 8, "frameRate": 8, "loop": true },
    { "name": "attack-1", "sourceRow": 6, "frames": 8, "frameRate": 12, "loop": false },
    { "name": "die", "sourceRow": 18, "frames": 6, "frameRate": 6, "loop": false }
  ]
}
```

Run:

```text
python tools/process_zombie_armour_asset.py --recipe source_assets/zombie_armour_recipe.json
```

## Enemy Data

The enemy encounter file is:

```text
data/enemies/zombie_armour.json
```

It references:

```text
assets/animations/zombie_armour.png
assets/animations/zombie_armour.json
assets/UI/turn-view-zombie-armour.png
data/skills/zombie_armour_attack.json
```

The default stats make it a slow defensive enemy:

- High HP.
- High DEF.
- Low SPD.
- Medium ATK.

## Overworld Spawn

The spawn entry lives in:

```text
data/overworld_spawns.json
```

Current spawn id:

```text
glass_shrine_zombie_armour
```

The spawn is near the glass shrine route. This makes the enemy a mid-to-late
roadblock before the mirror gate.

## Missing Asset Behavior

The raw image is not committed to the repository by this script. Until the
processor writes the runtime files, `OverworldState` skips the zombie armour
spawn and logs a warning.

This keeps the game bootable even if a teammate has not run the asset processor
yet. Once the generated PNG/JSON files exist, the spawn automatically appears.

## Build Verification

After processing the asset, build with:

```bat
.\build_src_static.bat 2>&1
```

Expected successful tail:

```text
[OK] Build succeeded > bin\game.exe  [Debug]
```
