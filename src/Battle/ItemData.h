// ============================================================
// File: ItemData.h
// Responsibility: Plain-data description of ONE usable battle item,
//                 loaded from data/items/*.json.
//
// Design — items are data, not classes:
//   Every usable item shares the same execution pipeline (pick targets,
//   run effect, consume one from inventory, log).  Only the tuning
//   values differ.  Instead of one class per item (PotionSmall,
//   PotionLarge, Bomb, Ether, …) we declare ONE ItemData struct and
//   let BuildItemActions() dispatch on the effect kind.
//
//   Adding a new item = writing a new .json file.  Zero C++ changes.
//
// Scope of an ItemData:
//   - identity (id / name / description / iconPath)
//   - targeting rule (who can this item be pointed at?)
//   - effect kind  (what happens when it is used?)
//   - tuning parameters (amount, duration, stat, op, value)
//
// Not covered here (yet):
//   - rarity / sell price / drop tables          — inventory concerns
//   - stack limits                                — inventory concerns
//   - multi-effect items (e.g. heal + cleanse)   — composition layer
//     (a later feature could change `effect` into a vector<Effect>)
// ============================================================
#pragma once
#include <string>
#include "StatId.h"
#include "StatModifier.h"     // Reuse StatModifier::Op for buff items

// ------------------------------------------------------------
// Targeting: who the player is allowed to aim the item at.
// ------------------------------------------------------------
enum class ItemTargeting
{
    SelfOnly,       // user always targets themselves — skip target-select UI
    SingleAlly,     // one alive ally (excluding KO'd)
    SingleAllyAny,  // one ally INCLUDING KO'd — for Revive items
    SingleEnemy,    // one alive enemy
    AllAllies,      // all alive allies — skip target-select UI
    AllEnemies      // all alive enemies — skip target-select UI
};

// ------------------------------------------------------------
// ItemKind: top-level taxonomy for the item.
//
// Drives:
//   - Which inventory tab the item appears in (Items vs Equipment)
//   - Whether the item is consumed on use (Consumable) or
//     equipped/unequipped (Weapon/BodyArmor/Helmet/Accessory)
//   - Whether the item ignores Enter (KeyItem)
//
// Existing items default to Consumable so adding the kind field is
// backwards-compatible — every JSON without an explicit "kind" still
// works exactly as before.
// ------------------------------------------------------------
enum class ItemKind
{
    Consumable,    // potions, bombs, anything Used in battle/inventory
    Weapon,        // equippable to EquipSlot::Weapon
    BodyArmor,     // equippable to EquipSlot::Body
    Helmet,        // equippable to EquipSlot::Head
    Accessory,     // equippable to EquipSlot::Accessory
    KeyItem        // story / quest items — never consumed, never equipped
};

// ------------------------------------------------------------
// EquipSlot: which character slot an equipment item occupies.
//
// PartyManager owns one std::array<std::string, kEquipSlotCount>
// per character indexed by SlotIndex(EquipSlot).  Items with
// equipSlot == None are not equippable.
// ------------------------------------------------------------
enum class EquipSlot
{
    None,          // not equippable (consumables, key items)
    Weapon,        // sword, staff, bow, ...
    Body,          // tunic, robe, plate
    Head,          // cap, helmet, hood
    Accessory      // ring, amulet, charm
};

// Total equippable slots per character.  Match this to the size of
// PartyManager::mEquipped.  Used by SlotIndex() to convert the enum
// to an array index — DO NOT include EquipSlot::None in the count.
constexpr int kEquipSlotCount = 4;

