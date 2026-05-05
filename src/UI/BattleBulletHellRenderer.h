#pragma once
#include <d3d11.h>
#include <vector>
#include <memory>
#include "../Renderer/NineSliceRenderer.h"
#include "../Renderer/CircleRenderer.h"
#include "../Battle/BattleEvents.h"
#include <SpriteBatch.h>
#include <CommonStates.h>
#include <string>
#include <unordered_map>
#include "../Battle/BattleEvents.h"

class BattleBulletHellRenderer
{
public:
    BattleBulletHellRenderer(ID3D11Device* device, ID3D11DeviceContext* context);
    ~BattleBulletHellRenderer();

    // Invoked by BattleState natively via Events Subscription
    void UpdateState(const BulletHellPayload& payload);

    // Call continuously during HUD rendering FSM Faze
    void Render(ID3D11DeviceContext* context, int screenW, int screenH);

private:
    CircleRenderer mCircleRenderer;
    NineSliceRenderer mFrameRenderer;

    Microsoft::WRL::ComPtr<ID3D11Device> mDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> mContext;

    std::unique_ptr<DirectX::SpriteBatch> mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates> mStates;

    struct LoadedTex {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        float radiusNorm = 1.0f;
    };
    std::unordered_map<std::string, LoadedTex> mTextureCache;
    
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mHeartTex;

    // Local snapshot synced over from Publisher
    BulletHellPayload mLastPayload;
};
