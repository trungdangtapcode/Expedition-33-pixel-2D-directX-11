# Title Menu System

## Goal

The title screen is now a real visual menu instead of a log-only input router.
It uses the existing game logo/banner asset and keeps save-slot behavior routed
through `SaveManager`.

The feature has three priorities:

- Make the first screen feel authored and game-like.
- Keep save/load slot ownership in `SaveManager`.
- Keep rendering separate from title command logic.

## Local Research

The previous title flow lived entirely in `MenuState`:

- `Enter` created a new game in Slot 1.
- `C` loaded the first occupied slot.
- `1` through `9` loaded numbered slots directly.
- `Render()` intentionally drew nothing.

The project already had reusable UI primitives:

- `NineSliceRenderer` for RPG panel chrome.
- `BattleTextRenderer` for SpriteFont text.
- `SaveManager::GetSlotInfos()` for read-only slot metadata.
- `assets/e33_pixel_banner.png` as a full-screen title/logo image.

The new design keeps those responsibilities intact.

## Runtime Flow

`MenuState` owns input and transitions. It has two phases:

- `MainOptions`
- `LoadSlots`

Main options:

- `New Game`
- `Continue`
- `Load Slot`
- `Quit`

`Continue` and `Load Slot` are disabled until at least one save slot exists.
Cursor movement skips disabled entries so the player does not land on commands
that cannot be activated.

`Load Slot` opens a slot picker using `SaveSlotInfo` converted into renderer
view data. Empty slots stay visible and play the unavailable SFX if confirmed.

## Rendering Split

The visual side lives in:

```text
src/UI/TitleMenuRenderer.h
src/UI/TitleMenuRenderer.cpp
```

`TitleMenuRenderer` owns:

- The banner texture SRV.
- A 1x1 fill texture for dimming and highlights.
- `SpriteBatch` and `CommonStates`.
- `NineSliceRenderer` for the menu panels.
- `BattleTextRenderer` for command and slot labels.

It receives one `TitleMenuRenderState` per frame. The renderer does not call
`SaveManager`, mutate gameplay systems, or perform state transitions.

## Layout Data

Layout is configured in:

```text
data/main_menu_layout.json
```

The file controls:

- Banner image path.
- Panel texture and nine-slice JSON paths.
- Font path.
- Main and slot panel dimensions.
- Panel placement offsets.
- Logo pulse alpha.
- Row positions and spacing.
- Flash message duration.

This keeps the composition adjustable without recompiling C++.

## Visual Direction

The screen uses the existing `assets/e33_pixel_banner.png` as the first visual
signal. The renderer cover-scales it to the current viewport, applies a subtle
breathing alpha, and draws a translucent dim overlay so the command list stays
readable.

The command panel sits in the lower-right portion of the screen at 1280x720 so
it does not cover the center of the logo. The load-slot picker uses a wider
bottom panel because slot metadata needs more horizontal space.

## Input

Current controls:

- `Up` / `Down`: move the cursor.
- `Enter`: confirm.
- `Backspace`: return from the slot picker to the main list.
- `Quit`: closes the game from the menu option.

The global `Escape` handling in `GameApp` still exits the process.

## Save/Load Contract

`MenuState` still delegates all persistence work:

- New Game calls `PartyManager::ResetToDefaults()`,
  `Inventory::ResetToDefaults()`, `GameProgress::Reset()`, then
  `SaveManager::SaveCheckpointToSlot(0, "new_game")`.
- Continue calls `SaveManager::FindFirstExistingSlot()` and then loads that
  slot.
- Load Slot calls `SaveManager::LoadCheckpointFromSlot(slotIndex, &sceneId)`.

The renderer receives only display strings and booleans. It never parses save
files directly.

## Build Integration

`src/UI/TitleMenuRenderer.cpp` is listed explicitly in `build_src_static.bat`.
This is required because the project does not glob translation units.
