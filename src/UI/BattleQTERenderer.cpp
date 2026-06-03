// ============================================================
// File: BattleQTERenderer.cpp
// ============================================================
#include "BattleQTERenderer.h"
#include "../Utils/Log.h"
#include "../Events/EventManager.h"
#include <WICTextureLoader.h>
#include <cmath>

#undef min
#undef max
#include <algorithm>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

bool BattleQTERenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int screenW, int screenH)
{
    mScreenW = screenW;
    mScreenH = screenH;

    HRESULT hr = CreateWICTextureFromFileEx(
        device, context,
        L"assets/UI/number-letter-frame.png",
        0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        WIC_LOADER_IGNORE_SRGB, nullptr, mFrameSRV.GetAddressOf()
    );
    if (FAILED(hr)) {
        LOG("[BattleQTERenderer] Failed to load frame texture (0x%08X)", (unsigned)hr);
        return false;
    }

    // Create 1x1 white texture for drawing the clockwise filling diamond lines
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    const UINT32 white = 0xFFFFFFFF;
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = &white; initData.SysMemPitch = sizeof(UINT32);

    ComPtr<ID3D11Texture2D> tex;
    hr = device->CreateTexture2D(&td, &initData, tex.GetAddressOf());
    if (FAILED(hr)) return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    hr = device->CreateShaderResourceView(tex.Get(), &srvd, mWhiteFillSRV.GetAddressOf());
    if (FAILED(hr)) return false;

    mSpriteBatch = std::make_unique<SpriteBatch>(context);
    mStates = std::make_unique<CommonStates>(device);

    mQteListenerID = EventManager::Get().Subscribe("battler_qte_update",
        [this](const EventData& d) { this->OnQteUpdate(d); });

    LOG("[BattleQTERenderer] Initialized.");
    return true;
}

void BattleQTERenderer::SetScreenSize(int w, int h)
{
    mScreenW = w;
    mScreenH = h;
    mQtePositions.clear();
}

void BattleQTERenderer::OnQteUpdate(const EventData& data)
{
    if (data.payload) {
        const QTEStatePayload* state = static_cast<const QTEStatePayload*>(data.payload);
        const bool wasActive = mState.isActive;
        const int incomingCount = std::clamp(state->totalCount, 0, BattleEventLimits::MaxQteNodes);

        if (wasActive && state->isActive) {
            if (mFlashTimers.size() < static_cast<size_t>(incomingCount)) {
                mFlashTimers.resize(static_cast<size_t>(incomingCount), 0.0f);
            }
            // Check for freshly resolved individual nodes to trigger flashes
            // Active QTE resolved successfully or missed inside this frame!
            for (int i = 0; i < incomingCount; ++i) {
                if (mState.results[i] == QTEResult::None && state->results[i] != QTEResult::None) {
                    // Flash initialized based on explicit fade out config
                    mFlashTimers[static_cast<size_t>(i)] = state->fadeOutDuration > 0.001f ? state->fadeOutDuration : 0.2f;
                }
            }
        }

        mState = *state;
        mState.totalCount = incomingCount;

        if (!mState.isActive) {
            mQtePositions.clear();
            mFlashTimers.clear();
            return;
        }

        const bool needsPositions = !wasActive || mQtePositions.size() != static_cast<size_t>(incomingCount);
        if (needsPositions) {
            mQtePositions.clear();
            mFlashTimers.assign(static_cast<size_t>(incomingCount), 0.0f);
            if (mState.presentationMode == QTEPresentationMode::Chain)
            {
                const float px = static_cast<float>(mScreenW) * mState.chainAnchorXRatio;
                const float py = static_cast<float>(mScreenH) * mState.chainAnchorYRatio;
                for (int i = 0; i < incomingCount; ++i) {
                    mQtePositions.push_back({ px, py });
                }
            }
            else
            {
                // Staggered QTE keeps the old scattered rhythm presentation.
                float paddingRatio = 0.30f;
                int minX = static_cast<int>(mScreenW * paddingRatio);
                int maxX = static_cast<int>(mScreenW * (1.0f - paddingRatio));
                int minY = static_cast<int>(mScreenH * paddingRatio);
                int maxY = static_cast<int>(mScreenH * (1.0f - paddingRatio));

                for (int i = 0; i < incomingCount; ++i) {
                    float px = static_cast<float>(minX + (rand() % (maxX - minX)));
                    float py = static_cast<float>(minY + (rand() % (maxY - minY)));
                    mQtePositions.push_back({ px, py });
                }
            }
        }
    }
}

void BattleQTERenderer::Update(float dt)
{
    for (float& flashTimer : mFlashTimers) {
        if (flashTimer > 0.0f) {
            flashTimer -= dt;
        }
    }
}

