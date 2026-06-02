// ============================================================
// File: ObjectiveTrackerRenderer.cpp
// Responsibility: Implement compact, data-driven overworld objective text.
//
// Common mistakes:
//   1. Wrapping by character count -> Vietnamese and French strings overflow.
//   2. Sharing coin HUD space -> objective text collides with currency.
//   3. Drawing one concatenated objective line -> action hints become hard
//      to scan during movement.
// ============================================================
#define NOMINMAX
#include "ObjectiveTrackerRenderer.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <algorithm>
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

    float ReadFloat(const std::string& src,
                    const std::string& key,
                    float fallback)
    {
        return JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(src, key),
            fallback);
    }

    int ReadInt(const std::string& src,
                const std::string& key,
                int fallback)
    {
        return JsonLoader::detail::ParseInt(
            JsonLoader::detail::ValueOf(src, key),
            fallback);
    }
}

// ------------------------------------------------------------
// Function: Initialize
// Purpose:
//   Load objective tracker presentation tuning.
// Why:
//   The overworld objective copy changes with language and story progress, so
//   text bounds must be adjustable without recompiling the game.
// Parameters:
//   configPath - JSON file containing layout, color, and wrapping values.
// ------------------------------------------------------------
bool ObjectiveTrackerRenderer::Initialize(const std::string& configPath)
{
    return LoadConfig(configPath);
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Draw area, objective body, and action hint as a compact screen-space HUD.
// Why:
//   The player needs guidance while moving, but the text must not compete with
//   the coin counter or become one long unreadable line in localized builds.
// Parameters:
//   context        - D3D11 immediate context for SpriteBatch drawing.
//   textRenderer   - active font renderer, already initialized by the state.
//   screenWidth    - current back-buffer width for right-side reservation.
//   area           - localized area title.
//   objectiveBody  - localized objective sentence.
//   objectiveHint  - localized contextual action hint.
// ------------------------------------------------------------
void ObjectiveTrackerRenderer::Render(ID3D11DeviceContext* context,
                                      BattleTextRenderer& textRenderer,
                                      int screenWidth,
                                      const std::string& area,
                                      const std::string& objectiveBody,
                                      const std::string& objectiveHint) const
{
    if (!context || !textRenderer.IsReady()) return;
    if (area.empty() && objectiveBody.empty() && objectiveHint.empty()) return;

    const float availableWidth = static_cast<float>(screenWidth) -
                                 mConfig.x -
                                 mConfig.rightReserveWidth;
    const float maxWidth = std::max(
        mConfig.minWidth,
        std::min(mConfig.maxWidth, availableWidth));

    const DirectX::XMVECTORF32 titleColor =
        MakeColor(mConfig.titleR, mConfig.titleG, mConfig.titleB, mConfig.titleA);
    const DirectX::XMVECTORF32 bodyColor =
        MakeColor(mConfig.bodyR, mConfig.bodyG, mConfig.bodyB, mConfig.bodyA);
    const DirectX::XMVECTORF32 hintColor =
        MakeColor(mConfig.hintR, mConfig.hintG, mConfig.hintB, mConfig.hintA);

    float y = mConfig.y;
    textRenderer.BeginBatch(context);

    if (!area.empty())
    {
        DrawLine(textRenderer, area, mConfig.x, y, titleColor, mConfig.titleScale);
        y += mConfig.titleLineHeight + mConfig.titleBodyGap;
    }

    const std::vector<std::string> bodyLines = WrapText(
        textRenderer,
        objectiveBody,
        maxWidth,
        mConfig.bodyScale,
        mConfig.bodyMaxLines);

    for (const std::string& line : bodyLines)
    {
        DrawLine(textRenderer, line, mConfig.x, y, bodyColor, mConfig.bodyScale);
        y += mConfig.bodyLineHeight;
    }

    if (!objectiveHint.empty())
    {
        y += mConfig.bodyHintGap;
        const std::vector<std::string> hintLines = WrapText(
            textRenderer,
            objectiveHint,
            maxWidth,
            mConfig.hintScale,
            1);

        if (!hintLines.empty())
        {
            DrawLine(textRenderer, hintLines.front(), mConfig.x, y, hintColor, mConfig.hintScale);
        }
    }

    textRenderer.EndBatch();
}

// ------------------------------------------------------------
// Function: LoadConfig
// Purpose:
//   Parse JSON presentation values into ObjectiveTrackerConfig.
// Why:
//   Keeping these values in data makes HUD spacing tunable for new languages
//   and screen ratios without modifying OverworldState.
// ------------------------------------------------------------
bool ObjectiveTrackerRenderer::LoadConfig(const std::string& configPath)
{
    std::string src;
    const std::filesystem::path path = ResolveReadablePath(configPath);
    if (!ReadTextFile(path, src))
    {
        LOG("[ObjectiveTrackerRenderer] WARNING: Missing '%s'; using defaults.",
            configPath.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, path.string());
    mConfig.x = ReadFloat(src, "x", mConfig.x);
    mConfig.y = ReadFloat(src, "y", mConfig.y);
    mConfig.maxWidth = ReadFloat(src, "maxWidth", mConfig.maxWidth);
    mConfig.rightReserveWidth = ReadFloat(src, "rightReserveWidth", mConfig.rightReserveWidth);
    mConfig.minWidth = ReadFloat(src, "minWidth", mConfig.minWidth);
    mConfig.titleScale = ReadFloat(src, "titleScale", mConfig.titleScale);
    mConfig.bodyScale = ReadFloat(src, "bodyScale", mConfig.bodyScale);
    mConfig.hintScale = ReadFloat(src, "hintScale", mConfig.hintScale);
    mConfig.titleLineHeight = ReadFloat(src, "titleLineHeight", mConfig.titleLineHeight);
    mConfig.bodyLineHeight = ReadFloat(src, "bodyLineHeight", mConfig.bodyLineHeight);
    mConfig.titleBodyGap = ReadFloat(src, "titleBodyGap", mConfig.titleBodyGap);
    mConfig.bodyHintGap = ReadFloat(src, "bodyHintGap", mConfig.bodyHintGap);
    mConfig.shadowOffset = ReadFloat(src, "shadowOffset", mConfig.shadowOffset);
    mConfig.bodyMaxLines = std::max(1, ReadInt(src, "bodyMaxLines", mConfig.bodyMaxLines));

    mConfig.titleR = ReadFloat(src, "titleR", mConfig.titleR);
    mConfig.titleG = ReadFloat(src, "titleG", mConfig.titleG);
    mConfig.titleB = ReadFloat(src, "titleB", mConfig.titleB);
    mConfig.titleA = ReadFloat(src, "titleA", mConfig.titleA);

    mConfig.bodyR = ReadFloat(src, "bodyR", mConfig.bodyR);
    mConfig.bodyG = ReadFloat(src, "bodyG", mConfig.bodyG);
    mConfig.bodyB = ReadFloat(src, "bodyB", mConfig.bodyB);
    mConfig.bodyA = ReadFloat(src, "bodyA", mConfig.bodyA);

    mConfig.hintR = ReadFloat(src, "hintR", mConfig.hintR);
    mConfig.hintG = ReadFloat(src, "hintG", mConfig.hintG);
    mConfig.hintB = ReadFloat(src, "hintB", mConfig.hintB);
    mConfig.hintA = ReadFloat(src, "hintA", mConfig.hintA);
    mConfig.shadowA = ReadFloat(src, "shadowA", mConfig.shadowA);
    return true;
}

// ------------------------------------------------------------
// Function: WrapText
// Purpose:
//   Split a UTF-8 objective sentence into measured word-wrapped lines.
// Why:
//   Width-based wrapping keeps Vietnamese and French text readable with the
//   actual active SpriteFont instead of guessing by byte or character count.
// ------------------------------------------------------------
std::vector<std::string> ObjectiveTrackerRenderer::WrapText(
    BattleTextRenderer& textRenderer,
    const std::string& text,
    float maxWidth,
    float scale,
    int maxLines) const
{
    std::vector<std::string> lines;
    if (text.empty() || maxLines <= 0) return lines;

    std::istringstream words(text);
    std::string word;
    std::string current;

    while (words >> word)
    {
        const std::string candidate = current.empty() ? word : current + " " + word;
        if (textRenderer.MeasureStringRaw(candidate.c_str(), scale).x <= maxWidth || current.empty())
        {
            current = candidate;
            continue;
        }

        lines.push_back(current);
        current = word;

        if (static_cast<int>(lines.size()) == maxLines)
        {
            lines.back() = FitWithEllipsis(textRenderer, lines.back(), maxWidth, scale);
            return lines;
        }
    }

    if (!current.empty())
    {
        lines.push_back(current);
    }

    if (static_cast<int>(lines.size()) > maxLines)
    {
        lines.resize(maxLines);
        lines.back() = FitWithEllipsis(textRenderer, lines.back(), maxWidth, scale);
    }

    return lines;
}

// ------------------------------------------------------------
// Function: FitWithEllipsis
// Purpose:
//   Shorten the last visible line when hidden words remain.
// Why:
//   Truncating at word boundaries avoids cutting UTF-8 byte sequences and
//   prevents corrupted localized text from reaching SpriteFont.
// ------------------------------------------------------------
std::string ObjectiveTrackerRenderer::FitWithEllipsis(
    BattleTextRenderer& textRenderer,
    const std::string& line,
    float maxWidth,
    float scale) const
{
    const std::string suffix = "...";
    std::string candidate = line;

    while (!candidate.empty())
    {
        const std::string withSuffix = candidate + suffix;
        if (textRenderer.MeasureStringRaw(withSuffix.c_str(), scale).x <= maxWidth)
        {
            return withSuffix;
        }

        const std::size_t lastSpace = candidate.find_last_of(' ');
        if (lastSpace == std::string::npos)
        {
            break;
        }
        candidate = candidate.substr(0, lastSpace);
    }

    return suffix;
}

// ------------------------------------------------------------
// Function: DrawLine
// Purpose:
//   Draw one objective line with a data-tuned shadow.
// Why:
//   The overworld background varies by biome, so shadowed text remains readable
//   without introducing a bulky panel over the map.
// ------------------------------------------------------------
void ObjectiveTrackerRenderer::DrawLine(BattleTextRenderer& textRenderer,
                                        const std::string& text,
                                        float x,
                                        float y,
                                        DirectX::XMVECTORF32 color,
                                        float scale) const
{
    DirectX::XMVECTORF32 shadow = { 0.0f, 0.0f, 0.0f, mConfig.shadowA };
    textRenderer.DrawStringRawScaled(text.c_str(),
                                     x + mConfig.shadowOffset,
                                     y + mConfig.shadowOffset,
                                     shadow,
                                     scale,
                                     false);
    textRenderer.DrawStringRawScaled(text.c_str(),
                                     x,
                                     y,
                                     color,
                                     scale,
                                     false);
}

DirectX::XMVECTORF32 ObjectiveTrackerRenderer::MakeColor(float r,
                                                        float g,
                                                        float b,
                                                        float a) const
{
    return DirectX::XMVECTORF32{ r, g, b, a };
}
