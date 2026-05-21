// ============================================================
// File: SaveManager.cpp
// Responsibility: Serialize and restore numbered save slots using the
//                 existing JsonLoader helper style.
// ============================================================
#define NOMINMAX
#include "SaveManager.h"
#include "GameProgress.h"
#include "Inventory.h"
#include "PartyManager.h"
#include "../Events/EventManager.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    constexpr int kSaveSchemaVersion = 1;
    constexpr const char* kConfigPath = "data/save_checkpoints.json";

    // ------------------------------------------------------------
    // Function: ResolveReadablePath
    // Purpose:
    //   Find a data file from either the workspace root or bin directory.
    // Why:
    //   The executable may run with different working directories during
    //   debugging, manual launches, or build-script verification.
    // ------------------------------------------------------------
    std::filesystem::path ResolveReadablePath(const std::string& path)
    {
        namespace fs = std::filesystem;

        fs::path direct(path);
        if (fs::exists(direct)) return direct;

        fs::path parent = fs::path("..") / path;
        if (fs::exists(parent)) return parent;

        return direct;
    }

    // ------------------------------------------------------------
    // Function: ResolveWritablePath
    // Purpose:
    //   Pick the workspace-root save path when launched from bin.
    // Why:
    //   Save slots should live beside the project data directory instead of
    //   splitting between save/ and bin/save/ depending on launch method.
    // ------------------------------------------------------------
    std::filesystem::path ResolveWritablePath(const std::string& path)
    {
        namespace fs = std::filesystem;

        fs::path direct(path);
        if (direct.is_absolute()) return direct;

        if (!fs::exists("data") && fs::exists(fs::path("..") / "data"))
        {
            return fs::path("..") / direct;
        }

        return direct;
    }

    // ------------------------------------------------------------
    // Function: ReadTextFile
    // Purpose:
    //   Read an entire UTF-8 text file into memory.
    // Why:
    //   The project JSON helpers operate on full strings and the save file
    //   is intentionally small.
    // ------------------------------------------------------------
    bool ReadTextFile(const std::filesystem::path& path, std::string& out)
    {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::ostringstream buffer;
        buffer << file.rdbuf();
        out = buffer.str();
        return true;
    }

    // ------------------------------------------------------------
    // Function: JsonString
    // Purpose:
    //   Escape a C++ string for safe JSON output.
    // Why:
    //   Save files store stable ids and paths, but escaping keeps the writer
    //   correct if future ids contain quotes or slashes.
    // ------------------------------------------------------------
    std::string JsonString(const std::string& value)
    {
        std::string out;
        out.reserve(value.size() + 2);
        out.push_back('"');

        for (char c : value)
        {
            switch (c)
            {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out.push_back(c); break;
            }
        }

        out.push_back('"');
        return out;
    }

    std::string ReadJsonString(const std::string& src, const std::string& key, const std::string& fallback = "")
    {
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        if (raw.empty()) return fallback;
        return JsonLoader::detail::CleanString(raw);
    }

    int ReadJsonInt(const std::string& src, const std::string& key, int fallback = 0)
    {
        return JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, key), fallback);
    }

    float ReadJsonFloat(const std::string& src, const std::string& key, float fallback = 0.0f)
    {
        return JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, key), fallback);
    }

    // ------------------------------------------------------------
    // Function: BuildSlotPath
    // Purpose:
    //   Compose a numbered slot path from save config fields.
    // Why:
    //   Keeping the pattern data-driven lets the UI expose more slots without
    //   hardcoding each file name in C++.
    // ------------------------------------------------------------
    std::string BuildSlotPath(const SaveCheckpointConfig& config, int slotIndex)
    {
        std::ostringstream path;
        path << config.slotDirectory;
        if (!config.slotDirectory.empty() &&
            config.slotDirectory.back() != '/' &&
            config.slotDirectory.back() != '\\')
        {
            path << "/";
        }
        path << config.slotFilePrefix << slotIndex << config.slotFileExtension;
        return path.str();
    }

    BattlerStats ReadStats(const std::string& src)
    {
        BattlerStats stats{};
        stats.hp = ReadJsonInt(src, "hp");
        stats.maxHp = ReadJsonInt(src, "maxHp", 1);
        stats.mp = ReadJsonInt(src, "mp");
        stats.maxMp = ReadJsonInt(src, "maxMp");
        stats.atk = ReadJsonInt(src, "atk");
        stats.def = ReadJsonInt(src, "def");
        stats.matk = ReadJsonInt(src, "matk");
        stats.mdef = ReadJsonInt(src, "mdef");
        stats.spd = ReadJsonInt(src, "spd");
        stats.rage = ReadJsonInt(src, "rage");
        stats.maxRage = ReadJsonInt(src, "maxRage");
        stats.level = ReadJsonInt(src, "level", 1);
        stats.exp = ReadJsonInt(src, "exp");
        stats.growth.maxHp = ReadJsonInt(src, "growthMaxHp");
        stats.growth.maxMp = ReadJsonInt(src, "growthMaxMp");
        stats.growth.atk = ReadJsonInt(src, "growthAtk");
        stats.growth.def = ReadJsonInt(src, "growthDef");
        stats.growth.matk = ReadJsonInt(src, "growthMatk");
        stats.growth.mdef = ReadJsonInt(src, "growthMdef");
        stats.growth.spd = ReadJsonInt(src, "growthSpd");
        return stats;
    }

    void WriteStats(std::ostream& out, const BattlerStats& stats, const std::string& indent)
    {
        out << indent << "\"hp\": " << stats.hp << ",\n";
        out << indent << "\"maxHp\": " << stats.maxHp << ",\n";
        out << indent << "\"mp\": " << stats.mp << ",\n";
        out << indent << "\"maxMp\": " << stats.maxMp << ",\n";
        out << indent << "\"atk\": " << stats.atk << ",\n";
        out << indent << "\"def\": " << stats.def << ",\n";
        out << indent << "\"matk\": " << stats.matk << ",\n";
        out << indent << "\"mdef\": " << stats.mdef << ",\n";
        out << indent << "\"spd\": " << stats.spd << ",\n";
        out << indent << "\"rage\": " << stats.rage << ",\n";
        out << indent << "\"maxRage\": " << stats.maxRage << ",\n";
        out << indent << "\"level\": " << stats.level << ",\n";
        out << indent << "\"exp\": " << stats.exp << ",\n";
        out << indent << "\"growthMaxHp\": " << stats.growth.maxHp << ",\n";
        out << indent << "\"growthMaxMp\": " << stats.growth.maxMp << ",\n";
        out << indent << "\"growthAtk\": " << stats.growth.atk << ",\n";
        out << indent << "\"growthDef\": " << stats.growth.def << ",\n";
        out << indent << "\"growthMatk\": " << stats.growth.matk << ",\n";
        out << indent << "\"growthMdef\": " << stats.growth.mdef << ",\n";
        out << indent << "\"growthSpd\": " << stats.growth.spd << ",\n";
    }

    std::array<std::string, kEquipSlotCount> ReadEquipment(const std::string& src)
    {
        std::array<std::string, kEquipSlotCount> equipped{};
        equipped[SlotIndex(EquipSlot::Weapon)] = ReadJsonString(src, "equipWeapon");
        equipped[SlotIndex(EquipSlot::Body)] = ReadJsonString(src, "equipBody");
        equipped[SlotIndex(EquipSlot::Head)] = ReadJsonString(src, "equipHead");
        equipped[SlotIndex(EquipSlot::Accessory)] = ReadJsonString(src, "equipAccessory");
        return equipped;
    }

    // ------------------------------------------------------------
    // Function: CampfireIdFromReason
    // Purpose:
    //   Recover a campfire id from older reason-only campfire saves.
    // Why:
    //   Slots created before playerX/playerY existed can still be migrated
    //   to the correct restore position by looking up data/campfires.json.
    // ------------------------------------------------------------
    std::string CampfireIdFromReason(const std::string& reason)
    {
        static constexpr const char* kPrefixes[] =
        {
            "campfire_save:",
            "campfire_rest:",
            "campfire_upgrade:"
        };

        for (const char* prefix : kPrefixes)
        {
            const std::string token(prefix);
            if (reason.rfind(token, 0) == 0)
            {
                return reason.substr(token.size());
            }
        }

        return "";
    }

    // ------------------------------------------------------------
    // Function: FindCampfirePosition
    // Purpose:
    //   Look up a campfire's overworld coordinates by stable id.
    // Why:
    //   Older save files only stored a campfire reason string, so migration
    //   needs the data file as the source of truth for restore coordinates.
    // ------------------------------------------------------------
    bool FindCampfirePosition(const std::string& campfireId, float& outX, float& outY)
    {
        if (campfireId.empty()) return false;

        std::string src;
        const std::filesystem::path path = ResolveReadablePath("data/campfires.json");
        if (!ReadTextFile(path, src)) return false;

        const std::vector<std::string> objects =
            JsonLoader::detail::ExtractObjectsFromArray(src, "campfires");
        for (const std::string& objectSrc : objects)
        {
            const std::string id = ReadJsonString(objectSrc, "id");
            if (id != campfireId) continue;

            outX = ReadJsonFloat(objectSrc, "worldX");
            outY = ReadJsonFloat(objectSrc, "worldY");
            return true;
        }

        return false;
    }

    // ------------------------------------------------------------
    // Function: ResolveWorldSnapshot
    // Purpose:
    //   Fill missing world restore metadata with configured defaults.
    // Why:
    //   Save/load must remain robust for new-game saves, migrated old slots,
    //   and any future caller that saves before OverworldState has published.
    // ------------------------------------------------------------
    OverworldProgressSnapshot ResolveWorldSnapshot(const OverworldProgressSnapshot& current,
                                                   const SaveCheckpointConfig& config)
    {
        OverworldProgressSnapshot world = current;
        if (world.sceneId.empty())
        {
            world.sceneId = config.autoSceneId;
        }
        if (world.checkpointId.empty())
        {
            world.checkpointId = config.defaultCheckpointId;
        }
        if (!world.hasPlayerPosition)
        {
            world.playerX = config.defaultPlayerX;
            world.playerY = config.defaultPlayerY;
            world.hasPlayerPosition = true;
        }
        return world;
    }
}

