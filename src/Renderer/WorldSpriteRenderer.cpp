// ============================================================
// File: WorldSpriteRenderer.cpp
// Responsibility: Implement WorldSpriteRenderer — animated sprite
//                 drawn at a world-space position, moved by the camera.
//
// Owns: ID3D11ShaderResourceView (texture), SpriteBatch, two D3D states.
//
// Lifetime:
//   Created in  → Initialize()  (PlayState::OnEnter)
//   Destroyed in → Shutdown()   (PlayState::OnExit)
//
// Important:
//   - SpriteBatch::Begin() 7th argument = camera.GetViewMatrix() (world→pixel).
//     Do NOT pass GetViewProjectionMatrix() (world→NDC): SpriteBatch internally
//     multiplies the user matrix by GetViewportTransform() (pixel→NDC), so
//     passing the full ViewProj would double-project and push all sprites off-screen.
//     Correct chain: GetViewMatrix() × GetViewportTransform() = world→pixel→NDC.
//   - mSpriteBatch->SetViewport(vp) MUST be called before Begin().
//     SpriteBatch::GetViewportTransform() calls RSGetViewports() internally;
//     if the D3D11 rasterizer has 0 viewports bound (common during
//     early-frame setup) that call throws std::exception.
//     SetViewport() sets an internal flag that bypasses RSGetViewports.
//   - Depth state must be set to OFF before Begin().  2-D world sprites
//     draw at z=0; any depth left by CircleRenderer's SDF shader will
//     cause the sprite to fail the depth test and become invisible.
//
// Common mistakes:
//   1. Passing screen pixels to Draw() instead of world units — sprite
//      appears thousands of pixels off-screen.
//   2. Forgetting camera.Update() before Draw() — uses the previous
//      frame's ViewProj, causing a 1-frame positional lag.
//   3. Not calling SetViewport() → RSGetViewports() returns count=0
//      → SpriteBatch::End() throws → mInBeginEndPair stays true
//      → every subsequent Begin() asserts "already in a Begin/End pair".
// ============================================================
#include "WorldSpriteRenderer.h"
#include "../Utils/Log.h"
#include "../Utils/HrCheck.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// ------------------------------------------------------------
// Function: Initialize
// Purpose:
//   1. Copy the SpriteSheet descriptor.
//   2. Upload the PNG texture to GPU via WICTextureLoader.
//   3. Create SpriteBatch bound to 'context'.
//   4. Create depth-off and alpha-blend D3D11 states.
//
// Why create states here instead of inline in Draw()?
//   Creating D3D11 state objects is an allocating GPU call.
//   Doing it every frame would stall the GPU pipeline and produce
//   an unbounded number of live state objects (DX debug will report them).
//   Creating once in Initialize() → zero per-frame allocation.
// ------------------------------------------------------------
bool WorldSpriteRenderer::Initialize(ID3D11Device*        device,
                                      ID3D11DeviceContext*  context,
                                      const std::wstring&  texturePath,
                                      const SpriteSheet&   sheet)
{
    mSheet = sheet;

    // ----------------------------------------------------------------
    // Step 1 — Upload PNG to GPU.
    // WIC_LOADER_IGNORE_SRGB: bypass the automatic sRGB gamma conversion.
    // Without this flag, WIC detects the embedded sRGB ICC profile in the PNG,
    // promotes the format to R8G8B8A8_UNORM_SRGB, and the GPU linearises the
    // pixel values before they reach the UNORM backbuffer — all colors appear
    // darker than their source values (e.g. #B5E61D becomes #76CA03 on screen).
    // IGNORE_SRGB loads raw 8-bit values as-is, matching the artist's intent.
    // ----------------------------------------------------------------
    HRESULT hr = CreateWICTextureFromFileEx(
        device,
        context,
        texturePath.c_str(),
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0, 0,
        WIC_LOADER_IGNORE_SRGB,
        nullptr,            // don't need the raw ID3D11Resource handle
        mTextureSRV.GetAddressOf()
    );
    if (FAILED(hr))
    {
        LOG("[WorldSpriteRenderer] ERROR — CreateWICTextureFromFile failed (0x%08X) for texture.", hr);
        return false;
    }
    LOG("[WorldSpriteRenderer] Texture uploaded to GPU.");

    // ----------------------------------------------------------------
    // Step 2 — Create SpriteBatch.
    // SpriteBatch stores the device context pointer internally.
    // All Begin/Draw/End calls must use the SAME context.
    // ----------------------------------------------------------------
    mSpriteBatch = std::make_unique<SpriteBatch>(context);
    LOG("[WorldSpriteRenderer] SpriteBatch created.");

    // ----------------------------------------------------------------
    // Step 3 — Depth-stencil state: depth test OFF, depth write OFF.
    // Why OFF?
    //   Sprites are 2-D quads drawn at z=0 in clip space.  If depth
    //   test is ON, any opaque geometry drawn earlier (CircleRenderer
    //   fills the depth buffer at varying z values via its SDF shader)
    //   will occlude the sprites, making them invisible.
    //   Turning depth test OFF ensures sprites always appear on top of
    //   whatever was drawn before them in the same frame — correct for
    //   a 2-D layer-order model.
    // ----------------------------------------------------------------
    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable    = FALSE;   // disable depth comparison
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;  // no depth writes either
    dsDesc.StencilEnable  = FALSE;

    hr = device->CreateDepthStencilState(&dsDesc, mDepthNone.GetAddressOf());
    CHECK_HR(hr, "WorldSpriteRenderer: CreateDepthStencilState (depth OFF) failed");
    if (FAILED(hr)) return false;

    // ----------------------------------------------------------------
    // Step 4 — Alpha-blend state: standard SRC_ALPHA / INV_SRC_ALPHA.
    // Passed explicitly to SpriteBatch::Begin() so SpriteBatch never
    // inherits whatever blend state was left active by other renderers.
    // ----------------------------------------------------------------
    D3D11_BLEND_DESC blendDesc           = {};
    blendDesc.RenderTarget[0].BlendEnable           = TRUE;
    blendDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = device->CreateBlendState(&blendDesc, mAlphaBlend.GetAddressOf());
    CHECK_HR(hr, "WorldSpriteRenderer: CreateBlendState (alpha blend) failed");
    if (FAILED(hr)) return false;

    LOG("[WorldSpriteRenderer] Initialize complete — sheet '%s', %d clip(s).",
        mSheet.spriteName.c_str(),
        static_cast<int>(mSheet.animations.size()));
    return true;
}

