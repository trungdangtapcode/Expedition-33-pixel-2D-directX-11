# Inventory & Equipment UI

This document describes the overworld inventory screen implemented in
`src/States/InventoryState.*`, how it connects to `Inventory` and
`PartyManager`, and how scrolling works when the player owns more items than
fit on screen.

---

## Overview

The inventory screen is a stacked `IGameState` that opens on top of the
overworld. It does three jobs:

1. Lets the player consume usable overworld items.
2. Lets the player equip and unequip weapons, armor, helmets, and accessories.
3. Shows the **effective** Verso stats that battle will actually use.

The state is pushed from `OverworldState` on `I`, so the overworld remains
alive underneath the stack and resumes unchanged on pop.

---

## Files

| File | Responsibility |
|---|---|
| `src/States/InventoryState.h` | State definition, FSM, render-helper declarations |
| `src/States/InventoryState.cpp` | Lifecycle, input handling, item use, equip/unequip actions |
| `src/States/InventoryStateRender.cpp` | Main render dispatch, tab bar, items grid, equipment list/picker, footers |
| `src/States/InventoryStateDetailPanel.cpp` | Right-side detail panel and stat preview |
| `src/Systems/Inventory.h/.cpp` | Persistent item-count storage |
| `src/Systems/PartyManager.h/.cpp` | Persistent player base stats + equipment slots + effective stat fold |
| `src/Battle/ItemData.h` | `ItemKind`, `EquipSlot`, and equipment bonus fields |

---

## Screen Layout

The screen is a centered panel over a dimmed overworld backdrop:

```text
+--------------------------------------------------------------+
| [Items] [Equipment]                              [Tab] Switch |
+-------------------------------+------------------------------+
| left interaction panel        | right detail/stat panel      |
|                               |                              |
| Items tab: 4x3 grid           | hovered item details         |
| Equipment tab: slot list      | equipped item bonuses        |
| Picker: scrolling item list   | current -> preview stat diff |
+--------------------------------------------------------------+
| VERSO HP/MP/ATK/DEF/MATK/MDEF/SPD (effective stats)          |
+--------------------------------------------------------------+
| control hints OR flash message                               |
+--------------------------------------------------------------+
```

The footer always shows **effective** values, not base values. That means the
numbers already include every currently equipped item's `bonus*` fields.

---

## Input State Machine

The UI uses a small three-phase FSM:

```text
ItemsGrid <-> EquipmentSlots -> EquipmentPicker
```

### `ItemsGrid`

- Active when the Items tab is selected.
- Arrow keys move over a 4-column consumable grid.
- Enter tries to use the hovered consumable.

### `EquipmentSlots`

- Active when the Equipment tab is selected.
- Up/Down moves over the four slots:
  - Weapon
  - Body
  - Head
  - Accessory
- Enter opens the picker for the selected slot.

### `EquipmentPicker`

- Overlay sub-state of Equipment.
- Up/Down moves through every owned item compatible with the slot plus one final
  pseudo-entry: `(unequip slot)`.
- Enter equips the selected item or unequips the slot.
- Esc / Backspace returns to `EquipmentSlots`.

### Global controls

- `Tab` toggles between Items and Equipment.
- `Esc`, `I`, or `Backspace` closes the screen.
- Exception: in `EquipmentPicker`, the first close press exits back to
  `EquipmentSlots` instead of popping the whole state.

---

## Data Flow

```text
InventoryState
  -> Inventory::OwnedIds() / GetCount()
  -> PartyManager::GetVersoStats()
  -> PartyManager::GetEffectiveVersoStats()
  -> PartyManager::PreviewEffectiveStats(slot, itemId)
  -> PartyManager::Equip(slot, itemId)
  -> PartyManager::Unequip(slot)
```

### Consumable use

`TryUseItem()` handles overworld-safe item effects directly:

- `HealHp`
- `HealMp`
- `FullHeal`

These effects mutate persisted player resources through
`PartyManager::SetVersoStats()`, then decrement the inventory count through
`Inventory::Remove()`.

Battle-only items such as revive, damage, cleanse, and rage restoration are
blocked in the overworld UI and produce a flash message instead.

### Equipment use

The equipment screen never computes stats on its own. It delegates to
`PartyManager`.

`PartyManager::GetEffectiveVersoStats()`:

1. Starts from base stats.
2. Walks every equipped item id.
3. Looks up each item in `ItemRegistry`.
4. Folds every `bonus*` field into a temporary `BattlerStats`.
5. Clamps hp/mp into the new max values.

`PartyManager::PreviewEffectiveStats(slot, itemId)` runs the exact same fold with
one slot replaced hypothetically. That guarantees the right-side preview is
identical to what a real equip would produce.

---

## Stat Preview

When the equipment picker is open, the right panel shows:

```text
STAT PREVIEW
MaxHP   100 -> 105 (+5)
ATK      25 -> 37 (+12)
SPD      10 ->  9 (-1)
```

Rendering rules:

- No change: white, compact single-value row.
- Positive delta: green.
- Negative delta: red.

The `(unequip slot)` pseudo-row previews with an empty item id, so the panel
shows the stat loss before the player confirms removal.

---

## Scrolling

Two parts of the UI now use a scroll-window mechanism.

### Items tab scrolling

The consumables grid is intentionally capped to:

- 4 columns
- 3 visible rows
- 12 visible items maximum at once

If the player owns more than 12 consumables:

1. The grid computes `totalRows = ceil(itemCount / 4)`.
2. It derives the hovered row from `mItemCursor / 4`.
3. It chooses a `firstRow` so the hovered row stays centered whenever possible.
4. Only rows inside `[firstRow, firstRow + 3)` are rendered.

Visual affordances:

- Up chevron when hidden rows exist above.
- Down chevron when hidden rows exist below.
- Vertical scrollbar track and thumb on the right edge.

This keeps the on-screen layout stable even when the inventory grows.

### Equipment picker scrolling

The picker uses the same idea, but with a one-column list:

1. `totalRows = itemCount + 1` because `(unequip slot)` is a real navigable row.
2. Visible rows are derived from available panel height.
3. The picker centers the hovered row when possible.
4. Only visible rows are drawn.

The picker also shows:

- Up/down chevrons when entries exist outside the visible window.
- A scrollbar whose thumb size reflects `visibleRows / totalRows`.

This prevents long equipment catalogs from flowing off the panel.

---

## Why Battle Sees Equipped Stats Automatically

Battle integration stays small because the equipment system does **not** patch
damage code directly.

`BattleManager::Initialize()` seeds Verso from:

```cpp
PartyManager::Get().GetEffectiveVersoStats()
```

That means every downstream battle system already sees the post-equipment stats:

- damage formulas
- HP bar max values
- agility / AV timeline ordering
- any future stat-based skill conditions

No battle renderer or battle UI class needs special equipment awareness.

---

## Adding More Equipment

To add a new piece of equipment:

1. Create a JSON file in `data/items/`.
2. Set `kind` to `Weapon`, `BodyArmor`, `Helmet`, or `Accessory`.
3. Fill in the relevant `bonus*` fields.
4. Seed it into `Inventory` or add it through future drops/save data.

No `InventoryState` code changes are needed unless the visual layout itself
changes.

---

## Current Limitations

- Placeholder icon swatches are used instead of authored PNG icons.
- Items tab scrolling is grid-only; there is no mouse wheel or drag support.
- Equipment picker scrolling is keyboard-driven only.
- The inventory screen is currently opened from the overworld only.

These are UX limitations, not architecture limits.