SaveManager& SaveManager::Get()
{
    static SaveManager instance;
    return instance;
}

SaveManager::SaveManager()
{
    LoadConfig();
    SubscribeAutoCheckpoint();
}

void SaveManager::LoadConfig()
{
    std::string src;
    const std::filesystem::path path = ResolveReadablePath(kConfigPath);
    if (!ReadTextFile(path, src))
    {
        LOG("[SaveManager] save_checkpoints.json missing; using built-in defaults.");
        return;
    }

    JsonLoader::detail::WarnIfUTF16(src, kConfigPath);
    mConfig.slotPath = ReadJsonString(src, "slotPath", mConfig.slotPath);
    mConfig.slotDirectory = ReadJsonString(src, "slotDirectory", mConfig.slotDirectory);
    mConfig.slotFilePrefix = ReadJsonString(src, "slotFilePrefix", mConfig.slotFilePrefix);
    mConfig.slotFileExtension = ReadJsonString(src, "slotFileExtension", mConfig.slotFileExtension);
    mConfig.slotCount = std::max(1, ReadJsonInt(src, "slotCount", mConfig.slotCount));
    mConfig.defaultSlotIndex = ReadJsonInt(src, "defaultSlotIndex", mConfig.defaultSlotIndex);
    if (mConfig.defaultSlotIndex < 0 || mConfig.defaultSlotIndex >= mConfig.slotCount)
    {
        mConfig.defaultSlotIndex = 0;
    }
    mConfig.autoCheckpointId = ReadJsonString(src, "autoCheckpointId", mConfig.autoCheckpointId);
    mConfig.autoSceneId = ReadJsonString(src, "autoSceneId", mConfig.autoSceneId);
    mConfig.defaultCheckpointId = ReadJsonString(src, "defaultCheckpointId", mConfig.defaultCheckpointId);
    mConfig.defaultPlayerX = ReadJsonFloat(src, "defaultPlayerX", mConfig.defaultPlayerX);
    mConfig.defaultPlayerY = ReadJsonFloat(src, "defaultPlayerY", mConfig.defaultPlayerY);
    mConfig.iconPath = ReadJsonString(src, "iconPath", mConfig.iconPath);
    mConfig.slotPath = BuildSlotPath(mConfig, mConfig.defaultSlotIndex);
    mActiveSlotIndex = mConfig.defaultSlotIndex;
}

