# Campfire Equipment Gate

## Summary

Equipment changes are now a campfire-only action. The player can still open the
inventory away from campfires to inspect items, use overworld-safe consumables,
and review equipped gear, but equip and unequip mutations are blocked until the
player is standing near a checkpoint campfire.

This closes the loophole where inventory equipment changes could bypass the
`L` key campfire restriction.

## Entry Points

- `L` from the overworld opens `LineupState` only when
  `OverworldState::FindNearbyCampfire()` succeeds.
- `I` from the overworld opens `InventoryState` everywhere, but passes an
  `allowEquipmentChanges` flag based on the same campfire proximity check.
- Campfire menu lineup access remains allowed because it is already opened from
  `CampfireState`.

## Mutation Rules

`InventoryState` owns the gate for inventory equipment actions:

- `TryEquip()` returns without mutating `PartyManager` when the flag is false.
- `TryUnequip()` returns without mutating `PartyManager` when the flag is false.
- Opening the equipment picker is blocked when the flag is false.
- If a future code path accidentally reaches the picker while locked, the picker
  confirm path still blocks the mutation.

Read-only equipment display remains available outside campfires. This lets the
player plan gear choices while exploring without changing battle stats between
checkpoint decisions.

## Player Feedback

When equipment changes are locked, inventory shows a localized flash message and
uses a locked equipment hint:

- `inventory.flash.equipment_campfire_only`
- `inventory.equipment_campfire_required`
- `inventory.hint.equipment_locked`

The failed action plays the existing `battle_no_ap` SFX, matching other rejected
menu actions.

## Test Checklist

- Away from campfire, press `L`: lineup does not open.
- Away from campfire, press `I`, switch to Equipment, press Enter: picker does
  not open and no gear changes.
- Away from campfire, consumables that are valid outside battle still work.
- Near a campfire, press `I`, switch to Equipment, press Enter: picker opens and
  equip/unequip works.
- Near a campfire, press `L`: lineup still opens.
- Campfire menu `Lineup` option still opens lineup.
- Save/load keeps equipment state unchanged by the gate itself.
