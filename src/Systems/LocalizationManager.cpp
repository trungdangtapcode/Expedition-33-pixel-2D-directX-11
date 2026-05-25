// ============================================================
// File: LocalizationManager.cpp
// Responsibility: Implement UTF-8 localization table loading.
// ============================================================
#define NOMINMAX
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

    void SkipWhitespace(const std::string& src, size_t& pos)
    {
        while (pos < src.size())
        {
            const char c = src[pos];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
            ++pos;
        }
    }

    bool ParseQuotedString(const std::string& src, size_t& pos, std::string& out)
    {
        SkipWhitespace(src, pos);
        if (pos >= src.size() || src[pos] != '"') return false;
        ++pos;

        out.clear();
        while (pos < src.size())
        {
            const char c = src[pos++];
            if (c == '"') return true;

            if (c == '\\' && pos < src.size())
            {
                const char escaped = src[pos++];
                switch (escaped)
                {
                case '"':  out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                default:
                    out.push_back('\\');
                    out.push_back(escaped);
                    break;
                }
                continue;
            }

            out.push_back(c);
        }

        return false;
    }

    std::map<std::string, std::string> ParseFlatStringTable(const std::string& src)
    {
        std::map<std::string, std::string> table;
        size_t pos = 0;

        while (pos < src.size())
        {
            std::string key;
            if (!ParseQuotedString(src, pos, key))
            {
                ++pos;
                continue;
            }

            SkipWhitespace(src, pos);
            if (pos >= src.size() || src[pos] != ':') continue;
            ++pos;

            std::string value;
            if (!ParseQuotedString(src, pos, value)) continue;
            table[key] = value;
        }

        return table;
    }

    std::string ReadJsonString(const std::string& src,
                               const std::string& key,
                               const std::string& fallback = "")
    {
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        if (raw.empty()) return fallback;
        return JsonLoader::detail::CleanString(raw);
    }
}

LocalizationManager& LocalizationManager::Get()
{
    static LocalizationManager instance;
    return instance;
}

bool LocalizationManager::Initialize(const std::string& metadataPath,
                                     const std::string& languageId)
{
    mMetadataPath = metadataPath;
    mInitialized = LoadMetadata(metadataPath);

    if (!LoadLanguageTable(mDefaultLanguageId, mEnglishText))
    {
        LOG("[LocalizationManager] WARNING: Failed to load English fallback table.");
        mEnglishText.clear();
    }

    if (!SetLanguage(languageId))
    {
        SetLanguage(mDefaultLanguageId);
    }

    return mInitialized;
}

