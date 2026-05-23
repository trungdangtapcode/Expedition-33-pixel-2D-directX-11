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

- `BattleTextRenderer` for SpriteFont text.
- `SaveManager::GetSlotInfos()` for read-only slot metadata.
- `assets/e33_pixel_banner.png` as a full-screen title/logo image.

The new design keeps those responsibilities intact.

The direct gameplay hand-off was also replaced with a deferred transition.
Menu commands now prepare save/load state first, then fade the title screen to
black before `StateManager::ChangeState()` swaps into the overworld.

## Runtime Flow

`MenuState` owns input and transitions. It has three phases:

- `PressStart`
- `MainOptions`
- `Options`
- `NewGameSlots`
- `LoadSlots`

`PressStart` is the first screen. It keeps the logo unobstructed and waits for
any common confirm/movement key before revealing menu choices.

Main options:

- `New Game`
- `Continue`
- `Load Slot`
- `Options`
- `Quit`

`Continue` and `Load Slot` are disabled until at least one save slot exists.
Cursor movement skips disabled entries so the player does not land on commands
that cannot be activated.

`New Game` opens a slot picker first. The cursor defaults to the first empty
slot when one exists, so players can create Slot 2 or Slot 3 directly from the
title screen instead of being forced into Slot 1.

`Load Slot` opens a slot picker using `SaveSlotInfo` converted into renderer
view data. Empty slots stay visible and play the unavailable SFX if confirmed.

`Options` opens a dedicated lower panel for language and audio preferences.
Audio rows adjust BGM, SFX, and future Voice volume in 10 percent steps and
save immediately to `save/settings.json`.

When `New Game`, `Continue`, or an occupied `Load Slot` succeeds, `MenuState`
starts a pending gameplay transition instead of changing state immediately.
While this transition is active, input is ignored, the black overlay alpha
advances with `dt`, and the state change runs only after the fade duration is
complete.

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
- `BattleTextRenderer` for the press prompt, command labels, and slot labels.

It receives one `TitleMenuRenderState` per frame. The renderer does not call
`SaveManager`, mutate gameplay systems, or perform state transitions.

## Layout Data

Layout is configured in:

```text
data/main_menu_layout.json
```

The file controls:

- Banner image path.
- Font path.
- Title BGM track id.
- Press-start prompt placement and blink speed.
- Centered command-list placement.
- Options panel and volume-meter dimensions.
- Slot-list dimensions.
- Logo pulse alpha.
- Ambient particle alpha.
- Row positions and spacing.
- Flash message duration.
- Gameplay transition fade-out duration.

This keeps the composition adjustable without recompiling C++.

Current title BGM tuning:

```json
"bgmTrackId": "menu"
```

The track id is resolved through `data/audio/bgm.json`, where `menu` currently
maps to `assets/sound/OST/alicia_menu.mp3`. Changing the title music should
only require data edits.

Current transition tuning:

```json
"transitionFadeOutDuration": 1.55
```

The value is in seconds and is read by `TitleMenuRenderer`, then exposed to
`MenuState` through `GetTransitionFadeOutDuration()`.

## Audio Contract

The title menu does not hardcode audio file paths. The split is:

- `data/audio/bgm.json` maps stable track ids to audio files.
- `data/main_menu_layout.json` selects the title screen track with
  `bgmTrackId`.
- `TitleMenuRenderer` loads that track id as layout data.
- `MenuState` broadcasts the generic `bgm_play` event using the loaded id.
- `AudioManager` remains the only system that loads and plays BGM files.
- Audio volume preferences are read from `SettingsManager` and applied through
  `AudioManager` bus volume APIs.

## Visual Direction

The target reference is a clean title composition:

- Centered logo remains the primary focus.
- No floating panel is drawn on the first screen.
- `PRESS ANY BUTTON` appears below the logo with a slow blink.
- Small deterministic particles drift over the title image to avoid a static
  splash-screen feeling.

After the prompt is accepted, the command list appears as centered text. The
selected row receives only a subtle warm highlight, not a large dialog panel.
The load-slot picker uses a dark bottom band because slot metadata needs more
contrast, but it avoids the previous gray RPG box.

## Input

Current controls:

- Any common confirm/movement key: leave the press-start screen.
- `Up` / `Down`: move the cursor.
- `Enter`: confirm.
- `Backspace`: return from the slot picker to the main list.
- `Left` / `Right`: change language or selected audio volume in Options.
- `Quit`: closes the game from the menu option.

The global `Escape` handling in `GameApp` still exits the process.

## Save/Load Contract

`MenuState` still delegates all persistence work:

- New Game first opens the slot picker, then calls
  `PartyManager::ResetToDefaults()`,
  `Inventory::ResetToDefaults()`, `GameProgress::Reset()`, then
  `SaveManager::SaveCheckpointToSlot(slotIndex, "new_game")`.
- Continue calls `SaveManager::FindFirstExistingSlot()` and then loads that
  slot.
- Load Slot calls `SaveManager::LoadCheckpointFromSlot(slotIndex, &sceneId)`.
  `SaveManager` also restores the saved overworld checkpoint id and player
  coordinates into `GameProgress`, so entering `OverworldState` places the
  player at the saved location instead of the map default.

The renderer receives only display strings and booleans. It never parses save
files directly.

## Transition Contract

The title-to-overworld transition has a strict split:

- `MenuState` owns the timer, target scene, and input lock.
- `TitleMenuRenderer` owns drawing the black full-screen overlay.
- `TitleMenuRenderState::transitionAlpha` is the only data passed between them.
- `data/main_menu_layout.json` owns the transition duration.

This keeps `StateManager` generic and avoids adding title-specific animation
state to global scene-stack code.

## Build Integration

`src/UI/TitleMenuRenderer.cpp` is listed explicitly in `build_src_static.bat`.
This is required because the project does not glob translation units.
