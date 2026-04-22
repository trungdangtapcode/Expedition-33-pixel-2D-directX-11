// ============================================================
// File: ItemRegistry.cpp
// Responsibility: Walk data/items/*.json on first access, parse each
//                 file into an ItemData, and warn on missing icon art.
// ============================================================
#include "ItemRegistry.h"
#include "../Utils/Log.h"
#include "../Utils/JsonLoader.h"      // reuse detail::ValueOf / ParseInt / ParseFloat

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace
{
    // ------------------------------------------------------------
    // StripQuotes: ValueOf returns "\"foo\"" for string fields.
    // Remove the surrounding quotes so the raw token becomes "foo".
    // Defined as a file-local helper because every string parse needs it.
    // ------------------------------------------------------------
    std::string StripQuotes(const std::string& s)
    {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
            return s.substr(1, s.size() - 2);
        return s;
    }

    // ------------------------------------------------------------
    // ParseTargeting / ParseEffect / ParseStatId / ParseOp
    //   Map JSON string tokens to their enum equivalents.
    //   Unknown tokens produce a warning and fall back to a safe default
    //   — the item is still registered so the designer notices the typo.
    // ------------------------------------------------------------
    ItemTargeting ParseTargeting(const std::string& raw)
    {
        const std::string s = StripQuotes(raw);
        if (s == "self")              return ItemTargeting::SelfOnly;
        if (s == "single_ally")       return ItemTargeting::SingleAlly;
        if (s == "single_ally_any")   return ItemTargeting::SingleAllyAny;
        if (s == "single_enemy")      return ItemTargeting::SingleEnemy;
        if (s == "all_allies")        return ItemTargeting::AllAllies;
        if (s == "all_enemies")       return ItemTargeting::AllEnemies;
        LOG("[ItemRegistry] Unknown targeting '%s' — defaulting to self.", s.c_str());
        return ItemTargeting::SelfOnly;
    }

    ItemEffectKind ParseEffect(const std::string& raw)
    {
        const std::string s = StripQuotes(raw);
        if (s == "heal_hp")       return ItemEffectKind::HealHp;
        if (s == "heal_mp")       return ItemEffectKind::HealMp;
        if (s == "full_heal")     return ItemEffectKind::FullHeal;
        if (s == "revive")        return ItemEffectKind::Revive;
        if (s == "restore_rage")  return ItemEffectKind::RestoreRage;
        if (s == "deal_damage")   return ItemEffectKind::DealDamage;
        if (s == "stat_buff")     return ItemEffectKind::StatBuff;
        if (s == "cleanse")       return ItemEffectKind::Cleanse;
        LOG("[ItemRegistry] Unknown effect '%s' — defaulting to heal_hp.", s.c_str());
        return ItemEffectKind::HealHp;
    }

    StatId ParseStatId(const std::string& raw)
    {
        const std::string s = StripQuotes(raw);
        if (s == "atk")    return StatId::ATK;
        if (s == "def")    return StatId::DEF;
        if (s == "matk")   return StatId::MATK;
        if (s == "mdef")   return StatId::MDEF;
        if (s == "spd")    return StatId::SPD;
        if (s == "max_hp") return StatId::MAX_HP;
        if (s == "max_mp") return StatId::MAX_MP;
        return StatId::ATK;
    }

    StatModifier::Op ParseOp(const std::string& raw)
    {
        const std::string s = StripQuotes(raw);
        if (s == "add_flat")    return StatModifier::Op::AddFlat;
        if (s == "add_percent") return StatModifier::Op::AddPercent;
        if (s == "multiply")    return StatModifier::Op::Multiply;
        return StatModifier::Op::AddFlat;
    }

    // ------------------------------------------------------------
    // ParseKind / ParseEquipSlot
    //   Map JSON tokens to enum values.  Unknown tokens fall back to
    //   safe defaults — Consumable / None — so a typo never crashes
    //   the registry, only loses the equipment behavior.
    // ------------------------------------------------------------
    ItemKind ParseKind(const std::string& raw)
    {
        const std::string s = StripQuotes(raw);
        if (s == "consumable") return ItemKind::Consumable;
        if (s == "weapon")     return ItemKind::Weapon;
        if (s == "body_armor") return ItemKind::BodyArmor;
        if (s == "helmet")     return ItemKind::Helmet;
        if (s == "accessory")  return ItemKind::Accessory;
        if (s == "key_item")   return ItemKind::KeyItem;
        return ItemKind::Consumable;
    }

    EquipSlot ParseEquipSlot(const std::string& raw)
    {
        const std::string s = StripQuotes(raw);
        if (s == "weapon")    return EquipSlot::Weapon;
        if (s == "body")      return EquipSlot::Body;
        if (s == "head")      return EquipSlot::Head;
        if (s == "accessory") return EquipSlot::Accessory;
        return EquipSlot::None;
    }
}

