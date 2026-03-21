// ============================================================
// File: TurnQueueUI.cpp
// ============================================================
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

    JsonLoader::TurnViewConfig config;
    if (JsonLoader::LoadTurnViewConfig(configPath, config)) {
        mNodeWidth = config.width;
        mNodeHeight = config.height;
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

    mStartX = 10.0f; // Padding from left
    mStartY = 10.0f; // Padding from top

    return true;
}

void TurnQueueUI::Shutdown()
{
    mNodes.clear();
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

    LOG("[TurnQueueUI] WARNING � failed to load portrait: %ls", path.c_str());
    mTextureCache[path] = nullptr;
    return nullptr;
}

void TurnQueueUI::UpdateQueue(const std::vector<IBattler*>& anticipatedQueue)
{
    const int count = static_cast<int>(anticipatedQueue.size());

    // We keep existing currentY for animations if matches, but for simplicity here we just rebuild logic, 
    // or we can reuse structs to tween smoothly.
    
    // Smooth update
    std::vector<QueueNode> newNodes;
    newNodes.reserve(count);

    float layoutY = mStartY;

    for (int i = 0; i < count; ++i) {
        IBattler* b = anticipatedQueue[i];
        if (!b) continue;

        QueueNode node;
        node.portraitPath = b->GetTurnViewPath();
        node.targetY = layoutY;
        
        // Find if this path was roughly in previous so we can continue animation
        // For a more advanced logic we could check object pointer mapping. Here we just snap or animate.
        node.currentY = layoutY + 20.0f; // start slightly offset for pop-in, but let's just match for MVP
        
        // Let's try to find an existing node with same portrait to tween from
        if (i < mNodes.size() && mNodes[i].portraitPath == node.portraitPath) {
            node.currentY = mNodes[i].currentY; // continue tweening
        } else {
            node.currentY = node.targetY; // hard snap on change
        }

        node.srv = GetTexture(node.portraitPath);
        newNodes.push_back(node);

        float renderScale = 0.6f;
        layoutY += (mNodeHeight * renderScale) + mSpacing;
    }

    mNodes = std::move(newNodes);
}

void TurnQueueUI::Update(float dt)
{
    const float kLerpSpeed = 10.0f;
    for (auto& node : mNodes) {
        node.currentY += (node.targetY - node.currentY) * kLerpSpeed * dt;
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

    float scale = 0.6f; // Adjust scale for UI so it doesn't take up entire screen
    
    // Draw bottom-up ordering logic: index 0 (next to act) must be drawn last so it appears on top conceptually if they overlap.
    // Index 0 has the smallest Y coordinate (highest on screen).
    for (int i = static_cast<int>(mNodes.size()) - 1; i >= 0; --i)
    {
        const auto& node = mNodes[i];
        
        XMFLOAT2 pos(mStartX, node.currentY);
        
        // Base sizes
        RECT destRect;
        destRect.left   = static_cast<LONG>(pos.x);
        destRect.top    = static_cast<LONG>(pos.y);
        destRect.right  = static_cast<LONG>(pos.x + mNodeWidth * scale);
        destRect.bottom = static_cast<LONG>(pos.y + mNodeHeight * scale);

        XMVECTOR color = Colors::White;

        // Background
        if (mBgSRV) {
            mSpriteBatch->Draw(mBgSRV.Get(), destRect, color);
        }

        // Portrait
        if (node.srv) {
            mSpriteBatch->Draw(node.srv.Get(), destRect, color);
        }

        // Frame
        if (mFrameSRV) {
            mSpriteBatch->Draw(mFrameSRV.Get(), destRect, color);
        }
    }

    mSpriteBatch->End();
}
