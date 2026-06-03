# Maelle Sister Duel Story Event System

## Purpose

The opening story now starts with Verso alone on the left side of the Paris route. After the first road fight, the player reaches Maelle, sees a confrontation dialogue, duels her, and recruits her only after the post-victory reconciliation dialogue.

The feature is intentionally split across data and small systems:

- `PartyManager` owns active roster state and recruitment.
- `StoryDirector` evaluates story triggers and queues commands.
- `OverworldState` executes commands because it owns state-stack transitions and player position.
- `DialogueState` and `BattleState` remain reusable and only broadcast completion events.

## Data Files

- `data/party_roster.json`
  Defines all recruitable party members. `startingMember: true` starts in a new game. Verso starts active; Maelle starts inactive.

- `data/story_events.json`
  Defines trigger rectangles and event responses. The Maelle chain uses:
  `enemy_defeated:meadow_scout` -> `maelle_confrontation` -> `maelle_duel` -> `maelle_reconcile` -> recruit Maelle.

- `data/dialogues/maelle_confrontation.json`
  Pre-duel accusation dialogue.

- `data/dialogues/maelle_reconcile.json`
  Post-victory recruitment dialogue.

- `data/enemies/maelle_duel.json`
  Story duel encounter with Maelle battle art, Paris backdrop, fight BGM, and victory BGM.

- `data/skills/maelle_duel_attack.json`
  Maelle's duel attack. It reuses the existing attack and bullet-hell infrastructure.

- `data/overworld_npcs.json`
  Places Maelle at the confrontation point. `showIfFlag` keeps her hidden until the scout is defeated; `hideIfFlag` removes her after recruitment.

## Story Flow

1. New Game calls `PartyManager::ResetToDefaults()`, which loads `data/party_roster.json` and activates only starting members.
2. The default save position comes from `data/save_checkpoints.json` at `(-560, 80)`.
3. Defeating `meadow_scout` sets `enemy_defeated:meadow_scout`.
4. Entering the Maelle trigger rectangle starts `maelle_confrontation`.
5. Dialogue completion broadcasts `dialogue_completed`, and `StoryDirector` queues the Maelle duel.
6. Winning the duel broadcasts `battle_end_victory`, queues `maelle_reconcile`, and uses `maelle_duel_victory` as the non-looping result theme.
7. Completing reconciliation recruits Maelle, sets `story.maelle_joined`, hides her NPC, and saves a checkpoint.
8. Losing and choosing Leave broadcasts `battle_end_defeat`, pushes Verso back near `(820, -120)`, and keeps the duel available.

## Save Migration

Old save files that already contain Maelle are treated as post-recruitment saves. During load, `SaveManager` adds:

- `story.maelle_duel_won`
- `story.maelle_joined`

This prevents old saves from replaying the sister duel while still preserving the saved active party list.

## Authoring Rules

- Story state must be represented by stable `GameProgress` flags.
- Story events must queue commands; they must not push states directly.
- Dialogue ids and story battle ids should be stable because save/load and event chains depend on them.
- New story battles should use `storyBattleId` so victory, defeat, and flee can resolve to the correct event.
- Non-English player-facing text belongs only in localization JSON.

## Verification

Build command:

```bat
.\build_src_static.bat 2>&1
```

Expected tail:

```text
[OK] Build succeeded > bin\game.exe  [Debug]
```

Manual checks:

- New Game starts with Verso only.
- Maelle is hidden until `meadow_scout` is defeated.
- Entering the trigger zone starts the confrontation automatically.
- The Maelle duel uses `maelle.mp3`.
- Victory result uses `maelle_victory.mp3` and does not loop.
- Maelle joins only after `maelle_reconcile` completes.
- Defeat -> Retry restarts the duel from the pre-battle party state.
- Defeat -> Leave pushes Verso back and allows a later rematch.
