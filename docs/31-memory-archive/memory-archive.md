# Memory Archive

## Purpose

The memory archive turns memory shards into persistent story progression instead
of one-time popups. Recovered memories can be reviewed at campfires, which makes
campfires feel more like preparation hubs and gives optional exploration lasting
narrative value.

## Runtime Flow

1. `CampfireState` shows `Memory Archive` in the main campfire menu.
2. Selecting it pushes `MemoryArchiveState` over the campfire screen.
3. `MemoryArchiveState` loads rows from `data/overworld_memory_shards.json`.
4. Each row checks its `collectedFlag` against `GameProgress`.
5. Recovered rows can replay their `dialoguePath` through `DialogueState`.
6. Locked rows remain visible as goals, but cannot be replayed.

Replay is read-only. The archive does not grant coins, items, or progress flags.
Rewards remain owned by `OverworldState::HandleMemoryShardInput()` when the
player first collects a shard in the overworld.

## Data Files

`data/overworld_memory_shards.json` is the archive source of truth:

- `id`: stable archive id.
- `displayNameKey`: localized display name.
- `displayName`: English fallback.
- `collectedFlag`: progress flag that unlocks replay.
- `dialoguePath`: script replayed by the archive.

`data/memory_archive_layout.json` controls panel dimensions, list spacing, icon
placement, detail panel placement, flash timing, and hint placement.

`assets/UI/memory_archive_icon.png` is the generated archive emblem used in the
campfire archive screen. The source was generated with the built-in image tool on
a flat chroma-key background, then processed locally into an alpha PNG.

## Localization

Player-facing archive text lives in:

- `data/localization/en_us.json`
- `data/localization/vi_vn.json`
- `data/localization/fr_fr.json`

The implementation also corrected previously mixed English equipment-lock
messages in Vietnamese and French localization data.

## Design Rules

- Keep archive access at campfires for V1.
- Do not grant memory shard rewards from archive replay.
- Do not hide locked memories; use them as exploration goals.
- Add new memory rows by editing `data/overworld_memory_shards.json`.
- Keep layout tuning in `data/memory_archive_layout.json`.
- Keep localized copy in localization JSON, not C++.

## Verification

- Build with `.\build_src_static.bat 2>&1`.
- Open a campfire and confirm `Memory Archive` appears.
- Confirm locked memories display as locked.
- Collect a memory shard, return to a campfire, and confirm it is replayable.
- Replay a memory and confirm no extra coins or items are granted.
- Confirm English, Vietnamese, and French archive strings load without fallback
  keys appearing on screen.
