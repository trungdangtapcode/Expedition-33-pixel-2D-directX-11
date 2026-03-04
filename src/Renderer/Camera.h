// ============================================================
// File: Camera.h
// Responsibility: 2-D game camera — produces a View × Projection matrix
//                 that is passed directly to the GPU (SpriteBatch transform
//                 or a custom Vertex Shader constant buffer).
//
// Why GPU matrices instead of CPU WorldToScreen()?
//   The old approach called WorldToScreen() for every object per frame on
//   the CPU.  With 1000 grass blades, 200 enemies, and 500 particles that
//   is 1700 multiplications the CPU must complete before issuing draw calls.
//   With the matrix approach:
//     1. Build ONE 4x4 ViewProj matrix on the CPU   (done once per frame)
//     2. Pass it to SpriteBatch::Begin(matrix)       (one parameter, zero extra work)
//     3. The GPU Vertex Shader multiplies every vertex (fully parallel)
//   This is the standard pattern in every commercial 2-D engine.
//
// Matrix pipeline (left-hand, row-major — matches DirectXMath convention):
//
//   World space  -[T(-pos)]->  Camera-local space
//              -[R(-rot)]->   Rotated view space
//              -[S(zoom)]->   Zoomed view space
//              -[T(W/2,H/2)]-> Screen origin at top-left
//              -[OrthoProj]-> Clip space
//
//   ViewMatrix  = T(-pos) * R(-rot) * S(zoom) * T(screenW/2, screenH/2)
//   ProjMatrix  = XMMatrixOrthographicOffCenterLH(0, W, H, 0, 0.1, 1000)
//   ViewProj    = ViewMatrix * ProjMatrix
//
// SpriteBatch integration (no custom shader needed):
//   SpriteBatch::Begin(SpriteSortMode_Deferred, nullptr, nullptr, nullptr,
//                      nullptr, nullptr, camera.GetViewProjectionMatrix());
//   Every sprite vertex is multiplied by ViewProj entirely on the GPU.
//   CPU submits positions in world units — no manual per-object subtraction.
//
// Dirty flag optimization:
//   Matrices are only recomputed when position/zoom/rotation/screen size
//   actually changes.  A stationary camera during a battle cutscene never
//   re-does the 4x4 multiplication.
//
// Coordinate convention:
//   World origin (0,0) maps to the screen center when camera pos = (0,0).
//   +X right, +Y down (matches SpriteBatch top-left pixel origin).
//   zoom > 1 = zoomed in, zoom < 1 = zoomed out.
//
// Common mistakes:
//   1. Forgetting Update() after SetPosition/SetZoom/Follow — the dirty
//      flag defers the matrix rebuild; old matrix is returned until Update().
//   2. Passing Camera2D to UIRenderer — UI is screen-space, must NOT use
//      the camera transform.
//   3. Using XMMatrixOrthographicLH (centered) instead of OffCenterLH —
//      OffCenterLH with (0,W,H,0) matches SpriteBatch's top-left origin.
// ============================================================
#pragma once
#include <DirectXMath.h>

// ============================================================
// Class: Camera2D
// ============================================================
class Camera2D
{
public:
    // ------------------------------------------------------------
    // Constructor
    // Purpose:
    //   Initialize all camera state to neutral values and immediately
    //   build the first View and Projection matrices so the camera is
    //   safe to use without an explicit Update() call after construction.
    // Parameters:
    //   screenWidth, screenHeight — initial render target size in pixels.
    // ------------------------------------------------------------
    Camera2D(int screenWidth, int screenHeight)
        : mPosition{ 0.0f, 0.0f }
        , mZoom(1.0f)
        , mRotation(0.0f)
        , mScreenW(screenWidth)
        , mScreenH(screenHeight)
        , mDirty(true)
    {
        // Build initial matrices so GetViewProjectionMatrix() is safe
        // to call immediately after construction.
        RebuildProjection();
        RebuildView();
        mDirty = false;
    }

    // ------------------------------------------------------------
    // Function: SetScreenSize
    // Purpose:
    //   Update the orthographic projection when the window is resized.
    //   Marks dirty so Update() will rebuild the combined ViewProj.
    //   Call this from the window resize handler.
    // ------------------------------------------------------------
    void SetScreenSize(int screenWidth, int screenHeight)
    {
        mScreenW = screenWidth;
        mScreenH = screenHeight;
        // Projection depends only on screen dimensions — rebuild immediately
        // so it is always consistent with mScreenW/H.
        RebuildProjection();
        mDirty = true;  // combined ViewProj needs a fresh multiply in Update()
    }

