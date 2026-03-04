// ============================================================
// File: UIRenderer.cpp
// Responsibility: Screen-space animated sprite drawing for UI/HUD.
//
// Architecture decision:
//   UIRenderer is SCREEN-SPACE ONLY.  It accepts no Camera and no
//   world coordinates.  All Draw methods anchor the sprite to a named
//   screen position (TopLeft, BottomCenter, etc.) computed from the
//   render target dimensions stored via SetScreenSize().
//
//   Frame slicing is HORIZONTAL ONLY:
//     The sprite sheet is a single row.  Frame N is at:
//       srcRect.left   = N * frameWidth
//       srcRect.right  = srcRect.left + frameWidth
//       srcRect.top    = 0
//       srcRect.bottom = frameHeight
//     An assert in Initialize() enforces sheetHeight == frameHeight.
//     An assert in DrawAtPosition() double-checks numFrames against
//     the computed framesPerRow so mis-configured JSON is caught early.
//
// Common mistakes:
//   1. Calling DrawXxx() before SetScreenSize() — mScreenW/H are 0,
//      sprite appears at (0, 0) or off-screen.
//   2. Multi-row sheets — the assert in Initialize() fires immediately.
//   3. Drawing UI with WorldRenderer — WorldRenderer applies camera
//      offset, so the sprite would move with the camera.
// ============================================================
#include "UIRenderer.h"
#include "../Utils/Log.h"
#include <cassert>

// ============================================================
// Initialize
// ============================================================
bool UIRenderer::Initialize(ID3D11Device*        device,
                            ID3D11DeviceContext*  context,
                            const std::wstring&  texturePath,
                            const SpriteSheet&   sheet)
{
    // --- Validate single-row sheet ---
    // UIRenderer only slices horizontally.  If the sheet has more rows
    // than one, the caller has the wrong renderer or the wrong JSON file.
    assert(sheet.sheetHeight == sheet.frameHeight &&
           "UIRenderer requires a single-row sprite sheet "
           "(sheetHeight must equal frameHeight). "
           "Use a multi-row sheet with WorldRenderer or a dedicated renderer.");

    if (sheet.sheetHeight != sheet.frameHeight) {
        // Safety: log in non-assert release builds and bail out gracefully.
        LOG("[UIRenderer] ERROR — Sheet '%s' is multi-row (%dpx tall, frame %dpx). "
            "UIRenderer only supports single-row sheets.",
            sheet.spriteName.c_str(), sheet.sheetHeight, sheet.frameHeight);
        return false;
    }

    mSheet = sheet;

    // --- Load GPU texture via WICTextureLoader ---
    // WIC_LOADER_IGNORE_SRGB: bypass the automatic sRGB gamma conversion.
    // Without this flag WIC detects the embedded sRGB profile in the PNG,
    // loads it as R8G8B8A8_UNORM_SRGB, and the GPU linearises pixel values
    // before they reach the UNORM backbuffer — all colors appear darker than
    // intended (e.g. pixel-art green #B5E61D becomes #76CA03 on screen).
    HRESULT hr = DirectX::CreateWICTextureFromFileEx(
        device,
        context,
        texturePath.c_str(),
        0,                              // maxsize — 0 = auto
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0, 0,                           // no CPU access, no misc flags
        DirectX::WIC_LOADER_IGNORE_SRGB,  // load raw pixel values, no gamma conversion
        nullptr,                           // we only need the SRV
        mTextureSRV.GetAddressOf()
    );
    if (FAILED(hr)) {
        LOG("[UIRenderer] Failed to load texture '%ls' (HRESULT 0x%08X).",
            texturePath.c_str(), hr);
        return false;
    }
    LOG("[UIRenderer] Texture loaded: '%ls'", texturePath.c_str());

    // --- Create SpriteBatch ---
    // SpriteBatch stores the device context internally for Begin/End.
    // It allocates its own internal vertex buffer, index buffer, and constant buffer.
    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    if (!mSpriteBatch) {
        LOG("[UIRenderer] Failed to create SpriteBatch.");
        return false;
    }

    // --- Create CommonStates ---
    // CommonStates is a DirectXTK helper that creates one instance of each
    // commonly-needed D3D11 state object (blend/depth/raster/sampler).
    // We use mStates->DepthNone() in Begin() to prevent CircleRenderer's
    // depth-buffer writes from discarding our sprite pixels.
    // Root cause: CircleRenderer's full-screen quad sets z=0 on every pixel.
    // Default depth test is LESS — sprite pixels at z=0 fail "0 < 0" → invisible.
    // DepthNone() disables depth test entirely, which is correct for UI/HUD.
    mStates = std::make_unique<DirectX::CommonStates>(device);
    if (!mStates) {
        LOG("[UIRenderer] Failed to create CommonStates.");
        return false;
    }

    // --- Activate the first clip ---
    if (!mSheet.animations.empty()) {
        PlayClip(mSheet.animations[0].name);
    }

    LOG("[UIRenderer] Initialized. Sheet: '%s', clips: %d, size: %dx%d, frame: %dx%d",
        mSheet.spriteName.c_str(),
        static_cast<int>(mSheet.animations.size()),
        mSheet.sheetWidth, mSheet.sheetHeight,
        mSheet.frameWidth, mSheet.frameHeight);

    return true;
}

