// ============================================================
// File: BattleAmbientParticleRenderer.cpp
// Responsibility: Implement layered battle ambient particle rendering.
//
// Rendering pipeline:
//   1. BattleState draws the environment background.
//   2. Render(Back) draws small low-alpha particles behind combatants.
//   3. BattleRenderer draws all combatant sprites.
//   4. Render(Front) draws larger brighter particles in front of combatants.
//   5. BattleState draws the authored environment foreground and UI.
//
// Common mistakes:
//   1. Updating particle positions per frame with unbounded mutation -> drift
//      changes after save/load or pause. This renderer derives positions from
//      elapsed time and stable seeds instead.
//   2. Using screen-space coordinates -> particles ignore battle camera motion.
//   3. Rebuilding particles every frame -> unnecessary allocations.
// ============================================================
#define NOMINMAX
#include "BattleAmbientParticleRenderer.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <DirectXColors.h>
#include <WICTextureLoader.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace
{
    constexpr float kTwoPi = 6.28318530718f;
    constexpr float kTexturePadding = 96.0f;

    // ------------------------------------------------------------
    // Function: ReadTextFile
    // Purpose:
    //   Load a tiny JSON config into memory.
    // Why:
    //   The local JSON helpers operate on whole strings and the particle
    //   config is intentionally shallow.
    // ------------------------------------------------------------
    bool ReadTextFile(const std::string& path, std::string& out)
    {
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::ostringstream buffer;
        buffer << file.rdbuf();
        out = buffer.str();
        return true;
    }

    // ------------------------------------------------------------
    // Function: ReadJsonString
    // Purpose:
    //   Read an optional top-level JSON string.
    // Why:
    //   Missing artist-authored fields should fall back gracefully during
    //   iteration instead of disabling the whole renderer.
    // ------------------------------------------------------------
    std::string ReadJsonString(const std::string& src,
                               const std::string& key,
                               const std::string& fallback)
    {
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        if (raw.empty()) return fallback;
        return JsonLoader::detail::CleanString(raw);
    }

    // ------------------------------------------------------------
    // Function: ReadJsonFloat
    // Purpose:
    //   Read an optional top-level JSON float.
    // Why:
    //   Particle motion and depth values are authored data, not code tuning.
    // ------------------------------------------------------------
    float ReadJsonFloat(const std::string& src,
                        const std::string& key,
                        float fallback)
    {
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        return JsonLoader::detail::ParseFloat(raw, fallback);
    }

    // ------------------------------------------------------------
    // Function: ReadJsonInt
    // Purpose:
    //   Read an optional top-level JSON integer.
    // Why:
    //   Layer density should be adjustable without recompiling C++.
    // ------------------------------------------------------------
    int ReadJsonInt(const std::string& src,
                    const std::string& key,
                    int fallback)
    {
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        return JsonLoader::detail::ParseInt(raw, fallback);
    }

    // ------------------------------------------------------------
    // Function: ReadJsonBool
    // Purpose:
    //   Read an optional top-level JSON boolean.
    // Why:
    //   Designers can disable the effect quickly while testing battle
    //   readability or performance.
    // ------------------------------------------------------------
    bool ReadJsonBool(const std::string& src,
                      const std::string& key,
                      bool fallback)
    {
        const std::string raw = JsonLoader::detail::ValueOf(src, key);
        return JsonLoader::detail::ParseBool(raw, fallback);
    }
}

