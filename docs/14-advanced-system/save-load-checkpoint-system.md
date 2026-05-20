# Save/Load Checkpoint System

## Goal

The checkpoint system persists only durable gameplay authority data:

- Party member base stats, current resources, level, EXP, growth, and equipment.
- Inventory item counts.
- The scene id that should be restored after loading.

It does not serialize live state instances, battle action queues, renderer state, GPU resources, or event listeners.

## Architecture

`SaveManager` is a system-level singleton in `src/Systems/SaveManager.*`.

It depends on:

- `PartyManager` for party snapshots.
- `Inventory` for item-count snapshots.
- `EventManager` for automatic checkpoint writes after battle victory.
- `GameProgress` for one-time world flags such as claimed campfire upgrades.
- `JsonLoader::detail` helpers for the same small hand-rolled JSON style used elsewhere.

The state layer stays thin:

- `MenuState` starts a New Game by resetting durable systems, writing an initial checkpoint, and pushing `OverworldState`.
- `MenuState` continues by loading the checkpoint and pushing `OverworldState`.
- `OverworldState` spawns checkpoint campfires from `data/campfires.json`.
- Near a campfire: `F` quick-saves, `C` quick-loads, `U` opens the campfire menu, and `L` opens lineup/equipment.
- Battle code does not know about save files. It already broadcasts `battle_end_victory`, which `SaveManager` listens to.

## File Format

The active checkpoint path is configured in `data/save_checkpoints.json`.

Current output:

```json
{
  "schemaVersion": 1,
  "checkpointId": "overworld_after_battle",
  "sceneId": "overworld",
  "reason": "battle_victory",
  "party": [],
  "flags": [],
  "inventory": []
}
```

Schema versioning is mandatory so future save migrations can reject or upgrade old saves explicitly.

## Asset

The checkpoint badge lives at:

```text
assets/UI/save_checkpoint_badge.png
assets/UI/save_checkpoint_badge.json
assets/animations/campfire_checkpoint.png
assets/animations/campfire_checkpoint.json
```

The source generator is:

```text
tools/draw_save_load_assets.py
tools/draw_campfire_asset.py
```

The generator uses only the Python standard library, writes a PNG directly, and can be rerun without external tools.

## Future Extension Points

- Add named checkpoint triggers in map data once overworld spawn positions are map-authored.
- Add a `sceneState` object when the overworld has defeated enemy ids or cutscene progress.
- Add multiple slots by changing `slotPath` into a slot table in `data/save_checkpoints.json`.
- Add migration functions keyed by `schemaVersion`.