// ============================================================
// SetScreenSize
// ============================================================
void UIRenderer::SetScreenSize(int width, int height)
{
    // Store dimensions so all DrawXxx() methods can compute anchor positions.
    // This must be called again whenever the window is resized.
    mScreenW = width;
    mScreenH = height;
}

// ============================================================
// PlayClip
// ============================================================
void UIRenderer::PlayClip(const std::string& clipName)
{
    // No-op if already playing — avoids a frame-0 stutter when called every frame.
    if (mActiveClipName == clipName && mActiveClip != nullptr) return;

    const AnimationClip* clip = mSheet.FindClip(clipName);
    if (!clip) {
        LOG("[UIRenderer] Clip '%s' not found in sheet '%s'.",
            clipName.c_str(), mSheet.spriteName.c_str());
        return;
    }

    // Validate that the clip's frame count fits in the single-row strip.
    // framesPerRow() = sheetWidth / frameWidth.  numFrames must not exceed it.
    assert(clip->numFrames <= mSheet.framesPerRow() &&
           "Clip numFrames exceeds the number of horizontal frames in the strip. "
           "Check the JSON num_frames and the sheet width/frame_width.");

    mActiveClip     = clip;
    mActiveClipName = clipName;
    mFrameIndex     = 0;
    mFrameTimer     = 0.0f;
}

// ============================================================
// Update
// ============================================================
void UIRenderer::Update(float dt)
{
    if (!mActiveClip || mActiveClip->numFrames <= 1) return;

    mFrameTimer += dt;

    const float frameDuration = 1.0f / mActiveClip->frameRate;

    // Advance frame(s) with leftover carry-over to prevent animation drift.
    // Resetting to 0 would lose partial-frame time; subtraction preserves it.
    while (mFrameTimer >= frameDuration) {
        mFrameTimer -= frameDuration;
        mFrameIndex++;

        if (mFrameIndex >= mActiveClip->numFrames) {
            if (mActiveClip->loop) {
                mFrameIndex = 0;  // wrap to first frame
            } else {
                mFrameIndex = mActiveClip->numFrames - 1;  // hold last frame
                mFrameTimer = 0.0f;
                break;
            }
        }
    }
}

// ============================================================
// DrawAtPosition  (private — shared implementation)
// ============================================================
void UIRenderer::DrawAtPosition(ID3D11DeviceContext* context,
                                float screenX, float screenY,
                                float scale)
{
    // Guard: nothing to draw if not initialized or no active clip.
    if (!mActiveClip || !mTextureSRV || !mSpriteBatch) return;

    const int fw = mSheet.frameWidth;
    const int fh = mSheet.frameHeight;

    // Single-row: column = frameIndex, row = 0.
    // Double-check that the frame index is within the horizontal strip.
    assert(mFrameIndex < mSheet.framesPerRow() &&
           "mFrameIndex exceeds framesPerRow — sheet width or num_frames mismatch.");

    // Slice the source RECT for this frame from the horizontal strip.
    RECT srcRect = {
        static_cast<LONG>(mFrameIndex * fw),   // left
        0L,                                    // top  — always 0 (single row)
        static_cast<LONG>(mFrameIndex * fw + fw),  // right
        static_cast<LONG>(fh)                  // bottom
    };
	// LOG("[UIRenderer] srcRect: L=%ld T=%ld R=%ld B=%ld",
	// 	srcRect.left,
	// 	srcRect.top,
	// 	srcRect.right,
	// 	srcRect.bottom);

    // Origin = JSON pivot, local to the source RECT.
    // For a standing character: pivotX = frameWidth/2, pivotY = frameHeight
    // places the sprite feet at the screen anchor position.
    DirectX::XMFLOAT2 origin(
        static_cast<float>(mActiveClip->pivotX),
        static_cast<float>(mActiveClip->pivotY)
    );

    DirectX::XMFLOAT2 position(screenX, screenY);

    // Re-bind viewport explicitly before Begin() AND tell SpriteBatch about it.
    //
    // SpriteBatch::GetViewportTransform() normally queries RSGetViewports() each
    // frame to build its orthographic projection (pixel coords → NDC).  If
    // RSGetViewports() returns viewportCount=0, SpriteBatch throws
    // std::runtime_error("No viewport is set") inside End().  That exception
    // silently unwinds through WinMain (no catch block), leaving
    // mInBeginEndPair=true permanently — every subsequent Begin() throws
    // "Cannot nest Begin calls" and the sprite is never drawn again.
    //
    // SetViewport() sets mSetViewport=true inside SpriteBatch::Impl, so
    // GetViewportTransform() uses the stored mViewPort instead of calling
    // RSGetViewports().  This makes SpriteBatch immune to RS-stage resets
    // performed by other renderers (CircleRenderer, etc.).
    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(mScreenW);
    vp.Height   = static_cast<float>(mScreenH);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    context->RSSetViewports(1, &vp);  // bind for other renderers that do query RS
    mSpriteBatch->SetViewport(vp);     // bypass RSGetViewports inside SpriteBatch::End()

    mSpriteBatch->Begin(
        DirectX::SpriteSortMode_Deferred,
        mStates->NonPremultiplied(),
        mStates->LinearClamp(),
        mStates->DepthNone()
    );

    mSpriteBatch->Draw(
        mTextureSRV.Get(),
        position,
        &srcRect,
        DirectX::Colors::White,
        0.0f,
        origin,
        scale
    );

    mSpriteBatch->End();
}

