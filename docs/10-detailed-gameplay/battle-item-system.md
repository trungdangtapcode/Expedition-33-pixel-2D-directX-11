# Battle Item System

This document describes how usable consumable items are designed, loaded,
selected, executed, and rendered inside `BattleState`. The goal of the
design is **zero per-item C++ classes** — adding a new potion, bomb, or
buff is one new JSON file, nothing else.

---

## Table of Contents

1. [Design Principles](#1-design-principles)
2. [Architecture Overview](#2-architecture-overview)
3. [ItemData — JSON Schema](#3-itemdata--json-schema)
4. [Targeting Rules](#4-targeting-rules)
5. [Effect Kinds](#5-effect-kinds)
6. [ItemRegistry — Lazy Catalog](#6-itemregistry--lazy-catalog)
7. [Inventory — Persistent Storage](#7-inventory--persistent-storage)
8. [Action Pipeline — From Pick to Resolve](#8-action-pipeline--from-pick-to-resolve)
9. [Input FSM Integration](#9-input-fsm-integration)
10. [On-Screen Menu Rendering](#10-on-screen-menu-rendering)
11. [Adding a New Item](#11-adding-a-new-item)
12. [Adding a New Effect Kind](#12-adding-a-new-effect-kind)
13. [Files Changed](#13-files-changed)

---

## 1. Design Principles

The whole subsystem is built on five rules. Every later decision flows
from these.

1. **Items are data, not classes.** A new item is a `.json` file under
   `data/items/`. There is **one** `ItemData` POD and **one** dispatch
   function (`BuildItemActions::Build`). No `IItem` virtual base; no
   per-item subclass; no `if (id == "potion_small")` switches.

2. **Inventory is global and persistent.** `Inventory` is a Meyers'
   singleton (`src/Systems/Inventory.h`) that survives the entire
   process lifetime. Battles read it; battles write it; the overworld
   will eventually read and write it too. There is one bag, ever.

3. **Stat changes must go through the modifier pipeline.** When an item
   buffs a stat it pushes a `StatModifier` (see
   `docs/10-detailed-gameplay/stat-modifier-pipeline.md`, planned), not
   a direct `stats.atk += 30`. This is the same rule status effects
   already follow.

4. **Mutations happen inside `IAction::Execute`.** Items do not mutate
   state from the menu code or from the input controller. They emit a
   sequence of `IAction` subclasses that the `ActionQueue` runs in
   order, identical to skill execution. The combat timeline stays
   deterministic and replayable.

5. **Inventory is decremented exactly once per use.** Even an AoE item
   that hits five enemies removes one stack from the bag, not five.
   This is enforced by emitting one `ItemConsumeAction` at the end of
   the action sequence regardless of target count.

---

## 2. Architecture Overview

```
                                 +-------------------+
   data/items/*.json   ------->  |  ItemRegistry     |
   (POD ItemData)                |  (lazy singleton) |
                                 +---------+---------+
                                           |
                                           v Find(id)
   +-----------------------+    +----------+----------+    +-------------------+
   | BattleInputController |    |   BuildItemActions  |    |  Inventory        |
   |  ITEM_SELECT          |--->|  Build(user, item,  |--->|  (count-per-id    |
   |  ITEM_TARGET_SELECT   |    |   target, battle)   |    |   singleton)      |
   +-----------+-----------+    +----------+----------+    +-------------------+
               |                            |
               | SetPlayerItem              v
               v                  +---------+----------+
   +-----------+-----------+      |  IAction sequence  |
   |   PlayerCombatant     |      |  [LogAction]       |
   |   pending item slot   |      |  [ItemEffectAction]
   +-----------+-----------+      |  [ItemEffectAction]
               |                  |  ...               |
               v                  |  [ItemConsumeAction]
   +-----------+-----------+      +---------+----------+
   |    BattleManager      |                |
   |  HandlePlayerTurn     +--------------->+
   |   EnqueueItemActions  |                | enqueue
   +-----------------------+                v
                                 +----------+----------+
                                 |    ActionQueue      |
                                 |    (drains 1/frame) |
                                 +----------+----------+
                                            |
                                            v Execute(dt)
                                 +----------+----------+
                                 |  TimedStatBuffEffect|  (for stat_buff items)
                                 |  TakeDamage         |  (for deal_damage items)
                                 |  Inventory.Remove() |  (for ItemConsumeAction)
                                 +---------------------+
```

Key insight: the rectangles on the right are exactly the same primitives
that **skills** already use. Items reuse `IAction`, `IStatusEffect`,
`StatModifier`, and the `ActionQueue` directly. The item system is a
new **input + dispatch** layer on top of an unchanged execution layer.

---

## 3. ItemData — JSON Schema

`ItemData` (`src/Battle/ItemData.h`) is a Plain Old Data struct loaded
from `data/items/*.json`. Every field except `buffStat` / `buffOp` /
`buffValue` is shared across all items; the buff fields are only read
when `effect == "stat_buff"`.

```json
{
  "id":            "power_tonic",
  "name":          "Power Tonic",
  "description":   "Boosts an ally's ATK by 30% for 3 turns.",
  "iconPath":      "assets/items/power_tonic.png",
  "targeting":     "single_ally",
  "effect":        "stat_buff",
  "amount":        0,
  "durationTurns": 3,
  "buffStat":      "atk",
  "buffOp":        "add_percent",
  "buffValue":     30
}
```

| Field | Type | Used by | Notes |
|---|---|---|---|
| `id` | string | Inventory, registry lookup | Stable key. Save files reference this. |
| `name` | string | Battle menu label | Shown to the player. |
| `description` | string | Tooltip / debug HUD | Optional but encouraged. |
| `iconPath` | string | Icon renderer (placeholder for now) | If missing on disk, registry warns and continues. |
| `targeting` | enum string | Menu flow, `BuildItemActions` | See §4. |
| `effect` | enum string | `ItemEffectAction::Execute` dispatch | See §5. |
| `amount` | int | HealHp, HealMp, Revive HP, RestoreRage, DealDamage | Overloaded per effect. |
| `durationTurns` | int | StatBuff only | Turns the modifier persists. |
| `buffStat` | enum string | StatBuff only | `atk` / `def` / `matk` / `mdef` / `spd` / `max_hp` / `max_mp` |
| `buffOp` | enum string | StatBuff only | `add_flat` / `add_percent` / `multiply` |
| `buffValue` | float | StatBuff only | Magnitude. `add_percent` value 30 means +30%. |

The parser is the existing `JsonLoader::detail::ValueOf` /
`ParseInt` / `ParseFloat` helpers. There is no third-party JSON
dependency — the schema is intentionally flat scalars only.

---

## 4. Targeting Rules

Six targeting modes are supported. The mode determines:
- whether the player picks a target after picking the item, and
- which battlers `BuildItemActions::ResolveTargets` returns

| `targeting` | Picks a target? | Resolves to |
|---|---|---|
| `self` | no — auto-commits | `[ user ]` |
| `single_ally` | yes — alive allies only | `[ chosenAlly ]` |
| `single_ally_any` | yes — alive **and** dead allies | `[ chosenAlly ]` (used by Revive) |
| `single_enemy` | yes — alive enemies only | `[ chosenEnemy ]` |
| `all_allies` | no — auto-commits | every alive player |
| `all_enemies` | no — auto-commits | every alive enemy |

**Self / AoE shortcut.** When the player presses Enter on a self-only
or AoE item, `BattleInputController::HandleItemSelect` skips the
target step entirely and calls `BattleManager::SetPlayerItem(id, nullptr)`
directly. The dispatch resolves the target list from the targeting rule,
so `nullptr` is fine — see §8.

**Revive special case.** Phoenix Down uses `single_ally_any`, which
includes KO'd allies in the candidate list. This is the only way the
target cursor can land on a dead combatant. Every other targeting mode
filters out non-alive battlers so the cursor never sits on a corpse.

---

## 5. Effect Kinds

`ItemEffectKind` (`src/Battle/ItemData.h`) is a closed enum. Each entry
maps to one branch of `ItemEffectAction::Execute` (`src/Battle/ItemEffectAction.cpp`).
Adding a new branch is a four-step change — see §12.

| `effect` JSON value | Enum | Behavior |
|---|---|---|
| `heal_hp` | `HealHp` | `target.hp += amount`, clamped at `maxHp`. No-op on dead allies. |
| `heal_mp` | `HealMp` | `target.mp += amount`, clamped at `maxMp`. |
| `full_heal` | `FullHeal` | `target.hp = maxHp; target.mp = maxMp`. |
| `revive` | `Revive` | `target.hp = amount` if dead, no-op if already alive. |
| `restore_rage` | `RestoreRage` | `target.AddRage(amount)`. No-op on combatants with `maxRage == 0` (e.g. enemies — using a Rage Gem on an enemy silently wastes the item). |
| `deal_damage` | `DealDamage` | Builds a `DamageResult` with `effectiveDamage = max(1, amount)` and calls `target.TakeDamage(result, nullptr)`. Bypasses `IDamageCalculator` entirely — items deal **true** damage by convention so the JSON value is exactly what the player sees. |
| `stat_buff` | `StatBuff` | Attaches a `TimedStatBuffEffect(buffStat, buffOp, buffValue, durationTurns)` to the target. The effect pushes a `StatModifier` on `Apply()` and strips it on `Revert()`. |
| `cleanse` | `Cleanse` | Calls `target.ClearAllStatusEffects()`, which `Revert()`s every effect first so any pushed `StatModifier` entries get stripped before the effects are released. |

**Why item damage skips `IDamageCalculator`:** items are tuning data
authored by humans. If a JSON file says "60 damage", the player should
see 60. Routing item damage through ATK/DEF + crit roll + status bonus
would mean the JSON number bears no relation to the screen number, and
balancing items would become guesswork. Skills are subject to the
calculator because their damage scales with the caster; items are not.

---

## 6. ItemRegistry — Lazy Catalog

`ItemRegistry` (`src/Battle/ItemRegistry.h`) is a Meyers' singleton
that owns one `vector<ItemData>` keyed by id. It is **not** loaded at
startup — the first call to `EnsureLoaded()` walks `data/items/`
once, parses every `.json` file, and caches the result.

Lazy loading means:
- The title screen and main menu pay zero I/O cost.
- The first battle pays a small one-time cost (~15 small files today).
- Subsequent battles see a hot cache.

The walk supports both workspace-root and `bin/` working directories
(matching `JsonLoader::LoadSkillData`'s dual-path convention) so
launching the game from either location works.

**Missing assets:** if `iconPath` does not exist on disk, the registry
logs a warning but **still registers the item**. The menu renders a
color-tinted placeholder so the game never crashes on missing art —
see `idea/asset-todo.md` for the icon punch list.

**Missing id:** if a `.json` file has no `id` field, it is skipped
with a warning. This is the only fatal-per-file failure mode.

---

## 7. Inventory — Persistent Storage

`Inventory` (`src/Systems/Inventory.h`) is a Meyers' singleton holding
a `unordered_map<string, int>` (counts) and a `vector<string>` (insertion
order, for stable menu ordering). It is **the** authority on "how many
of X does the player have right now". It survives the entire process
lifetime — battles, overworld, menus all share one bag.

**Seed bundle.** On first construction (i.e. first call to
`Inventory::Get()`), the constructor seeds a starter bundle so a fresh
launch has testable counts. The seed values are intentional tuning and
the only literal item counts in C++ source. When a real save system is
added it will overwrite these on load.

```cpp
Add("potion_small",  5);
Add("potion_medium", 3);
Add("potion_large",  1);
Add("ether_small",   3);
// ... 11 more
```

**Public API:**
- `int  GetCount(id) const` — 0 for unknown ids; never throws
- `bool Has(id) const` — sugar for `GetCount > 0`
- `void Add(id, count)` — increments; appends to insertion order on first add
- `int  Remove(id, count)` — clamped at 0, returns actual removed
- `vector<string> OwnedIds() const` — insertion order, filtered to count > 0

**Save/load is not yet wired.** When it lands, the singleton is the
single hook point: serialize `mCounts` to a save blob; restore it on
load. The seed bundle becomes a fallback for new games only.

**Current overworld coupling: none.** The overworld does not yet call
`Add` from chests, drops, or shops. The architecture supports it
trivially — call `Inventory::Get().Add(id, count)` from any handler.

---

## 8. Action Pipeline — From Pick to Resolve

This is the full path of one item use, end-to-end. The numbered steps
correspond to the diagram in §2.

**Step 1 — Player input.** `BattleInputController::HandleItemSelect`
runs Up/Down/Enter on `Inventory::OwnedIds()`. On Enter, it inspects
the targeting rule:
- self / AoE → call `BattleManager::SetPlayerItem(id, nullptr)`,
  return to `COMMAND_SELECT`
- single → advance to `ITEM_TARGET_SELECT`

`HandleItemTargetSelect` resolves the candidate list per frame (alive
allies / all allies / alive enemies), runs the cursor over it, and
on Enter calls `ConfirmItemAndTarget` which in turn calls
`SetPlayerItem(id, picked)`.

**Step 2 — Pending slot.** `BattleManager::SetPlayerItem` forwards to
`PlayerCombatant::SetPendingItem(id, target)`. The combatant has a
**parallel pending slot** to its skill slot:
```cpp
bool        mHasPendingAction;
int         mPendingSkillIndex;   // -1 when this is an item action
std::string mPendingItemId;       // empty when this is a skill action
IBattler*   mPendingTarget;
```
The two are mutually exclusive — calling `SetPendingItem` overwrites any
prior `SetPendingAction` and vice versa. `HasPendingAction()` returns
true for either.

**Step 3 — Dispatch.** `BattleManager::HandlePlayerTurn` runs every
frame during `PLAYER_TURN`. When `HasPendingAction()` flips true, it
inspects `GetPendingItemId()`:
- non-empty → `EnqueueItemActions(*player, itemId, target)`
- empty → existing skill code path (`EnqueueSkillActions`)

**Step 4 — Build the action sequence.**
`BuildItemActions::Build(user, item, primaryTarget, battle)` produces:
```
[LogAction]              "<user> uses <name>!"
[ItemEffectAction T0]    apply effect to target 0
[ItemEffectAction T1]    apply effect to target 1   (AoE only)
...
[ItemConsumeAction]      Inventory::Remove(id, 1)   (exactly once)
```

The `LogAction` is constructed with a `nullptr` log pointer; the manager
upgrades it to point at `mBattleLog` inside `EnqueueItemActions`, the
same trick `EnqueueSkillActions` uses.

**Step 5 — Validation gates.** `EnqueueItemActions` checks three
conditions before queuing anything:
1. `ItemRegistry::Find(id)` returns non-null (typo guard)
2. `Inventory::GetCount(id) > 0` (anti-double-spend)
3. `BuildItemActions::Build` returns non-empty (targeting succeeded)

If any check fails, the manager logs to the battle log and refunds the
turn — no inventory decrement runs because the action sequence is never
queued.

**Step 6 — Execute.** `ActionQueue::Update` drains one frame at a time.
Each `ItemEffectAction::Execute` dispatches on the effect kind, mutates
the target, and returns true. The terminal `ItemConsumeAction` decrements
inventory and returns true. The queue is now empty; `HandleResolving`
checks win/lose and advances the turn.

**Crucially, every mutation flows through `IAction::Execute`.** No item
code mutates `BattlerStats` from the menu thread, the input handler, or
the registry. This is the same guarantee skills give.

---

## 9. Input FSM Integration

`PlayerInputPhase` (`src/Battle/BattleInputController.h`) gained two
new states:

```
COMMAND_SELECT  ->  SKILL_SELECT       ->  TARGET_SELECT       -> commit
COMMAND_SELECT  ->  ITEM_SELECT        ->  ITEM_TARGET_SELECT  -> commit
                       (skipped for self / AoE items)
COMMAND_SELECT  ->  FleeCommand        -> iris close + pop state
```

The top-level command list is a `vector<unique_ptr<IBattleCommand>>`.
`ItemCommand::Execute` simply calls
`state.SetInputPhase(PlayerInputPhase::ITEM_SELECT)` — it is one line
of routing logic and contains zero gameplay state. This mirrors
`FightCommand`. **There is no switch statement that maps menu entries
to behaviors** — adding a new top-level command means writing one
class and appending it to `BuildCommandList()`. The Open/Closed
principle in two lines.

`SetInputPhase(ITEM_SELECT)` triggers two side effects:
1. `RefreshItemList()` snapshots `Inventory::OwnedIds()` into
   `mItemIds`. This happens **only on phase entry**, not per frame, so
   the cursor stays stable while scrolling — items consumed in earlier
   turns don't shift the indices mid-scroll.
2. `mSkillMenuTimer` is reset so the menu's slide-in animation
   restarts (the item menu reuses the skill menu's animation timer).

---

## 10. On-Screen Menu Rendering

Rendered inside `BattleState::Render`. Three layers:

**Layer A — Item rows.** Up to `kVisibleItems = 3` rows are drawn at any
time. The window is centered on the cursor:
```
first = clamp(hovered - 1, 0, n - 3)
```
Each row contains:
1. The shared 9-slice dialog box (`mDialogBox`)
2. A small icon placeholder — also a `mDialogBox` quad, tinted by
   effect kind (green = heal, blue = mana, red = damage, yellow = buff,
   cyan = cleanse, pink = revive, orange = rage)
3. Text label `"<name> x<count>"` shifted right to clear the icon column

When a real PNG icon library lands, replace the tinted-quad placeholder
with `SpriteBatch::Draw(item->iconSRV, ...)` — see
`idea/asset-todo.md` for the file list and the exact code site.

**Layer B — Scroll chevrons.** A `^` glyph above the menu when there
are items off-screen above; a `v` glyph below when there are items
off-screen below. Drawn with `mTextRenderer.DrawString` for now;
swappable to chevron PNGs later.

**Layer C — Scrollbar.** Drawn only when `itemCount > visibleCount`.
Two thin tinted rectangles to the right of the menu:
- **Track** — full menu height, dim grey, represents the entire inventory
- **Thumb** — bright yellow; height = `(visibleCount / itemCount) * trackHeight`,
  position = `progress * (trackHeight - thumbHeight)` where
  `progress = first / (itemCount - visibleCount)`

This is the same mental model as a browser scrollbar: thumb size shows
how much of the list is visible, thumb position shows where you are
within it.

**Item-target pointer.** During `ITEM_TARGET_SELECT`, the standard
target pointer is reused but the candidate-list lookup branches on the
targeting rule. For ally-targeting items the pointer draws over the
player's slot via `GetPlayerSlotPos`; for enemy items it draws over an
enemy slot via `GetEnemySlotPos`. This is the only place in the
codebase where the pointer can land on an ally.

---

## 11. Adding a New Item

This is the entire process for adding a new healing item:

1. Create `data/items/super_potion.json`:
   ```json
   {
     "id": "super_potion",
     "name": "Super Potion",
     "description": "Restores 120 HP to one ally.",
     "iconPath": "assets/items/super_potion.png",
     "targeting": "single_ally",
     "effect": "heal_hp",
     "amount": 120
   }
   ```

2. (Optional) Add it to the starter bundle in `Inventory::Inventory()`:
   ```cpp
   Add("super_potion", 2);
   ```
   Or — when overworld pickup is wired — drop it from a chest.

3. Done. No C++ recompile is needed for the item itself, only for the
   seed bundle change in step 2. The next launch picks it up.

The icon will render as a green tinted placeholder until you also drop
`assets/items/super_potion.png` into place.

---

## 12. Adding a New Effect Kind

Effect kinds are the closed enum that the dispatch switches on. Adding
a new one (e.g. "shield: grants temporary HP") requires four edits, all
in the Battle subsystem:

1. **Enum entry** — `src/Battle/ItemData.h`:
   ```cpp
   enum class ItemEffectKind {
       // ...
       Cleanse,
       GrantShield   // <-- new
   };
   ```

2. **JSON token mapping** — `src/Battle/ItemRegistry.cpp`, inside
   `ParseEffect`:
   ```cpp
   if (s == "grant_shield") return ItemEffectKind::GrantShield;
   ```

3. **Dispatch branch** — `src/Battle/ItemEffectAction.cpp`, add a `case`
   inside `Execute`'s switch:
   ```cpp
   case ItemEffectKind::GrantShield:
   {
       // Grant temporary HP via a new ShieldEffect or a StatModifier
       // pushed onto MAX_HP.
       break;
   }
   ```

4. **(Optional) Menu icon tint** — `src/States/BattleState.cpp`, inside
   the `IconTint` lambda. Add a new color so the placeholder UI shows
   the new category. Skip this when real PNG icons are wired up.

That's it. No new files, no new classes — the existing dispatch table
absorbs the new effect.

---

## 13. Files Changed

### New files

| File | Purpose |
|---|---|
| `src/Battle/ItemData.h` | POD struct + `ItemTargeting` / `ItemEffectKind` enums |
| `src/Battle/ItemRegistry.h` | Lazy singleton catalog interface |
| `src/Battle/ItemRegistry.cpp` | JSON walker + parser |
| `src/Battle/BuildItemActions.h` | Free function to build the action sequence |
| `src/Battle/BuildItemActions.cpp` | Targeting resolution + sequence assembly |
| `src/Battle/ItemEffectAction.h` | Single-target effect dispatcher |
| `src/Battle/ItemEffectAction.cpp` | Switch over `ItemEffectKind` |
| `src/Battle/ItemConsumeAction.h` | Inventory decrement action |
| `src/Battle/ItemConsumeAction.cpp` | Calls `Inventory::Remove` |
| `src/Battle/ItemCommand.h` | "Item" top-level menu entry |
| `src/Battle/ItemCommand.cpp` | One-line phase transition |
| `src/Battle/TimedStatBuffEffect.h` | Generic N-turn stat buff effect |
| `src/Battle/TimedStatBuffEffect.cpp` | Pushes/strips a `StatModifier` |
| `src/Systems/Inventory.h` | Persistent inventory singleton interface |
| `src/Systems/Inventory.cpp` | Storage + starter bundle seed |
| `data/items/*.json` | 15 starter items |

### Modified files

| File | Change |
|---|---|
| `src/Battle/IBattler.h` | Added `ClearAllStatusEffects()` for Cleanse items |
| `src/Battle/Combatant.h/.cpp` | Implemented `ClearAllStatusEffects` and `HasAnyStatusEffect` |
| `src/Battle/BattleManager.h/.cpp` | New `SetPlayerItem` + `EnqueueItemActions` paths |
| `src/Battle/PlayerCombatant.h/.cpp` | Parallel pending-item slot + `SetPendingItem` |
| `src/Battle/BattleInputController.h/.cpp` | New phases `ITEM_SELECT` / `ITEM_TARGET_SELECT` + handlers |
| `src/States/BattleState.cpp` | Item menu rendering: rows, icons, chevrons, scrollbar |
| `src/UI/BattleDebugHUD.h/.cpp` | New `ItemRow` snapshot section + `PushItems` printer |
| `build_src_static.bat` | Added every new `.cpp` to the source list |

---

## See also

- `docs/10-detailed-gameplay/damage-calculation-interface.md` — the
  damage pipeline that item `deal_damage` deliberately bypasses
- `idea/context-driven-battle-system.md` — the broader stat-modifier /
  context-driven design `TimedStatBuffEffect` plugs into
- `idea/asset-todo.md` — the punch list of art assets the item menu
  references but does not yet have
