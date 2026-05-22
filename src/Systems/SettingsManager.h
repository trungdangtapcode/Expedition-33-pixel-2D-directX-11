// ============================================================
// File: SettingsManager.h
// Responsibility: Persist global player preferences outside save slots.
//
// Owns:
//   Language id and future audio preference values loaded from
//   save/settings.json.
//
// Lifetime:
//   Created as a Meyers singleton on first access.
//   Initialized in -> GameApp::Initialize() before MenuState is pushed.
//
// Important:
//   - Language is global so the title menu can localize before any save
//     slot is loaded.
//   - Save slots remain gameplay state only.
// ============================================================
#pragma once

#include <string>

class SettingsManager
{
public:
    static SettingsManager& Get();

    void Initialize();
    bool Save() const;

    const std::string& GetLanguageId() const { return mLanguageId; }
    void SetLanguageId(const std::string& languageId);

    float GetBgmVolume() const { return mBgmVolume; }
    float GetSfxVolume() const { return mSfxVolume; }

private:
    SettingsManager() = default;
    SettingsManager(const SettingsManager&) = delete;
    SettingsManager& operator=(const SettingsManager&) = delete;

    bool Load();

    std::string mLanguageId = "en_us";
    float mBgmVolume = 1.0f;
    float mSfxVolume = 1.0f;
    bool mInitialized = false;
};
