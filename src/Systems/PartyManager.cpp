// ============================================================
// File: PartyManager.cpp
// Responsibility: Implement persistent party stats, equipment slots,
//                 level progression, and save/load snapshots.
// ============================================================
#define NOMINMAX
#include "PartyManager.h"
#include "Inventory.h"
#include "../Battle/ItemRegistry.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    // ------------------------------------------------------------
    // Function: ClampCurrentResources
    // Purpose:
    //   Keep volatile resources inside valid bounds after save/load or battle.
    // Why:
    //   Corrupt saves, equipment changes, or combat actions must never leave
    //   current HP above max HP or below zero.
    // ------------------------------------------------------------
    void ClampCurrentResources(BattlerStats& stats)
    {
        if (stats.maxHp < 1) stats.maxHp = 1;
        if (stats.maxMp < 0) stats.maxMp = 0;
        if (stats.maxRage < 0) stats.maxRage = 0;

        if (stats.hp < 0) stats.hp = 0;
        if (stats.hp > stats.maxHp) stats.hp = stats.maxHp;

        if (stats.mp < 0) stats.mp = 0;
        if (stats.mp > stats.maxMp) stats.mp = stats.maxMp;

        if (stats.rage < 0) stats.rage = 0;
        if (stats.maxRage == 0) stats.rage = 0;
        if (stats.rage > stats.maxRage) stats.rage = stats.maxRage;
    }

    // ------------------------------------------------------------
    // Function: FoldEquipmentBonuses
    // Purpose:
    //   Add every equipped item's flat stat bonus into the output stats.
    // Why:
    //   Equipment is durable and unconditional, so it belongs in a compact
    //   fold rather than the temporary combat stat-modifier pipeline.
    // ------------------------------------------------------------
    void FoldEquipmentBonuses(BattlerStats& out,
                              const std::array<std::string, kEquipSlotCount>& equipped)
    {
        const ItemRegistry& reg = ItemRegistry::Get();
        for (const std::string& id : equipped)
        {
            if (id.empty()) continue;

            const ItemData* item = reg.Find(id);
            if (!item) continue;

            out.atk   += item->bonusAtk;
            out.def   += item->bonusDef;
            out.matk  += item->bonusMatk;
            out.mdef  += item->bonusMdef;
            out.spd   += item->bonusSpd;
            out.maxHp += item->bonusMaxHp;
            out.maxMp += item->bonusMaxMp;
        }

        if (out.atk < 0) out.atk = 0;
        if (out.def < 0) out.def = 0;
        if (out.matk < 0) out.matk = 0;
        if (out.mdef < 0) out.mdef = 0;
        if (out.spd < 0) out.spd = 0;
        if (out.maxHp < 1) out.maxHp = 1;
        if (out.maxMp < 0) out.maxMp = 0;
    }

    std::filesystem::path ResolveReadablePath(const std::string& path)
    {
        namespace fs = std::filesystem;

        fs::path direct(path);
        if (fs::exists(direct)) return direct;

        fs::path parent = fs::path("..") / path;
        if (fs::exists(parent)) return parent;

        return direct;
    }

    bool ReadTextFile(const std::filesystem::path& path, std::string& out)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        std::ostringstream buffer;
        buffer << file.rdbuf();
        out = buffer.str();
        return true;
    }

    std::wstring ToWidePath(const std::string& path)
    {
        return std::wstring(path.begin(), path.end());
    }

    PartyMember BuildMember(const PartyRosterEntry& entry)
    {
        PartyMember member{};
        member.id = entry.id;
        member.name = entry.name;
        member.animationPath = entry.animationPath;
        member.animJsonPath = entry.animJsonPath;
        member.hpFramePath = entry.hpFramePath;
        member.turnViewPath = entry.turnViewPath;

        if (!JsonLoader::LoadCharacterData(entry.dataPath, member.baseStats))
        {
            LOG("[PartyManager] WARNING: Failed to load character data '%s'.", entry.dataPath.c_str());
        }
        member.skillPaths = JsonLoader::LoadStringArrayFromFile(entry.dataPath, "skillPaths");
        ClampCurrentResources(member.baseStats);
        return member;
    }
}

