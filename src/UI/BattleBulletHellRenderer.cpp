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

    // Dynamically check and load replacement texture without blocking rendering logic!
    if (payload.bulletTexturePath != mLoadedBulletTexturePath) {
        mLoadedBulletTexturePath = payload.bulletTexturePath;
        mDynamicBulletTex.Reset();
        mDynamicBulletRadiusNorm = 1.0f; // Reset scalar

        if (!mLoadedBulletTexturePath.empty()) {
            std::wstring wpath(mLoadedBulletTexturePath.begin(), mLoadedBulletTexturePath.end());
            HRESULT hr = DirectX::CreateWICTextureFromFile(mDevice.Get(), mContext.Get(), wpath.c_str(), nullptr, mDynamicBulletTex.ReleaseAndGetAddressOf());
            
            if (SUCCEEDED(hr) && mDynamicBulletTex) {
                // Calculate scaling normalization metadata strictly once!
                Microsoft::WRL::ComPtr<ID3D11Resource> res;
                mDynamicBulletTex->GetResource(&res);
                Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2d;
                res.As(&tex2d);
                if (tex2d) {
                    D3D11_TEXTURE2D_DESC desc;
                    tex2d->GetDesc(&desc);
                    mDynamicBulletRadiusNorm = (float)desc.Width / 2.0f; 
                }
            } else {
                LOG("[BattleBulletHellRenderer] FAILED to securely load custom Bullet texture: %s", mLoadedBulletTexturePath.c_str());
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

        // Draw Dynamic Bullets
        if (mDynamicBulletTex) {
            for (const auto& b : mLastPayload.bullets) {
                 DirectX::XMFLOAT2 bPos(b.x, b.y);
                 DirectX::XMFLOAT2 pivot(mDynamicBulletRadiusNorm, mDynamicBulletRadiusNorm);
                 float scale = b.radius / mDynamicBulletRadiusNorm;
                 mSpriteBatch->Draw(mDynamicBulletTex.Get(), bPos, nullptr, DirectX::Colors::White, b.angle, pivot, scale); 
            }
        }
        
        mSpriteBatch->End();
        
        // Final fallback bullet catch
        if (!mDynamicBulletTex) {
            for (const auto& b : mLastPayload.bullets) {
                 mCircleRenderer.Draw(context, b.x, b.y, b.radius, 1.0f, 1.0f, 1.0f, screenW, screenH);
            }
        }
    }
}
