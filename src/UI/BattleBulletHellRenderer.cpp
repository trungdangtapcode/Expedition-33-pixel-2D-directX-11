#include "BattleBulletHellRenderer.h"
#include "../Utils/Log.h"
#include <WICTextureLoader.h>
#include <cmath>

BattleBulletHellRenderer::BattleBulletHellRenderer(ID3D11Device* device, ID3D11DeviceContext* context)
    : mDevice(device), mContext(context)
{
    mCircleRenderer.Initialize(device);
    mFrameRenderer.Initialize(device, context, L"assets/UI/bullet_hell_frame.png", "assets/UI/bullet_hell_frame.json", 1280, 720);
    
    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    mStates = std::make_unique<DirectX::CommonStates>(device);

    HRESULT hr = DirectX::CreateWICTextureFromFile(device, context, L"assets/UI/bullet_hell_heart.png", nullptr, mHeartTex.ReleaseAndGetAddressOf());
    if (FAILED(hr)) LOG("[BattleBulletHellRenderer] WARNING: Missing assets/UI/bullet_hell_heart.png");
}

BattleBulletHellRenderer::~BattleBulletHellRenderer() = default;

void BattleBulletHellRenderer::UpdateState(const BulletHellPayload& payload)
{
    mLastPayload = payload;

    // Dynamically check and load replacement textures without blocking rendering logic!
    for (const std::string& path : payload.texturePaths) {
        if (path.empty()) continue;

        // Only load if not cached this frame
        if (mTextureCache.find(path) == mTextureCache.end()) {
            LoadedTex lt;
            std::wstring wpath(path.begin(), path.end());
            HRESULT hr = DirectX::CreateWICTextureFromFile(mDevice.Get(), mContext.Get(), wpath.c_str(), nullptr, lt.srv.ReleaseAndGetAddressOf());
            
            if (SUCCEEDED(hr) && lt.srv) {
                // Calculate scaling normalization metadata strictly once!
                Microsoft::WRL::ComPtr<ID3D11Resource> res;
                lt.srv->GetResource(&res);
                Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2d;
                res.As(&tex2d);
                if (tex2d) {
                    D3D11_TEXTURE2D_DESC desc;
                    tex2d->GetDesc(&desc);
                    lt.radiusNorm = (float)desc.Width / 2.0f; 
                }
                mTextureCache[path] = lt;
            } else {
                LOG("[BattleBulletHellRenderer] FAILED to securely load custom Bullet texture: %s", path.c_str());
            }
        }
    }
}

void BattleBulletHellRenderer::Render(ID3D11DeviceContext* context, int screenW, int screenH)
{
    if (!mLastPayload.isActive) return;

    // Draw Constraints Nine-Slice Scaling Box Outline using the native UI box
    float left = mLastPayload.boxCenterX - (mLastPayload.boxWidth / 2.0f);
    float top = mLastPayload.boxCenterY - (mLastPayload.boxHeight / 2.0f);
    
    mFrameRenderer.Draw(context, left, top, mLastPayload.boxWidth, mLastPayload.boxHeight);

    // Begin Particle Sprite Batch
    if (mSpriteBatch) {
        mSpriteBatch->Begin(DirectX::SpriteSortMode_Deferred, mStates->NonPremultiplied());
        
        // Draw Core Heart
        if (mHeartTex) {
            DirectX::XMFLOAT2 hPos(mLastPayload.heartX, mLastPayload.heartY);
            DirectX::XMFLOAT2 pivot(26.0f, 20.0f); // 52x40 divided by 2
            
            float alpha = 1.0f;
            if (mLastPayload.invincibilityTimer > 0.0f) {
                alpha = 0.35f + 0.65f * std::abs(std::sin(mLastPayload.invincibilityTimer * 35.0f));
            }
            DirectX::XMVECTOR heartColor = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, alpha);
            
            mSpriteBatch->Draw(mHeartTex.Get(), hPos, nullptr, heartColor, 0.0f, pivot, 0.5f);
        } else {
            // Unlikely fallback if it failed to load
            mSpriteBatch->End();
            mCircleRenderer.Draw(context, mLastPayload.heartX, mLastPayload.heartY, mLastPayload.heartRadius, 1.0f, 0.0f, 0.0f, screenW, screenH);
            mSpriteBatch->Begin(DirectX::SpriteSortMode_Deferred, mStates->NonPremultiplied());
        }

        // Keep track of bullets that need pure circle renderer
        std::vector<BulletHellPayload::Bullet> fallbackBullets;

        // Draw Dynamic Bullets
        for (const auto& b : mLastPayload.bullets) {
            if (b.textureIndex >= 0 && b.textureIndex < (int)mLastPayload.texturePaths.size()) {
                const std::string& path = mLastPayload.texturePaths[b.textureIndex];
                if (mTextureCache.find(path) != mTextureCache.end()) {
                    auto& tex = mTextureCache[path];
                    DirectX::XMFLOAT2 bPos(b.x, b.y);
                    DirectX::XMFLOAT2 pivot(tex.radiusNorm, tex.radiusNorm);
                    float scale = b.radius / tex.radiusNorm;
                    mSpriteBatch->Draw(tex.srv.Get(), bPos, nullptr, DirectX::Colors::White, b.angle, pivot, scale); 
                } else {
                    fallbackBullets.push_back(b);
                }
            } else {
                fallbackBullets.push_back(b);
            }
        }
        
        mSpriteBatch->End();
        
        // Final fallback bullet catch for ones without valid textures
        for (const auto& b : fallbackBullets) {
            mCircleRenderer.Draw(context, b.x, b.y, b.radius, 1.0f, 1.0f, 1.0f, screenW, screenH);
        }
    }
}
