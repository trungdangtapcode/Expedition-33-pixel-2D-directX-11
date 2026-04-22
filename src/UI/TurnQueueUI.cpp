
#include "TurnQueueUI.h"
#include "../Utils/JsonLoader.h"
#include "../Utils/Log.h"
#include <WICTextureLoader.h>

using namespace DirectX;

bool TurnQueueUI::Initialize(ID3D11Device* device, ID3D11DeviceContext* context,
                             const std::string& configPath,
                             const std::wstring& bgPath, const std::wstring& framePath,
                             int screenW, int screenH)
{
    Shutdown();
    mDevice = device;
    mContext = context;
    mScreenW = screenW;
    mScreenH = screenH;

    mSpriteBatch = std::make_unique<SpriteBatch>(context);
    mStates      = std::make_unique<CommonStates>(device);
    mConfigPath  = configPath;

    if (!JsonLoader::LoadTurnViewConfig(configPath, mConfig)) {
        LOG("[TurnQueueUI] Failed to load config from %s, using defaults.", configPath.c_str());
    }

    auto loadTexture = [&](const std::wstring& path, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV) {
        if (path.empty()) return E_FAIL;
        HRESULT hr = CreateWICTextureFromFileEx(
            device, context,
            path.c_str(),
            0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
            WIC_LOADER_IGNORE_SRGB,
            nullptr, outSRV.GetAddressOf()
        );
        if (FAILED(hr)) {
            LOG("[TurnQueueUI] Failed to load texture: %ls", path.c_str());
        }
        return hr;
    };

    loadTexture(bgPath, mBgSRV);
    loadTexture(framePath, mFrameSRV);

    return true;
}

void TurnQueueUI::Shutdown()
{
    mNodes.clear();
    mFadingNodes.clear();
    mTextureCache.clear();
    mBgSRV.Reset();
    mFrameSRV.Reset();
    mSpriteBatch.reset();
    mStates.reset();
}

void TurnQueueUI::SetScreenSize(int screenW, int screenH)
{
    mScreenW = screenW;
    mScreenH = screenH;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> TurnQueueUI::GetTexture(const std::wstring& path)
{
    if (path.empty()) return nullptr;

    auto it = mTextureCache.find(path);
    if (it != mTextureCache.end()) return it->second;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = CreateWICTextureFromFileEx(
        mDevice, mContext,
        path.c_str(),
        0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        WIC_LOADER_IGNORE_SRGB,
        nullptr, srv.GetAddressOf()
    );

    if (SUCCEEDED(hr)) {
        mTextureCache[path] = srv;
        return srv;
    }

    LOG("[TurnQueueUI] WARNING - failed to load portrait: %ls", path.c_str());
    mTextureCache[path] = nullptr;
    return nullptr;
}

void TurnQueueUI::UpdateQueue(const std::vector<IBattler*>& anticipatedQueue)
{
    const int count = static_cast<int>(anticipatedQueue.size());
    std::vector<QueueNode> newNodes;
    newNodes.reserve(count);

    float layoutY = mConfig.startY;

    for (int i = 0; i < count; ++i) {
        IBattler* b = anticipatedQueue[i];
        if (!b) continue;

        QueueNode node;
        node.battler = b;
        node.portraitPath = b->GetTurnViewPath();
        
        // Size configuration: Top one is bigger
        if (i == 0) {
            node.targetScale = mConfig.topScale;
        } else {
            node.targetScale = mConfig.normalScale;
        }
        
        node.targetX = mConfig.startX;
        node.targetY = layoutY;

// The core constraint: Items in a queue should strictly slide UP (index decreases) 
        // to take the place of finished actions. An action should NEVER slide DOWN.
        int bestOldIdx = -1;
        int minDistance = 9999;

        for (int oldIdx = 0; oldIdx < (int)mNodes.size(); ++oldIdx) {
            auto& oldNode = mNodes[oldIdx];
            if (oldNode.battler == b && !oldNode.matched) {
                // oldIdx >= i means either staying in place or shifting UP the visual queue 
                if (oldIdx >= i) {
                    int dist = oldIdx - i;
                    if (dist < minDistance) {
                        minDistance = dist;
                        bestOldIdx = oldIdx;
                    }
                }
            }
        }

        bool found = false;
        if (bestOldIdx != -1) {
            auto& oldNode = mNodes[bestOldIdx];
            node.currentX = oldNode.currentX;
            node.currentY = oldNode.currentY;
            node.currentScale = oldNode.currentScale;
            oldNode.matched = true;
            found = true;
        }

        // Initial setup for completely new items coming from the bottom        
        if (!found) {
            node.currentX = mConfig.startX + mConfig.slideOffsetX;
            node.currentY = layoutY + mConfig.spawnOffsetY;
            node.currentScale = mConfig.normalScale;
        }

        node.srv = GetTexture(node.portraitPath);
        newNodes.push_back(node);

        float spacing = (i == 0) ? mConfig.topSpacing : mConfig.spacing;
        layoutY += (mConfig.height * node.targetScale) + spacing;
    }

    for (auto& oldNode : mNodes) {
        if (!oldNode.matched) {
            oldNode.targetX += mConfig.popOffX; 
            oldNode.targetScale *= mConfig.popScale; 
            mFadingNodes.push_back(oldNode);
        }
    }
    mNodes = std::move(newNodes);
}

void TurnQueueUI::Update(float dt)
{
    // Simple Hot-Reload for Config every 1 second
    mConfigReloadTimer += dt;
    if (mConfigReloadTimer >= 1.0f) {
        mConfigReloadTimer = 0.0f;
        JsonLoader::LoadTurnViewConfig(mConfigPath, mConfig);
        
        // Re-calculate target positions based on new config instantly
        float layoutY = mConfig.startY;
        for (int i = 0; i < (int)mNodes.size(); ++i) {
            auto& node = mNodes[i];
            node.targetScale = (i == 0) ? mConfig.topScale : mConfig.normalScale;
            node.targetX = mConfig.startX;
            node.targetY = layoutY;
            float spacing = (i == 0) ? mConfig.topSpacing : mConfig.spacing;
            layoutY += (mConfig.height * node.targetScale) + spacing;
        }
    }

    for (auto& node : mNodes) {
        node.currentX += (node.targetX - node.currentX) * mConfig.animSpeed * dt;
        node.currentY += (node.targetY - node.currentY) * mConfig.animSpeed * dt;
        node.currentScale += (node.targetScale - node.currentScale) * mConfig.animSpeed * dt;
    }

    // Update Fading Nodes
    for (auto it = mFadingNodes.begin(); it != mFadingNodes.end(); ) {
        it->currentX += (it->targetX - it->currentX) * mConfig.animSpeed * dt;
        it->currentY += (it->targetY - it->currentY) * mConfig.animSpeed * dt;
        it->currentScale += (it->targetScale - it->currentScale) * mConfig.animSpeed * dt;
        it->alpha -= mConfig.fadeSpeed * dt; 
        
        if (it->alpha <= 0.0f) {
            it = mFadingNodes.erase(it);
        } else {
            ++it;
        }
    }
}

void TurnQueueUI::BindViewport(ID3D11DeviceContext* context)
{
    D3D11_VIEWPORT vp{};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width    = static_cast<float>(mScreenW);
    vp.Height   = static_cast<float>(mScreenH);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);
}