    // ------------------------------------------------------------
    // Function: Update
    // Purpose:
    //   Recompute the View matrix and combined ViewProj matrix if any
    //   camera property changed since the last call (dirty flag).
    //   Call ONCE per frame, before passing the matrix to the renderer.
    //
    // Why dirty flag?
    //   Matrix multiplication is cheap but pointless when the camera is
    //   still.  During a static dialogue scene this is a free win.
    // ------------------------------------------------------------
    void Update()
    {
        if (!mDirty) return;
        RebuildView();
        mDirty = false;
    }

    // ------------------------------------------------------------
    // Function: Follow
    // Purpose:
    //   Smoothly move the camera toward a world-space target using
    //   exponential decay (lerp).  Produces the "camera glide" effect
    //   characteristic of 2-D JRPGs.
    //
    // Parameters:
    //   targetX, targetY — world-space position to follow (e.g. player)
    //   smoothing        — catch-up speed coefficient:
    //                        5.0f = fast / snappy
    //                        2.0f = slow / cinematic
    //                      Formula: pos += (target - pos) * smoothing * dt
    //
    // Why lerp and not snap?
    //   pos += (target - pos) * k is frame-rate-independent when k = smoothing * dt.
    //   Snapping (pos = target) produces a 1-frame jump visible at any frame rate.
    //   Note: call Update() after Follow() to rebuild the matrix.
    // ------------------------------------------------------------
    void Follow(float targetX, float targetY, float smoothing, float dt)
    {
        const float k = smoothing * dt;
        mPosition.x += (targetX - mPosition.x) * k;
        mPosition.y += (targetY - mPosition.y) * k;
        mDirty = true;
    }

    // --- Property setters (each marks dirty) ---

    void SetPosition(float x, float y)
    {
        mPosition.x = x;
        mPosition.y = y;
        mDirty = true;
    }

    // Zoom clamp: values <= 0 would invert or collapse the view.
    void SetZoom(float zoom)
    {
        mZoom  = (zoom > 0.01f) ? zoom : 0.01f;
        mDirty = true;
    }

    // Rotation in radians.  Positive angle rotates the visible world
    // counter-clockwise (like an earthquake screen-shake effect).
    void SetRotation(float radians)
    {
        mRotation = radians;
        mDirty    = true;
    }

    // --- Getters ---
    DirectX::XMFLOAT2 GetPosition()  const { return mPosition; }
    float             GetZoom()      const { return mZoom; }
    float             GetRotation()  const { return mRotation; }
    int               GetScreenW()   const { return mScreenW; }
    int               GetScreenH()   const { return mScreenH; }

    // ------------------------------------------------------------
    // Function: GetViewProjectionMatrix
    // Purpose:
    //   Return the combined View x Projection matrix.
    //   Pass this to SpriteBatch::Begin() or upload to a VS constant buffer.
    //   Update() must have been called after any property change.
    // ------------------------------------------------------------
    DirectX::XMMATRIX GetViewProjectionMatrix() const { return mViewProj; }
    DirectX::XMMATRIX GetViewMatrix()           const { return mView; }
    DirectX::XMMATRIX GetProjectionMatrix()     const { return mProj; }