// ------------------------------------------------------------
// Function: Initialize
// Purpose:
//   Load config data, upload the particle texture, and create SpriteBatch.
// Why:
//   BattleState owns this renderer for one battle session, so all GPU
//   resources are created once when the state enters.
// Parameters:
//   device/context - D3D11 objects used for texture creation and drawing.
//   configPath     - Shallow JSON file with motion and layer tuning.
//   screenW/H      - Current back-buffer size for SpriteBatch viewport setup.
// ------------------------------------------------------------
bool BattleAmbientParticleRenderer::Initialize(ID3D11Device* device,
                                               ID3D11DeviceContext* context,
                                               const std::string& configPath,
                                               int screenW,
                                               int screenH)
{
    Shutdown();

    if (!device || !context)
    {
        LOG("%s", "[BattleAmbientParticleRenderer] Initialize failed: missing D3D device or context.");
        return false;
    }

    mScreenW = screenW;
    mScreenH = screenH;
    mElapsed = 0.0f;
    mConfig = Config{};

    if (!LoadConfig(configPath))
    {
        Shutdown();
        return false;
    }

    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    mStates = std::make_unique<DirectX::CommonStates>(device);

    if (!mConfig.enabled)
    {
        mInitialized = true;
        LOG("[BattleAmbientParticleRenderer] Config '%s' is disabled.", configPath.c_str());
        return true;
    }

    if (!LoadTexture(device, context))
    {
        Shutdown();
        return false;
    }

    RebuildParticles();
    mInitialized = true;

    LOG("[BattleAmbientParticleRenderer] Initialized from '%s'.", configPath.c_str());
    return true;
}

// ------------------------------------------------------------
// Function: LoadConfig
// Purpose:
//   Parse the particle tuning JSON into mConfig.
// Why:
//   Layer density, world bounds, speed, scale, and alpha are content
//   decisions and must remain data-driven.
// ------------------------------------------------------------
bool BattleAmbientParticleRenderer::LoadConfig(const std::string& configPath)
{
    std::string src;
    if (!ReadTextFile(configPath, src))
    {
        LOG("[BattleAmbientParticleRenderer] Config '%s' missing.",
            configPath.c_str());
        return false;
    }

    JsonLoader::detail::WarnIfUTF16(src, configPath);

    mConfig.enabled = ReadJsonBool(src, "enabled", mConfig.enabled);
    mConfig.texturePath = ReadJsonString(src, "texturePath", mConfig.texturePath);
    mConfig.backCount = (std::max)(0, ReadJsonInt(src, "backCount", mConfig.backCount));
    mConfig.frontCount = (std::max)(0, ReadJsonInt(src, "frontCount", mConfig.frontCount));
    mConfig.worldLeft = ReadJsonFloat(src, "worldLeft", mConfig.worldLeft);
    mConfig.worldRight = ReadJsonFloat(src, "worldRight", mConfig.worldRight);
    mConfig.worldTop = ReadJsonFloat(src, "worldTop", mConfig.worldTop);
    mConfig.worldBottom = ReadJsonFloat(src, "worldBottom", mConfig.worldBottom);
    mConfig.minFallSpeed = ReadJsonFloat(src, "minFallSpeed", mConfig.minFallSpeed);
    mConfig.maxFallSpeed = ReadJsonFloat(src, "maxFallSpeed", mConfig.maxFallSpeed);
    mConfig.minDriftSpeed = ReadJsonFloat(src, "minDriftSpeed", mConfig.minDriftSpeed);
    mConfig.maxDriftSpeed = ReadJsonFloat(src, "maxDriftSpeed", mConfig.maxDriftSpeed);
    mConfig.minSway = ReadJsonFloat(src, "minSway", mConfig.minSway);
    mConfig.maxSway = ReadJsonFloat(src, "maxSway", mConfig.maxSway);
    mConfig.backMinScale = ReadJsonFloat(src, "backMinScale", mConfig.backMinScale);
    mConfig.backMaxScale = ReadJsonFloat(src, "backMaxScale", mConfig.backMaxScale);
    mConfig.frontMinScale = ReadJsonFloat(src, "frontMinScale", mConfig.frontMinScale);
    mConfig.frontMaxScale = ReadJsonFloat(src, "frontMaxScale", mConfig.frontMaxScale);
    mConfig.backAlpha = Clamp01(ReadJsonFloat(src, "backAlpha", mConfig.backAlpha));
    mConfig.frontAlpha = Clamp01(ReadJsonFloat(src, "frontAlpha", mConfig.frontAlpha));
    mConfig.windX = ReadJsonFloat(src, "windX", mConfig.windX);
    mConfig.rotationSpeed = ReadJsonFloat(src, "rotationSpeed", mConfig.rotationSpeed);

    if (mConfig.worldRight <= mConfig.worldLeft)
    {
        LOG("%s", "[BattleAmbientParticleRenderer] Invalid world bounds in config.");
        mConfig.worldRight = mConfig.worldLeft + 1.0f;
    }
    if (mConfig.worldBottom <= mConfig.worldTop)
    {
        LOG("%s", "[BattleAmbientParticleRenderer] Invalid vertical bounds in config.");
        mConfig.worldBottom = mConfig.worldTop + 1.0f;
    }
    if (mConfig.maxFallSpeed < mConfig.minFallSpeed)
    {
        std::swap(mConfig.minFallSpeed, mConfig.maxFallSpeed);
    }
    if (mConfig.maxDriftSpeed < mConfig.minDriftSpeed)
    {
        std::swap(mConfig.minDriftSpeed, mConfig.maxDriftSpeed);
    }
    if (mConfig.maxSway < mConfig.minSway)
    {
        std::swap(mConfig.minSway, mConfig.maxSway);
    }
    if (mConfig.backMaxScale < mConfig.backMinScale)
    {
        std::swap(mConfig.backMinScale, mConfig.backMaxScale);
    }
    if (mConfig.frontMaxScale < mConfig.frontMinScale)
    {
        std::swap(mConfig.frontMinScale, mConfig.frontMaxScale);
    }

    return true;
}

