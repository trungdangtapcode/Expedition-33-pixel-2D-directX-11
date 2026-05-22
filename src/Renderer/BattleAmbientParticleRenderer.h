// ============================================================
// File: BattleAmbientParticleRenderer.h
// Responsibility: Render data-driven ambient world-space particles in battle.
//
// Owns:
//   SpriteBatch and CommonStates for particle draw passes.
//   ID3D11ShaderResourceView for the particle texture.
//   Deterministic particle descriptors for back and front depth layers.
//
// Lifetime:
//   Created in  -> BattleState::OnEnter()
//   Destroyed in -> BattleState::OnExit() via Shutdown()
//
// Important:
//   - This renderer never mutates battle simulation state.
//   - Render() must be called twice by the owning state: once behind
//     combatants and once in front of combatants.
//   - All tuning comes from the environment-selected particle JSON file.
//
// Common mistakes:
//   1. Passing Camera2D::GetViewProjectionMatrix() -> double projection.
//   2. Rendering both layers in one pass -> no visual depth around actors.
//   3. Forgetting SetViewport() before SpriteBatch::Begin() -> runtime throw.
// ============================================================
#pragma once

#include "Camera.h"
#include <CommonStates.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include <string>
#include <vector>

enum class BattleAmbientParticleLayer
{
    Back,
    Front
};

class BattleAmbientParticleRenderer
{
public:
    bool Initialize(ID3D11Device* device,
                    ID3D11DeviceContext* context,
                    const std::string& configPath,
                    int screenW,
                    int screenH);

    void SetScreenSize(int screenW, int screenH);
    void Update(float dt);
    void Render(ID3D11DeviceContext* context,
                const Camera2D& camera,
                BattleAmbientParticleLayer layer);
    void Shutdown();

    bool IsInitialized() const { return mInitialized; }

private:
    struct Config
    {
        bool enabled = false;
        std::string texturePath;
        int backCount = 0;
        int frontCount = 0;
        float worldLeft = 0.0f;
        float worldRight = 0.0f;
        float worldTop = 0.0f;
        float worldBottom = 0.0f;
        float minFallSpeed = 0.0f;
        float maxFallSpeed = 0.0f;
        float minDriftSpeed = 0.0f;
        float maxDriftSpeed = 0.0f;
        float minSway = 0.0f;
        float maxSway = 0.0f;
        float backMinScale = 0.0f;
        float backMaxScale = 0.0f;
        float frontMinScale = 0.0f;
        float frontMaxScale = 0.0f;
        float backAlpha = 0.0f;
        float frontAlpha = 0.0f;
        float windX = 0.0f;
        float rotationSpeed = 0.0f;
    };

    struct Particle
    {
        float x01 = 0.0f;
        float y01 = 0.0f;
        float depth = 0.0f;
        float phase = 0.0f;
        float fallSpeed = 0.0f;
        float driftSpeed = 0.0f;
        float sway = 0.0f;
        float scale = 1.0f;
        float rotationSpeed = 0.0f;
    };

    bool LoadConfig(const std::string& configPath);
    bool LoadTexture(ID3D11Device* device, ID3D11DeviceContext* context);
    void RebuildParticles();
    void BuildLayer(std::vector<Particle>& out,
                    int count,
                    float minScale,
                    float maxScale,
                    float salt) const;
    void BindViewport(ID3D11DeviceContext* context, const Camera2D& camera);
    void DrawParticle(const Particle& particle,
                      float layerAlpha,
                      ID3D11ShaderResourceView* texture);

    static float Clamp01(float value);
    static float Hash01(int index, float salt);
    static float Lerp(float a, float b, float t);
    static float Wrap(float value, float minValue, float maxValue);
    static std::wstring ToWidePath(const std::string& path);

    Config mConfig;
    int mScreenW = 1280;
    int mScreenH = 720;
    int mTextureW = 32;
    int mTextureH = 32;
    float mElapsed = 0.0f;
    bool mInitialized = false;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mParticleSRV;
    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;
    std::vector<Particle> mBackParticles;
    std::vector<Particle> mFrontParticles;
};