PartyManager& PartyManager::Get()
{
    static PartyManager instance;
    return instance;
}

PartyManager::PartyManager()
{
    ResetToDefaults();
}

void PartyManager::ResetToDefaults()
{
    mActiveParty.clear();

    if (EnsureRosterLoaded())
    {
        for (const PartyRosterEntry& entry : mRoster)
        {
            if (entry.startingMember)
            {
                mActiveParty.push_back(BuildMemberFromRoster(entry));
            }
        }
    }

    if (mActiveParty.empty())
    {
        mActiveParty.push_back(BuildFallbackVerso());
    }
}

// ------------------------------------------------------------
// Function: RecruitMember
// Purpose:
//   Add a roster member to the active party if they are not active yet.
// Why:
//   Story rewards should unlock party members through stable ids instead of
//   manually constructing UI, sprite, and character-data metadata in states.
// ------------------------------------------------------------
bool PartyManager::RecruitMember(const std::string& id)
{
    if (IsMemberActive(id)) return true;
    if (!EnsureRosterLoaded()) return false;

    const PartyRosterEntry* entry = FindRosterEntry(id);
    if (!entry)
    {
        LOG("[PartyManager] WARNING: Cannot recruit unknown party member '%s'.", id.c_str());
        return false;
    }

    mActiveParty.push_back(BuildMemberFromRoster(*entry));
    LOG("[PartyManager] Recruited party member '%s'.", id.c_str());
    return true;
}

// ------------------------------------------------------------
// Function: IsMemberActive
// Purpose:
//   Check whether a roster id is already present in the active party.
// Why:
//   Recruitment commands can replay after save/load or event ordering edges,
//   and duplicate members would break battle slot assumptions.
// ------------------------------------------------------------
bool PartyManager::IsMemberActive(const std::string& id) const
{
    return std::any_of(mActiveParty.begin(), mActiveParty.end(),
        [&id](const PartyMember& member)
        {
            return member.id == id;
        });
}

BattlerStats PartyManager::GetEffectiveStats(size_t index) const
{
    BattlerStats stats = mActiveParty[index].baseStats;
    FoldEquipmentBonuses(stats, mActiveParty[index].equipped);
    ClampCurrentResources(stats);
    return stats;
}

BattlerStats PartyManager::PreviewEffectiveStats(size_t index, EquipSlot slot, const std::string& itemId) const
{
    const int slotIdx = SlotIndex(slot);
    if (slotIdx < 0) return GetEffectiveStats(index);

    BattlerStats stats = mActiveParty[index].baseStats;
    std::array<std::string, kEquipSlotCount> previewSlots = mActiveParty[index].equipped;
    previewSlots[slotIdx] = itemId;

    FoldEquipmentBonuses(stats, previewSlots);
    ClampCurrentResources(stats);
    return stats;
}

void PartyManager::SetMemberStats(size_t index, const BattlerStats& stats)
{
    if (index >= mActiveParty.size()) return;

    BattlerStats& stored = mActiveParty[index].baseStats;
    stored.hp = std::min(std::max(stats.hp, 0), stored.maxHp);
    stored.mp = std::min(std::max(stats.mp, 0), stored.maxMp);

    // Rage currently resets after battle by design so stocked rage cannot
    // trivialize the next encounter.
    stored.rage = 0;
}

void PartyManager::RestoreFullHP()
{
    for (auto& member : mActiveParty)
    {
        member.baseStats.hp = member.baseStats.maxHp;
        member.baseStats.mp = member.baseStats.maxMp;
        member.baseStats.rage = 0;
    }
}

