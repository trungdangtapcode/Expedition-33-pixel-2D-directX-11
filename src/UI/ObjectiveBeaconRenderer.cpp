// ============================================================
// File: ObjectiveBeaconRenderer.cpp
// Responsibility: Implement data-driven overworld objective marker rendering.
// ============================================================
#define NOMINMAX
#include "ObjectiveBeaconRenderer.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <cmath>
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

    std::string ReadString(
        const std::string& src,
        const std::string& key,
        const std::string& fallback)
    {
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        if (raw.empty()) return fallback;
        return JsonLoader::detail::CleanString(raw);
    }
}

bool ObjectiveBeaconRenderer::Initialize(ID3D11Device* device,
                                         ID3D11DeviceContext* context,
                                         int screenW,
                                         int screenH)
{
    Shutdown();
    LoadConfig("data/objective_beacon.json");

    if (!mConfig.enabled)
    {
        LOG("[ObjectiveBeaconRenderer] Disabled by data.");
        return false;
    }

    const std::filesystem::path texturePath = ResolveReadablePath(mConfig.texturePath);
    mInitialized = mPointer.Initialize(
        device,
        context,
        texturePath.wstring(),
        mConfig.layoutPath,
        screenW,
        screenH);

    if (!mInitialized)
    {
        LOG("[ObjectiveBeaconRenderer] WARNING: Failed to initialize marker asset '%s'.",
            mConfig.texturePath.c_str());
    }

    return mInitialized;
}

bool ObjectiveBeaconRenderer::LoadConfig(const std::string& path)
{
    std::string src;
    const std::filesystem::path resolved = ResolveReadablePath(path);
    if (!ReadTextFile(resolved, src))
    {
        LOG("[ObjectiveBeaconRenderer] WARNING: Missing '%s'; using defaults.",
            path.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, path);
    mConfig.enabled = JsonLoader::detail::ParseBool(
        JsonLoader::detail::ValueOf(src, "enabled"),
        mConfig.enabled);
    mConfig.texturePath = ReadString(src, "texturePath", mConfig.texturePath);
    mConfig.layoutPath = ReadString(src, "layoutPath", mConfig.layoutPath);
    mConfig.hideWithinDistanceUnits = JsonLoader::detail::ParseFloat(
        JsonLoader::detail::ValueOf(src, "hideWithinDistanceUnits"),
        mConfig.hideWithinDistanceUnits);
    return true;
}

void ObjectiveBeaconRenderer::Update(float dt)
{
    if (!mInitialized) return;
    mPointer.Update(dt);
}

void ObjectiveBeaconRenderer::Render(ID3D11DeviceContext* context,
                                     const ObjectiveView& objective,
                                     float playerX,
                                     float playerY,
                                     const Camera2D& camera)
{
    if (!mInitialized || !ShouldRender(objective, playerX, playerY)) return;

    mPointer.Draw(
        context,
        objective.waypointX,
        objective.waypointY,
        camera.GetViewMatrix());
}

void ObjectiveBeaconRenderer::SetScreenSize(int screenW, int screenH)
{
    mPointer.SetScreenSize(screenW, screenH);
}

void ObjectiveBeaconRenderer::Shutdown()
{
    mPointer.Shutdown();
    mInitialized = false;
}

bool ObjectiveBeaconRenderer::ShouldRender(
    const ObjectiveView& objective,
    float playerX,
    float playerY) const
{
    if (!mConfig.enabled || !objective.active || !objective.hasWaypoint)
    {
        return false;
    }

    const float dx = objective.waypointX - playerX;
    const float dy = objective.waypointY - playerY;
    const float distanceSq = (dx * dx) + (dy * dy);
    const float hideDistance = mConfig.hideWithinDistanceUnits;
    return distanceSq > (hideDistance * hideDistance);
}
