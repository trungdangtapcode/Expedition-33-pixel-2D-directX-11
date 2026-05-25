// ============================================================
// File: DialogueManager.cpp
// Responsibility: Parse dialogue scripts and resolve localized line text.
// ============================================================
#define NOMINMAX
#include "DialogueManager.h"
#include "LocalizationManager.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
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

    std::string ReadJsonString(const std::string& src,
                               const std::string& key,
                               const std::string& fallback = "")
    {
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        if (raw.empty()) return fallback;
        return JsonLoader::detail::CleanString(raw);
    }

    bool ReadJsonBool(const std::string& src, const std::string& key, bool fallback = false)
    {
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        if (raw.empty()) return fallback;
        return raw.find("true") != std::string::npos || raw.find("1") != std::string::npos;
    }
}

bool DialogueManager::LoadScript(const std::string& path, DialogueScript& outScript) const
{
    outScript = DialogueScript{};

    std::string src;
    const std::filesystem::path resolved = ResolveReadablePath(path);
    if (!ReadTextFile(resolved, src))
    {
        LOG("[DialogueManager] ERROR: Could not read dialogue script '%s'.", path.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, path);

    outScript.id = ReadJsonString(src, "id");
    outScript.completionFlag = ReadJsonString(src, "completionFlag");
    outScript.skippable = ReadJsonBool(src, "skippable", false);

    const std::vector<std::string> lineObjects =
        JsonLoader::detail::ExtractObjectsFromArray(src, "lines");
    for (const std::string& objectSrc : lineObjects)
    {
        DialogueLine line{};
        line.speakerId = ReadJsonString(objectSrc, "speakerId");

        const std::string speakerKey = ReadJsonString(objectSrc, "speakerKey");
        const std::string speakerName = ReadJsonString(objectSrc, "speakerName", line.speakerId);
        line.speakerName = LocalizationManager::Get().TextOrFallback(speakerKey, speakerName);

        const std::string textKey = ReadJsonString(objectSrc, "textKey");
        const std::string fallbackText = ReadJsonString(objectSrc, "text");
        line.text = LocalizationManager::Get().TextOrFallback(textKey, fallbackText);

        if (line.speakerName.empty() || line.text.empty())
        {
            LOG("[DialogueManager] WARNING: Skipping invalid line in '%s'.", path.c_str());
            continue;
        }

        outScript.lines.push_back(line);
    }

    if (outScript.id.empty() || outScript.lines.empty())
    {
        LOG("[DialogueManager] ERROR: Dialogue script '%s' is missing id or lines.", path.c_str());
        outScript = DialogueScript{};
        return false;
    }

    LOG("[DialogueManager] Loaded dialogue '%s' with %zu line(s).",
        outScript.id.c_str(),
        outScript.lines.size());
    return true;
}
