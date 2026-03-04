// ============================================================
// File: SpriteRenderer.cpp
// Responsibility: Load sprite sheet texture, manage animation state,
//                 issue SpriteBatch draw calls each frame.
//
// Architecture decisions:
//   - SpriteBatch is owned by SpriteRenderer (not shared).
//     Each renderer has its own Begin/End scope so multiple renderers
//     can coexist without interfering with each other's state.
//   - The device context is NOT stored as a member; it is passed into
//     Draw() each frame.  This is safer: the context pointer cannot
//     become stale if the device is recreated on a resize.
//   - Texture is loaded via WICTextureLoader which supports PNG/JPEG/BMP.
//     The resulting SRV is the only GPU resource we own directly;
//     SpriteBatch manages its own internal VB/IB/CB.
//
// Common mistakes:
//   1. Calling SpriteBatch::Draw() outside Begin/End → silent assert.
//   2. Using the wrong RECT origin: SpriteBatch origin is relative to
//      the source RECT, not the full texture.
//   3. Accumulating mFrameTimer without resetting → frame never advances.
// ============================================================
#include "SpriteRenderer.h"
#include "../Utils/Log.h"
#include <cassert>

// ------------------------------------------------------------
// Function: Initialize
// Purpose:
//   1. Copy the SpriteSheet descriptor (used for frame slicing every Draw).
//   2. Load the PNG texture from disk into a GPU SRV via WICTextureLoader.
//   3. Create a SpriteBatch instance bound to the device context.
//   4. Activate the first animation clip in the sheet (usually "idle").
//
// Why WICTextureLoader?
//   It decodes any WIC-supported format (PNG, JPEG, BMP, TIFF) and
//   uploads the mip level 0 texture to a D3D11_USAGE_DEFAULT resource in
//   one call.  No manual staging buffer or D3D11_SUBRESOURCE_DATA setup.
//
// Why SpriteBatch from DirectXTK?
//   SpriteBatch handles its own internal VB/IB pair with a built-in
//   sort mode, batching multiple Draw() calls into as few GPU draw calls
//   as possible.  Rolling a custom sprite renderer would duplicate all
//   that infrastructure for no architectural benefit at this stage.
// ------------------------------------------------------------
bool SpriteRenderer::Initialize(ID3D11Device*        device,
                                ID3D11DeviceContext* context,
                                const std::wstring&  texturePath,
                                const SpriteSheet&   sheet)
{
    // Store the sheet descriptor — we need it every frame for frame slicing.
    mSheet = sheet;

    // --- Load texture ---
    // CreateWICTextureFromFileEx with WIC_LOADER_IGNORE_SRGB loads raw 8-bit
    // pixel values without gamma conversion.  Without this flag, WIC detects
    // the sRGB ICC profile embedded in most PNGs, promotes the format to
    // R8G8B8A8_UNORM_SRGB, and the GPU linearises colors before they reach
    // the UNORM backbuffer — making all pixel-art sprites appear darker than
    // their source colors (e.g. #B5E61D → #76CA03).
    HRESULT hr = DirectX::CreateWICTextureFromFileEx(
        device,
        context,
        texturePath.c_str(),
        0,                              // maxsize — 0 = auto
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0, 0,                           // no CPU access, no misc flags
        DirectX::WIC_LOADER_IGNORE_SRGB,  // load raw pixel values, no gamma conversion
        nullptr,                           // we do not need the raw ID3D11Resource
        mTextureSRV.GetAddressOf()
    );
    if (FAILED(hr)) {
        LOG("[SpriteRenderer] Failed to load texture '%ls' (0x%08X)", texturePath.c_str(), hr);
        return false;
    }
    LOG("[SpriteRenderer] Texture loaded: '%ls'", texturePath.c_str());

    // --- Create SpriteBatch ---
    // SpriteBatch stores the context pointer internally for Begin/End calls.
    // It creates its own internal VB, IB, and constant buffer on the device.
    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    if (!mSpriteBatch) {
        LOG("[SpriteRenderer] Failed to create SpriteBatch.");
        return false;
    }

    // Store the initial screen dimensions so Draw() can resolve alignment.
    // The caller must call SetScreenSize() again if the window is resized.
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;
    ID3D11Texture2D*        tex = nullptr;
    context->OMGetRenderTargets(1, &rtv, &dsv);
    if (rtv) {
        rtv->GetResource(reinterpret_cast<ID3D11Resource**>(&tex));
        if (tex) {
            D3D11_TEXTURE2D_DESC desc = {};
            tex->GetDesc(&desc);
            mScreenW = static_cast<int>(desc.Width);
            mScreenH = static_cast<int>(desc.Height);
            tex->Release();
        }
        rtv->Release();
    }
    if (dsv) dsv->Release();

    // --- Activate first clip ---
    if (!mSheet.animations.empty()) {
        PlayClip(mSheet.animations[0].name);
    }

    LOG("[SpriteRenderer] Initialized. Sheet: '%s', clips: %d",
        mSheet.spriteName.c_str(), (int)mSheet.animations.size());
    return true;
}

