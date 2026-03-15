// ============================================================
// File: NineSliceRenderer.cpp
// ============================================================
#include "NineSliceRenderer.h"
#include "../Utils/Log.h"
#include "../Utils/JsonLoader.h"
#include <WICTextureLoader.h>
#include <fstream>
#include <sstream>

// Small helper specific to this module to keep JsonLoader.h clean.
static bool LoadNineSliceDataFromFile(const std::string& path, NineSliceData& out)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        LOG("[NineSliceRenderer] Cannot open NineSliceData file: '%s'", path.c_str());
        return false;
    }

    std::stringstream buf;
    buf << file.rdbuf();
    const std::string src = buf.str();

    out.width = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "width"));
    out.height = JsonLoader::detail::ParseInt(JsonLoader::detail::ValueOf(src, "height"));

    // JsonLoader::ValueOf doesn't handle objects well, so we take a substring starting at the object key
    size_t cropRegionPos = src.find("\"crop-region\"");
    std::string cropRegionStr = (cropRegionPos != std::string::npos) ? src.substr(cropRegionPos) : src;
    out.cropRegion.left = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(cropRegionStr, "left"));
    out.cropRegion.top = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(cropRegionStr, "top"));
    out.cropRegion.right = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(cropRegionStr, "right"));
    out.cropRegion.bottom = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(cropRegionStr, "bottom"));

    size_t nineSlicePos = src.find("\"nine-slice\"");
    std::string nineSliceStr = (nineSlicePos != std::string::npos) ? src.substr(nineSlicePos) : src;
    out.nineSlice.left = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(nineSliceStr, "left"));
    out.nineSlice.top = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(nineSliceStr, "top"));
    out.nineSlice.right = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(nineSliceStr, "right"));
    out.nineSlice.bottom = JsonLoader::detail::ParseFloat(JsonLoader::detail::ValueOf(nineSliceStr, "bottom"));

    // Fallback if data is 0 (parsing failed)
    if (out.cropRegion.bottom == 0) out.cropRegion = {16, 80, 231, 188};
    if (out.nineSlice.bottom == 0) out.nineSlice = {46, 110, 197, 152};

    LOG("[NineSliceRenderer] Loaded NineSliceData from '%s'.", path.c_str());
    return true;
}

bool NineSliceRenderer::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, const std::wstring& texturePath, const std::string& jsonPath, int screenW, int screenH)
{
    Shutdown();

    mScreenW = screenW;
    mScreenH = screenH;

    HRESULT hr = DirectX::CreateWICTextureFromFileEx(
        device,
        context,
        texturePath.c_str(),
        0,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_SHADER_RESOURCE,
        0,
        0,
        DirectX::WIC_LOADER_IGNORE_SRGB,
        nullptr,
        mTextureSRV.GetAddressOf()
    );
    if (FAILED(hr))
    {
        LOG("[NineSliceRenderer] Failed to load texture: %ls", texturePath.c_str());
        return false;
    }

    if (!LoadNineSliceDataFromFile(jsonPath, mData))
    {
        LOG("[NineSliceRenderer] Failed to load JSON data: %s", jsonPath.c_str());
        return false;
    }

    mSpriteBatch = std::make_unique<DirectX::SpriteBatch>(context);
    mStates = std::make_unique<DirectX::CommonStates>(device);

    return true;
}

