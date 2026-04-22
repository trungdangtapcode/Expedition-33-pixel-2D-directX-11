// ============================================================
// File: BattleQTERenderer.h
// Responsibility: Draw QTE prompts (diamond border + letter) via Event listener.
// ============================================================
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <SpriteBatch.h>
#include <CommonStates.h>
#include <string>
#include <memory>
#include "../Battle/BattleEvents.h"

// Forward declaration
struct EventData;

class BattleQTERenderer
{
public:
    BattleQTERenderer() = default;

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int screenW, int screenH);
    void SetScreenSize(int w, int h);
    
    // Updates internal visual effects and timers
    void Update(float dt);
    
    // Draws the QTE overlay if active
    void Render(ID3D11DeviceContext* context);
    
    void Shutdown();

private:
    void OnQteUpdate(const EventData& data);

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFrameSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mWhiteFillSRV;
    std::unique_ptr<DirectX::SpriteBatch>            mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates>           mStates;

    int mScreenW = 1280;
    int mScreenH = 720;
    
    int mQteListenerID = -1;
    QTEStatePayload mState;
    float mResultTimer = 0.0f;
    QTEResult mLastResult = QTEResult::None;
    
    // To flash red/white on perfect/fail individually per node
    float mFlashTimers[8] = {0};

    // Random on-screen locations for each QTE in the sequence
    std::vector<DirectX::XMFLOAT2> mQtePositions;
};
