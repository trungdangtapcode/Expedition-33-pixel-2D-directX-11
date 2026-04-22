// ============================================================
// File: BattleTextRenderer.cpp
// Responsibility: Implementation of BattleTextRenderer.
//
// Key design decisions:
//   - SpriteBatch uses NonPremultiplied blend so text glyphs alpha-composite
//     correctly over any background.  The .spritefont was generated with
//     /NoPremultiply so straight-alpha glyph edges are preserved.
//   - viewport is re-bound before every Begin() because WorldSpriteRenderer
//     calls RSSetViewports(0, nullptr) after its render pass.
//   - DrawString / DrawStringCentered are convenience wrappers that each open
//     their own Begin/End; the raw batch API avoids repeated state changes for
//     callers that draw many strings per frame (e.g. EnemyHpBarRenderer names).
//
// Common mistakes:
//   1. Generating the font with premultiplied alpha (no /NoPremultiply flag)
//      and then using NonPremultiplied blend — glyphs appear washed out.
//   2. Forgetting that MeasureString returns XMVECTOR (use XMVectorGetX/Y).
//   3. Using XMFLOAT2 as color — DrawString takes FXMVECTOR, not a struct.
// ============================================================
#include "BattleTextRenderer.h"
#include "../Utils/Log.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// ============================================================
// BindViewport
// ============================================================
void BattleTextRenderer::BindViewport(ID3D11DeviceContext* context)
{
    // Re-establish the RS viewport so SpriteBatch::Begin() does not throw
    // when the previous renderer left the rasterizer with 0 active viewports.
    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(mScreenW);
    vp.Height   = static_cast<float>(mScreenH);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context->RSSetViewports(1, &vp);
}

// ============================================================
// Initialize
// ============================================================
bool BattleTextRenderer::Initialize(ID3D11Device*       device,
                                    ID3D11DeviceContext* context,
                                    const std::wstring& fontPath,
                                    int screenW, int screenH)
{
    mScreenW = screenW;
    mScreenH = screenH;

    // ----------------------------------------------------------------
    // 1. Load SpriteFont from the .spritefont binary atlas.
    //    SpriteFont constructor throws std::exception on failure (bad path,
    //    corrupt file, wrong magic bytes).  Catch and log gracefully.
    // ----------------------------------------------------------------
    try
    {
        mFont = std::make_unique<SpriteFont>(device, fontPath.c_str());
    }
    catch (const std::exception& e)
    {
        LOG("[BattleTextRenderer] Failed to load font '%ls': %s",
            fontPath.c_str(), e.what());
        return false;
    }
    LOG("[BattleTextRenderer] Font loaded: %ls", fontPath.c_str());

    // ----------------------------------------------------------------
    // 2. Create SpriteBatch and CommonStates.
    //    CommonStates caches all standard blend/depth/raster/sampler states
    //    as lazy singletons keyed by device — no per-renderer overhead after
    //    the first creation.
    // ----------------------------------------------------------------
    mSpriteBatch = std::make_unique<SpriteBatch>(context);
    mStates      = std::make_unique<CommonStates>(device);

    LOG("[BattleTextRenderer] Initialized. Screen: %dx%d", screenW, screenH);
    return true;
}

// ============================================================
// DrawString
// ============================================================
void BattleTextRenderer::DrawString(ID3D11DeviceContext* context,
                                    const char*          text,
                                    float                x,
                                    float                y,
                                    FXMVECTOR            color,
                                    CXMMATRIX            transform)
{
    if (!IsReady() || !text || !*text) return;

    BindViewport(context);

    // NonPremultiplied: glyph atlas was generated without premultiplied alpha.
    // LinearClamp: smooth sub-pixel glyph rendering at non-integer positions.
    mSpriteBatch->Begin(SpriteSortMode_Deferred, mStates->NonPremultiplied(), mStates->LinearClamp(), mStates->DepthNone(), nullptr, nullptr, transform);

    mFont->DrawString(mSpriteBatch.get(), text, XMFLOAT2(x, y), color);

    mSpriteBatch->End();
}