void TurnQueueUI::Render(ID3D11DeviceContext* context)
{
    if (!mSpriteBatch) return;

    BindViewport(context);

    mSpriteBatch->Begin(
        SpriteSortMode_Deferred,
        mStates->NonPremultiplied(),
        mStates->PointClamp(),
        mStates->DepthNone(),
        mStates->CullNone()
    );

    // Draw bottom-up: index 0 (next to act) must be drawn last
    for (int i = static_cast<int>(mNodes.size()) - 1; i >= 0; --i)
    {
        const auto& node = mNodes[i];

        RECT destRect;
        destRect.left   = static_cast<LONG>(node.currentX);
        destRect.top    = static_cast<LONG>(node.currentY);
        destRect.right  = static_cast<LONG>(node.currentX + mConfig.width * node.currentScale);
        destRect.bottom = static_cast<LONG>(node.currentY + mConfig.height * node.currentScale);

        XMVECTOR color = Colors::White;

        if (mBgSRV) {
            mSpriteBatch->Draw(mBgSRV.Get(), destRect, color);
        }

        if (node.srv) {
            mSpriteBatch->Draw(node.srv.Get(), destRect, color);
        }

        if (mFrameSRV) {
            mSpriteBatch->Draw(mFrameSRV.Get(), destRect, color);
        }
    }

    // Render fading out nodes on top
    for (const auto& node : mFadingNodes)
    {
        RECT destRect;
        destRect.left   = static_cast<LONG>(node.currentX);
        destRect.top    = static_cast<LONG>(node.currentY);
        destRect.right  = static_cast<LONG>(node.currentX + mConfig.width * node.currentScale);
        destRect.bottom = static_cast<LONG>(node.currentY + mConfig.height * node.currentScale);

        XMVECTOR color = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, node.alpha);

        if (mBgSRV)   mSpriteBatch->Draw(mBgSRV.Get(), destRect, color);
        if (node.srv) mSpriteBatch->Draw(node.srv.Get(), destRect, color);
        if (mFrameSRV) mSpriteBatch->Draw(mFrameSRV.Get(), destRect, color);
    }
    
    mSpriteBatch->End();
}

