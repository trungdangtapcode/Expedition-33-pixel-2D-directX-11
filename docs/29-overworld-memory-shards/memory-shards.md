# Overworld Memory Shards

## Purpose

Memory shards add optional exploration rewards to the overworld. They are inspired by Expedition-style exploration where route detours uncover traces of previous expeditions, companion reactions, and small useful rewards.

## Runtime Flow

`OverworldState` loads shard records from `data/overworld_memory_shards.json`. Available shards are spawned as `OverworldMemoryShard` entities in `SceneGraph`.

When the player stands near a shard, the overworld prompt asks for `E`. On collection:

1. `GameProgress` sets the shard's `collectedFlag`.
2. The shard entity is marked dead and removed by `SceneGraph`.
3. Optional coins and items are granted.
4. Optional lore dialogue is pushed through `DialogueState`.

## Data Contract

Each shard entry supports:

- `id`: stable authoring id.
- `displayNameKey`: localization key for the prompt.
- `texturePath`: project asset path for the shard sprite.
- `collectedFlag`: durable flag that prevents respawn.
- `dialoguePath`: optional lore dialogue script.
- `requiresFlags`: optional story gates.
- `blockedByFlags`: optional hide gates.
- `coinReward`: optional coin reward.
- `itemId`, `itemAmount`: optional item reward.
- `worldX`, `worldY`: map position.
- `contactRadius`: interaction range.
- `scale`, `bobAmplitude`, `bobSpeed`: visual tuning.
- `layer`, `sortYOffset`: SceneGraph draw ordering.

## Asset Pipeline

The current shard asset was generated with the built-in imagegen tool on a flat chroma-key background, processed through the installed imagegen chroma-key helper, cropped, and downscaled to `assets/UI/memory_shard.png`.

The asset is committed as a real PNG because it is consumed by runtime data. The original generated image remains under the Codex generated image directory.

## Authoring Rules

- Use shards for optional lore, not required main-route instructions.
- Gate shards with story flags when they reference companion context.
- Keep rewards small so shards motivate exploration without replacing battle rewards.
- Put all player-facing text in localization JSON.
- Keep C++ limited to loading, rendering, interaction, and progress ownership.