// ------------------------------------------------------------
// Function: LoadTexture
// Purpose:
//   Upload the particle texture and cache its dimensions.
// Why:
//   SpriteBatch rotation needs a center origin measured in source pixels.
// ------------------------------------------------------------
bool BattleAmbientParticleRenderer::LoadTexture(ID3D11Device* device,
                                                ID3D11DeviceContext* context)
{
    ComPtr<ID3D11Resource> resource;
    const std::wstring path = ToWidePath(mConfig.texturePath);

    if (mConfig.texturePath.empty())
    {
        LOG("%s", "[BattleAmbientParticleRenderer] Config is enabled but texturePath is empty.");
        return false;
    }

    const HRESULT hr = DirectX::CreateWICTextureFromFileEx(
        device,
        context,
        path.c_str(),
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0,
        0,
        DirectX::WIC_LOADER_IGNORE_SRGB,
        resource.GetAddressOf(),
        mParticleSRV.GetAddressOf());

    if (FAILED(hr))
    {
        LOG("[BattleAmbientParticleRenderer] Failed to load texture '%ls' (0x%08X).",
            path.c_str(), static_cast<unsigned>(hr));
        return false;
    }

    ComPtr<ID3D11Texture2D> texture;
    if (SUCCEEDED(resource.As(&texture)) && texture)
    {
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        mTextureW = static_cast<int>(desc.Width);
        mTextureH = static_cast<int>(desc.Height);
    }

    return true;
}

// ------------------------------------------------------------
// Function: RebuildParticles
// Purpose:
//   Recreate deterministic particle descriptors for both depth layers.
// Why:
//   The render loop should only draw existing particles, not allocate or
//   reseed them every frame.
// ------------------------------------------------------------
void BattleAmbientParticleRenderer::RebuildParticles()
{
    BuildLayer(mBackParticles,
               mConfig.backCount,
               mConfig.backMinScale,
               mConfig.backMaxScale,
               11.0f);

    BuildLayer(mFrontParticles,
               mConfig.frontCount,
               mConfig.frontMinScale,
               mConfig.frontMaxScale,
               47.0f);
}

