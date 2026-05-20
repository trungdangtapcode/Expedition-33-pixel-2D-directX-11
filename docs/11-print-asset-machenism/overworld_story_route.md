# Overworld Story Route

## Goal

The overworld is no longer just a large traversal test. It is structured as
a short playable journey with a visible motivation:

```text
Recover the lost campfires, cross the ruined roads, and confront the mirror
clone at the eastern gate.
```

The route is intentionally readable from the camera:

1. Start at the central meadow camp.
2. Follow roads into the silent market ruins.
3. Choose side routes for recovery and training.
4. Push through enemy-held crossings.
5. Rest at the glass shrine.
6. Reach the mirror gate and fight the clone.

## Map Shape

Current map:

- Path: `assets/environments/overworld_map.json`
- Size: `128 x 96` tiles
- Tile size: `64 x 64` pixels
- World span: `8192 x 6144` pixels
- Runtime origin: centered on world `(0, 0)`

The map generator lives at:

```text
patches/generate_map.py
```

The generator creates:

- A central meadow camp.
- A broad east-west road that gives the player a natural main path.
- A northern market ruin loop.
- A western watch side route.
- A southern shrine route.
- A diagonal eastern road into the mirror gate.
- Landmark clearings large enough to read on camera.
- Object-layer houses, tables, rocks, and mirror-gate pillars.
- Collision rectangles matching those objects.

## Story Data

Area names and objective text live in:

```text
data/overworld_story.json
```

The file contains:

- `defaultArea`
- `defaultObjective`
- `regions[]`

Each region uses world-space bounds:

```json
{
  "id": "mirror_gate",
  "name": "Mirror Gate",
  "objective": "The clone waits beyond the gate. Defeat it to prove Verso is more than an echo.",
  "minX": 2840.0,
  "minY": -2100.0,
  "maxX": 3960.0,
  "maxY": -940.0
}
```

`OverworldState` checks the player's world position against these regions and
renders the active area title plus objective through `BattleTextRenderer`.

This is deliberately smaller than a full quest system. It gives the player
short-term motivation while keeping the current architecture simple.

## Campfire Placement

Campfires live in:

```text
data/campfires.json
```

Current campfires:

- `meadow_start`: first checkpoint and menu introduction.
- `western_watch`: optional side-route recovery.
- `pilgrim_crossing`: mid-route recovery before stronger patrols.
- `glass_shrine`: final recovery before the eastern gate.

Each campfire keeps the existing fields:

- `id`
- `texturePath`
- `jsonPath`
- `idleClip`
- `worldX`
- `worldY`
- `contactRadius`
- `renderScale`
- `upgradeExpReward`

The `upgradeExpReward` values create a light exploration reward. Reaching
optional campfires gives one-time EXP through the campfire menu's `Upgrade
Party` option.

## Enemy Placement

Overworld enemy positions now live in:

```text
data/overworld_spawns.json
```

Each spawn contains:

- `id`
- `encounterPath`
- `worldX`
- `worldY`

Example:

```json
{
  "id": "pilgrim_crossing_patrol",
  "encounterPath": "data/enemies/skeleton_group.json",
  "worldX": 1780.0,
  "worldY": -900.0
}
```

`OverworldState` loads this file, then loads the referenced encounter JSON
through `JsonLoader::LoadEnemyEncounterData`.

This replaces hardcoded enemy positions in C++ and lets encounter pacing follow
the map layout.

## Pacing Intent

The current route uses six overworld encounters:

- A solo skeleton near the meadow road to teach battle entry.
- A group ambush in the silent market ruins.
- A western watch guard on the optional side route.
- A stronger group at pilgrim crossing.
- A shrine sentinel before the final recovery point.
- The mirror clone near the eastern gate.

The player motivation is layered:

- Roads answer "where should I go?"
- Region objectives answer "why am I here?"
- Campfires answer "what did I gain for exploring?"
- Enemy gates answer "what is blocking progress?"
- The mirror gate answers "what is the destination?"

## Editing Workflow

To change the geography:

1. Edit `patches/generate_map.py`.
2. Run:

```text
python patches/generate_map.py
```

3. Open `assets/environments/overworld_map.json` in Tiled to inspect the result.
4. Update `data/campfires.json`, `data/overworld_spawns.json`, and
   `data/overworld_story.json` if the route landmarks move.
5. Build:

```bat
.\build_src_static.bat 2>&1
```

Expected successful tail:

```text
[OK] Build succeeded > bin\game.exe  [Debug]
```

## Future Work

The next natural step is a data-driven interactable system for signs, memory
stones, and story triggers. That would move beyond passive objective text and
allow the player to discover narrative beats directly in the world.