// ------------------------------------------------------------
// Function: PlayClip
// Purpose:
//   Set the active animation clip by name.
//   Resets frame index + timer so the new clip always starts at frame 0.
// Why reset the timer?
//   Without a reset, switching from a slow animation (low frameRate)
//   mid-frame could cause the new clip to immediately skip ahead by
//   the accumulated timer value — a visible pop.
// ------------------------------------------------------------
void SpriteRenderer::PlayClip(const std::string& clipName)
{
    // No-op if already playing this clip — avoids a stutter/restart.
    if (mActiveClipName == clipName && mActiveClip != nullptr) return;


    const AnimationClip* clip = mSheet.FindClip(clipName);
    if (!clip) {
        LOG("[SpriteRenderer] Clip '%s' not found in sheet '%s'.",
            clipName.c_str(), mSheet.spriteName.c_str());
        return;
    }

    mActiveClip     = clip;
    mActiveClipName = clipName;
    mFrameIndex     = 0;
    mFrameTimer     = 0.0f;
}

// ------------------------------------------------------------
// Function: Update
// Purpose:
//   Advance mFrameTimer by dt (seconds).
//   When mFrameTimer reaches the frame duration (1 / frameRate),
//   advance mFrameIndex by 1 and subtract the frame duration from the timer.
//
// Why subtract instead of reset to 0?
//   Subtracting carries over leftover time into the next frame.
//   Resetting to 0 discards the remainder, causing the animation to
//   drift slower than the target frameRate over time (especially at
//   low frame rates where dt > frame_duration).
// ------------------------------------------------------------
void SpriteRenderer::Update(float dt)
{
    if (!mActiveClip || mActiveClip->numFrames <= 1) return;

    mFrameTimer += dt;

    // How many seconds one frame should last.
    const float frameDuration = 1.0f / mActiveClip->frameRate;

    while (mFrameTimer >= frameDuration) {
        mFrameTimer -= frameDuration;   // carry over remainder
        mFrameIndex++;

        if (mFrameIndex >= mActiveClip->numFrames) {
            if (mActiveClip->loop) {
                mFrameIndex = 0;        // wrap around to the start
            } else {
                mFrameIndex = mActiveClip->numFrames - 1;  // hold on last frame
                mFrameTimer = 0.0f;     // stop accumulating
                break;
            }
        }
    }
}

// ------------------------------------------------------------
// Function: SetScreenSize
// Purpose:  Store the render target dimensions used by AlignToPosition().
// Why a separate call?
//   The renderer does not hold a reference to D3DContext — keeping it
//   decoupled makes it reusable in any context (HUD, battle, cutscene).
//   The caller owns the knowledge of the current resolution and passes
//   it in on init and on every resize.
// ------------------------------------------------------------
void SpriteRenderer::SetScreenSize(int width, int height)
{
    mScreenW = width;
    mScreenH = height;
}

