# Zombie Armour Enemy Asset Pipeline

## Goal

The zombie armour is an enemy-class asset, similar to the existing Skeleton
enemy. The runtime asset is committed as a normal engine sprite sheet so the
enemy can appear immediately in the overworld and battle scenes.

The tool can also process a future exact source sheet. If an artist saves the
raw black-background sheet locally, the same script can detect source rows and
repack selected poses into the runtime layout.

The processor lives at:

```text
tools/process_zombie_armour_asset.py
```

Optional raw source path:

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

The runtime asset should be reproducible from script instead of hand-edited
binary files. This matters because:

- `WorldSpriteRenderer` expects fixed frame dimensions from JSON.
- Overworld and battle rendering both need the same clip names.
- Turn queue portraits need a separate `256 x 128` UI texture.
- A future raw sheet may arrive in an irregular layout with a black background.
- Generated assets need to stay consistent across machines.

The default mode now generates the zombie armour from the existing Skeleton
runtime atlas. This keeps the layout identical to a known-good enemy sheet while
adding an undead tint, helmet, chest armour, shoulder plates, belt details, and
glowing eyes on every frame.

## Processing Command

Run:

```text
python tools/process_zombie_armour_asset.py
```

If `source_assets/zombie_armour_raw.png` exists, the script processes that raw
sheet. If it does not exist, the script falls back to the skeleton-reference
generator and still writes playable runtime assets.

To force the committed reference generator:

```text
python tools/process_zombie_armour_asset.py --source-mode skeleton-reference
```

To force raw-sheet processing, save the provided source image as:

```text
source_assets/zombie_armour_raw.png
```

Then run:

```text
python tools/process_zombie_armour_asset.py --source-mode raw
```

The script writes the animation atlas, sprite-sheet JSON, and turn-view portrait.

## Reference Generation Strategy

The default fallback path:

1. Loads `assets/animations/skeleton.png`.
2. Loads clip names, frame counts, frame rates, and loop flags from
   `assets/animations/skeleton.json`.
3. Tints the source pixels toward old bone, rust, and dark leather values while
   preserving original alpha and shading.
4. Draws armour overlays inside each `128 x 128` frame.
5. Writes `assets/animations/zombie_armour.png`.
6. Writes `assets/animations/zombie_armour.json` with `sprite_name` and
   `character` changed to `zombie_armour`.
7. Writes `assets/UI/turn-view-zombie-armour.png`.

This path is deterministic and does not require a local source sheet.

## Raw Sheet Detection Strategy

The raw-sheet path:

1. Converts the source image to RGBA.
2. Treats pixels brighter than `--threshold` as sprite pixels.
3. Scans rows to find source animation bands.
4. Scans columns inside each row band to find individual frame boxes.
5. Crops each detected frame.
6. Scales frames with nearest-neighbor sampling.
7. Pastes each frame into a transparent `128 x 128` cell.
8. Writes project-format sprite-sheet JSON.

Default raw-sheet packing:

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

Current spawn ids:

```text
glass_shrine_zombie_armour
glass_shrine_zombie_armour_west
glass_shrine_zombie_armour_east
glass_shrine_zombie_armour_south
glass_shrine_zombie_armour_far_south
```

Current overworld positions:

```text
glass_shrine_zombie_armour       worldX = 1220.0  worldY = 2240.0
glass_shrine_zombie_armour_west  worldX = 620.0   worldY = 2320.0
glass_shrine_zombie_armour_east  worldX = 1560.0  worldY = 2140.0
glass_shrine_zombie_armour_south worldX = 940.0   worldY = 2580.0
glass_shrine_zombie_armour_far_south worldX = 1360.0  worldY = 2700.0
```

The spawns are near the glass shrine route. They use the same enemy path as the
solo Skeleton-style overworld encounters: one overworld body starts one solo
zombie armour battle. The cluster makes the shrine feel occupied without adding
a separate enemy class or special group behavior.

## Missing Asset Behavior

The generated runtime assets are committed, so the zombie armour spawn is
visible in a normal checkout.

`OverworldState` still skips the spawn and logs a warning if the files are
deleted or not generated. This keeps the game bootable while assets are being
reprocessed.

Once the generated PNG/JSON files exist again, the spawn automatically appears.

## Build Verification

After processing the asset, build with:

```bat
.\build_src_static.bat 2>&1
```

Expected successful tail:

```text
[OK] Build succeeded > bin\game.exe  [Debug]
```
