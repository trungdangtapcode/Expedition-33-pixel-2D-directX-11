// ============================================================
// File: ObjectiveDirector.cpp
// Responsibility: Implement data-driven overworld objective selection.
// ============================================================
#define NOMINMAX
#include "ObjectiveDirector.h"
#include "GameProgress.h"
#include "LocalizationManager.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

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

    std::string ReadString(const std::string& src, const std::string& key)
    {
        return JsonLoader::detail::CleanString(JsonLoader::detail::ValueOf(src, key));
    }

    bool ReadBool(const std::string& src, const std::string& key, bool fallback)
    {
        return JsonLoader::detail::ParseBool(JsonLoader::detail::ValueOf(src, key), fallback);
    }

    bool HasProgressFlag(const std::string& flag)
    {
        return !flag.empty() && GameProgress::Get().HasFlag(flag);
    }

    std::string ReplaceAll(
        std::string text,
        const std::vector<std::pair<std::string, std::string>>& values)
    {
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
}

// ------------------------------------------------------------
// Function: Initialize
// Purpose:
//   Load ordered objective stages from JSON.
// Why:
//   Chapter guidance should change with saved story flags without hardcoding
//   route logic in OverworldState.
// ------------------------------------------------------------
bool ObjectiveDirector::Initialize(const std::string& path)
{
    mStages.clear();

    std::string src;
    const std::filesystem::path resolved = ResolveReadablePath(path);
    if (!ReadTextFile(resolved, src))
    {
        LOG("[ObjectiveDirector] WARNING: Could not read objective file '%s'.",
            path.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, path);
    return LoadFromSource(src, path);
}

// ------------------------------------------------------------
// Function: Resolve
// Purpose:
//   Return the first objective whose requirements match current progress.
// Why:
//   Ordered data lets designers express a linear chapter spine while leaving
//   optional fights and campfires free-form.
// ------------------------------------------------------------
ObjectiveView ObjectiveDirector::Resolve(float playerX, float playerY) const
{
    for (const ObjectiveStage& stage : mStages)
    {
        if (!RequirementsMet(stage)) continue;

        ObjectiveView view{};
        view.active = true;
        view.id = stage.id;
        view.title = LocalizationManager::Get().TextOrFallback(
            stage.titleKey,
            stage.titleFallback);
        view.body = LocalizationManager::Get().TextOrFallback(
            stage.bodyKey,
            stage.bodyFallback);
        view.targetKind = stage.targetKind;
        view.targetId = stage.targetId;
        view.waypointX = stage.waypointX;
        view.waypointY = stage.waypointY;
        view.arrivalDistanceUnits = stage.arrivalDistanceUnits > 0.0f
            ? stage.arrivalDistanceUnits
            : mArrivalDistanceUnits;
        view.hasWaypoint = stage.hasWaypoint;
        view.waypointHint = BuildWaypointHint(stage, playerX, playerY);
        return view;
    }

    return ObjectiveView{};
}

// ------------------------------------------------------------
// Function: LoadFromSource
// Purpose:
//   Parse objective stages from a flat JSON schema.
// Why:
//   The project uses a lightweight JSON loader, so objectives stay as shallow
//   objects with string arrays instead of nested quest graphs.
// ------------------------------------------------------------
bool ObjectiveDirector::LoadFromSource(const std::string& src, const std::string& path)
{
    mDistanceUnitsPerMeter = JsonLoader::detail::ParseFloat(
        JsonLoader::detail::ValueOf(src, "distanceUnitsPerMeter"),
        64.0f);
    mArrivalDistanceUnits = JsonLoader::detail::ParseFloat(
        JsonLoader::detail::ValueOf(src, "arrivalDistanceUnits"),
        96.0f);

    const std::vector<std::string> objects =
        JsonLoader::detail::ExtractObjectsFromArray(src, "objectives");

    for (const std::string& objectSrc : objects)
    {
        ObjectiveStage stage{};
        stage.id = ReadString(objectSrc, "id");
        stage.titleKey = ReadString(objectSrc, "titleKey");
        stage.titleFallback = ReadString(objectSrc, "title");
        stage.bodyKey = ReadString(objectSrc, "bodyKey");
        stage.bodyFallback = ReadString(objectSrc, "body");
        stage.waypointLabelKey = ReadString(objectSrc, "waypointLabelKey");
        stage.waypointLabelFallback = ReadString(objectSrc, "waypointLabel");
        stage.arrivalHintKey = ReadString(objectSrc, "arrivalHintKey");
        stage.arrivalHintFallback = ReadString(objectSrc, "arrivalHint");
        stage.targetKind = ReadString(objectSrc, "targetKind");
        stage.targetId = ReadString(objectSrc, "targetId");
        stage.requiresFlags =
            JsonLoader::detail::ExtractStringArray(objectSrc, "requiresFlags");
        stage.blockedByFlags =
            JsonLoader::detail::ExtractStringArray(objectSrc, "blockedByFlags");
        stage.hasWaypoint = ReadBool(objectSrc, "hasWaypoint", false);
        stage.waypointX = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "waypointX"),
            0.0f);
        stage.waypointY = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "waypointY"),
            0.0f);
        stage.arrivalDistanceUnits = JsonLoader::detail::ParseFloat(
            JsonLoader::detail::ValueOf(objectSrc, "arrivalDistanceUnits"),
            0.0f);

        if (stage.id.empty() || stage.bodyKey.empty())
        {
            LOG("[ObjectiveDirector] WARNING: Skipping invalid objective in '%s'.",
                path.c_str());
            continue;
        }

        mStages.push_back(std::move(stage));
    }

    LOG("[ObjectiveDirector] Loaded %zu objective stage(s).", mStages.size());
    return !mStages.empty();
}