// ------------------------------------------------------------
// Function: AlignToPosition
// Purpose:
//   Convert a SpriteAlign enum value into a concrete screen-space pixel
//   coordinate.  This is the ONLY place in the codebase where alignment
//   strings translate to pixel positions — no caller ever hardcodes them.
//
// Mapping:
//   Horizontal:  left  → 0       center → screenW / 2     right  → screenW
//   Vertical:    top   → 0       middle → screenH / 2     bottom → screenH
//
// The returned position is where the sprite's pivot will land.
// Combined with the JSON pivot (bottom-center of the frame for a standing
// character), the sprite is correctly anchored without any caller math.
// ------------------------------------------------------------
DirectX::XMFLOAT2 SpriteRenderer::AlignToPosition(SpriteAlign align) const
{
    const float W = static_cast<float>(mScreenW);
    const float H = static_cast<float>(mScreenH);

    float x = 0.0f;
    float y = 0.0f;

    switch (align) {
        case SpriteAlign::TopLeft:      x = 0.0f;   y = 0.0f;   break;
        case SpriteAlign::TopCenter:    x = W*0.5f; y = 0.0f;   break;
        case SpriteAlign::TopRight:     x = W;      y = 0.0f;   break;
        case SpriteAlign::MiddleLeft:   x = 0.0f;   y = H*0.5f; break;
        case SpriteAlign::MiddleCenter: x = W*0.5f; y = H*0.5f; break;
        case SpriteAlign::MiddleRight:  x = W;      y = H*0.5f; break;
        case SpriteAlign::BottomLeft:   x = 0.0f;   y = H;      break;
        case SpriteAlign::BottomRight:  x = W;      y = H;      break;
        case SpriteAlign::BottomCenter: // intentional fall-through — default
        case SpriteAlign::Unknown:
        default:                        x = W*0.5f; y = H;      break;
    }

    return { x, y };
}

// ------------------------------------------------------------
// Function: Draw
// Purpose:
//   Compute the draw position from the active clip's align field,
//   slice the correct source RECT, and issue one SpriteBatch draw call.
//
// No pixel coordinates are accepted as parameters.
// The screen anchor is resolved entirely from:
//   mActiveClip->align  (parsed from JSON "align" field)
//   mScreenW / mScreenH (set by SetScreenSize)
//   mActiveClip->pivotX/Y (set by JSON "pivot" field)
// ------------------------------------------------------------
void SpriteRenderer::Draw(ID3D11DeviceContext* context, float scale)
{
    if (!mActiveClip || !mTextureSRV || !mSpriteBatch) return;

    const int fw  = mSheet.frameWidth;
    const int fh  = mSheet.frameHeight;
    const int fpr = mSheet.framesPerRow();

    // Determine which column and row in the sprite sheet grid this frame lives in.
    const int col = mFrameIndex % fpr;
    const int row = mFrameIndex / fpr;

    // Source rectangle: the pixel region within the texture for this frame.
    RECT srcRect = {
        static_cast<LONG>(col * fw),
        static_cast<LONG>(row * fh),
        static_cast<LONG>(col * fw + fw),
        static_cast<LONG>(row * fh + fh)
    };

    // Origin: pivot point within the source RECT in pixels.
    // Comes straight from the JSON "pivot" field — no hardcoding.
    DirectX::XMFLOAT2 origin(
        static_cast<float>(mActiveClip->pivotX),
        static_cast<float>(mActiveClip->pivotY)
    );

    // Screen position: derived from the JSON "align" field.
    // AlignToPosition() maps the enum to a pixel coordinate using mScreenW/H.
    // No caller ever computes or passes a pixel position.
    DirectX::XMFLOAT2 position = AlignToPosition(mActiveClip->align);

    mSpriteBatch->Begin();

    mSpriteBatch->Draw(
        mTextureSRV.Get(),
        position,
        &srcRect,
        DirectX::Colors::White,
        0.0f,     // rotation
        origin,   // pivot in source-RECT-local pixels (from JSON)
        scale
    );

    mSpriteBatch->End();
}

// ------------------------------------------------------------
// Function: Shutdown
// Purpose:
//   Release all GPU resources owned by this renderer.
//   Resets the SpriteBatch unique_ptr and the SRV ComPtr.
// Why explicit Shutdown instead of relying on destructor?
//   Gives PlayState::OnExit() deterministic control over when GPU
//   resources are released — before the D3D device is torn down.
// ------------------------------------------------------------
void SpriteRenderer::Shutdown()
{
    mSpriteBatch.reset();   // destroys internal VB/IB/CB
    mTextureSRV.Reset();    // releases the SRV; GPU texture freed if ref-count hits 0
    mActiveClip     = nullptr;
    mActiveClipName.clear();
    LOG("[SpriteRenderer] Shutdown complete.");
}