    // ------------------------------------------------------------
    // Function: WorldToScreen  (CPU convenience — NOT the hot path)
    // Purpose:
    //   Convert a single world position to screen pixels via the GPU matrix.
    //   Use for: on-screen culling checks, UI tooltip placement above a
    //   world object, debug overlays.
    //   Do NOT call this inside a per-sprite draw loop — let the GPU do it.
    // ------------------------------------------------------------
    DirectX::XMFLOAT2 WorldToScreen(float worldX, float worldY) const
    {
        DirectX::XMVECTOR pos  = DirectX::XMVectorSet(worldX, worldY, 0.0f, 1.0f);
        DirectX::XMVECTOR clip = DirectX::XMVector4Transform(pos, mViewProj);

        // Perspective divide — w=1 for orthographic, but written explicitly
        // so swapping to a perspective projection still works correctly.
        float w = DirectX::XMVectorGetW(clip);
        if (w == 0.0f) w = 1.0f;
        float ndcX =  DirectX::XMVectorGetX(clip) / w;
        float ndcY =  DirectX::XMVectorGetY(clip) / w;

        // DirectX NDC: X in [-1,+1] left-to-right, Y in [-1,+1] bottom-to-top.
        // Convert to pixel space: top-left = (0,0).
        return {
            ( ndcX + 1.0f) * 0.5f * static_cast<float>(mScreenW),
            (-ndcY + 1.0f) * 0.5f * static_cast<float>(mScreenH)
        };
    }

private:
    // ------------------------------------------------------------
    // RebuildView  (private — called by Update() when dirty)
    //
    // Matrix order (row-major, left-to-right multiplication):
    //   T(-pos)      — move the world so the camera center is at the origin
    //   R(-rotation) — counter-rotate the world by the camera's angle
    //   S(zoom)      — scale: zoom in/out
    //   T(W/2, H/2)  — shift origin to screen center (top-left = pixel 0,0)
    //
    // Combined: mView = T(-pos) * R(-rot) * S(zoom) * T(W/2, H/2)
    //           mViewProj = mView * mProj
    // ------------------------------------------------------------
    void RebuildView()
    {
        using namespace DirectX;

        // Translate the world so the camera position becomes the new origin.
        XMMATRIX trans  = XMMatrixTranslation(-mPosition.x, -mPosition.y, 0.0f);

        // Rotate the world opposite to the camera rotation.
        // Earthquake: camera.SetRotation(sin(time) * 0.05f) → scene shakes.
        XMMATRIX rot    = XMMatrixRotationZ(-mRotation);

        // Scale for zoom.  zoom=2 makes every world unit appear as 2 pixels.
        XMMATRIX scale  = XMMatrixScaling(mZoom, mZoom, 1.0f);

        // Shift the coordinate system origin from world-center to screen top-left.
        // Without this, world (0,0) maps to pixel (0,0) — the top-left corner.
        // With this, world (0,0) maps to pixel (W/2, H/2) — the screen center.
        XMMATRIX center = XMMatrixTranslation(
            static_cast<float>(mScreenW) * 0.5f,
            static_cast<float>(mScreenH) * 0.5f,
            0.0f
        );

        mView     = trans * rot * scale * center;
        mViewProj = mView * mProj;
    }

    // ------------------------------------------------------------
    // RebuildProjection  (private — called by constructor + SetScreenSize)
    //
    // XMMatrixOrthographicOffCenterLH maps:
    //   left=0   → NDC -1 (left screen edge)
    //   right=W  → NDC +1 (right screen edge)
    //   top=0    → NDC +1 (top screen edge,   Y=0 is the top pixel row)
    //   bottom=H → NDC -1 (bottom screen edge, Y grows downward)
    //
    // This matches SpriteBatch's pixel-space convention exactly.
    // Z range [0.1, 1000]: sprites at z=0 are inside the frustum.
    // Near=0 would make the near plane touch the eye, causing z artifacts.
    // ------------------------------------------------------------
    void RebuildProjection()
    {
        mProj = DirectX::XMMatrixOrthographicOffCenterLH(
            0.0f,                            // left edge   = pixel 0
            static_cast<float>(mScreenW),    // right edge  = pixel W
            static_cast<float>(mScreenH),    // bottom edge = pixel H  (Y-down)
            0.0f,                            // top edge    = pixel 0
            0.1f,                            // near Z plane
            1000.0f                          // far  Z plane
        );
    }

    // --- Camera state ---
    DirectX::XMFLOAT2 mPosition;   // world-space position of the camera center
    float             mZoom;        // zoom factor  (1.0 = native pixel size)
    float             mRotation;    // rotation in radians (0 = no rotation)
    int               mScreenW;     // render target width  in pixels
    int               mScreenH;     // render target height in pixels

    // --- Cached GPU matrices ---
    // These are rebuilt lazily via the dirty flag in Update().
    DirectX::XMMATRIX mView;        // view transform
    DirectX::XMMATRIX mProj;        // orthographic projection
    DirectX::XMMATRIX mViewProj;    // mView * mProj — passed to GPU each frame

    // True when any property changed and the matrices need to be rebuilt.
    bool mDirty;
};