void PartyManager::AddExp(int amount)
{
    if (amount <= 0) return;

    for (PartyMember& member : mActiveParty)
    {
        member.baseStats.exp += amount;

        while (member.baseStats.exp >= GetExpCurve(member.baseStats.level))
        {
            member.baseStats.exp -= GetExpCurve(member.baseStats.level);
            ++member.baseStats.level;

            member.baseStats.maxHp += member.baseStats.growth.maxHp;
            member.baseStats.maxMp += member.baseStats.growth.maxMp;
            member.baseStats.atk += member.baseStats.growth.atk;
            member.baseStats.def += member.baseStats.growth.def;
            member.baseStats.matk += member.baseStats.growth.matk;
            member.baseStats.mdef += member.baseStats.growth.mdef;
            member.baseStats.spd += member.baseStats.growth.spd;

            member.baseStats.hp = member.baseStats.maxHp;
            member.baseStats.mp = member.baseStats.maxMp;

            LOG("[PartyManager] LEVEL UP: %s reached level %d. Max HP: %d. ATK: %d.",
                member.name.c_str(),
                member.baseStats.level,
                member.baseStats.maxHp,
                member.baseStats.atk);
        }
    }
}

std::vector<PartyMemberProgress> PartyManager::CaptureProgress() const
{
    std::vector<PartyMemberProgress> progress;
    progress.reserve(mActiveParty.size());

    for (const PartyMember& member : mActiveParty)
    {
        PartyMemberProgress entry{};
        entry.id = member.id;
        entry.baseStats = member.baseStats;
        entry.equipped = member.equipped;
        progress.push_back(entry);
    }
    return progress;
}

void PartyManager::ApplyProgress(const std::vector<PartyMemberProgress>& progress)
{
    if (progress.empty())
    {
        ResetToDefaults();
        return;
    }

    EnsureRosterLoaded();
    mActiveParty.clear();

    for (const PartyMemberProgress& entry : progress)
    {
        const PartyRosterEntry* rosterEntry = FindRosterEntry(entry.id);
        if (!rosterEntry)
        {
            LOG("[PartyManager] WARNING: Save referenced unknown party member '%s'.", entry.id.c_str());
            continue;
        }

        PartyMember member = BuildMemberFromRoster(*rosterEntry);
        member.baseStats = entry.baseStats;
        ClampCurrentResources(member.baseStats);
        member.equipped = entry.equipped;
        mActiveParty.push_back(std::move(member));
    }

    if (mActiveParty.empty())
    {
        mActiveParty.push_back(BuildFallbackVerso());
    }
}

std::string PartyManager::GetEquippedItem(size_t index, EquipSlot slot) const
{
    const int slotIdx = SlotIndex(slot);
    if (slotIdx < 0 || index >= mActiveParty.size()) return "";
    return mActiveParty[index].equipped[slotIdx];
}

bool PartyManager::EquipItem(size_t index, EquipSlot slot, const std::string& itemId)
{
    if (index >= mActiveParty.size()) return false;

    const int slotIdx = SlotIndex(slot);
    if (slotIdx < 0) return false;

    const ItemData* item = ItemRegistry::Get().Find(itemId);
    if (!item || item->equipSlot != slot) return false;

    if (Inventory::Get().GetCount(itemId) <= 0) return false;

    const std::string previousId = mActiveParty[index].equipped[slotIdx];
    if (!previousId.empty())
    {
        Inventory::Get().Add(previousId, 1);
    }

    Inventory::Get().Remove(itemId, 1);
    mActiveParty[index].equipped[slotIdx] = itemId;
    return true;
}

bool PartyManager::UnequipItem(size_t index, EquipSlot slot)
{
    if (index >= mActiveParty.size()) return false;

    const int slotIdx = SlotIndex(slot);
    if (slotIdx < 0) return false;

    const std::string id = mActiveParty[index].equipped[slotIdx];
    if (id.empty()) return false;

    Inventory::Get().Add(id, 1);
    mActiveParty[index].equipped[slotIdx].clear();
    return true;
}