// ------------------------------------------------------------
// Effect kind: what happens to the chosen target(s) on use.
//
// Each branch of BuildItemActions() owns exactly one of these.
// New kinds require: (1) enum entry, (2) JSON string mapping,
// (3) case in ItemEffectAction::Execute, (4) sometimes a new helper action.
//
// EquipmentBonus is NOT in this enum because equipment is not "used"
// in the IAction sense — it's installed to a slot via PartyManager,
// not pushed through the action queue.  See ItemKind / EquipSlot.
// ------------------------------------------------------------
enum class ItemEffectKind
{
    HealHp,         // restore `amount` HP to each target
    HealMp,         // restore `amount` MP to each target
    FullHeal,       // restore HP + MP to max on each target
    Revive,         // revive KO'd target at `amount` HP (no-op on alive)
    RestoreRage,    // add `amount` rage to each target (capped at maxRage)
    DealDamage,     // apply `amount` true damage to each target
    StatBuff,       // apply TimedStatBuffEffect (buffStat/op/value/duration)
    Cleanse         // remove every status effect from each target
};

// ------------------------------------------------------------
// ItemData: POD description.  Value-copied everywhere.
// ------------------------------------------------------------
struct ItemData
{
    // Identity
    std::string id;            // stable key used by Inventory and saves
    std::string name;          // displayed in menu
    std::string description;   // tooltip / flavor text
    std::string iconPath;      // "assets/items/xxx.png" — may not exist yet

    // Targeting + effect selection
    ItemTargeting  targeting = ItemTargeting::SelfOnly;
    ItemEffectKind effect    = ItemEffectKind::HealHp;

    // ---- Tuning parameters ----
    // `amount` is overloaded per effect: heal value, damage value, rage
    // delta, revive-HP.  Unused for FullHeal and Cleanse.
    int amount = 0;

    // For StatBuff: how many turns before the modifier self-reverts.
    int durationTurns = 0;

    // For StatBuff: which stat + how the modifier applies.
    StatId           buffStat  = StatId::ATK;
    StatModifier::Op buffOp    = StatModifier::Op::AddFlat;
    float            buffValue = 0.0f;   // e.g. 30.0 for +30% with AddPercent

    // ---- Equipment fields ----
    // Default Consumable so every existing item JSON still parses to a
    // valid ItemData with no edits.
    ItemKind  kind      = ItemKind::Consumable;
    EquipSlot equipSlot = EquipSlot::None;

    // Flat stat bonuses applied while the item is equipped.  Negative
    // values are valid (e.g. a heavy weapon with bonusSpd = -1).
    //
    // PartyManager::GetEffectiveVersoStats() walks every equipped item
    // and folds these into the BattlerStats it returns.  No StatModifier
    // pipeline involvement — equipment bonuses are flat, persistent,
    // and never conditional, so the simpler additive path is correct.
    //
    // Why flat fields instead of a vector<StatModifier>:
    //   1. JSON parsing stays flat-scalar-only (no need to extend the
    //      hand-written parser to read nested arrays of structs).
    //   2. The inventory UI shows "ATK +5 SPD -1" lines directly without
    //      iterating a generic list.
    //   3. Equipment stats never need predicate gating — they apply
    //      whenever the item is in the slot, full stop.
    int bonusAtk    = 0;
    int bonusDef    = 0;
    int bonusMatk   = 0;
    int bonusMdef   = 0;
    int bonusSpd    = 0;
    int bonusMaxHp  = 0;
    int bonusMaxMp  = 0;
};

// ------------------------------------------------------------
// SlotIndex: enum -> array index for PartyManager::mEquipped.
// EquipSlot::None returns -1 (caller must guard).
// Defined inline so PartyManager.h can use it without a .cpp hop.
// ------------------------------------------------------------
inline int SlotIndex(EquipSlot slot)
{
    switch (slot)
    {
    case EquipSlot::Weapon:    return 0;
    case EquipSlot::Body:      return 1;
    case EquipSlot::Head:      return 2;
    case EquipSlot::Accessory: return 3;
    case EquipSlot::None:      return -1;
    }
    return -1;
}

// Pretty label for UI.  Kept here so every UI surface (battle, inventory,
// debug HUD) prints the same string for a given slot.
inline const char* SlotLabel(EquipSlot slot)
{
    switch (slot)
    {
    case EquipSlot::Weapon:    return "Weapon";
    case EquipSlot::Body:      return "Body";
    case EquipSlot::Head:      return "Head";
    case EquipSlot::Accessory: return "Accessory";
    case EquipSlot::None:      return "(none)";
    }
    return "(none)";
}
