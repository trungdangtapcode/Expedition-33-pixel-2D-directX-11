# Overworld Objective HUD

## Purpose

The overworld objective HUD gives the player compact route guidance without covering the map or colliding with the coin counter. It separates the area title, objective body, and contextual action hint so localized text does not become one long unreadable line.

## Runtime Flow

`ObjectiveDirector` resolves the active objective from story flags and player position. `OverworldState` stores the resolved body and waypoint hint separately. `ObjectiveTrackerRenderer` draws the text in screen space after world rendering and before the currency HUD.

## Data File

Layout lives in `data/overworld_objective_hud.json`.

Important fields:

- `x`, `y`: top-left screen position.
- `maxWidth`: preferred text width.
- `rightReserveWidth`: space reserved for the coin HUD.
- `bodyMaxLines`: maximum body lines before ellipsis.
- `titleScale`, `bodyScale`, `hintScale`: SpriteFont scale per row.
- `titleR/G/B/A`, `bodyR/G/B/A`, `hintR/G/B/A`: text colors.
- `shadowA`, `shadowOffset`: readability shadow tuning.

Objective progression lives in `data/objectives.json`.

Important fields:

- `targetKind`: semantic target type such as `enemy`, `npc`, or `story_area`.
- `targetId`: stable id from the relevant data file, such as an enemy spawn id.
- `waypointX`, `waypointY`: world-space marker position.
- `arrivalDistanceUnits`: optional per-objective arrival radius.
- `arrivalHintKey`: localized action hint shown at arrival range.

For fight objectives, `targetKind` should be `enemy` and `targetId` should match `data/overworld_spawns.json`. `OverworldState` uses that id as a fallback when `B` is pressed near the objective marker, so the HUD and input behavior stay aligned even if the enemy's contact radius is tighter than the objective arrival radius.

## Wrapping Rules

Text wrapping uses `BattleTextRenderer::MeasureStringRaw`, so it is based on the active language font rather than character count. Lines are split on word boundaries. If a localized objective still exceeds the configured line limit, the final visible line is shortened at a word boundary and receives `...`.

## Authoring Rules

- Keep objective bodies short enough to fit in two lines at 1280x720.
- Put direct input instructions in the waypoint hint, not the body.
- Do not concatenate body and hint in code.
- Keep coin HUD reservation large enough for all supported languages.
- Do not author a fight waypoint without `targetKind: "enemy"` and a valid `targetId`.
- Keep objective beacon art small and restrained; it should identify the route target without covering enemies, NPCs, or campfires.

## Integration

`OverworldState` owns one `ObjectiveTrackerRenderer` and one shared `BattleTextRenderer`. The tracker owns no GPU resources, so language font reload remains centralized in `OverworldState`.

The current objective beacon uses `assets/UI/objective_beacon_v2.png`, generated with the built-in imagegen tool on a chroma-key background and processed locally into a transparent 64x64 PNG. Visual tuning stays in `assets/UI/objective-beacon-ui.json`.
