// ============================================================
// File: SaveManager.cpp
// Responsibility: Serialize and restore checkpoint data using the
//                 existing JsonLoader helper style.
// ============================================================
#include "SaveManager.h"
#include "GameProgress.h"
#include "Inventory.h"
#include "PartyManager.h"
#include "../Events/EventManager.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
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
    //   Checkpoints should live beside the project data directory instead of
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
    mConfig.autoCheckpointId = ReadJsonString(src, "autoCheckpointId", mConfig.autoCheckpointId);
    mConfig.autoSceneId = ReadJsonString(src, "autoSceneId", mConfig.autoSceneId);
    mConfig.iconPath = ReadJsonString(src, "iconPath", mConfig.iconPath);
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
    return std::filesystem::exists(ResolveReadablePath(mConfig.slotPath));
}

bool SaveManager::SaveCheckpoint(const std::string& reason) const
{
    namespace fs = std::filesystem;

    const std::vector<PartyMemberProgress> party = PartyManager::Get().CaptureProgress();
    const std::vector<InventoryEntry> inventory = Inventory::Get().CaptureEntries();
    const std::vector<std::string> flags = GameProgress::Get().CaptureFlags();

    const fs::path finalPath = ResolveWritablePath(mConfig.slotPath);
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
    file << "  \"checkpointId\": " << JsonString(mConfig.autoCheckpointId) << ",\n";
    file << "  \"sceneId\": " << JsonString(mConfig.autoSceneId) << ",\n";
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

    LOG("[SaveManager] Checkpoint saved to '%s'. Reason: %s.", finalPath.string().c_str(), reason.c_str());
    return true;
}

bool SaveManager::LoadCheckpoint(std::string* outSceneId) const
{
    const std::filesystem::path path = ResolveReadablePath(mConfig.slotPath);

    std::string src;
    if (!ReadTextFile(path, src))
    {
        LOG("[SaveManager] No checkpoint found at '%s'.", mConfig.slotPath.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, mConfig.slotPath);
    const int schemaVersion = ReadJsonInt(src, "schemaVersion");
    if (schemaVersion != kSaveSchemaVersion)
    {
        LOG("[SaveManager] ERROR: Unsupported save schema %d.", schemaVersion);
        return false;
    }

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
    PartyManager::Get().ApplyProgress(partyProgress);

    const std::string sceneId = ReadJsonString(src, "sceneId", mConfig.autoSceneId);
    if (outSceneId) *outSceneId = sceneId;

    LOG("[SaveManager] Checkpoint loaded from '%s'. Scene: %s.", path.string().c_str(), sceneId.c_str());
    return true;
}