// ============================================================
// Named draw methods — public interface
// Each method resolves screen dimensions to a concrete pixel
// position and delegates to DrawAtPosition().
// ============================================================

void UIRenderer::DrawTopLeft(ID3D11DeviceContext* context, float scale)
{
    DrawAtPosition(context, 0.0f, 0.0f, scale);
}

void UIRenderer::DrawTopCenter(ID3D11DeviceContext* context, float scale)
{
    DrawAtPosition(context,
                   static_cast<float>(mScreenW) * 0.5f,
                   0.0f,
                   scale);
}

void UIRenderer::DrawTopRight(ID3D11DeviceContext* context, float scale)
{
    DrawAtPosition(context,
                   static_cast<float>(mScreenW),
                   0.0f,
                   scale);
}

void UIRenderer::DrawMiddleLeft(ID3D11DeviceContext* context, float scale)
{
    DrawAtPosition(context,
                   0.0f,
                   static_cast<float>(mScreenH) * 0.5f,
                   scale);
}

void UIRenderer::DrawCenter(ID3D11DeviceContext* context, float scale)
{
    DrawAtPosition(context,
                   static_cast<float>(mScreenW) * 0.5f,
                   static_cast<float>(mScreenH) * 0.5f,
                   scale);
}

void UIRenderer::DrawMiddleRight(ID3D11DeviceContext* context, float scale)
{
    DrawAtPosition(context,
                   static_cast<float>(mScreenW),
                   static_cast<float>(mScreenH) * 0.5f,
                   scale);
}

void UIRenderer::DrawBottomLeft(ID3D11DeviceContext* context, float scale)
{
    DrawAtPosition(context,
                   0.0f,
                   static_cast<float>(mScreenH),
                   scale);
}

void UIRenderer::DrawBottomCenter(ID3D11DeviceContext* context, float scale)
{
    // Most common anchor for standing characters:
    // pivot = (frameWidth/2, frameHeight) → feet land at screen-bottom-center.
    DrawAtPosition(context,
                   static_cast<float>(mScreenW) * 0.5f,
                   static_cast<float>(mScreenH),
                   scale);
}

void UIRenderer::DrawBottomRight(ID3D11DeviceContext* context, float scale)
{
    DrawAtPosition(context,
                   static_cast<float>(mScreenW),
                   static_cast<float>(mScreenH),
                   scale);
}

void UIRenderer::DrawAt(ID3D11DeviceContext* context,
                        float screenX, float screenY,
                        float scale)
{
    DrawAtPosition(context, screenX, screenY, scale);
}

// ============================================================
// Shutdown
// ============================================================
void UIRenderer::Shutdown()
{
    mSpriteBatch.reset();    // releases internal VB/IB/CB
    mStates.reset();         // releases all CommonStates D3D11 objects
    mTextureSRV.Reset();     // decrements SRV ref count; texture freed if last ref
    mActiveClip     = nullptr;
    mActiveClipName.clear();
    mFrameIndex  = 0;
    mFrameTimer  = 0.0f;
    LOG("[UIRenderer] Shutdown complete.");
}
