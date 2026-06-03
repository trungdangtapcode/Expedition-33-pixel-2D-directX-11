# Overworld Spawn Gates

## Purpose

The overworld can contain enemies for the whole route while still preserving
story-only encounters such as Maelle's duel. Normal enemies are now farmable:
they appear from the start, they still set `enemy_defeated:<spawn_id>` after
victory for objective progression, and they can reappear after the overworld is
rebuilt if their data marks them as respawnable.

Spawn gates remain available for future story locks, but the current route uses
them sparingly. Maelle is not in `data/overworld_spawns.json`; she is controlled
by NPC and StoryDirector data.

## Data Shape

Each spawn still uses the existing placement fields:

```json
{
  "id": "pilgrim_crossing_patrol",
  "encounterPath": "data/enemies/skeleton_group.json",
  "requiresFlags": [ "enemy_defeated:silent_market_ambush" ],
  "blockedByFlags": [ "story.route_closed" ],
  "respawnAfterDefeat": true,
  "worldX": 1780.0,
  "worldY": -900.0
}
```

Fields:

- `requiresFlags`: every listed flag must be active before the spawn appears.
- `blockedByFlags`: if any listed flag is active, the spawn is hidden.
- `respawnAfterDefeat`: if `true`, the spawn ignores its own
  `enemy_defeated:<spawn_id>` flag when the overworld is rebuilt.
- Missing arrays mean the spawn is available by default.

Defeated enemies still use the existing `enemy_defeated:<spawn_id>` flag. That
flag always drives objectives and story checks. It only hides the overworld
entity when `respawnAfterDefeat` is omitted or `false`.

## Current Route Pacing

The first chapter keeps all normal combat spawns visible so the player can farm
and test encounters without hidden progression gates:

1. `meadow_scout` appears from a new game.
2. Silent Market and Western Watch skeletons appear from a new game.
3. Pilgrim Crossing patrol appears from a new game.
4. Glass Shrine skeleton and zombie armour patrols appear from a new game.
5. Mirror Gate clone appears from a new game.

Maelle remains the exception. Her confrontation and duel are authored through
`data/overworld_npcs.json` and `data/story_events.json`, not through normal
farmable enemy spawns.

## Implementation Notes

`OverworldState::LoadEnemySpawnData()` parses the flag arrays and
`respawnAfterDefeat` into `OverworldEnemySpawnData`. The spawning loop first
checks whether a non-respawnable enemy has already been defeated, then calls
`IsEnemySpawnAvailable()` before loading encounter assets.

The check is intentionally in `OverworldState`, not the enemy entity:

- The enemy entity should not know about story flags.
- Hidden spawns should not allocate textures or animation state.
- Save/load can rebuild the overworld from flags without special entity state.
- Respawnable enemies can still advance objectives because the defeat flag is
  written even when the spawn is allowed to return later.

## Future Work

- Add optional spawn groups so a single flag can unlock a named patrol cluster.
- Add route blockers that use the same flag requirements as spawns.
- Add objective rewards after mandatory patrol chains.
- Add explicit respawn timers if farming needs same-session enemy returns
  instead of reload/state-rebuild returns.
