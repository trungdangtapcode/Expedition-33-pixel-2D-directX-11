# Story Reward Commands

## Purpose

Mandatory route victories now support one-time authored rewards. Normal battle
rewards still come from enemy data, but StoryDirector can also grant coins or
items after a specific story or overworld encounter id.

This gives the chapter route a clearer progression reward layer without putting
reward logic inside battle, enemy entities, or UI code.

## Command Types

Story events now support two extra command types:

```json
{
  "type": "grant_coins",
  "amount": 35
}
```

```json
{
  "type": "grant_item",
  "itemId": "ether_small",
  "amount": 1
}
```

Rules:

- `amount <= 0` is ignored by the receiver.
- `grant_item` writes through `Inventory`.
- `grant_coins` writes through `Wallet`.
- Save/load captures both systems through the existing SaveManager path.

## Triggering From Overworld Encounters

`OverworldState` now notifies `StoryDirector` after a normal overworld enemy
victory using the spawn id, for example `silent_market_ambush`.

The existing story battle path still notifies with `storyBattleId`, so Maelle's
duel and route encounters share the same event trigger system.

## Player Feedback

Reward commands set a short bottom-screen prompt using localized strings:

- `overworld.reward.coins`
- `overworld.reward.item`

The prompt duration is loaded from:

```text
data/overworld_feedback.json
```

This keeps feedback presentation separate from reward rules.

## Current Rewards

- `meadow_scout`: one medium potion and bonus coins.
- `silent_market_ambush`: one small ether and bonus coins.
- `pilgrim_crossing_patrol`: one phoenix down and bonus coins.
- `glass_shrine_sentinel`: one medium ether and bonus coins.
- `mirror_gate_clone`: one elixir and bonus coins.

## Authoring Rules

- Use `onceFlag` on every reward event so rewards cannot duplicate after reload.
- Keep normal EXP and base coin rewards in enemy data.
- Use story reward commands for chapter pacing, key consumables, and route
  milestones.
- Do not mutate inventory or wallet directly from battle result code for
  route-specific rewards.
