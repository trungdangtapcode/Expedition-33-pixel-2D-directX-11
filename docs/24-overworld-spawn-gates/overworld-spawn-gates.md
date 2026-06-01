# Overworld Spawn Gates

## Purpose

The overworld can contain enemies for the whole route, but not every encounter
should be visible from the start. Opening with every patrol active makes the
chapter feel noisy and lets the player drift away from the intended Verso-alone
setup.

Spawn gates add data-driven visibility rules to `data/overworld_spawns.json`.
They use the same `GameProgress` flags as save/load, StoryDirector, and
ObjectiveDirector.

## Data Shape

Each spawn still uses the existing placement fields:

```json
{
  "id": "pilgrim_crossing_patrol",
  "encounterPath": "data/enemies/skeleton_group.json",
  "requiresFlags": [ "enemy_defeated:silent_market_ambush" ],
  "blockedByFlags": [ "story.route_closed" ],
  "worldX": 1780.0,
  "worldY": -900.0
}
```

Fields:

- `requiresFlags`: every listed flag must be active before the spawn appears.
- `blockedByFlags`: if any listed flag is active, the spawn is hidden.
- Missing arrays mean the spawn is available by default.

Defeated enemies still use the existing `enemy_defeated:<spawn_id>` flag and
are skipped before spawning.

## Current Route Pacing

The first chapter now unlocks encounters in a deliberate order:

1. `meadow_scout` is available from a new game.
2. Silent Market and Western Watch unlock after `story.maelle_joined`.
3. Pilgrim Crossing unlocks after `enemy_defeated:silent_market_ambush`.
4. Glass Shrine patrols unlock after `enemy_defeated:pilgrim_crossing_patrol`.
5. Mirror Gate clone unlocks after `enemy_defeated:glass_shrine_sentinel`.

This keeps the opening focused on Verso, the scout, Maelle, and the duel before
the larger exploration route opens.

## Implementation Notes

`OverworldState::LoadEnemySpawnData()` parses the flag arrays into
`OverworldEnemySpawnData`. The spawning loop calls
`IsEnemySpawnAvailable()` before loading encounter assets.

The check is intentionally in `OverworldState`, not the enemy entity:

- The enemy entity should not know about story flags.
- Hidden spawns should not allocate textures or animation state.
- Save/load can rebuild the overworld from flags without special entity state.

## Future Work

- Add optional spawn groups so a single flag can unlock a named patrol cluster.
- Add route blockers that use the same flag requirements as spawns.
- Add objective rewards after mandatory patrol chains.