void SaveManager::SubscribeAutoCheckpoint()
{
    if (mVictoryListenerId >= 0) return;

    mVictoryListenerId = EventManager::Get().Subscribe("battle_end_victory",
        [this](const EventData&)
        {
            SaveCheckpoint("battle_victory");
        });
}

bool SaveManager::CheckpointExists() const
{
    return FindFirstExistingSlot() >= 0;
}

bool SaveManager::IsValidSlotIndex(int slotIndex) const
{
    return slotIndex >= 0 && slotIndex < mConfig.slotCount;
}

std::string SaveManager::GetSlotPath(int slotIndex) const
{
    if (!IsValidSlotIndex(slotIndex))
    {
        slotIndex = mConfig.defaultSlotIndex;
    }
    return BuildSlotPath(mConfig, slotIndex);
}

bool SaveManager::SlotExists(int slotIndex) const
{
    if (!IsValidSlotIndex(slotIndex)) return false;
    return std::filesystem::exists(ResolveReadablePath(GetSlotPath(slotIndex)));
}

int SaveManager::FindFirstExistingSlot() const
{
    for (int i = 0; i < mConfig.slotCount; ++i)
    {
        if (SlotExists(i)) return i;
    }
    return -1;
}

SaveSlotInfo SaveManager::GetSlotInfo(int slotIndex) const
{
    SaveSlotInfo info{};
    info.slotIndex = slotIndex;
    info.path = GetSlotPath(slotIndex);

    if (!IsValidSlotIndex(slotIndex)) return info;

    const std::filesystem::path path = ResolveReadablePath(info.path);
    std::string src;
    if (!ReadTextFile(path, src))
    {
        return info;
    }

    JsonLoader::detail::WarnIfUTF16(src, info.path);
    info.exists = true;
    info.schemaVersion = ReadJsonInt(src, "schemaVersion");
    info.checkpointId = ReadJsonString(src, "checkpointId");
    info.sceneId = ReadJsonString(src, "sceneId");
    info.reason = ReadJsonString(src, "reason");
    info.playerX = ReadJsonFloat(src, "playerX", mConfig.defaultPlayerX);
    info.playerY = ReadJsonFloat(src, "playerY", mConfig.defaultPlayerY);
    info.hasPlayerPosition = !JsonLoader::detail::ValueOf(src, "playerX").empty() &&
                             !JsonLoader::detail::ValueOf(src, "playerY").empty();
    if (!info.hasPlayerPosition)
    {
        const std::string campfireId = CampfireIdFromReason(info.reason);
        if (FindCampfirePosition(campfireId, info.playerX, info.playerY))
        {
            info.checkpointId = "campfire:" + campfireId;
            info.hasPlayerPosition = true;
        }
        else if (info.reason == "new_game")
        {
            info.checkpointId = mConfig.defaultCheckpointId;
        }
    }
    if (!info.hasPlayerPosition)
    {
        info.playerX = mConfig.defaultPlayerX;
        info.playerY = mConfig.defaultPlayerY;
        info.hasPlayerPosition = true;
    }

    const std::vector<std::string> partyObjects =
        JsonLoader::detail::ExtractObjectsFromArray(src, "party");
    if (!partyObjects.empty())
    {
        info.leadMemberId = ReadJsonString(partyObjects.front(), "id");
        info.leadLevel = ReadJsonInt(partyObjects.front(), "level", 1);
    }

    return info;
}