// ------------------------------------------------------------
// Function: BuildLayer
// Purpose:
//   Fill a particle layer with stable pseudo-random motion values.
// Why:
//   Depth should come from authored layer ranges while individual leaves
//   still feel varied.
// ------------------------------------------------------------
void BattleAmbientParticleRenderer::BuildLayer(std::vector<Particle>& out,
                                               int count,
                                               float minScale,
                                               float maxScale,
                                               float salt) const
{
    out.clear();
    out.reserve(static_cast<size_t>((std::max)(0, count)));

    for (int i = 0; i < count; ++i)
    {
        Particle particle{};
        particle.x01 = Hash01(i, salt + 0.13f);
        particle.y01 = Hash01(i, salt + 0.29f);
        particle.depth = Hash01(i, salt + 0.47f);
        particle.phase = Hash01(i, salt + 0.71f) * kTwoPi;
        particle.fallSpeed = Lerp(mConfig.minFallSpeed, mConfig.maxFallSpeed, Hash01(i, salt + 1.03f));
        particle.driftSpeed = Lerp(mConfig.minDriftSpeed, mConfig.maxDriftSpeed, Hash01(i, salt + 1.31f));
        particle.sway = Lerp(mConfig.minSway, mConfig.maxSway, Hash01(i, salt + 1.73f));
        particle.scale = Lerp(minScale, maxScale, Hash01(i, salt + 2.11f));
        particle.rotationSpeed = Lerp(-mConfig.rotationSpeed, mConfig.rotationSpeed, Hash01(i, salt + 2.41f));
        out.push_back(particle);
    }
}

