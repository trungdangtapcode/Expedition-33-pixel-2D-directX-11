# Objective Director

## Purpose

The overworld previously displayed objective text from the region the player
stood in. That helped describe places, but it did not answer the more important
chapter question: what should the player do next after save/load, battle wins,
or dialogue completion?

`ObjectiveDirector` adds a small data-driven chapter spine. It reads
`data/objectives.json`, evaluates ordered objective stages against
`GameProgress` flags, and returns the first active objective for
`OverworldState` to display.

## Data Shape

Objective stages are ordered. The first stage whose requirements pass becomes
the active objective.

```json
{
  "id": "meet_maelle",
  "titleKey": "objective.meet_maelle.title",
  "bodyKey": "objective.meet_maelle.body",
  "requiresFlags": [ "enemy_defeated:meadow_scout" ],
  "blockedByFlags": [ "dialogue_completed:maelle_confrontation" ],
  "hasWaypoint": true,
  "waypointX": 1080.0,
  "waypointY": -160.0,
  "waypointLabelKey": "objective.waypoint.maelle",
  "arrivalHintKey": "objective.arrival.talk"
}
```

Fields:

- `requiresFlags`: every listed flag must exist in `GameProgress`.
- `blockedByFlags`: if any listed flag exists, the objective is skipped.
- `hasWaypoint`: enables a compact distance and compass hint.
- `distanceUnitsPerMeter`: controls how world units convert to displayed meters.
- `arrivalDistanceUnits`: controls when a waypoint stops showing distance text
  and starts showing the action hint.
- `arrivalHintKey`: localized template shown at the waypoint, such as
  `{label}: Press B to fight.` or `{label}: Press E to talk.`

## Runtime Flow

1. `OverworldState::OnEnter()` initializes `ObjectiveDirector`.
2. `UpdateStoryRegion()` still resolves the current area and biome theme.
3. `ObjectiveDirector::Resolve()` evaluates saved progress flags.
4. If an objective is active, its body replaces the region objective.
5. If no objective matches, the region objective remains the fallback.

This keeps region text useful as local flavor while the objective director owns
chapter-level guidance.

## Authoring Rules

- Do not store runtime objective booleans in `OverworldState`.
- Use stable `GameProgress` flags already written by battle, dialogue, and story
  systems.
- Keep objectives ordered from most immediate to most complete.
- Keep waypoint hints compact; the overworld HUD has limited horizontal space.
- Author arrival hints for every actionable waypoint so the HUD never displays
  a useless `0m` direction when the player is already standing at the objective.
- Localized display strings live in `data/localization/*.json`; C++ stores keys
  and English fallback only.

## Current Chapter Spine

The initial route now resolves these objectives:

1. Defeat the first road scout.
2. Find Maelle at the boulevard trigger.
3. Win Maelle's duel.
4. Speak with Maelle so she joins the party.
5. Clear the silent market ambush.
6. Win the pilgrim crossing patrol.
7. Recover the glass shrine route.
8. Reach the mirror gate and defeat the clone.
9. Prepare for the next route after the mirror clone is defeated.

## Future Work

- Add optional objective rewards for side paths.
- Add a minimap marker renderer that consumes the same waypoint data.
- Add objective completion toast SFX and animation through data.
- Add chapter ids when the game has more than one route.
