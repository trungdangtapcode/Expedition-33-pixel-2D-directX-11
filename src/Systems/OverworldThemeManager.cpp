// ============================================================
// File: OverworldThemeManager.cpp
// Responsibility: Parse overworld theme data and blend color settings.
//
// Blend design:
//   Theme changes are not instant because abrupt full-screen color shifts
//   feel like a bug while walking between regions. A short data-driven
//   blend keeps the transition perceptible but calm.
// ============================================================
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "OverworldThemeManager.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace
{
    std::ifstream OpenDataFile(std::filesystem::path& path)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            path = std::filesystem::path("..") / path;
            file.clear();
            file.open(path);
        }
        return file;
    }

    float ReadFloat(const std::string& objectSrc,
                    const std::string& key,
                    float fallback)
    {
        return JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, key),
            fallback);
    }
}

// ------------------------------------------------------------
// Function: Initialize
// Purpose:
//   Load all overworld theme records from JSON.
// Why:
//   Mood tuning belongs in data so designers can adjust biome identity
//   without recompiling the renderer or overworld state.
// Parameters:
//   path - UTF-8 JSON path, usually data/overworld_themes.json.
// Returns:
//   true when at least one theme was loaded.
// ------------------------------------------------------------
bool OverworldThemeManager::Initialize(const std::string& path)
{
    std::filesystem::path fsPath(path);
    std::ifstream file = OpenDataFile(fsPath);
    if (!file.is_open())
    {
        LOG("[OverworldThemeManager] Cannot open theme config '%s'.", path.c_str());
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string src = buffer.str();
    JsonLoader::detail::WarnIfUTF16(src, fsPath.string());

    const std::string defaultThemeId = JsonLoader::detail::CleanString(
        JsonLoader::detail::ValueOf(src, "defaultThemeId"));
    if (!defaultThemeId.empty())
    {
        mDefaultThemeId = defaultThemeId;
    }

    mThemes.clear();
    const std::vector<std::string> objects =
        JsonLoader::detail::ExtractObjectsFromArray(src, "themes");

    for (const std::string& objectSrc : objects)
    {
        ThemeData theme{};
        theme.id = JsonLoader::detail::CleanString(
            JsonLoader::detail::ValueOf(objectSrc, "id"));
        if (theme.id.empty())
        {
            LOG("[OverworldThemeManager] WARNING: Skipping theme with no id.");
            continue;
        }

        theme.grade.tintR = ReadFloat(objectSrc, "tintR", 1.0f);
        theme.grade.tintG = ReadFloat(objectSrc, "tintG", 1.0f);
        theme.grade.tintB = ReadFloat(objectSrc, "tintB", 1.0f);
        theme.grade.tintStrength = ReadFloat(objectSrc, "tintStrength", 0.0f);
        theme.grade.saturation = ReadFloat(objectSrc, "saturation", 1.0f);
        theme.grade.contrast = ReadFloat(objectSrc, "contrast", 1.0f);
        theme.grade.brightness = ReadFloat(objectSrc, "brightness", 0.0f);
        theme.grade.vignetteStrength = ReadFloat(objectSrc, "vignetteStrength", 0.0f);
        theme.blendSeconds = ReadFloat(objectSrc, "blendSeconds", 1.0f);

        mThemes[theme.id] = theme;
    }

    if (mThemes.empty())
    {
        LOG("[OverworldThemeManager] No themes were loaded from '%s'.", path.c_str());
        return false;
    }

    if (!FindTheme(mDefaultThemeId))
    {
        mDefaultThemeId = mThemes.begin()->first;
        LOG("[OverworldThemeManager] Default theme missing; using '%s'.",
            mDefaultThemeId.c_str());
    }

    SetTheme(mDefaultThemeId, true);
    LOG("[OverworldThemeManager] Loaded %zu theme(s).", mThemes.size());
    return true;
}

// ------------------------------------------------------------
// Function: SetTheme
// Purpose:
//   Select a target theme and optionally snap to it immediately.
// Why:
//   Region movement can call this every frame; repeated calls to the same
//   target must be cheap and must not restart the blend.
// Parameters:
//   themeId - requested theme id. Unknown IDs fall back to default.
//   instant - true for initial state or reload; false for walking changes.
// ------------------------------------------------------------
void OverworldThemeManager::SetTheme(const std::string& themeId, bool instant)
{
    const ThemeData* theme = FindTheme(themeId);
    if (!theme)
    {
        theme = FindTheme(mDefaultThemeId);
    }
    if (!theme) return;

    if (!instant && theme->id == mTargetThemeId)
    {
        return;
    }

    mTargetThemeId = theme->id;
    mTargetGrade = theme->grade;
    mBlendStartGrade = mCurrentGrade;
    mBlendTimer = 0.0f;
    mBlendDuration = std::max(0.0f, theme->blendSeconds);

    if (instant || mBlendDuration <= 0.0f)
    {
        mCurrentThemeId = theme->id;
        mCurrentGrade = theme->grade;
        mBlendStartGrade = theme->grade;
        mTargetGrade = theme->grade;
        mBlendDuration = 0.0f;
        return;
    }
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Advance the active blend toward the requested target theme.
// Why:
//   Frame-rate-independent blending keeps theme changes stable across
//   different machines.
// Parameters:
//   dt - gameplay delta time in seconds.
// ------------------------------------------------------------
void OverworldThemeManager::Update(float dt)
{
    if (mBlendDuration <= 0.0f)
    {
        return;
    }

    mBlendTimer += std::max(0.0f, dt);
    const float t = std::min(1.0f, mBlendTimer / mBlendDuration);
    mCurrentGrade = Lerp(mBlendStartGrade, mTargetGrade, t);

    if (t >= 1.0f)
    {
        mCurrentThemeId = mTargetThemeId;
        mBlendDuration = 0.0f;
        mBlendTimer = 0.0f;
    }
}

const OverworldThemeManager::ThemeData* OverworldThemeManager::FindTheme(
    const std::string& themeId) const
{
    const auto it = mThemes.find(themeId);
    if (it == mThemes.end()) return nullptr;
    return &it->second;
}

ColorGradeSettings OverworldThemeManager::Lerp(const ColorGradeSettings& a,
                                               const ColorGradeSettings& b,
                                               float t)
{
    const float clampedT = std::max(0.0f, std::min(1.0f, t));
    const auto lerpFloat = [clampedT](float start, float end)
    {
        return start + (end - start) * clampedT;
    };

    ColorGradeSettings result{};
    result.tintR = lerpFloat(a.tintR, b.tintR);
    result.tintG = lerpFloat(a.tintG, b.tintG);
    result.tintB = lerpFloat(a.tintB, b.tintB);
    result.tintStrength = lerpFloat(a.tintStrength, b.tintStrength);
    result.saturation = lerpFloat(a.saturation, b.saturation);
    result.contrast = lerpFloat(a.contrast, b.contrast);
    result.brightness = lerpFloat(a.brightness, b.brightness);
    result.vignetteStrength = lerpFloat(a.vignetteStrength, b.vignetteStrength);
    return result;
}