std::vector<SaveSlotInfo> SaveManager::GetSlotInfos() const
{
    std::vector<SaveSlotInfo> infos;
    infos.reserve(static_cast<size_t>(mConfig.slotCount));
    for (int i = 0; i < mConfig.slotCount; ++i)
    {
        infos.push_back(GetSlotInfo(i));
    }
    return infos;
}

bool SaveManager::SaveCheckpoint(const std::string& reason) const
{
    return SaveCheckpointToSlot(mActiveSlotIndex, reason);
}

bool SaveManager::SaveCheckpointToSlot(int slotIndex, const std::string& reason) const
{
    namespace fs = std::filesystem;

    if (!IsValidSlotIndex(slotIndex))
    {
        LOG("[SaveManager] ERROR: Cannot save invalid slot %d.", slotIndex + 1);
        return false;
    }

    const std::vector<PartyMemberProgress> party = PartyManager::Get().CaptureProgress();
    const std::vector<InventoryEntry> inventory = Inventory::Get().CaptureEntries();
    const std::vector<std::string> flags = GameProgress::Get().CaptureFlags();
    const OverworldProgressSnapshot world =
        ResolveWorldSnapshot(GameProgress::Get().CaptureOverworldSnapshot(), mConfig);
    GameProgress::Get().ReplaceOverworldSnapshot(world);

    const std::string slotPath = GetSlotPath(slotIndex);
    const fs::path finalPath = ResolveWritablePath(slotPath);
    const fs::path parent = finalPath.parent_path();
    if (!parent.empty())
    {
        std::error_code dirError;
        fs::create_directories(parent, dirError);
        if (dirError)
        {
            LOG("[SaveManager] ERROR: Could not create save directory '%s'.", parent.string().c_str());
            return false;
        }
    }

    fs::path tempPath = finalPath;
    tempPath += ".tmp";

    std::ofstream file(tempPath, std::ios::trunc);
    if (!file.is_open())
    {
        LOG("[SaveManager] ERROR: Could not open temp save '%s'.", tempPath.string().c_str());
        return false;
    }

    file << "{\n";
    file << "  \"schemaVersion\": " << kSaveSchemaVersion << ",\n";
    file << "  \"slotIndex\": " << slotIndex << ",\n";
    file << "  \"checkpointId\": " << JsonString(world.checkpointId) << ",\n";
    file << "  \"sceneId\": " << JsonString(world.sceneId) << ",\n";
    file << "  \"playerX\": " << world.playerX << ",\n";
    file << "  \"playerY\": " << world.playerY << ",\n";
    file << "  \"reason\": " << JsonString(reason) << ",\n";
    file << "  \"party\": [\n";

    for (size_t i = 0; i < party.size(); ++i)
    {
        const PartyMemberProgress& member = party[i];
        file << "    {\n";
        file << "      \"id\": " << JsonString(member.id) << ",\n";
        WriteStats(file, member.baseStats, "      ");
        file << "      \"equipWeapon\": " << JsonString(member.equipped[SlotIndex(EquipSlot::Weapon)]) << ",\n";
        file << "      \"equipBody\": " << JsonString(member.equipped[SlotIndex(EquipSlot::Body)]) << ",\n";
        file << "      \"equipHead\": " << JsonString(member.equipped[SlotIndex(EquipSlot::Head)]) << ",\n";
        file << "      \"equipAccessory\": " << JsonString(member.equipped[SlotIndex(EquipSlot::Accessory)]) << "\n";
        file << "    }" << ((i + 1 < party.size()) ? "," : "") << "\n";
    }

    file << "  ],\n";
    file << "  \"flags\": [\n";

    for (size_t i = 0; i < flags.size(); ++i)
    {
        file << "    { \"id\": " << JsonString(flags[i]) << " }"
             << ((i + 1 < flags.size()) ? "," : "") << "\n";
    }

    file << "  ],\n";
    file << "  \"inventory\": [\n";

    for (size_t i = 0; i < inventory.size(); ++i)
    {
        file << "    { \"id\": " << JsonString(inventory[i].id)
             << ", \"count\": " << inventory[i].count << " }"
             << ((i + 1 < inventory.size()) ? "," : "") << "\n";
    }

    file << "  ]\n";
    file << "}\n";
    file.close();

    std::error_code replaceError;
    fs::remove(finalPath, replaceError);

    replaceError.clear();
    fs::rename(tempPath, finalPath, replaceError);
    if (replaceError)
    {
        LOG("[SaveManager] ERROR: Could not finalize save '%s'.", finalPath.string().c_str());
        return false;
    }

    mActiveSlotIndex = slotIndex;
    LOG("[SaveManager] Slot %d saved to '%s'. Reason: %s.",
        slotIndex + 1, finalPath.string().c_str(), reason.c_str());
    return true;
}

