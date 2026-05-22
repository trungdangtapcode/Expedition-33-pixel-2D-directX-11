// ============================================================
// File: SettingsManager.cpp
// Responsibility: Load and save global settings JSON.
// ============================================================
#define NOMINMAX
#include "SettingsManager.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
    constexpr const char* kSettingsPath = "save/settings.json";

    std::filesystem::path ResolveReadablePath(const std::string& path)
    {
        namespace fs = std::filesystem;

        fs::path direct(path);
        if (fs::exists(direct)) return direct;

        fs::path parent = fs::path("..") / path;
        if (fs::exists(parent)) return parent;

        return direct;
    }

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

    bool ReadTextFile(const std::filesystem::path& path, std::string& out)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return false;

        std::ostringstream buffer;
        buffer << file.rdbuf();
        out = buffer.str();
        return true;
    }

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

    std::string ReadJsonString(const std::string& src,
                               const std::string& key,
                               const std::string& fallback)
    {
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        if (raw.empty()) return fallback;
        return JsonLoader::detail::CleanString(raw);
    }
}

SettingsManager& SettingsManager::Get()
{
    static SettingsManager instance;
    return instance;
}

void SettingsManager::Initialize()
{
    if (mInitialized) return;
    mInitialized = true;

    if (!Load())
    {
        LOG("[SettingsManager] No settings file found. Using defaults.");
    }
}

bool SettingsManager::Load()
{
    std::string src;
    const std::filesystem::path path = ResolveReadablePath(kSettingsPath);
    if (!ReadTextFile(path, src)) return false;

    JsonLoader::detail::WarnIfUTF16(src, kSettingsPath);

    mLanguageId = ReadJsonString(src, "language", mLanguageId);
    mBgmVolume = JsonLoader::detail::ParseFloat(
        JsonLoader::detail::ValueOf(src, "bgmVolume"), mBgmVolume);
    mSfxVolume = JsonLoader::detail::ParseFloat(
        JsonLoader::detail::ValueOf(src, "sfxVolume"), mSfxVolume);

    LOG("[SettingsManager] Loaded settings from '%s'.", path.string().c_str());
    return true;
}

bool SettingsManager::Save() const
{
    const std::filesystem::path path = ResolveWritablePath(kSettingsPath);

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec)
    {
        LOG("[SettingsManager] Failed to create settings directory '%s'.",
            path.parent_path().string().c_str());
        return false;
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        LOG("[SettingsManager] Failed to open '%s' for writing.",
            path.string().c_str());
        return false;
    }

    file << "{\n";
    file << "  \"language\": " << JsonString(mLanguageId) << ",\n";
    file << "  \"bgmVolume\": " << mBgmVolume << ",\n";
    file << "  \"sfxVolume\": " << mSfxVolume << "\n";
    file << "}\n";

    LOG("[SettingsManager] Saved settings to '%s'.", path.string().c_str());
    return true;
}

void SettingsManager::SetLanguageId(const std::string& languageId)
{
    if (languageId.empty() || languageId == mLanguageId) return;

    mLanguageId = languageId;
    Save();
}