void BattleQTERenderer::Render(ID3D11DeviceContext* context)
{
    if (!mSpriteBatch || !mState.isActive) return;

    D3D11_VIEWPORT vp = { 0.0f, 0.0f, static_cast<float>(mScreenW), static_cast<float>(mScreenH), 0.0f, 1.0f };
    context->RSSetViewports(1, &vp);
    mSpriteBatch->SetViewport(vp);

    mSpriteBatch->Begin(SpriteSortMode_Deferred, mStates->NonPremultiplied(), mStates->LinearClamp(), mStates->DepthNone());

    const float radius = (std::max)(1.0f, mState.promptRadius);
    const float frameTextureSize = (std::max)(1.0f, mState.frameTextureSize);
    RECT srcFull = {
        0,
        0,
        static_cast<LONG>(frameTextureSize),
        static_cast<LONG>(frameTextureSize)
    };
    const XMFLOAT2 frameOrigin(frameTextureSize * 0.5f, frameTextureSize * 0.5f);

    // Queued mode keeps the old randomized multi-prompt feel, but input still
    // resolves strictly from the oldest unresolved prompt to the newest.
    // Chain mode draws only the current prompt and moves future nodes into a
    // small preview row.
    int activeIndex = mState.activeIndex;
    int totalCount = mState.totalCount;
    // If flashed, activeIndex might have been incremented. We cap bounded draws.
    if (activeIndex >= totalCount) activeIndex = totalCount - 1;
    if (activeIndex < 0) activeIndex = 0;

    if (mState.presentationMode == QTEPresentationMode::Chain &&
        activeIndex < static_cast<int>(mQtePositions.size()))
    {
        const int remainingCount = totalCount - activeIndex;
        const float previewSpacing = mState.chainPreviewSpacing;
        const float inactiveMarkerSize = radius * mState.chainPreviewScale;
        const float activeMarkerSize = radius * mState.chainPreviewActiveScale;
        const float startX = mQtePositions[activeIndex].x -
            (static_cast<float>(remainingCount - 1) * previewSpacing * 0.5f);
        const float y = mQtePositions[activeIndex].y + mState.chainPreviewOffsetY;

        for (int slot = 0; slot < remainingCount; ++slot)
        {
            const int nodeIndex = activeIndex + slot;
            if (nodeIndex < 0 || nodeIndex >= totalCount) continue;

            XMVECTOR tint = slot == 0 ? Colors::Gold : Colors::White;
            tint.m128_f32[3] = slot == 0 ? 0.85f : 0.35f;
            const float markerSize = slot == 0 ? activeMarkerSize : inactiveMarkerSize;
            mSpriteBatch->Draw(
                mWhiteFillSRV.Get(),
                XMFLOAT2(startX + static_cast<float>(slot) * previewSpacing, y),
                nullptr,
                tint,
                XM_PIDIV4,
                XMFLOAT2(0.5f, 0.5f),
                XMFLOAT2(markerSize, markerSize));
        }
    }

    const bool isQueued = mState.presentationMode == QTEPresentationMode::Queued;
    const int visibleAhead = (std::max)(0, mState.queueVisibleAheadCount);
    const int drawStart = mState.presentationMode == QTEPresentationMode::Chain
        ? activeIndex
        : (isQueued ? (std::min)(totalCount - 1, activeIndex + visibleAhead) : totalCount - 1);
    for (int i = drawStart; i >= 0; --i)
    {
        bool isActiveDiamond = (i == activeIndex);
        if (i >= static_cast<int>(mQtePositions.size())) break; // memory safety
        const float flashTimer = i < static_cast<int>(mFlashTimers.size())
            ? mFlashTimers[static_cast<size_t>(i)]
            : 0.0f;
        if (mState.presentationMode == QTEPresentationMode::Chain &&
            (!isActiveDiamond || mState.results[i] != QTEResult::None))
        {
            continue;
        }
        if (mState.presentationMode != QTEPresentationMode::Chain && i < activeIndex)
        {
            break;
        }

        // Skip resolved staggered nodes once their configured flash expires.
        if (mState.results[i] != QTEResult::None && flashTimer <= 0.0f) {
            continue;
        }

        float pdx = mQtePositions[i].x;
        float pdy = mQtePositions[i].y;

        XMVECTORF32 frameTint = Colors::White;
        XMVECTORF32 lineTint = { 1.0f, 0.2f, 0.2f, 1.0f }; // Base red

        // Fade effect for the QTE frame to help players see it arrive
        float alpha = isActiveDiamond ? 1.0f : 0.4f;
        float scale = 1.0f;

        // Guard against zero-division in configs
        float fadeRatioLimit = mState.fadeInRatio > 0.001f ? mState.fadeInRatio : 0.001f;

        // Fade effect for the QTE frame to help players see it arrive
        if (mState.progressRatios[i] <= 0.0f) {
            alpha = 0.0f; // completely invisible before timeline start
            scale = 1.0f;
        } else if (isActiveDiamond) {
            float tFade = std::clamp(mState.progressRatios[i] / fadeRatioLimit, 0.0f, 1.0f);
            alpha = 0.3f + (tFade * 0.7f); // Fade from 0.3 up to 1.0
            
            // Smoothly shrink from 1.5x down to 1.0x steadily over the entire 100% life span
            scale = 1.0f + (1.0f - mState.progressRatios[i]) * 0.5f; 
        } else {
            float tFade = std::clamp(mState.progressRatios[i] / fadeRatioLimit, 0.0f, 1.0f);
            alpha = 0.1f + (tFade * 0.3f); // Fade from 0.1 up to 0.4
            
            // Smoothly shrink pending targets over the entire 100% life span too
            scale = 1.0f + (1.0f - mState.progressRatios[i]) * 0.5f; 
        }

        if (flashTimer > 0.0f) {
            if (mState.results[i] == QTEResult::Perfect) { frameTint = Colors::Gold; lineTint = Colors::Gold; }
            else if (mState.results[i] == QTEResult::Good) { frameTint = Colors::Yellow; lineTint = Colors::Yellow; }
            else { frameTint = Colors::Gray; lineTint = Colors::Gray; }
            
            float safeFadeOutLimit = mState.fadeOutDuration > 0.001f ? mState.fadeOutDuration : 0.2f;
            float flashT = flashTimer / safeFadeOutLimit; // goes 1.0 -> 0.0
            alpha = flashT; // Fade out completely 
            scale = 1.0f + (1.0f - flashT) * 0.5f; // Explode outwards: Scale from 1.0 up to 1.5
            
        } else if (isActiveDiamond && mState.isActive) {
            if (mState.progressRatios[i] >= 0.85f) lineTint = Colors::Gold;
            else if (mState.progressRatios[i] >= 0.60f) lineTint = Colors::Yellow;
        }

        // Apply alpha
        XMVECTOR renderTint = frameTint;
        renderTint.m128_f32[3] *= alpha;

        if (mState.isActive || flashTimer > 0.0f) {
            mSpriteBatch->Draw(mFrameSRV.Get(), XMFLOAT2(pdx, pdy), &srcFull, renderTint, 0.0f, frameOrigin, scale);
        }

        // Draw the shrinking/filling clockwise traces if active and hasn't completely missed
        if (mState.isActive && mState.results[i] == QTEResult::None && mState.progressRatios[i] > 0.0f) {
            float lineLength = radius * 1.4142f * scale; // sqrt(2) scaled
            float lineThickness = 6.0f * scale;
            
            XMFLOAT2 verts[4] = {
                { pdx, pdy - (radius * scale) },
                { pdx + (radius * scale), pdy },
                { pdx, pdy + (radius * scale) },
                { pdx - (radius * scale), pdy }
            };
            
            float totalProgressInLines = std::clamp(mState.progressRatios[i], 0.0f, 1.0f) * 4.0f;
            int fullLines = static_cast<int>(totalProgressInLines);
            float partialLine = totalProgressInLines - fullLines;
            
            for (int seg = 0; seg < 4; ++seg) {
                if (seg > fullLines) break; 
                
                int nextIdx = (seg + 1) % 4;
                float pLength = (seg == fullLines) ? (partialLine * lineLength) : lineLength;
                
                float dY = verts[nextIdx].y - verts[seg].y;
                float dX = verts[nextIdx].x - verts[seg].x;
                float rot = std::atan2(dY, dX);
                
                // Start drawing lines once alpha fade-in completes
                if (mState.progressRatios[i] > 0.05f) {
                    mSpriteBatch->Draw(
                        mWhiteFillSRV.Get(), verts[seg], nullptr, lineTint,
                        rot, XMFLOAT2(0.0f, 0.5f), 
                        XMFLOAT2(pLength, lineThickness)
                    );
                }
            }
        }
    }

    mSpriteBatch->End();
}

void BattleQTERenderer::Shutdown()
{
    if (mQteListenerID >= 0) {
        EventManager::Get().Unsubscribe("battler_qte_update", mQteListenerID);
        mQteListenerID = -1;
    }
    mSpriteBatch.reset();
    mStates.reset();
    mFrameSRV.Reset();
    mWhiteFillSRV.Reset();
}
