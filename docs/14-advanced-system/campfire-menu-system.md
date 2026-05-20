# Campfire Menu System

## Purpose

The campfire menu is the overworld recovery and save-slot hub. It is opened only
when the player stands near a checkpoint campfire and presses `U`.

This feature moves campfire actions out of the raw overworld hotkey branch and
into a dedicated pushed game state. That keeps the campfire entity passive and
preserves the project rule that game flow belongs in states, not in renderable
overworld objects.

## Current Player Flow

Near a campfire:

- `U` opens the campfire menu.
- `L` opens the party lineup directly.
- `F` quick-saves the active slot.
- `C` quick-loads the active slot.

Inside the campfire menu:

- `Up` and `Down` move the cursor.
- `Enter` confirms the selected option.
- `Escape`, `Backspace`, or `U` closes the menu.

The current menu options are:

- `Rest`
- `Save Slot`
- `Load Slot`
- `Upgrade Party`
- `Lineup`
- `Exit`

`Save Slot` and `Load Slot` each open a second menu phase where the player
chooses a numbered slot.

## Architecture

`CheckpointCampfire` remains an `IGameObject`. Its job is still limited to:

- Loading and rendering the campfire sprite.
- Exposing proximity through `IsPlayerNearby`.
- Providing immutable campfire data loaded from `data/campfires.json`.

`OverworldState` owns the interaction routing:

- It tracks one-press input edges for `F`, `C`, `U`, and `L`.
- It checks `FindNearbyCampfire` before allowing campfire-only actions.
- It pushes `CampfireState` when the player presses `U` near a campfire.

`CampfireState` owns the modal menu:

- It is pushed onto the `StateManager` stack.
- While it is on top, overworld update is paused.
- It owns its UI renderers and releases them in `OnExit`.
- It delegates durable operations to existing systems.
- It has separate phases for the main menu, save-slot selection, and load-slot
  selection.

## System Boundaries

`CampfireState` uses these existing services:

- `PartyManager` for party restoration and EXP training.
- `SaveManager` for active-slot and numbered-slot save/load.
- `GameProgress` for one-time campfire training flags.
- `StateManager` to push `LineupState` or pop itself.
- `AudioManager` for menu feedback SFX.

It does not:

- Own or mutate overworld entities.
- Read or write save files directly.
- Create party members or item definitions.
- Bypass `PartyManager`, `SaveManager`, or `GameProgress`.

## Save And Training Behavior

`Rest` restores every active party member through `PartyManager::RestoreFullHP`
and saves the active slot with reason:

```text
campfire_rest:<campfireId>
```

`Save Slot` opens a numbered slot picker. Confirming a slot writes that slot
with reason:

```text
campfire_save:<campfireId>
```

`Load Slot` opens a numbered slot picker. Confirming an occupied slot restores
that slot through `SaveManager`.

Slot rows are supplied by `SaveManager::GetSlotInfo`. Empty slots are labeled
`Empty`; occupied slots show lead member id, lead level, and save reason.

`Upgrade Party` preserves the existing one-time campfire training behavior:

- The reward amount comes from `upgradeExpReward` in `data/campfires.json`.
- The one-time flag key is `campfire_upgrade:<campfireId>`.
- If the flag is missing, the active party receives the EXP reward.
- If the flag already exists, the menu still restores the party but does not
  duplicate the EXP reward.

## UI Implementation

The menu uses the same UI primitives as the existing RPG screens:

- `NineSliceRenderer` for the dim screen overlay and panel frame.
- `BattleTextRenderer` for all menu text.

The state starts every input edge tracker as pressed in `OnEnter`. This absorbs
the `U` key that opened the menu so the state does not immediately close on its
first frame.

## Files

Implementation files:

- `src/States/CampfireState.h`
- `src/States/CampfireState.cpp`
- `src/States/OverworldState.cpp`
- `src/States/OverworldState.h`
- `build_src_static.bat`

Data file used by the feature:

- `data/campfires.json`

## Build Notes

The project build script lists every `.cpp` file explicitly. Because
`CampfireState.cpp` is a new translation unit, it must remain listed in
`build_src_static.bat`.

Verification command:

```bat
.\build_src_static.bat 2>&1
```

The expected successful tail is:

```text
[OK] Build succeeded > bin\game.exe  [Debug]
```

## Future Shop Extension

The campfire menu is the correct entry point for the planned shop and character
upgrade systems.

Recommended next additions:

- Add a `CurrencyWallet` system for coins.
- Persist currency through `SaveManager`.
- Add data-driven shop stock files under `data/shops`.
- Add a shop submenu under `CampfireState`.
- Add data-driven upgrade tables for stat upgrades.
- Persist upgrade counters through `GameProgress` or a typed party-progress
  extension.

The important constraint is that shop and upgrade transactions should remain
service-driven:

- Spend coins through the currency system.
- Add purchased items through `Inventory`.
- Apply permanent party upgrades through `PartyManager`.
- Save after confirmed transactions through `SaveManager`.