bool SaveManager::LoadCheckpoint(std::string* outSceneId) const
{
    return LoadCheckpointFromSlot(mActiveSlotIndex, outSceneId);
}

bool SaveManager::LoadCheckpointFromSlot(int slotIndex, std::string* outSceneId) const
{
    if (!IsValidSlotIndex(slotIndex))
    {
        LOG("[SaveManager] ERROR: Cannot load invalid slot %d.", slotIndex + 1);
        return false;
    }

    const std::string slotPath = GetSlotPath(slotIndex);
    const std::filesystem::path path = ResolveReadablePath(slotPath);

    std::string src;
    if (!ReadTextFile(path, src))
    {
        LOG("[SaveManager] No checkpoint found in slot %d at '%s'.",
            slotIndex + 1, slotPath.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, slotPath);
    const int schemaVersion = ReadJsonInt(src, "schemaVersion");
    if (schemaVersion != kSaveSchemaVersion)
    {
        LOG("[SaveManager] ERROR: Unsupported save schema %d in slot %d.",
            schemaVersion, slotIndex + 1);
        return false;
    }

    const std::string reason = ReadJsonString(src, "reason");
    OverworldProgressSnapshot world{};
    world.sceneId = ReadJsonString(src, "sceneId", mConfig.autoSceneId);
    world.checkpointId = ReadJsonString(src, "checkpointId", mConfig.defaultCheckpointId);
    world.hasPlayerPosition = !JsonLoader::detail::ValueOf(src, "playerX").empty() &&
                              !JsonLoader::detail::ValueOf(src, "playerY").empty();
    world.playerX = ReadJsonFloat(src, "playerX", mConfig.defaultPlayerX);
    world.playerY = ReadJsonFloat(src, "playerY", mConfig.defaultPlayerY);

    if (!world.hasPlayerPosition)
    {
        const std::string campfireId = CampfireIdFromReason(reason);
        if (FindCampfirePosition(campfireId, world.playerX, world.playerY))
        {
            world.checkpointId = "campfire:" + campfireId;
            world.hasPlayerPosition = true;
        }
        else if (reason == "new_game")
        {
            world.checkpointId = mConfig.defaultCheckpointId;
        }
    }

    world = ResolveWorldSnapshot(world, mConfig);

    std::vector<PartyMemberProgress> partyProgress;
    const std::vector<std::string> partyObjects = JsonLoader::detail::ExtractObjectsFromArray(src, "party");
    for (const std::string& objectSrc : partyObjects)
    {
        PartyMemberProgress member{};
        member.id = ReadJsonString(objectSrc, "id");
        member.baseStats = ReadStats(objectSrc);
        member.equipped = ReadEquipment(objectSrc);
        if (!member.id.empty()) partyProgress.push_back(member);
    }

    if (partyProgress.empty())
    {
        LOG("[SaveManager] ERROR: Save contains no party progress.");
        return false;
    }

    std::vector<InventoryEntry> entries;
    const std::vector<std::string> inventoryObjects = JsonLoader::detail::ExtractObjectsFromArray(src, "inventory");
    for (const std::string& objectSrc : inventoryObjects)
    {
        InventoryEntry entry{};
        entry.id = ReadJsonString(objectSrc, "id");
        entry.count = ReadJsonInt(objectSrc, "count");
        if (!entry.id.empty() && entry.count > 0) entries.push_back(entry);
    }

    std::vector<std::string> flags;
    const std::vector<std::string> flagObjects = JsonLoader::detail::ExtractObjectsFromArray(src, "flags");
    for (const std::string& objectSrc : flagObjects)
    {
        const std::string id = ReadJsonString(objectSrc, "id");
        if (!id.empty()) flags.push_back(id);
    }

    PartyManager::Get().ResetToDefaults();
    Inventory::Get().ReplaceAll(entries);
    GameProgress::Get().ReplaceFlags(flags);
    GameProgress::Get().ReplaceOverworldSnapshot(world);
    PartyManager::Get().ApplyProgress(partyProgress);

    if (outSceneId) *outSceneId = world.sceneId;

    mActiveSlotIndex = slotIndex;
    LOG("[SaveManager] Slot %d loaded from '%s'. Scene: %s.",
        slotIndex + 1, path.string().c_str(), world.sceneId.c_str());
    return true;
}