// ------------------------------------------------------------
// Function: PlayClip
// Purpose:  Switch to the named animation clip.
//           Resets frame index and timer to the beginning of the clip.
// Why no-op on same clip?
//   A game state may call PlayClip("idle") every frame to keep the
//   character in idle.  Resetting the frame index each time would
//   restart the animation from frame 0 each frame — a frozen sprite.
// ------------------------------------------------------------
void WorldSpriteRenderer::PlayClip(const std::string& clipName)
{
    // Guard: skip the reset if this clip is already playing.
    if (mActiveClipName == clipName && mActiveClip != nullptr) return;

    const AnimationClip* clip = mSheet.FindClip(clipName);
    if (!clip)
    {
        LOG("[WorldSpriteRenderer] Clip '%s' not found in sheet '%s'.",
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
//   Accumulate dt into mFrameTimer.
//   When the accumulated time reaches the frame duration (1/frameRate),
//   advance mFrameIndex and subtract the duration from the timer.
//
// Why a while loop instead of an if?
//   If dt is large (e.g. after an OS sleep or debugger pause), dt may
//   exceed MULTIPLE frame durations.  An if-check would only advance
//   one frame, skipping the rest.  The while loop catches up correctly.
//
// Non-loop clips hold the last frame (mFrameIndex stays at numFrames-1)
// and stop accumulating time once the clip finishes.
// ------------------------------------------------------------
void WorldSpriteRenderer::Update(float dt)
{
    // Nothing to animate on a single-frame clip or when no clip is active.
    if (!mActiveClip || mActiveClip->numFrames <= 1) return;

    mFrameTimer += dt;

    // Duration of one frame in seconds.
    const float frameDuration = 1.0f / mActiveClip->frameRate;

    while (mFrameTimer >= frameDuration)
    {
        mFrameTimer -= frameDuration;   // carry the remainder to the next frame
        mFrameIndex++;

        if (mFrameIndex >= mActiveClip->numFrames)
        {
            if (mActiveClip->loop)
            {
                mFrameIndex = 0;   // wrap back to the start for looping clips
            }
            else
            {
                // Hold on the last frame for non-looping clips (e.g. death animation).
                mFrameIndex = mActiveClip->numFrames - 1;
                mFrameTimer = 0.0f;   // prevent further accumulation
                break;
            }
        }
    }
}

// ------------------------------------------------------------
// Function: Draw
// Purpose:
//   Issue one SpriteBatch draw call.
//
//   The sprite's world position (worldX, worldY) is where the JSON pivot
//   will land on screen after the GPU applies the camera's ViewProj matrix.
//   For a "bottom-center" sprite (pivot=[frameW/2, frameH]), this means:
//     - The foot of the character lands exactly at (worldX, worldY).
//     - The sprite extends frame_width/2 to the left and right.
//     - The sprite extends frame_height upward.
//   This matches the JRPG convention of placing a character by its feet.
//
// How the world → screen transform works:
//   SpriteBatch::Begin() accepts an optional XMMATRIX as the 7th argument,
//   stored as mTransformMatrix.  In PrepareForRendering(), SpriteBatch computes:
//
//     CB0 = mTransformMatrix * GetViewportTransform(deviceContext, mRotation)
//
//   GetViewportTransform() maps pixel space → NDC:
//     scale( 2/W, -2/H ) + translate( -1, +1 )
//
//   Because mRotation is initialized to DXGI_MODE_ROTATION_IDENTITY (not
//   DXGI_MODE_ROTATION_UNSPECIFIED), SpriteBatch ALWAYS applies this multiply.
//
//   Therefore the correct matrix to supply is camera.GetViewMatrix()
//   (world → screen pixels), NOT GetViewProjectionMatrix() (world → NDC).
//   Supplying ViewProj here would produce:
//     ViewProj(world→NDC) × GetViewportTransform(pixel→NDC) = double projection
//   which maps world position (400, 200) to an NDC value far outside [-1,+1],
//   making the sprite invisible due to hardware clip rejection.
//
//   Correct pipeline:
//     vertex_world_px × GetViewMatrix()  →  screen pixel space
//     screen pixel space × GetViewportTransform()  →  NDC  ✓
//
// SetViewport bypass:
//   SpriteBatch normally calls RSGetViewports() inside GetViewportTransform()
//   to infer the render target size.  If the rasterizer has 0 viewports bound
//   (which happens when the viewport was set by D3DContext::BeginFrame but
//   subsequently cleared by another renderer's state cleanup), this throws.
//   mSpriteBatch->SetViewport(vp) sets an internal flag that bypasses
//   RSGetViewports entirely.  The viewport is queried fresh every Draw() call
//   so it stays correct after window resize.
// ------------------------------------------------------------
void WorldSpriteRenderer::Draw(ID3D11DeviceContext* context,
                                const Camera2D&     camera,
                                float               worldX,
                                float               worldY,
                                float               scale,
                                bool                flipX)
{
    if (!mActiveClip || !mTextureSRV || !mSpriteBatch) return;

    // ----------------------------------------------------------------
    // Step 1 — Query current viewport so SetViewport gets the real size.
    // We cannot cache this: it changes on every window resize.
    // ----------------------------------------------------------------
    UINT vpCount = 1;
    D3D11_VIEWPORT vp = {};
    context->RSGetViewports(&vpCount, &vp);
    if (vpCount == 0)
    {
        // No viewport bound — nothing visible anyway; skip the draw.
        LOG("[WorldSpriteRenderer] WARNING — RSGetViewports returned 0; skipping Draw.");
        return;
    }

    // Forward the viewport to SpriteBatch so it bypasses its own
    // RSGetViewports() call inside GetViewportTransform().
    mSpriteBatch->SetViewport(vp);

    // ----------------------------------------------------------------
    // Step 2 — Explicitly set depth OFF before Begin().
    // SpriteBatch::Begin() does NOT set depth state on its own when
    // a depthStencilState argument is provided — it binds whatever we
    // pass.  We force it here (outside Begin) as well to be safe, because
    // some DX debug configurations warn if state changes happen mid-Begin.
    // ----------------------------------------------------------------
    context->OMSetDepthStencilState(mDepthNone.Get(), 0);

    // ----------------------------------------------------------------
    // Step 3 — Build the source RECT for the current animation frame.
    //
    // Atlas layout convention (one clip per row):
    //   animations[0] → row 0  (e.g. "idle"  — frames at y=0..127)
    //   animations[1] → row 1  (e.g. "walk"  — frames at y=128..255)
    //   ...
    //
    // mFrameIndex is the clip-local index [0, numFrames).
    // fpr = framesPerRow = sheetWidth / frameWidth.
    //
    // For a clip that fits within one row (the common case):
    //   col     = mFrameIndex % fpr    ← which column in that row
    //   atlasRow = clip.startRow        ← the clip's dedicated row (from JSON order)
    //
    // For a very wide clip that spans multiple rows (unusual but supported):
    //   atlasRow = clip.startRow + (mFrameIndex / fpr)
    //   col      = mFrameIndex % fpr
    //
    // clip.startRow is set by JsonLoader from the clip's index in the
    // animations array — no hardcoded pixel offsets anywhere.
    // ----------------------------------------------------------------
    const int fpr      = mSheet.framesPerRow();
    const int col      = mFrameIndex % fpr;
    const int atlasRow = mActiveClip->startRow + (mFrameIndex / fpr);

    RECT srcRect = {
        static_cast<LONG>(col      * mSheet.frameWidth),
        static_cast<LONG>(atlasRow * mSheet.frameHeight),
        static_cast<LONG>(col      * mSheet.frameWidth  + mSheet.frameWidth),
        static_cast<LONG>(atlasRow * mSheet.frameHeight + mSheet.frameHeight)
    };

    // ----------------------------------------------------------------
    // Step 4 — Pivot (origin) from JSON — no hardcoding.
    //
    // SpriteBatch::Draw 'origin' is in SOURCE-RECT-LOCAL pixels.
    // JSON pivot=[64,128] on a 128×128 frame means:
    //   x=64  = horizontal center of the frame
    //   y=128 = bottom edge of the frame
    // The GPU places exactly this pixel at the world position (worldX, worldY).
    // ----------------------------------------------------------------
    const XMFLOAT2 origin(
        static_cast<float>(mActiveClip->pivotX),
        static_cast<float>(mActiveClip->pivotY)
    );

    // World-space position where the pivot will land.
    const XMFLOAT2 worldPos(worldX, worldY);

    // ----------------------------------------------------------------
    // Step 5 — SpriteBatch draw pass.
    //
    // camera.GetViewMatrix() (world→pixel) is passed as the 7th arg.
    // SpriteBatch's PrepareForRendering() then computes internally:
    //   CB0 = GetViewMatrix() * GetViewportTransform()
    //       = world→pixel × pixel→NDC
    //       = world→NDC  ✓
    //
    // DO NOT pass GetViewProjectionMatrix() here — that matrix already
    // encodes world→NDC.  Multiplying it by GetViewportTransform() again
    // would produce world→NDC→?? (double projection), pushing the sprite
    // completely off-screen.
    // ----------------------------------------------------------------
    mSpriteBatch->Begin(
        SpriteSortMode_Deferred,
        mAlphaBlend.Get(),   // explicit alpha blend — do not inherit pipeline state
        nullptr,             // sampler: null = SpriteBatch default (linear + wrap)
        mDepthNone.Get(),    // depth state: off — 2-D sprites ignore depth buffer
        nullptr,             // rasterizer: null = SpriteBatch default
        nullptr,             // effect: null = SpriteBatch built-in VS/PS
        camera.GetViewMatrix()   // world → screen pixels; SpriteBatch adds pixel → NDC
    );

    mSpriteBatch->Draw(
        mTextureSRV.Get(),
        worldPos,
        &srcRect,
        Colors::White,  // tint: white = no tint, draw as-is
        0.0f,           // rotation in radians: 0 = upright
        origin,         // pivot from JSON — encodes all alignment information
        scale,          // uniform scale (1.0 = native pixel size)
        // Horizontal flip for left-facing movement.
        // The default sprite faces RIGHT (positive X direction).
        // SpriteEffects_FlipHorizontally mirrors the source rect in U-space:
        //   U' = 1 - U   (GPU handles this; zero CPU cost)
        // SpriteEffects_None is the zero-value so there is no branch cost
        // when flipX is false.
        flipX ? SpriteEffects_FlipHorizontally : SpriteEffects_None
    );

    mSpriteBatch->End();
}

// ------------------------------------------------------------
// Function: Shutdown
// Purpose:
//   Explicitly release all GPU resources.
//   Called from PlayState::OnExit() — before the D3D device is destroyed.
//   Smart pointers would release on destructor, but calling Shutdown()
//   makes the release order deterministic and explicit.
// Leak consequence if omitted:
//   ID3D11Debug::ReportLiveDeviceObjects() reports:
//     1 SRV  (+ underlying Texture2D)
//     1 Buffer (SpriteBatch internal VB/IB/CB)
//     1 DepthStencilState
//     1 BlendState
// ------------------------------------------------------------
void WorldSpriteRenderer::Shutdown()
{
    mSpriteBatch.reset();   // destroys SpriteBatch — flushes pending draw calls first
    mTextureSRV.Reset();    // releases SRV; GPU texture freed when ref-count hits 0
    mDepthNone.Reset();
    mAlphaBlend.Reset();

    mActiveClip     = nullptr;
    mActiveClipName.clear();
    mFrameIndex = 0;
    mFrameTimer = 0.0f;

    LOG("[WorldSpriteRenderer] Shutdown complete.");
}