// ------------------------------------------------------------
// Function: SetScreenSize
// Purpose:
//   Store the latest render-target size for viewport binding.
// Why:
//   BattleState can survive window resizing, and SpriteBatch's projection
//   must match the active back buffer dimensions.
// ------------------------------------------------------------
void BattleAmbientParticleRenderer::SetScreenSize(int screenW, int screenH)
{
    mScreenW = screenW;
    mScreenH = screenH;
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Advance the shared particle clock by delta time.
// Why:
//   All motion must be frame-rate independent and pause-friendly.
// ------------------------------------------------------------
void BattleAmbientParticleRenderer::Update(float dt)
{
    if (!mInitialized || !mConfig.enabled) return;
    if (dt <= 0.0f) return;

    mElapsed += dt;
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Draw one particle depth layer in world space.
// Why:
//   BattleState controls the draw order, so back leaves appear behind
//   characters and front leaves pass over them.
// ------------------------------------------------------------
void BattleAmbientParticleRenderer::Render(ID3D11DeviceContext* context,
                                           const Camera2D& camera,
                                           BattleAmbientParticleLayer layer)
{
    if (!mInitialized || !mConfig.enabled || !context) return;
    if (!mSpriteBatch || !mStates || !mParticleSRV) return;

    const std::vector<Particle>& particles =
        (layer == BattleAmbientParticleLayer::Back) ? mBackParticles : mFrontParticles;
    if (particles.empty()) return;

    const float layerAlpha =
        (layer == BattleAmbientParticleLayer::Back) ? mConfig.backAlpha : mConfig.frontAlpha;

    BindViewport(context, camera);

    mSpriteBatch->Begin(
        DirectX::SpriteSortMode_Deferred,
        mStates->NonPremultiplied(),
        mStates->LinearClamp(),
        mStates->DepthNone(),
        nullptr,
        nullptr,
        camera.GetViewMatrix());

    for (const Particle& particle : particles)
    {
        DrawParticle(particle, layerAlpha, mParticleSRV.Get());
    }

    mSpriteBatch->End();
}

// ------------------------------------------------------------
// Function: BindViewport
// Purpose:
//   Bind and forward a viewport before SpriteBatch::Begin().
// Why:
//   SpriteBatch can otherwise query an empty rasterizer viewport after a
//   previous renderer resets pipeline state.
// ------------------------------------------------------------
void BattleAmbientParticleRenderer::BindViewport(ID3D11DeviceContext* context,
                                                 const Camera2D& camera)
{
    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    const int viewportW = (camera.GetScreenW() > 0) ? camera.GetScreenW() : mScreenW;
    const int viewportH = (camera.GetScreenH() > 0) ? camera.GetScreenH() : mScreenH;
    viewport.Width = static_cast<float>((std::max)(1, viewportW));
    viewport.Height = static_cast<float>((std::max)(1, viewportH));
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    context->RSSetViewports(1, &viewport);
    mSpriteBatch->SetViewport(viewport);
}

// ------------------------------------------------------------
// Function: DrawParticle
// Purpose:
//   Convert one deterministic particle seed into a world-space draw call.
// Why:
//   Position is derived from elapsed time, which gives continuous motion
//   without accumulating per-particle floating point drift.
// ------------------------------------------------------------
void BattleAmbientParticleRenderer::DrawParticle(const Particle& particle,
                                                 float layerAlpha,
                                                 ID3D11ShaderResourceView* texture)
{
    const float spanX = mConfig.worldRight - mConfig.worldLeft;
    const float spanY = mConfig.worldBottom - mConfig.worldTop;

    const float baseX = mConfig.worldLeft + particle.x01 * spanX;
    const float baseY = particle.y01 * spanY;
    const float fallY = Wrap(baseY + mElapsed * particle.fallSpeed, 0.0f, spanY);
    const float swayX = std::sin(mElapsed * particle.driftSpeed + particle.phase) * particle.sway;
    const float windX = mElapsed * mConfig.windX * (0.45f + particle.depth * 0.75f);
    const float worldX = Wrap(baseX + swayX + windX,
                              mConfig.worldLeft - kTexturePadding,
                              mConfig.worldRight + kTexturePadding);
    const float worldY = mConfig.worldTop + fallY;
    const float rotation = particle.phase + mElapsed * particle.rotationSpeed;
    const float alpha = Clamp01(layerAlpha * (0.60f + particle.depth * 0.40f));

    const DirectX::XMVECTOR color =
        DirectX::XMVectorSet(0.88f, 0.78f + particle.depth * 0.10f, 0.48f, alpha);
    const DirectX::XMFLOAT2 origin(
        static_cast<float>(mTextureW) * 0.5f,
        static_cast<float>(mTextureH) * 0.5f);

    mSpriteBatch->Draw(texture,
                       DirectX::XMFLOAT2(worldX, worldY),
                       nullptr,
                       color,
                       rotation,
                       origin,
                       particle.scale);
}

// ------------------------------------------------------------
// Function: Shutdown
// Purpose:
//   Release all GPU and CPU-side particle resources.
// Why:
//   BattleState can be pushed repeatedly, so explicit cleanup prevents
//   stale resources from surviving across battle sessions.
// ------------------------------------------------------------
void BattleAmbientParticleRenderer::Shutdown()
{
    mBackParticles.clear();
    mFrontParticles.clear();
    mParticleSRV.Reset();
    mSpriteBatch.reset();
    mStates.reset();
    mInitialized = false;
    mElapsed = 0.0f;
}

float BattleAmbientParticleRenderer::Clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

float BattleAmbientParticleRenderer::Hash01(int index, float salt)
{
    const float n = std::sin((static_cast<float>(index) * 12.9898f + salt) * 78.233f) * 43758.5453f;
    return n - std::floor(n);
}

float BattleAmbientParticleRenderer::Lerp(float a, float b, float t)
{
    return a + (b - a) * Clamp01(t);
}

float BattleAmbientParticleRenderer::Wrap(float value, float minValue, float maxValue)
{
    const float range = (std::max)(0.001f, maxValue - minValue);
    float wrapped = std::fmod(value - minValue, range);
    if (wrapped < 0.0f)
    {
        wrapped += range;
    }
    return minValue + wrapped;
}

std::wstring BattleAmbientParticleRenderer::ToWidePath(const std::string& path)
{
    return std::wstring(path.begin(), path.end());
}
