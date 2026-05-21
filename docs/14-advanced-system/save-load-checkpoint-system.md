# Save/Load Slot System

## Goal

The save system now supports numbered save slots instead of a single fixed
checkpoint file.

The system still persists only durable gameplay authority data:

- Party member base stats, current resources, level, EXP, growth, and equipment.
- Inventory item counts.
- One-time world flags from `GameProgress`.
- The scene id, checkpoint id, and overworld player position that should be
  restored after loading.

It does not serialize live state instances, battle action queues, renderer
state, GPU resources, transient input state, or event listeners.

## Architecture

`SaveManager` is the only authority for save files. It lives in:

```text
src/Systems/SaveManager.h
src/Systems/SaveManager.cpp
```

It depends on:

- `PartyManager` for party snapshots.
- `Inventory` for item-count snapshots.
- `GameProgress` for one-time world flags such as claimed campfire upgrades.
- `EventManager` for automatic post-battle saves.
- `JsonLoader::detail` helpers for the project-local JSON parser style.

State classes stay thin:

- `MenuState` starts a New Game by opening a slot picker and writing the
  selected slot.
- `MenuState` renders a visual title menu with New Game, Continue, Load Slot,
  and Quit commands.
- `MenuState` can continue the first occupied slot through the Continue command.
- `MenuState` can open a visual slot picker through the Load Slot command.
- `CampfireState` opens explicit Save Slot and Load Slot submenus.
- `OverworldState` keeps `F` and `C` as quick save/load shortcuts for the
  active slot while near a campfire.

## Slot Configuration

Slot behavior is configured in:

```text
data/save_checkpoints.json
```

Current config:

```json
{
  "slotPath": "save/checkpoint_slot_0.json",
  "slotDirectory": "save",
  "slotFilePrefix": "checkpoint_slot_",
  "slotFileExtension": ".json",
  "slotCount": 3,
  "defaultSlotIndex": 0,
  "autoCheckpointId": "overworld_after_battle",
  "autoSceneId": "overworld",
  "defaultCheckpointId": "new_game",
  "defaultPlayerX": 120.0,
  "defaultPlayerY": 80.0,
  "iconPath": "assets/UI/save_checkpoint_badge.png"
}
```

`slotPath` remains for backward compatibility with older docs/tools, but the
runtime now computes slot paths from:

```text
slotDirectory + "/" + slotFilePrefix + slotIndex + slotFileExtension
```

With the current config:

```text
save/checkpoint_slot_0.json
save/checkpoint_slot_1.json
save/checkpoint_slot_2.json
```

The player-facing labels are Slot 1, Slot 2, and Slot 3. Internally those are
indices 0, 1, and 2.

## SaveManager API

Compatibility API:

```cpp
bool SaveCheckpoint(const std::string& reason) const;
bool LoadCheckpoint(std::string* outSceneId = nullptr) const;
```

These functions operate on the active slot.

Slot-specific API:

```cpp
bool SaveCheckpointToSlot(int slotIndex, const std::string& reason) const;
bool LoadCheckpointFromSlot(int slotIndex, std::string* outSceneId = nullptr) const;
bool SlotExists(int slotIndex) const;
int FindFirstExistingSlot() const;
std::string GetSlotPath(int slotIndex) const;
SaveSlotInfo GetSlotInfo(int slotIndex) const;
std::vector<SaveSlotInfo> GetSlotInfos() const;
```

`SaveSlotInfo` is read-only metadata for UI:

```cpp
struct SaveSlotInfo
{
    int slotIndex;
    std::string path;
    bool exists;
    int schemaVersion;
    std::string checkpointId;
    std::string sceneId;
    std::string reason;
    std::string leadMemberId;
    int leadLevel;
    float playerX;
    float playerY;
    bool hasPlayerPosition;
};
```

## Active Slot

`SaveManager` tracks an active slot index.

- Saving to a slot makes that slot active.
- Loading from a slot makes that slot active.
- Automatic post-battle saves write to the active slot.
- Old compatibility calls use the active slot.

This gives expected RPG behavior: after loading Slot 2, campfire quick-save and
battle auto-save continue writing Slot 2 until the player chooses another slot.

## File Format

Each slot writes the same schema as the earlier checkpoint file, plus
`slotIndex` metadata:

```json
{
  "schemaVersion": 1,
  "slotIndex": 0,
  "checkpointId": "overworld_after_battle",
  "sceneId": "overworld",
  "playerX": 120.0,
  "playerY": 80.0,
  "reason": "campfire_save:meadow_start",
  "party": [],
  "flags": [],
  "inventory": []
}
```

Schema versioning remains mandatory so future save migrations can reject or
upgrade old saves explicitly.

## Campfire Flow

Near a campfire:

- `U` opens the campfire menu.
- `F` quick-saves the active slot.
- `C` quick-loads the active slot.
- `L` opens lineup/equipment.

Campfire saves now write an explicit checkpoint id (`campfire:<id>`) and the
player's overworld coordinates into the selected slot. Loading from the
campfire menu restores the saved managers, closes the menu, and asks the
underlying overworld state to rebuild itself from the loaded snapshot.

Inside the campfire menu:

- `Save Slot` opens a slot selection submenu.
- `Load Slot` opens a slot selection submenu.
- Empty slots are labeled `Empty`.
- Existing slots show lead member id, lead level, and save reason.

The campfire menu does not parse save files directly. It asks `SaveManager` for
`SaveSlotInfo` and calls the slot-specific save/load APIs.

## Title Menu Flow

Current title menu behavior:

- The first screen uses `assets/e33_pixel_banner.png` as a full-screen title
  banner with a `PRESS ANY BUTTON` prompt.
- Pressing a common confirm or movement key reveals the command list.
- `New Game` opens a slot picker, then resets durable systems and writes the
  selected slot.
- `Continue` loads the first occupied slot.
- `Load Slot` opens a visual slot picker backed by `SaveSlotInfo`.
- `Quit` closes the game.

The title menu renderer is documented in:

```text
docs/14-advanced-system/title-menu-system.md
```

## Assets

The save badge and campfire assets live at:

```text
assets/UI/save_checkpoint_badge.png
assets/UI/save_checkpoint_badge.json
assets/animations/campfire_checkpoint.png
assets/animations/campfire_checkpoint.json
```

The source generators are:

```text
tools/draw_save_load_assets.py
tools/draw_campfire_asset.py
```

## Future Extension Points

- Add timestamp metadata once the project has a stable wall-clock policy for
  save metadata.
- Add `sceneState` for defeated overworld enemy ids and cutscene progress.
- Promote the current `enemy_defeated:<spawnId>` flags into a dedicated
  structured scene-state array if the world grows beyond simple one-time
  overworld enemies.
- Add migration functions keyed by `schemaVersion`.