// ============================================================
// DrawStringCentered
// ============================================================
void BattleTextRenderer::DrawStringCentered(ID3D11DeviceContext* context,
                                            const char*          text,
                                            float                centerX,
                                            float                y,
                                            FXMVECTOR            color,
                                            CXMMATRIX            transform)
{
    if (!IsReady() || !text || !*text) return;

    // MeasureString returns the bounding box as XMVECTOR (width=X, height=Y).
    // We only need width to center horizontally.
    const XMVECTOR size  = mFont->MeasureString(text);
    const float    halfW = XMVectorGetX(size) * 0.5f;

    BindViewport(context);

    mSpriteBatch->Begin(SpriteSortMode_Deferred, mStates->NonPremultiplied(), mStates->LinearClamp(), mStates->DepthNone(), nullptr, nullptr, transform);

    mFont->DrawString(mSpriteBatch.get(), text,
                      XMFLOAT2(centerX - halfW, y), color);

    mSpriteBatch->End();
}

// ============================================================
// BeginBatch / DrawStringRaw / DrawStringCenteredRaw / EndBatch
// ============================================================
void BattleTextRenderer::BeginBatch(ID3D11DeviceContext* context, DirectX::CXMMATRIX transform)
{
    if (!IsReady()) return;

    BindViewport(context);

    mSpriteBatch->Begin(SpriteSortMode_Deferred, mStates->NonPremultiplied(), mStates->LinearClamp(), mStates->DepthNone(), nullptr, nullptr, transform);
}

void BattleTextRenderer::DrawStringRaw(const char*  text,
                                       float        x,
                                       float        y,
                                       FXMVECTOR    color)
{
    if (!IsReady() || !text || !*text) return;
    mFont->DrawString(mSpriteBatch.get(), text, XMFLOAT2(x, y), color);
}

void BattleTextRenderer::DrawStringCenteredRaw(const char* text,
                                               float       centerX,
                                               float       y,
                                               FXMVECTOR   color,
                                               float       scale,
                                               bool        drawOutline)
{
    if (!IsReady() || !text || !*text) return;

    const XMVECTOR size  = mFont->MeasureString(text);
    XMFLOAT2 origin(XMVectorGetX(size) * 0.5f, 0.0f);

    if (drawOutline) {
        XMVECTOR outlineColor = Colors::Black;
        outlineColor.m128_f32[3] = color.m128_f32[3]; // match alpha
        
        mFont->DrawString(mSpriteBatch.get(), text, XMFLOAT2(centerX - 2.0f, y), outlineColor, 0.0f, origin, scale);
        mFont->DrawString(mSpriteBatch.get(), text, XMFLOAT2(centerX + 2.0f, y), outlineColor, 0.0f, origin, scale);
        mFont->DrawString(mSpriteBatch.get(), text, XMFLOAT2(centerX, y - 2.0f), outlineColor, 0.0f, origin, scale);
        mFont->DrawString(mSpriteBatch.get(), text, XMFLOAT2(centerX, y + 2.0f), outlineColor, 0.0f, origin, scale);
    }

    mFont->DrawString(mSpriteBatch.get(), text, XMFLOAT2(centerX, y), color, 0.0f, origin, scale);
}

void BattleTextRenderer::EndBatch()
{
    if (!IsReady()) return;
    mSpriteBatch->End();
}

// ============================================================
// SetScreenSize
// ============================================================
// (inline in header)

// ============================================================
// Shutdown
// ============================================================
void BattleTextRenderer::Shutdown()
{
    mSpriteBatch.reset();
    mStates.reset();
    mFont.reset();   // releases the GPU texture atlas owned by SpriteFont

    LOG("[BattleTextRenderer] Shutdown complete.");
}


