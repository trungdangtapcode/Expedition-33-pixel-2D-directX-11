// ============================================================
// File: ItemIconRenderer.h
// Responsibility: Stretch a caller-supplied texture (typically an
//                 item icon SRV from ItemIconCache) into a
//                 destination box on screen.
//
// Design contrast vs. NineSliceRenderer / SpriteRenderer:
//   Those classes own ONE texture chosen at Initialize() time.
//   ItemIconRenderer instead owns a SpriteBatch + CommonStates
//   only -- the SRV is passed per-Draw().  This lets a single
//   renderer instance drive every item icon in the menu without
//   re-uploading textures.
//
// World-space vs. screen-space:
//   Pass DirectX::XMMatrixIdentity() for screen-space (inventory)
//   and a Camera2D::GetViewMatrix() for world-space (battle item
//   menu anchored to the active player's slot).  SpriteBatch
//   composes the transform with its internal pixel->NDC matrix,
//   matching the convention documented in CLAUDE.md
//   "SpriteBatch + Camera Transform Rule".
//
// Sampling:
//   LinearClamp -- item icons are intended to be smooth-shaded
//   (vials, feathers, gems) per idea/asset-todo.md, so bilinear
//   filtering on stretch is correct.  Other UI sprites use
//   PointClamp because they are pixel art with hard edges.
//
// Owns:
//   SpriteBatch    - queues + flushes draw calls
//   CommonStates   - cached blend/sampler/depth state objects
//
// Lifetime:
//   Created in  -> Owning State::OnEnter() via Initialize().
//   Destroyed in -> Owning State::OnExit() via Shutdown().
//
// Common mistakes:
//   1. Calling Draw() with a SRV from a destroyed device -> crash.
//   2. Forgetting to call SetScreenSize() after a window resize ->
//      the bound viewport stays at the old size, sprites render
//      relative to the wrong NDC mapping.
// ============================================================
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>
#include <CommonStates.h>
#include <memory>

class ItemIconRenderer
{
public:
    bool Initialize(ID3D11Device* device,
                    ID3D11DeviceContext* context,
                    int screenW,
                    int screenH);

    // Draw the texture stretched to fill [destX, destY, destX+destW, destY+destH].
    // Source rect = full texture (no atlasing -- one PNG per icon).
    void Draw(ID3D11DeviceContext* context,
              ID3D11ShaderResourceView* texSRV,
              float destX, float destY, float destW, float destH,
              DirectX::CXMMATRIX transform = DirectX::XMMatrixIdentity(),
              DirectX::XMVECTOR color = DirectX::Colors::White);

    void SetScreenSize(int w, int h) { mScreenW = w; mScreenH = h; }
    void Shutdown();

private:
    void BindViewport(ID3D11DeviceContext* context);

    std::unique_ptr<DirectX::SpriteBatch>  mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;

    int mScreenW = 1280;
    int mScreenH = 720;
};
