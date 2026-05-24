// ============================================================
// File: LocalizationManager.h
// Responsibility: Load localized text tables and provide lookup helpers.
//
// Owns:
//   Language metadata and the active plus English fallback string tables.
//
// Lifetime:
//   Created as a Meyers singleton on first access.
//   Initialized in -> GameApp::Initialize() after SettingsManager.
//
// Important:
//   - String tables are flat UTF-8 JSON objects.
//   - Missing keys fall back to English, then to the key itself.
// ============================================================
#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

struct LanguageInfo
{
    std::string id;
    std::string nameKey;
    std::string fontPath;
};

class LocalizationManager
{
public:
    static LocalizationManager& Get();

    bool Initialize(const std::string& metadataPath, const std::string& languageId);
    bool SetLanguage(const std::string& languageId);

    std::string Text(const std::string& key) const;
    std::string TextOrFallback(const std::string& key, const std::string& fallback) const;
    std::string Format(
        const std::string& key,
        const std::vector<std::pair<std::string, std::string>>& values) const;
    std::string TextEnglish(const std::string& key) const;
    std::string TextOrFallbackEnglish(const std::string& key, const std::string& fallback) const;
    std::string FormatEnglish(
        const std::string& key,
        const std::vector<std::pair<std::string, std::string>>& values) const;

    const std::string& GetCurrentLanguageId() const { return mCurrentLanguageId; }
    std::string GetCurrentFontPath() const;
    const std::vector<LanguageInfo>& GetLanguages() const { return mLanguages; }

private:
    LocalizationManager() = default;
    LocalizationManager(const LocalizationManager&) = delete;
    LocalizationManager& operator=(const LocalizationManager&) = delete;

    bool LoadMetadata(const std::string& metadataPath);
    bool LoadLanguageTable(const std::string& languageId,
                           std::map<std::string, std::string>& out) const;
    const LanguageInfo* FindLanguage(const std::string& languageId) const;
    bool TryGetText(const std::string& key, std::string& out) const;
    void LogMissingKeyOnce(const std::string& key) const;

    std::string mMetadataPath = "data/localization/languages.json";
    std::string mDefaultLanguageId = "en_us";
    std::string mCurrentLanguageId = "en_us";
    std::vector<LanguageInfo> mLanguages;
    std::map<std::string, std::string> mEnglishText;
    std::map<std::string, std::string> mCurrentText;
    mutable std::set<std::string> mMissingKeys;
    bool mInitialized = false;
};
