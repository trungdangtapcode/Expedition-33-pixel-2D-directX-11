# Expedition Journal

## Purpose

The Expedition Journal gives the player a stable route overview without adding
a separate quest database. It supports the current full-game polish direction:
the player can see the chapter route, what is complete, what is current, and
which memory records have been recovered.

The design follows the Expedition 33 lesson already captured in the roadmap:
the route should feel character-driven and authored, but the implementation must
remain data-driven. Public references used for the current design direction:

- Official Expedition 33 overview: https://www.expedition33.com/overview
- Official gameplay preview: https://www.expedition33.com/post/your-first-look-at-gameplay-from-clair-obscur-expedition-33
- Xbox Wire beginner guide: https://news.xbox.com/en-us/2025/04/23/tips-to-get-started-clair-obscur-expedition-33/

## Runtime Flow

1. `CampfireState` exposes `Expedition Journal` as a read-only campfire hub row.
2. `PauseState` exposes the same journal for route context in the overworld.
3. `ExpeditionJournalState` loads:
   - `data/objectives.json`
   - `data/overworld_memory_shards.json`
   - `data/expedition_journal_layout.json`
4. The state checks `GameProgress` flags to derive row status.
5. The journal never sets flags, grants rewards, starts battles, saves, loads,
   or opens equipment.

## Status Rules

Objective status is derived from objective data:

- `Complete`: any `blockedByFlags` entry is already set.
- `Current`: requirements are met, completion flags are not set, and no earlier
  current row has been assigned.
- `Locked`: requirements are not met, or the row is later than the active row.

Memory status is derived from each shard's `collectedFlag`:

- `Complete`: the collected flag is present.
- `Locked`: the collected flag is absent.

This keeps the journal aligned with save/load, `ObjectiveDirector`, memory
shard spawning, and story events.

## Data Contract

`data/expedition_journal_layout.json` controls panel size, row spacing, tab
position, icon path, alpha values, and theme colors. Do not move these tuning
values into C++.

`assets/UI/expedition_journal_icon.png` is a generated transparent PNG. Source
generation used the built-in `image_gen` path with a flat chroma-key background,
then local chroma removal. The runtime asset is intentionally small and fixed at
96x96 pixels.

## Access Rules

The journal is safe to open from pause because it is read-only. It does not
expose inventory, lineup, save, load, shop, or equipment actions. Campfire-only
equipment gating remains unchanged.

## Extension Rules

- Add route beats by editing `data/objectives.json`.
- Add lore entries by editing `data/overworld_memory_shards.json`.
- Add new player-facing labels in localization JSON.
- Keep debug logs and docs English-only.
- If the journal needs activation behavior later, add explicit read-only actions
  such as "replay memory dialogue"; do not call collection or reward code.