// ------------------------------------------------------------
// Function: EnsureRosterLoaded
// Purpose:
//   Lazily load the full recruitable roster from data/party_roster.json.
// Why:
//   New Game, save/load, and recruitment all need the same member metadata
//   without hardcoded character construction inside gameplay states.
// ------------------------------------------------------------
bool PartyManager::EnsureRosterLoaded()
{
    if (!mRoster.empty()) return true;

    const std::filesystem::path path = ResolveReadablePath("data/party_roster.json");
    std::string src;
    if (!ReadTextFile(path, src))
    {
        LOG("[PartyManager] WARNING: Could not read party roster '%s'.",
            path.string().c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, path.string());
    const std::vector<std::string> objects =
        JsonLoader::detail::ExtractObjectsFromArray(src, "members");

    for (const std::string& objectSrc : objects)
    {
        PartyRosterEntry entry{};
        entry.id = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "id"));
        entry.name = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "name"));
        entry.animationPath = ToWidePath(JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "animationPath")));
        entry.animJsonPath = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "animJsonPath"));
        entry.hpFramePath = ToWidePath(JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "hpFramePath")));
        entry.turnViewPath = ToWidePath(JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "turnViewPath")));
        entry.dataPath = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "dataPath"));
        entry.startingMember = JsonLoader::detail::ParseBool(
            JsonLoader::detail::ValueOf(objectSrc, "startingMember"), false);

        if (entry.id.empty() ||
            entry.name.empty() ||
            entry.animationPath.empty() ||
            entry.animJsonPath.empty() ||
            entry.hpFramePath.empty() ||
            entry.turnViewPath.empty() ||
            entry.dataPath.empty())
        {
            LOG("[PartyManager] WARNING: Skipping invalid party roster entry.");
            continue;
        }

        mRoster.push_back(std::move(entry));
    }

    LOG("[PartyManager] Loaded %zu party roster member(s).", mRoster.size());
    return !mRoster.empty();
}

// ------------------------------------------------------------
// Function: FindRosterEntry
// Purpose:
//   Locate immutable roster metadata by stable member id.
// Why:
//   Save files and story commands reference ids, while PartyManager owns the
//   conversion from id to render and character-data paths.
// ------------------------------------------------------------
const PartyRosterEntry* PartyManager::FindRosterEntry(const std::string& id) const
{
    auto it = std::find_if(mRoster.begin(), mRoster.end(),
        [&id](const PartyRosterEntry& entry)
        {
            return entry.id == id;
        });

    return (it != mRoster.end()) ? &(*it) : nullptr;
}

// ------------------------------------------------------------
// Function: BuildMemberFromRoster
// Purpose:
//   Construct one active PartyMember from roster metadata.
// Why:
//   Saved stats and equipment should overlay onto fresh metadata so asset path
//   changes in data files migrate without rewriting old save slots.
// ------------------------------------------------------------
PartyMember PartyManager::BuildMemberFromRoster(const PartyRosterEntry& entry) const
{
    return BuildMember(entry);
}

// ------------------------------------------------------------
// Function: BuildFallbackVerso
// Purpose:
//   Create a safe solo Verso party when roster data is unavailable.
// Why:
//   A missing roster file should log a warning but not leave the game without
//   any playable combatant.
// ------------------------------------------------------------
PartyMember PartyManager::BuildFallbackVerso() const
{
    PartyRosterEntry entry{};
    entry.id = "verso";
    entry.name = "Verso";
    entry.animationPath = L"assets/animations/verso.png";
    entry.animJsonPath = "assets/animations/verso.json";
    entry.hpFramePath = L"assets/UI/UI_verso_hp.png";
    entry.turnViewPath = L"assets/UI/turn-view-verso.png";
    entry.dataPath = "data/characters/verso.json";
    entry.startingMember = true;
    return BuildMember(entry);
}
