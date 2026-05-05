#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <SpriteBatch.h>
#include <CommonStates.h>
#include <WICTextureLoader.h>
#include <memory>
#include <string>

class ExpBarRenderer
{
public:
    bool Initialize(ID3D11Device*        device,
                    ID3D11DeviceContext*  context,
                    int screenW, int screenH,
                    float renderX = 0.0f,
                    float renderY = 0.0f);

    void SetExp(int currentExp, int nextLevelExp);
    void SetScreenSize(int w, int h);
    
    // Renders the EXP background and the filled ratio tightly
    void Render(ID3D11DeviceContext* context);

    void Shutdown();

    bool IsInitialized() const { return mSpriteBatch != nullptr; }

private:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mBgSRV;    
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFillSRV;   
    std::unique_ptr<DirectX::SpriteBatch>            mSpriteBatch;
    std::unique_ptr<DirectX::CommonStates>           mStates;

    int mScreenW = 1280;
    int mScreenH = 720;

    int mCurrentExp = 0;
    int mNextLevelExp = 1;

    // Fixed mock asset dimensions
    static constexpr float kBarWidth = 140.0f;
    static constexpr float kBarHeight = 10.0f;

    float mRenderX = 0.0f;
    float mRenderY = 0.0f;

    void BindViewport(ID3D11DeviceContext* context);
};
