// ============================================================
// File: IScreenFilter.h
// Responsibility: Pure virtual interface for fullscreen post-process filters.
//
// Architecture — Interface Segregation (ISP):
//   Any fullscreen effect (pincushion distortion, chromatic aberration,
//   vignette, blur, color grade, etc.) implements this interface.
//   The owning state depends ONLY on IScreenFilter* — never on the
//   concrete class.  Swapping or stacking filters requires zero changes
//   in the state.
//
// Usage pattern (render loop):
//
//   // In state Render():
//   if (mFilter->IsActive()) {
//       mFilter->BeginCapture(ctx);   // redirect RT to offscreen texture
//   }
//   // ... draw all normal scene content here ...
//   if (mFilter->IsActive()) {
//       mFilter->EndCapture(ctx);     // restore original RT
//       mFilter->Render(ctx);         // draw fullscreen quad with filter PS
//   }
//
//   // In state Update():
//   mFilter->Update(dt, intensity);  // drive animated effects (e.g., ramp-up)
//
// Lifetime contract:
//   Initialize(device, w, h) — called once after D3D device is available.
//   Shutdown()               — called once before the device is destroyed.
//   Between Initialize and Shutdown, BeginCapture/EndCapture/Render/Update
//   may be called every frame.
//
// Common mistakes:
//   1. Calling BeginCapture without a matching EndCapture — RT left bound
//      as the offscreen texture; subsequent draws go to wrong target.
//   2. Calling Render() before EndCapture() — samples from a texture that
//      is still bound as the render target → D3D11 debug warning + blank.
//   3. Forgetting to call Update(dt) — animated effects freeze at frame 0.
// ============================================================
#pragma once
#include <d3d11.h>

// ------------------------------------------------------------
// IScreenFilter — fullscreen post-process filter interface.
// ------------------------------------------------------------
class IScreenFilter
{
public:
    virtual ~IScreenFilter() = default;

    // ----------------------------------------------------------------
    // Initialize — allocate all GPU resources needed by the filter.
    // Parameters:
    //   device  — D3D11 device
    //   width   — render target width in pixels
    //   height  — render target height in pixels
    // Returns true on success; false if any resource creation failed.
    // ----------------------------------------------------------------
    virtual bool Initialize(ID3D11Device* device, int width, int height) = 0;

    // Release all GPU resources.  Safe to call even if Initialize failed.
    virtual void Shutdown() = 0;

    // ----------------------------------------------------------------
    // BeginCapture — redirect rendering to the filter's offscreen texture.
    //
    // After this call, all draw calls go to the internal offscreen RT.
    // The caller MUST call EndCapture() before Render() or before
    // presenting the back buffer.
    // ----------------------------------------------------------------
    virtual void BeginCapture(ID3D11DeviceContext* ctx) = 0;

    // ----------------------------------------------------------------
    // EndCapture — restore the original (back buffer) render target.
    //
    // Must be called after BeginCapture and before Render().
    // ----------------------------------------------------------------
    virtual void EndCapture(ID3D11DeviceContext* ctx) = 0;

    // ----------------------------------------------------------------
    // Render — draw the fullscreen quad using the captured scene texture
    //   and the filter's pixel shader.
    //
    // Call this AFTER EndCapture() and after all normal scene draws.
    // Typically the last draw call before the iris overlay and Present.
    // ----------------------------------------------------------------
    virtual void Render(ID3D11DeviceContext* ctx) = 0;

    // ----------------------------------------------------------------
    // Update — advance filter animation state.
    // Parameters:
    //   dt        — frame delta-time in seconds
    //   intensity — current effect intensity in [0.0, 1.0].
    //               0.0 = no distortion (pass-through).
    //               1.0 = maximum distortion as defined by the filter.
    // ----------------------------------------------------------------
    virtual void Update(float dt, float intensity) = 0;

    // ----------------------------------------------------------------
    // IsActive — returns true when the filter should be applied this frame.
    //
    // Filters may implement a dead-zone: when intensity is below a threshold
    // (e.g., < 0.001f) they declare themselves inactive to skip the
    // BeginCapture/EndCapture overhead on frames where no effect is visible.
    // ----------------------------------------------------------------
    virtual bool IsActive() const = 0;
};
