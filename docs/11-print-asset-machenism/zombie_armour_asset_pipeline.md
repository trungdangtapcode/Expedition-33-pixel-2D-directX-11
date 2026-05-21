# Zombie Armour Enemy Asset Pipeline

## Goal

The zombie armour is an enemy-class asset, similar to the existing Skeleton
enemy. The runtime asset is committed as a normal engine sprite sheet so the
enemy can appear immediately in the overworld and battle scenes.

The committed raw source sheet is:

```text
source_assets/zombie_armour_raw.png
```

The processor lives at:

```text
tools/process_zombie_armour_asset.py
```

Primary raw source path:

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
- The raw sheet is irregular and contains many rows that should not all ship.
- Generated assets need to stay consistent across machines.

The default mode processes `source_assets/zombie_armour_raw.png` when it exists.
The skeleton-reference generator remains as an emergency fallback only.

## Processing Command

Run:

```text
python tools/process_zombie_armour_asset.py
```

The committed repository includes `source_assets/zombie_armour_raw.png`, so the
normal command processes the real raw sheet.

To force raw-sheet processing:

```text
python tools/process_zombie_armour_asset.py --source-mode raw
```

To force the emergency skeleton-reference generator:

```text
python tools/process_zombie_armour_asset.py --source-mode skeleton-reference
```

The script writes the animation atlas, sprite-sheet JSON, and turn-view portrait.

## Raw Sheet Row Selection

The provided source sheet is detected as 53 rows. Not every row belongs in the
current `Zombie Armour` enemy. The bottom half contains less-armoured zombie
walk and run cycles that are better saved for a future plain zombie enemy.

Current shipped clips:

```text
idle          source row 4   1 frame   loop
fight-state   source row 4   1 frame   loop
ready         source row 12  6 frames  one-shot
unready       source row 14  6 frames  one-shot
walk          source row 26  5 frames  loop
battle-move   source row 26  5 frames  loop
battle-unmove source row 26  5 frames  loop
attack-1      source row 6   6 frames  one-shot
hurt          source row 36  3 frames  one-shot
die           source row 19  6 frames  one-shot
```

These rows were chosen because they stay visually consistent with the armoured
enemy role, use the same side-view combat silhouette as `skeleton.png`, and
match the animation names already requested by the battle code.

The raw sheet contains several side-view walk cycles. Row 8 looks like a
reasonable armed side view at first glance, but it is actually a walking cycle,
so it must not be used for `idle` or `fight-state`. Those clips use a single
stable frame from row 4 instead.

Raw zombie armour side-view frames are mirrored before packing. The battle and
overworld enemy renderers flip enemy sprites at runtime because project enemy
sheets are authored facing right by convention. Mirroring during processing
keeps Zombie Armour consistent with `skeleton.png`, so the runtime flip makes it
face left toward the player party.

The default raw-sheet scale is `2`. The raw zombie armour frames are much
smaller inside the source sheet than the Skeleton frames, so pre-scaling before
packing keeps the battlefield sprite readable. The packed frame still fits
inside the same `128 x 128` runtime cell.

The turn-view image is generated as a close upper-body portrait from the first
idle frame, not as a full-body copy. This matches the visual density of
`assets/UI/turn-view-skeleton.png` and prevents the enemy from appearing tiny in
the timeline.

## Reference Generation Strategy

The emergency fallback path:

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

This path is deterministic and does not require a local source sheet, but it is
not the preferred authored asset now that the raw zombie armour sheet is
committed.

## Raw Sheet Detection Strategy

The raw-sheet path:

1. Converts the source image to RGBA.
2. Treats pixels brighter than `--threshold` as sprite pixels.
3. Scans rows to find source animation bands.
4. Scans columns inside each row band to find individual frame boxes.
5. Crops each detected frame.
6. Scales frames with nearest-neighbor sampling.
7. Mirrors raw side-view frames to the project enemy-facing convention.
8. Pastes each frame into a transparent `128 x 128` cell.
9. Writes project-format sprite-sheet JSON.

Default raw-sheet packing for generic sheets:

- `idle`: source row 0
- `walk`: source row 1
- `fight-state`: source row 0
- `attack-1`: source row 2
- `die`: last detected source row

For this specific zombie armour sheet, the processor uses the row selection
listed above.

## Optional Recipe

For precise animation mapping, create a recipe JSON:

```json
{
  "animations": [
    { "name": "idle", "sourceRow": 4, "frames": 1, "frameRate": 8, "loop": true },
    { "name": "walk", "sourceRow": 26, "frames": 5, "frameRate": 10, "loop": true },
    { "name": "fight-state", "sourceRow": 4, "frames": 1, "frameRate": 8, "loop": true },
    { "name": "attack-1", "sourceRow": 6, "frames": 6, "frameRate": 12, "loop": false },
    { "name": "hurt", "sourceRow": 36, "frames": 3, "frameRate": 8, "loop": false },
    { "name": "die", "sourceRow": 19, "frames": 6, "frameRate": 6, "loop": false }
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
