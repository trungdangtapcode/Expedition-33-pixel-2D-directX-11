// ============================================================
// File: PointerRenderer.cpp
// ============================================================
#include "PointerRenderer.h"
#include "../Utils/Log.h"
#include "../Utils/JsonLoader.h"
#include <WICTextureLoader.h>
#include <fstream>
#include <sstream>
#include <cmath>

bool PointerRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, 
                                 const std::wstring& texturePath, const std::string& jsonPath,
                                 int screenW, int screenH)
{
    Shutdown();

    mScreenW = screenW;
    mScreenH = screenH;

    HRESULT hr = DirectX::CreateWICTextureFromFile(device, texturePath.c_str(), nullptr, mTextureSRV.GetAddressOf());
    if (FAILED(hr))
    {
        LOG("[PointerRenderer] Failed to load texture: %ls", texturePath.c_str());
        return false;
    }

    std::ifstream file(jsonPath);
    if (file.is_open())
    {
        std::stringstream buf;
        buf << file.rdbuf();
        const std::string src = buf.str();

        mWidth = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "width"), 64);
        mHeight = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "height"), 64);
        
        // Very basic array parsing for pivot [32, 32]
        size_t pivotStart = src.find("pivot");
        if (pivotStart != std::string::npos)
        {
            size_t openBracket = src.find('[', pivotStart);
            size_t comma = src.find(',', openBracket);
            size_t closeBracket = src.find(']', comma);
            
            if (openBracket != std::string::npos && comma != std::string::npos && closeBracket != std::string::npos)
            {
                std::string pxStr = src.substr(openBracket + 1, comma - openBracket - 1);
                std::string pyStr = src.substr(comma + 1, closeBracket - comma - 1);
                mPivotX = std::stof(pxStr);
                mPivotY = std::stof(pyStr);
            }
        }

        mOffsetY = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "y_offset"), -128.0f);
        mBobSpeed = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "bob_speed"), 0.0f);
        mBobAmplitude = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(src, "bob_amplitude"), 0.0f);
    }
    else
    {
        LOG("[PointerRenderer] WARNING: Failed to load %s, using defaults.", jsonPath.c_str());
    }

    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    mStates = std::make_unique<DirectX::CommonStates>(device);

    return true;
}

void PointerRenderer::Update(float dt)
{
    mElapsedTime += dt;
}

void PointerRenderer::BindViewport(ID3D11DeviceContext* context)
{
    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(mScreenW);
    vp.Height   = static_cast<float>(mScreenH);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    context->RSSetViewports(1, &vp);
}

void PointerRenderer::Draw(ID3D11DeviceContext* context, float worldX, float worldY, DirectX::CXMMATRIX transform)
{
    if (!mSpriteBatch || !mTextureSRV) return;

    float yOffset = std::sin(mElapsedTime * mBobSpeed) * mBobAmplitude;

    BindViewport(context);
    mSpriteBatch->Begin(DirectX::SpriteSortMode_Deferred, mStates->AlphaBlend(), mStates->PointClamp(), mStates->DepthNone(), nullptr, nullptr, transform);

    RECT srcRect = { 0, 0, mWidth, mHeight };
    DirectX::XMFLOAT2 origin(mPivotX, mPivotY);
    
    DirectX::XMFLOAT2 pos(worldX, worldY + mOffsetY + yOffset);

    mSpriteBatch->Draw(mTextureSRV.Get(), pos, &srcRect, DirectX::Colors::White, 0.0f, origin);

    mSpriteBatch->End();
}

void PointerRenderer::Shutdown()
{
    mSpriteBatch.reset();
    mStates.reset();
    mTextureSRV.Reset();
}
