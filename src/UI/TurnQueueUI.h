// ============================================================
// File: TurnQueueUI.h
// Responsibility: Displays the simulated future Action Queue UI in BattleState.
//                 Top-left, vertical, bottom-up (top = next actor).
// ============================================================
#pragma once

#include <vector>
#include <map>
#include <string>
#include <d3d11.h>
#include <wrl/client.h>
#include <SpriteBatch.h>
#include <CommonStates.h>
#include "../Utils/JsonLoader.h"
#include "../Battle/IBattler.h"

class TurnQueueUI
{
public:
    TurnQueueUI() = default;
    ~TurnQueueUI() { Shutdown(); }

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, 
                    const std::string& configPath,
                    const std::wstring& bgPath, const std::wstring& framePath,
                    int screenW, int screenH);

    void Shutdown();
    void SetScreenSize(int screenW, int screenH);

    // Call each frame to rebuild queue if things change, or just update rendering state
    void UpdateQueue(const std::vector<IBattler*>& anticipatedQueue);
    
    // Animate
    void Update(float dt);

    void Render(ID3D11DeviceContext* context);

private:
    float mConfigReloadTimer = 0.0f;
    std::string mConfigPath;
    struct QueueNode {
        IBattler* battler = nullptr;
        std::wstring portraitPath;
        float currentY = 0.0f;
        float targetY = 0.0f;
        float currentX = 0.0f;
        float targetX = 0.0f;
        float currentScale = 0.0f;
        float targetScale = 0.0f;
        float alpha = 1.0f;
        bool matched = false;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };

    std::vector<QueueNode> mNodes;
    std::vector<QueueNode> mFadingNodes;
    std::map<std::wstring, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> mTextureCache;
    JsonLoader::TurnViewConfig mConfig;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetTexture(const std::wstring& path);

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mBgSRV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFrameSRV;
    std::unique_ptr<DirectX::SpriteBatch>            mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates>           mStates;

    ID3D11Device* mDevice = nullptr;
    ID3D11DeviceContext* mContext = nullptr;
    
    int mScreenW = 1280;
    int mScreenH = 720;
    
    void BindViewport(ID3D11DeviceContext* context);
};
