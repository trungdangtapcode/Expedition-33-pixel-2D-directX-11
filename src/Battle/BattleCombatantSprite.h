// ============================================================
// File: BattleCombatantSprite.h
// Responsibility: Wraps a WorldSpriteRenderer in an IGameObject interface
//                 so it can be managed and Y-sorted by a SceneGraph.
//
// Why:
//   Allows the BattleState to share the exact same SceneGraph architecture
//   as OverworldState, automatically inheriting Y-sorting (painter's algorithm)
//   and eliminating duplicate render-loop boilerplate.
// ============================================================
#pragma once
#include "../Scene/IGameObject.h"
#include "../Renderer/WorldSpriteRenderer.h"
#include "../Renderer/Camera.h"

class BattleCombatantSprite : public IGameObject
{
public:
    BattleCombatantSprite(ID3D11Device* device, ID3D11DeviceContext* ctx,
                          const std::wstring& texPath, const SpriteSheet& sheet,
                          const Camera2D* camera, 
                          float worldX, float worldY, float scale, bool flipX)
        : mCamera(camera), mWorldX(worldX), mWorldY(worldY), mScale(scale), mFlipX(flipX) 
    {
        mRenderer.Initialize(device, ctx, texPath, sheet);
    }

    void Update(float dt) override
    {
        mRenderer.Update(dt);
    }

    void Render(ID3D11DeviceContext* ctx) override
    {
        // Delegate to WorldSpriteRenderer. SceneGraph ensures this is called
        // in correct Y-sorted order.
        mRenderer.Draw(ctx, *mCamera, mWorldX, mWorldY, mScale, mFlipX);
    }

    int GetLayer() const override { return 50; } // Same default layer as world characters
    
    // SortY drives the painter's algorithm. Higher Y = drawn last = appears in front.
    float GetSortY() const override { return mWorldY; }
    
    bool IsAlive() const override { return true; } // Combatant slots live until End of Battle

    // Accessors for BattleRenderer to command the underlying sprite
    bool PlayClip(const std::string& clip) { return mRenderer.PlayClip(clip); }
    void FreezeCurrentFrame() { mRenderer.FreezeCurrentFrame(); }
    bool IsClipDone() const { return mRenderer.IsClipDone(); }
    void SetDrawOffset(float offX, float offY) {
        mDrawOffsetX = offX;
        mDrawOffsetY = offY;
        mRenderer.SetDrawOffset(offX, offY); 
    }
    void GetDrawOffset(float& x, float& y) const {
        x = mDrawOffsetX;
        y = mDrawOffsetY;
    }

private:
    WorldSpriteRenderer mRenderer;
    const Camera2D*     mCamera;
    float               mWorldX;
    float               mWorldY;
    float               mScale;
    bool                mFlipX;
    float               mDrawOffsetX = 0.f;
    float               mDrawOffsetY = 0.f;
};