// ------------------------------------------------------------
// Function: RequirementsMet
// Purpose:
//   Check required and blocked progress flags for one objective stage.
// Why:
//   Objectives are derived from the same durable flags as save/load and
//   StoryDirector, avoiding duplicate runtime quest state.
// ------------------------------------------------------------
bool ObjectiveDirector::RequirementsMet(const ObjectiveStage& stage) const
{
    for (const std::string& flag : stage.requiresFlags)
    {
        if (!HasProgressFlag(flag)) return false;
    }

    for (const std::string& flag : stage.blockedByFlags)
    {
        if (HasProgressFlag(flag)) return false;
    }

    return true;
}

// ------------------------------------------------------------
// Function: BuildWaypointHint
// Purpose:
//   Convert one objective waypoint into a compact distance and direction hint.
// Why:
//   The overworld map is large; players need navigational intent without a
//   full minimap or intrusive quest marker system.
// ------------------------------------------------------------
std::string ObjectiveDirector::BuildWaypointHint(
    const ObjectiveStage& stage,
    float playerX,
    float playerY) const
{
    if (!stage.hasWaypoint) return "";

    const float dx = stage.waypointX - playerX;
    const float dy = stage.waypointY - playerY;
    const float distanceUnits = std::sqrt((dx * dx) + (dy * dy));
    const float scale = mDistanceUnitsPerMeter > 0.0f
        ? mDistanceUnitsPerMeter
        : 64.0f;
    int distance = static_cast<int>(std::round(distanceUnits / scale));
    if (distance < 0) distance = 0;

    const std::string label = LocalizationManager::Get().TextOrFallback(
        stage.waypointLabelKey,
        stage.waypointLabelFallback);

    const float arrivalDistanceUnits = stage.arrivalDistanceUnits > 0.0f
        ? stage.arrivalDistanceUnits
        : mArrivalDistanceUnits;
    if (distanceUnits <= arrivalDistanceUnits)
    {
        const std::string hint = stage.arrivalHintKey.empty()
            ? LocalizationManager::Get().TextOrFallback(
                  "objective.arrival.reached",
                  stage.arrivalHintFallback)
            : LocalizationManager::Get().TextOrFallback(
                  stage.arrivalHintKey,
                  stage.arrivalHintFallback);
        return ReplaceAll(hint, { { "label", label } });
    }

    const std::string direction =
        LocalizationManager::Get().Text(ResolveDirectionKey(dx, dy));

    return LocalizationManager::Get().Format(
        "objective.distance_hint",
        {
            { "label", label },
            { "distance", std::to_string(distance) },
            { "direction", direction }
        });
}

// ------------------------------------------------------------
// Function: ResolveDirectionKey
// Purpose:
//   Convert a waypoint vector into an eight-way localization key.
// Why:
//   Returning keys keeps displayed compass words in localization data while
//   keeping the navigation math language-neutral.
// ------------------------------------------------------------
std::string ObjectiveDirector::ResolveDirectionKey(float dx, float dy) const
{
    const float absX = std::fabs(dx);
    const float absY = std::fabs(dy);

    if (absX < 1.0f && absY < 1.0f)
    {
        return "objective.direction.here";
    }

    if (absX > absY * 2.0f)
    {
        return dx >= 0.0f ? "objective.direction.east" : "objective.direction.west";
    }

    if (absY > absX * 2.0f)
    {
        return dy >= 0.0f ? "objective.direction.south" : "objective.direction.north";
    }

    if (dx >= 0.0f && dy < 0.0f) return "objective.direction.northeast";
    if (dx < 0.0f && dy < 0.0f) return "objective.direction.northwest";
    if (dx >= 0.0f && dy >= 0.0f) return "objective.direction.southeast";
    return "objective.direction.southwest";
}
