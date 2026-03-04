// ============================================================
// File: WorldRenderer.cpp
// Responsibility: World-space animated sprite drawing via Camera2D matrix.
//
// Architecture decision — GPU matrix transform instead of CPU WorldToScreen:
//   OLD approach: for each sprite, call camera.WorldToScreen(x, y) on CPU,
//     then pass the resulting screen pixel to SpriteBatch::Draw().
//     Cost: O(N) multiplications on the CPU, one per object per frame.
//
//   NEW approach: pass camera.GetViewProjectionMatrix() to SpriteBatch::Begin().
//     SpriteBatch forwards it as the transform matrix to its internal VS.
//     Every sprite vertex is multiplied by ViewProj on the GPU — in parallel.
//     CPU cost: one matrix multiplication per frame (Camera2D::Update()).
//     GPU cost: the VS multiply was already happening; now it carries real data.
//
//   This scales to thousands of grass blades, enemies, and particles with
//   zero additional CPU per-object work.
//
// Frame slicing: HORIZONTAL ONLY (single-row strip).
//   Assert: sheetHeight == frameHeight (enforced in Initialize).
//   Assert: mFrameIndex < framesPerRow (enforced in Draw).
//
// Scale compositing:
//   Camera zoom is baked into the ViewProj matrix (via S(zoom) in RebuildView).
//   The caller-supplied scale parameter is an additional per-sprite override
//   (e.g. a boss drawn at 3x on top of the zoom level).
//
// Common mistakes:
//   1. Calling Draw() without camera.Update() this frame — stale VP matrix,
//      camera appears frozen even though position changed.
//   2. Using WorldRenderer for UI — UI must not follow the camera.
//   3. Multi-row sheets — the Initialize() assert fires immediately.
// ============================================================
#include "WorldRenderer.h"
#include "../Utils/Log.h"
#include <cassert>

// ============================================================
// Initialize
// ============================================================
bool WorldRenderer::Initialize(ID3D11Device*        device,
                               ID3D11DeviceContext*  context,
                               const std::wstring&  texturePath,
                               const SpriteSheet&   sheet)
{
    // --- Enforce single-row sheet ---
    // A multi-row sheet would require a second row offset that WorldRenderer
    // does not compute.  Catch the misconfiguration here rather than silently
    // drawing garbage.
    assert(sheet.sheetHeight == sheet.frameHeight &&
           "WorldRenderer requires a single-row sprite sheet "
           "(sheetHeight must equal frameHeight). "
           "Multi-row sheets are not supported by this renderer.");

    if (sheet.sheetHeight != sheet.frameHeight) {
        LOG("[WorldRenderer] ERROR — Sheet '%s' is multi-row (%dpx tall, frame %dpx).",
            sheet.spriteName.c_str(), sheet.sheetHeight, sheet.frameHeight);
        return false;
    }

    mSheet = sheet;

    // --- Load GPU texture ---
    // WIC_LOADER_IGNORE_SRGB: bypass automatic sRGB gamma conversion.
    // Without this flag, colors in pixel-art sprites appear darker on screen
    // because WIC promotes the format to UNORM_SRGB and the GPU linearises
    // values before writing to the UNORM backbuffer.
    HRESULT hr = DirectX::CreateWICTextureFromFileEx(
        device,
        context,
        texturePath.c_str(),
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0, 0,
        DirectX::WIC_LOADER_IGNORE_SRGB,  // load raw pixel values, no gamma conversion
        nullptr,
        mTextureSRV.GetAddressOf()
    );
    if (FAILED(hr)) {
        LOG("[WorldRenderer] Failed to load texture '%ls' (HRESULT 0x%08X).",
            texturePath.c_str(), hr);
        return false;
    }
    LOG("[WorldRenderer] Texture loaded: '%ls'", texturePath.c_str());

    // --- Create SpriteBatch ---
    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    if (!mSpriteBatch) {
        LOG("[WorldRenderer] Failed to create SpriteBatch.");
        return false;
    }

    // --- Activate first clip by default ---
    if (!mSheet.animations.empty()) {
        PlayClip(mSheet.animations[0].name);
    }

    LOG("[WorldRenderer] Initialized. Sheet: '%s', clips: %d, strip: %dx1 frames",
        mSheet.spriteName.c_str(),
        static_cast<int>(mSheet.animations.size()),
        mSheet.framesPerRow());

    return true;
}

