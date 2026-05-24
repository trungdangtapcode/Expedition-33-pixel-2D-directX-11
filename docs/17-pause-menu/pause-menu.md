# Overworld Pause Menu

## Summary

The pause menu is an overworld-only overlay opened with `ESC`. It freezes
gameplay, keeps the current overworld visible underneath, and gives the player
three safe choices: resume, return to title, or quit the game.

## State Ownership

`PauseState` is pushed by `OverworldState` only when the overworld battle
transition phase is idle. It returns `ShouldRenderBelow() == true`, so
`StateManager` renders the preserved overworld before drawing the pause UI.
Only the top state updates, which means player movement, enemy checks, story
regions, and battle triggers stop while paused.

`TimeSystem::SetGameplayPaused(true)` is still used on entry. The pause screen
uses the UI clock for menu animation and fade timing so it remains responsive
while gameplay time is frozen.

## Menu Flow

The main menu rows are:

- Resume
- Return to Title
- Quit Game

Return to title and quit are protected by a confirmation prompt. The prompt
selects `No` by default so a repeated confirm press cannot accidentally leave
the current run. Returning to title runs a short black fade and then calls
`StateManager::ChangeState(std::make_unique<MenuState>())`, which now clears the
entire stack before entering the title menu.

The pause menu intentionally does not expose inventory, lineup, save, load, or
equipment changes. Those flows remain controlled by the overworld and campfire
states so campfire-only restrictions cannot be bypassed.

## Data And Localization

`data/pause_menu_layout.json` owns panel dimensions, dim alpha, highlight
dimensions, fade duration, text scale, and SFX ids. The renderer reads the
currently selected language font from `LocalizationManager`, matching the title
and campfire screens.

Display text uses localization keys:

- `pause.title`
- `pause.resume`
- `pause.return_to_title`
- `pause.quit_game`
- `pause.confirm_title`
- `pause.confirm_quit`
- `pause.yes`
- `pause.no`

SFX use existing groups from `data/audio/sfx.json`: `ui_navigate`,
`ui_confirm`, and `ui_back`.

## Input Rules

- `ESC` in overworld opens the pause menu.
- `ESC` or `Backspace` in the pause main menu resumes gameplay.
- Up/down or W/S moves the main cursor.
- Enter or Space activates the selected row.
- Left/right/up/down or WASD toggles the confirmation choice.
- `ESC` or `Backspace` on a confirmation prompt returns to the pause main menu.

Other states keep their existing local ESC behavior. Battle target menus,
battle result prompts, campfire, inventory, lineup, and title menu are not
converted to global pause in this version.

## Build Notes

New translation units are listed explicitly in `build_src_static.bat`:

- `src/States/PauseState.cpp`
- `src/UI/PauseMenuRenderer.cpp`

The feature should be verified with:

```bat
.\build_src_static.bat 2>&1
```

The expected successful tail is:

```text
[OK] Build succeeded > bin\game.exe  [Debug]
```