void NineSliceRenderer::Draw(ID3D11DeviceContext* context, float destX, float destY, 
                             float targetWidth, float targetHeight, float scale, DirectX::CXMMATRIX transform)
{
    if (!mSpriteBatch || !mTextureSRV) return;

    // Source rects logic
    float cx = mData.cropRegion.left;
    float cy = mData.cropRegion.top;
    float cw = mData.cropRegion.right - mData.cropRegion.left;
    float ch = mData.cropRegion.bottom - mData.cropRegion.top;

    float ml = mData.nineSlice.left - mData.cropRegion.left;
    float mr = mData.cropRegion.right - mData.nineSlice.right;
    float mt = mData.nineSlice.top - mData.cropRegion.top;
    float mb = mData.cropRegion.bottom - mData.nineSlice.bottom;
    
    float centerW = cw - ml - mr;
    float centerH = ch - mt - mb;

    // Dest rects logic
    float dl = ml * scale;
    float dr = mr * scale;
    float dt = mt * scale;
    float db = mb * scale;

    // 9 Source Rects
    RECT srcLeftTop     = { (LONG)cx,              (LONG)cy,              (LONG)(cx + ml),        (LONG)(cy + mt) };
    RECT srcCenterTop   = { (LONG)(cx + ml),       (LONG)cy,              (LONG)(cx + ml + centerW),(LONG)(cy + mt) };
    RECT srcRightTop    = { (LONG)(cx + cw - mr),  (LONG)cy,              (LONG)(cx + cw),        (LONG)(cy + mt) };

    RECT srcLeftMid     = { (LONG)cx,              (LONG)(cy + mt),       (LONG)(cx + ml),        (LONG)(cy + mt + centerH) };
    RECT srcCenterMid   = { (LONG)(cx + ml),       (LONG)(cy + mt),       (LONG)(cx + ml + centerW),(LONG)(cy + mt + centerH) };
    RECT srcRightMid    = { (LONG)(cx + cw - mr),  (LONG)(cy + mt),       (LONG)(cx + cw),        (LONG)(cy + mt + centerH) };

    RECT srcLeftBot     = { (LONG)cx,              (LONG)(cy + ch - mb),  (LONG)(cx + ml),        (LONG)(cy + ch) };
    RECT srcCenterBot   = { (LONG)(cx + ml),       (LONG)(cy + ch - mb),  (LONG)(cx + ml + centerW),(LONG)(cy + ch) };
    RECT srcRightBot    = { (LONG)(cx + cw - mr),  (LONG)(cy + ch - mb),  (LONG)(cx + cw),        (LONG)(cy + ch) };

    // 9 Dest Rects
    RECT dstLeftTop     = { (LONG)destX,                      (LONG)destY,                      (LONG)(destX + dl),               (LONG)(destY + dt) };
    RECT dstCenterTop   = { (LONG)(destX + dl),               (LONG)destY,                      (LONG)(destX + targetWidth - dr), (LONG)(destY + dt) };
    RECT dstRightTop    = { (LONG)(destX + targetWidth - dr), (LONG)destY,                      (LONG)(destX + targetWidth),      (LONG)(destY + dt) };

    RECT dstLeftMid     = { (LONG)destX,                      (LONG)(destY + dt),               (LONG)(destX + dl),               (LONG)(destY + targetHeight - db) };
    RECT dstCenterMid   = { (LONG)(destX + dl),               (LONG)(destY + dt),               (LONG)(destX + targetWidth - dr), (LONG)(destY + targetHeight - db) };
    RECT dstRightMid    = { (LONG)(destX + targetWidth - dr), (LONG)(destY + dt),               (LONG)(destX + targetWidth),      (LONG)(destY + targetHeight - db) };

    RECT dstLeftBot     = { (LONG)destX,                      (LONG)(destY + targetHeight - db),(LONG)(destX + dl),               (LONG)(destY + targetHeight) };
    RECT dstCenterBot   = { (LONG)(destX + dl),               (LONG)(destY + targetHeight - db),(LONG)(destX + targetWidth - dr), (LONG)(destY + targetHeight) };
    RECT dstRightBot    = { (LONG)(destX + targetWidth - dr), (LONG)(destY + targetHeight - db),(LONG)(destX + targetWidth),      (LONG)(destY + targetHeight) };

    BindViewport(context);
    mSpriteBatch->Begin(DirectX::SpriteSortMode_Deferred, mStates->AlphaBlend(), mStates->PointClamp(), mStates->DepthNone(), nullptr, nullptr, transform);



    DirectX::XMVECTOR color = DirectX::Colors::White;

    mSpriteBatch->Draw(mTextureSRV.Get(), dstLeftTop,   &srcLeftTop,   color);
    mSpriteBatch->Draw(mTextureSRV.Get(), dstCenterTop, &srcCenterTop, color);
    mSpriteBatch->Draw(mTextureSRV.Get(), dstRightTop,  &srcRightTop,  color);

    mSpriteBatch->Draw(mTextureSRV.Get(), dstLeftMid,   &srcLeftMid,   color);
    mSpriteBatch->Draw(mTextureSRV.Get(), dstCenterMid, &srcCenterMid, color);
    mSpriteBatch->Draw(mTextureSRV.Get(), dstRightMid,  &srcRightMid,  color);

    mSpriteBatch->Draw(mTextureSRV.Get(), dstLeftBot,   &srcLeftBot,   color);
    mSpriteBatch->Draw(mTextureSRV.Get(), dstCenterBot, &srcCenterBot, color);
    mSpriteBatch->Draw(mTextureSRV.Get(), dstRightBot,  &srcRightBot,  color);

    mSpriteBatch->End();
}

void NineSliceRenderer::Shutdown()
{
    mSpriteBatch.reset();
    mStates.reset();
    mTextureSRV.Reset();
}

void NineSliceRenderer::BindViewport(ID3D11DeviceContext* context) {
    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(mScreenW);
    vp.Height   = static_cast<float>(mScreenH);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    context->RSSetViewports(1, &vp);
}