// ============================================================
// PlayClip
// ============================================================
void WorldRenderer::PlayClip(const std::string& clipName)
{
    if (mActiveClipName == clipName && mActiveClip != nullptr) return;

    const AnimationClip* clip = mSheet.FindClip(clipName);
    if (!clip) {
        LOG("[WorldRenderer] Clip '%s' not found in sheet '%s'.",
            clipName.c_str(), mSheet.spriteName.c_str());
        return;
    }

    // Verify the clip fits within the horizontal strip.
    assert(clip->numFrames <= mSheet.framesPerRow() &&
           "Clip numFrames exceeds framesPerRow — check JSON num_frames vs sheet width.");

    mActiveClip     = clip;
    mActiveClipName = clipName;
    mFrameIndex     = 0;
    mFrameTimer     = 0.0f;
}

// ============================================================
// Update
// ============================================================
void WorldRenderer::Update(float dt)
{
    if (!mActiveClip || mActiveClip->numFrames <= 1) return;

    mFrameTimer += dt;

    const float frameDuration = 1.0f / mActiveClip->frameRate;

    // Carry-over subtraction prevents animation drift at low frame rates.
    while (mFrameTimer >= frameDuration) {
        mFrameTimer -= frameDuration;
        mFrameIndex++;

        if (mFrameIndex >= mActiveClip->numFrames) {
            if (mActiveClip->loop) {
                mFrameIndex = 0;
            } else {
                mFrameIndex = mActiveClip->numFrames - 1;
                mFrameTimer = 0.0f;
                break;
            }
        }
    }
}

// ============================================================
// Draw
// ============================================================
void WorldRenderer::Draw(ID3D11DeviceContext* context,
                         const Camera2D&      camera,
                         float worldX, float worldY,
                         float scale)
{
    if (!mActiveClip || !mTextureSRV || !mSpriteBatch) return;

    // Sanity: frame index must be within the horizontal strip.
    assert(mFrameIndex < mSheet.framesPerRow() &&
           "mFrameIndex exceeds framesPerRow — frame advancement bug.");

    const int fw = mSheet.frameWidth;
    const int fh = mSheet.frameHeight;

    // Slice the horizontal strip to the current frame.
    RECT srcRect = {
        static_cast<LONG>(mFrameIndex * fw),
        0L,
        static_cast<LONG>(mFrameIndex * fw + fw),
        static_cast<LONG>(fh)
    };

    // JSON pivot — local to the source RECT.
    // For a standing character: (frameWidth/2, frameHeight) puts feet at
    // the world-space position passed in.
    DirectX::XMFLOAT2 origin(
        static_cast<float>(mActiveClip->pivotX),
        static_cast<float>(mActiveClip->pivotY)
    );

    // World-space position — passed directly to SpriteBatch.
    // The GPU transforms this through the ViewProj matrix in the Vertex Shader.
    // No CPU math is required here; WorldToScreen() is NOT called.
    DirectX::XMFLOAT2 worldPos(worldX, worldY);

    // Retrieve the combined View x Projection matrix from the camera.
    // SpriteBatch::Begin() accepts a custom transform matrix as its last
    // parameter.  Every vertex position is multiplied by this matrix on
    // the GPU, equivalent to:
    //   clipPos = float4(worldPos, 0, 1) * ViewProjMatrix
    // This replaces all per-object CPU WorldToScreen() calls with a
    // single GPU matrix multiply — the standard approach for large scenes.
    DirectX::XMMATRIX viewProj = camera.GetViewProjectionMatrix();

    mSpriteBatch->Begin(
        DirectX::SpriteSortMode_Deferred, // batch and sort draw calls
        nullptr,  // blend state      — default (alpha blend)
        nullptr,  // sampler state    — default (linear clamp)
        nullptr,  // depth state      — default
        nullptr,  // rasterizer state — default
        nullptr,  // custom shader callback — none (SpriteBatch's own VS/PS)
        viewProj  // transform matrix — ViewProj baked in; GPU does the math
    );

    mSpriteBatch->Draw(
        mTextureSRV.Get(),
        worldPos,   // world-space position; the matrix handles the conversion
        &srcRect,
        DirectX::Colors::White,
        0.0f,       // per-sprite rotation (independent of camera rotation)
        origin,     // pivot in source-rect-local pixels (from JSON)
        scale       // artist scale; camera zoom is already inside the VP matrix
    );

    mSpriteBatch->End();
}

// ============================================================
// Shutdown
// ============================================================
void WorldRenderer::Shutdown()
{
    mSpriteBatch.reset();
    mTextureSRV.Reset();
    mActiveClip     = nullptr;
    mActiveClipName.clear();
    mFrameIndex  = 0;
    mFrameTimer  = 0.0f;
    LOG("[WorldRenderer] Shutdown complete.");
}