// ------------------------------------------------------------
// EnsureLoaded
//   Walk data/items/ (also look one level up in case cwd == bin/).
//   Missing directory is a warning, not a fatal — the game continues
//   with an empty catalog so a designer can add items incrementally.
// ------------------------------------------------------------
void ItemRegistry::EnsureLoaded()
{
    if (mLoaded) return;
    mLoaded = true;   // set up-front so a parse failure doesn't re-enter

    // Support both workspace-root cwd and bin/ cwd at runtime (matches
    // the dual-path convention used by JsonLoader::LoadSkillData).
    fs::path dir = "data/items";
    if (!fs::exists(dir))
    {
        fs::path alt = fs::path("..") / dir;
        if (fs::exists(alt)) dir = alt;
    }

    if (!fs::exists(dir) || !fs::is_directory(dir))
    {
        LOG("[ItemRegistry] WARNING: items directory missing ('data/items/'). "
            "No items will be available in battle.");
        return;
    }

    int okCount = 0;
    int failCount = 0;
    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        if (LoadFile(entry.path().string())) ++okCount;
        else                                 ++failCount;
    }

    LOG("[ItemRegistry] Loaded %d items (%d failed) from '%s'.",
        okCount, failCount, dir.string().c_str());
}

// ------------------------------------------------------------
// LoadFile: parse one JSON file into an ItemData and append.
// Uses JsonLoader::detail helpers rather than a real parser — our
// item schema is flat scalars only, so the existing ad-hoc extractor
// is sufficient.
// ------------------------------------------------------------
bool ItemRegistry::LoadFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        LOG("[ItemRegistry] Cannot open '%s'", path.c_str());
        return false;
    }

    std::stringstream buf;
    buf << file.rdbuf();
    const std::string src = buf.str();

    // Warn on UTF-16 — our parser only handles UTF-8.
    JsonLoader::detail::WarnIfUTF16(src, path);

    ItemData data;
    data.id          = StripQuotes(JsonLoader::detail::ValueOf(src, "id"));
    data.name        = StripQuotes(JsonLoader::detail::ValueOf(src, "name"));
    data.description = StripQuotes(JsonLoader::detail::ValueOf(src, "description"));
    data.iconPath    = StripQuotes(JsonLoader::detail::ValueOf(src, "iconPath"));

    if (data.id.empty())
    {
        LOG("[ItemRegistry] '%s' has no 'id' field — skipping.", path.c_str());
        return false;
    }

    data.targeting = ParseTargeting(JsonLoader::detail::ValueOf(src, "targeting"));
    data.effect    = ParseEffect   (JsonLoader::detail::ValueOf(src, "effect"));

    data.amount        = JsonLoader::detail::ParseInt  (JsonLoader::detail::ValueOf(src, "amount"));
    data.durationTurns = JsonLoader::detail::ParseInt  (JsonLoader::detail::ValueOf(src, "durationTurns"));
    data.buffStat      = ParseStatId(JsonLoader::detail::ValueOf(src, "buffStat"));
    data.buffOp        = ParseOp    (JsonLoader::detail::ValueOf(src, "buffOp"));
    data.buffValue     = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "buffValue"));

    // ---- Equipment fields ----
    // kind / equipSlot are optional in the JSON.  Items without them
    // default to Consumable / None — preserves backwards compatibility
    // with every existing item file.
    data.kind      = ParseKind     (JsonLoader::detail::ValueOf(src, "kind"));
    data.equipSlot = ParseEquipSlot(JsonLoader::detail::ValueOf(src, "equipSlot"));

    // Auto-derive equipSlot from kind when the JSON only specifies kind.
    // Saves the JSON author from typing both fields for the common case
    // where one weapon item goes in exactly one weapon slot.
    if (data.equipSlot == EquipSlot::None)
    {
        switch (data.kind)
        {
        case ItemKind::Weapon:    data.equipSlot = EquipSlot::Weapon;    break;
        case ItemKind::BodyArmor: data.equipSlot = EquipSlot::Body;      break;
        case ItemKind::Helmet:    data.equipSlot = EquipSlot::Head;      break;
        case ItemKind::Accessory: data.equipSlot = EquipSlot::Accessory; break;
        default: break;
        }
    }

    data.bonusAtk    = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "bonusAtk"));
    data.bonusDef    = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "bonusDef"));
    data.bonusMatk   = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "bonusMatk"));
    data.bonusMdef   = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "bonusMdef"));
    data.bonusSpd    = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "bonusSpd"));
    data.bonusMaxHp  = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "bonusMaxHp"));
    data.bonusMaxMp  = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "bonusMaxMp"));

    // Warn on missing icon art, but still register the item so the menu
    // can show a placeholder.  The game must never crash on missing assets.
    if (!data.iconPath.empty() && !fs::exists(data.iconPath))
    {
        LOG("[ItemRegistry] WARNING: item '%s' references missing icon '%s'.",
            data.id.c_str(), data.iconPath.c_str());
    }

    mItems.push_back(std::move(data));
    return true;
}

const ItemData* ItemRegistry::Find(const std::string& id) const
{
    for (const ItemData& d : mItems)
    {
        if (d.id == id) return &d;
    }
    return nullptr;
}
