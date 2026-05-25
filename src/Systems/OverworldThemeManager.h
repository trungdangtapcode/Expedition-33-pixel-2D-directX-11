// ============================================================
// File: OverworldThemeManager.h
// Responsibility: Load and blend overworld region color themes.
//
// Architecture:
//   OverworldState decides which story region the player is in, then asks
//   this manager to blend toward that region's theme. The renderer only
//   receives ColorGradeSettings and does not know about story regions.
//
// Data:
//   Theme IDs and tuning values live in data/overworld_themes.json.
//   Story regions reference those IDs with optional themeId fields.
//
// Lifetime:
//   Created in  -> OverworldState member construction.
//   Loaded in   -> OverworldState::OnEnter.
//   Destroyed in -> OverworldState destruction; no explicit shutdown needed.
// ============================================================
#pragma once

#include "../Renderer/ColorGradeSettings.h"
#include <string>
#include <unordered_map>

class OverworldThemeManager
{
public:
    bool Initialize(const std::string& path);
    void SetTheme(const std::string& themeId, bool instant = false);
    void Update(float dt);

    const ColorGradeSettings& GetCurrentGrade() const { return mCurrentGrade; }
    const std::string& GetCurrentThemeId() const { return mCurrentThemeId; }
    const std::string& GetDefaultThemeId() const { return mDefaultThemeId; }

private:
    struct ThemeData
    {
        std::string id;
        ColorGradeSettings grade;
        float blendSeconds = 1.0f;
    };

    const ThemeData* FindTheme(const std::string& themeId) const;
    static ColorGradeSettings Lerp(const ColorGradeSettings& a,
                                   const ColorGradeSettings& b,
                                   float t);

    std::unordered_map<std::string, ThemeData> mThemes;
    std::string mDefaultThemeId = "neutral";
    std::string mCurrentThemeId = "neutral";
    std::string mTargetThemeId = "neutral";

    ColorGradeSettings mCurrentGrade;
    ColorGradeSettings mBlendStartGrade;
    ColorGradeSettings mTargetGrade;

    float mBlendTimer = 0.0f;
    float mBlendDuration = 0.0f;
};