bool LocalizationManager::LoadMetadata(const std::string& metadataPath)
{
    std::string src;
    const std::filesystem::path path = ResolveReadablePath(metadataPath);
    if (!ReadTextFile(path, src))
    {
        LOG("[LocalizationManager] WARNING: Missing metadata '%s'.", metadataPath.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, metadataPath);

    mDefaultLanguageId = ReadJsonString(src, "defaultLanguage", "en_us");
    mLanguages.clear();

    const std::vector<std::string> objects =
        JsonLoader::detail::ExtractObjectsFromArray(src, "languages");
    for (const std::string& objectSrc : objects)
    {
        LanguageInfo info;
        info.id = ReadJsonString(objectSrc, "id");
        info.nameKey = ReadJsonString(objectSrc, "nameKey");
        info.fontPath = ReadJsonString(objectSrc, "fontPath");
        if (!info.id.empty() && !info.fontPath.empty())
        {
            mLanguages.push_back(info);
        }
    }

    if (mLanguages.empty())
    {
        LanguageInfo fallback;
        fallback.id = "en_us";
        fallback.nameKey = "language.english";
        fallback.fontPath = "assets/fonts/arial_16.spritefont";
        mLanguages.push_back(fallback);
    }

    LOG("[LocalizationManager] Loaded %zu language(s).", mLanguages.size());
    return true;
}

bool LocalizationManager::LoadLanguageTable(const std::string& languageId,
                                            std::map<std::string, std::string>& out) const
{
    std::string path = "data/localization/" + languageId + ".json";
    std::string src;
    const std::filesystem::path resolved = ResolveReadablePath(path);
    if (!ReadTextFile(resolved, src))
    {
        LOG("[LocalizationManager] WARNING: Missing string table '%s'.", path.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, path);
    out = ParseFlatStringTable(src);
    LOG("[LocalizationManager] Loaded table '%s' with %zu string(s).",
        languageId.c_str(), out.size());
    return true;
}

bool LocalizationManager::SetLanguage(const std::string& languageId)
{
    const LanguageInfo* language = FindLanguage(languageId);
    const std::string nextId = language ? language->id : mDefaultLanguageId;

    std::map<std::string, std::string> nextTable;
    if (!LoadLanguageTable(nextId, nextTable))
    {
        return false;
    }

    mCurrentLanguageId = nextId;
    mCurrentText = std::move(nextTable);
    mMissingKeys.clear();

    LOG("[LocalizationManager] Active language: '%s'.", mCurrentLanguageId.c_str());
    return true;
}

std::string LocalizationManager::Text(const std::string& key) const
{
    std::string value;
    if (TryGetText(key, value)) return value;

    LogMissingKeyOnce(key);
    return key;
}

std::string LocalizationManager::TextOrFallback(const std::string& key,
                                                const std::string& fallback) const
{
    std::string value;
    if (TryGetText(key, value)) return value;
    if (!fallback.empty()) return fallback;

    LogMissingKeyOnce(key);
    return key;
}

std::string LocalizationManager::Format(
    const std::string& key,
    const std::vector<std::pair<std::string, std::string>>& values) const
{
    std::string text = Text(key);
    for (const auto& entry : values)
    {
        const std::string token = "{" + entry.first + "}";
        size_t pos = 0;
        while ((pos = text.find(token, pos)) != std::string::npos)
        {
            text.replace(pos, token.size(), entry.second);
            pos += entry.second.size();
        }
    }
    return text;
}

std::string LocalizationManager::TextEnglish(const std::string& key) const
{
    auto it = mEnglishText.find(key);
    if (it != mEnglishText.end()) return it->second;

    LogMissingKeyOnce(key);
    return key;
}

std::string LocalizationManager::TextOrFallbackEnglish(const std::string& key,
                                                       const std::string& fallback) const
{
    auto it = mEnglishText.find(key);
    if (it != mEnglishText.end()) return it->second;
    if (!fallback.empty()) return fallback;

    LogMissingKeyOnce(key);
    return key;
}

std::string LocalizationManager::FormatEnglish(
    const std::string& key,
    const std::vector<std::pair<std::string, std::string>>& values) const
{
    std::string text = TextEnglish(key);
    for (const auto& entry : values)
    {
        const std::string token = "{" + entry.first + "}";
        size_t pos = 0;
        while ((pos = text.find(token, pos)) != std::string::npos)
        {
            text.replace(pos, token.size(), entry.second);
            pos += entry.second.size();
        }
    }
    return text;
}

std::string LocalizationManager::GetCurrentFontPath() const
{
    const LanguageInfo* language = FindLanguage(mCurrentLanguageId);
    if (language) return language->fontPath;

    language = FindLanguage(mDefaultLanguageId);
    if (language) return language->fontPath;

    return "assets/fonts/arial_16.spritefont";
}

const LanguageInfo* LocalizationManager::FindLanguage(const std::string& languageId) const
{
    for (const LanguageInfo& language : mLanguages)
    {
        if (language.id == languageId) return &language;
    }
    return nullptr;
}

bool LocalizationManager::TryGetText(const std::string& key, std::string& out) const
{
    auto it = mCurrentText.find(key);
    if (it != mCurrentText.end())
    {
        out = it->second;
        return true;
    }

    it = mEnglishText.find(key);
    if (it != mEnglishText.end())
    {
        out = it->second;
        return true;
    }

    return false;
}

void LocalizationManager::LogMissingKeyOnce(const std::string& key) const
{
    if (mMissingKeys.insert(key).second)
    {
        LOG("[LocalizationManager] Missing localization key '%s'.", key.c_str());
    }
}
