# How DirectX 11 + DirectXTK Rendering Works

This document explains **every step** of the rendering pipeline used in this project —
from device creation at startup to pixels appearing on screen each frame.
It covers DirectX 11 internals, DirectXTK helpers (SpriteBatch), the Camera2D math,
and the strict ownership rules that prevent double-present bugs.

---

## Table of Contents

1. [Big Picture — The Pipeline in One Diagram](#1-big-picture)
2. [D3D11 Initialization — What Gets Created and Why](#2-d3d11-initialization)
3. [The Per-Frame Render Loop](#3-per-frame-render-loop)
4. [Render Ownership Rule — Who Calls What](#4-render-ownership-rule)
5. [SpriteBatch — How DirectXTK Draws 2D Sprites](#5-spritebatch)
6. [Camera2D — World Space to Screen Space](#6-camera2d)
7. [WorldSpriteRenderer — Animated World-Space Characters](#7-worldspriterenderer)
8. [UIRenderer — Screen-Space HUD and Menus](#8-uirenderer)
9. [Sprite Sheet UV Math — How Frames Are Sliced](#9-sprite-sheet-uv-math)
10. [Depth State — Why 2D Sprites Turn Off Depth Testing](#10-depth-state)
11. [Common Mistakes Reference](#11-common-mistakes-reference)

---

## 1. Big Picture

```
WinMain
  └── GameApp::Run()  ← game loop
        ├── GameTimer::Tick()            — update deltaTime
        ├── StateManager::Update(dt)     — game logic
        └── GameApp::Render()
              │
              ├── D3DContext::BeginFrame()
              │     ├── OMSetRenderTargets(RTV, DSV)   — bind canvas
              │     ├── ClearRenderTargetView()         — wipe color buffer
              │     └── ClearDepthStencilView()         — wipe depth buffer
              │
              ├── StateManager::Render()
              │     └── ActiveState::Render()
              │           ├── SceneGraph::Render(ctx)
              │           │     └── for each IGameObject:
              │           │           Character::Render()
              │           │             └── WorldSpriteRenderer::Draw()
              │           │                   ├── SpriteBatch::Begin(viewMatrix)
              │           │                   ├── SpriteBatch::Draw(SRV, pos, srcRect, ...)
              │           │                   └── SpriteBatch::End()
              │           └── UIRenderer::DrawXxx()
              │                 ├── SpriteBatch::Begin(identity)
              │                 ├── SpriteBatch::Draw(...)
              │                 └── SpriteBatch::End()
              │
              └── D3DContext::EndFrame()
                    └── IDXGISwapChain::Present(0, 0)  — flip to screen
```

**One law: `Present()` is called exactly once per frame, inside `EndFrame()`.
No state may call `EndFrame()` or `Present()` itself.**

---

## 2. D3D11 Initialization

All DirectX setup happens in `D3DContext::Initialize()` (`src/Renderer/D3DContext.cpp`).

### 2.1 The Swap Chain

A **swap chain** is a pair (or chain) of buffers managed by DXGI:
- **Back buffer** — where the GPU draws the current frame (invisible to the user).
- **Front buffer** — what the monitor is currently displaying.

`Present()` atomically swaps them (or copies, with `DXGI_SWAP_EFFECT_DISCARD`).

```
Frame N:   GPU writes → [Back Buffer]      [Front Buffer] → Monitor displays frame N-1
Present(): DXGI swaps → [Front Buffer]     [Back Buffer]  (discarded, free for frame N+1)
```

Our configuration (`DXGI_SWAP_CHAIN_DESC`):

| Field | Value | Why |
|---|---|---|
| `BufferCount` | `1` | Simple discard model — DXGI_SWAP_EFFECT_DISCARD requires only 1 |
| `Format` | `DXGI_FORMAT_R8G8B8A8_UNORM` | 8 bits per channel, standard sRGB monitor output |
| `SampleDesc.Count` | `1` | No MSAA — 2D sprites do not need anti-aliasing |
| `SwapEffect` | `DXGI_SWAP_EFFECT_DISCARD` | Simplest present model; good enough for 2D |
| `Windowed` | `TRUE` | Windowed game; can be toggled to fullscreen later |

### 2.2 Device Creation — Debug Layer Fallback

```cpp
// Try with D3D11_CREATE_DEVICE_DEBUG first (gives detailed GPU error messages).
// If "Graphics Tools" optional feature is not installed, retry without it.
HRESULT hr = TryCreate(D3D11_CREATE_DEVICE_DEBUG);
if (hr == 0x887A002D)   // DXGI_ERROR_SDK_COMPONENT_MISSING
    hr = TryCreate(0);  // fallback — game still runs, just no validation messages
```

The debug layer is invaluable during development: it reports leaked COM objects,
invalid API calls, and state errors to the Output window in real time.
To enable it: **Windows Settings → Optional Features → Graphics Tools**.

### 2.3 Render Target View (RTV) and Depth/Stencil View (DSV)

After the device is created:

1. **RTV** — a view into the swap chain's back buffer texture.
   The GPU writes RGBA pixel colors here. This is the "canvas."

2. **DSV** — a separate texture (`DXGI_FORMAT_D24_UNORM_S8_UINT`) containing
   two per-pixel values:
   - **24-bit depth** — distance from the camera to each drawn pixel in [0, 1].
     Pixels behind a closer pixel are discarded by the depth test.
   - **8-bit stencil** — a per-pixel mask for advanced effects (mirrors, portals, outlines).

Both are bound together with `OMSetRenderTargets()` at the start of every frame.
**If this call is omitted, all draw calls output nowhere and the screen stays black.**

### 2.4 Viewport

The viewport tells the rasterizer how to map NDC coordinates `[-1, +1]` → pixel coordinates `[0, width] × [0, height]`.

```cpp
D3D11_VIEWPORT vp = {};
vp.Width    = (float)mWidth;
vp.Height   = (float)mHeight;
vp.MinDepth = 0.0f;
vp.MaxDepth = 1.0f;
```

**`SpriteBatch` depends on the viewport being set.** It internally calls
`RSGetViewports()` to infer the render target size for its NDC transform.
If the rasterizer has 0 viewports bound, `SpriteBatch::End()` throws.
This is why `WorldSpriteRenderer::Draw()` always calls `mSpriteBatch->SetViewport(vp)`
before `Begin()` — it bypasses the `RSGetViewports()` call entirely.

---

## 3. The Per-Frame Render Loop

Every frame (inside `GameApp::Render()`):

```cpp
void GameApp::Render() {
    // 1. Clear the canvas and depth buffer. Also re-binds RTV+DSV to the pipeline.
    D3DContext::Get().BeginFrame(0.05f, 0.05f, 0.1f);  // dark navy blue background

    // 2. The active state issues all draw calls into the back buffer.
    StateManager::Get().Render();

    // 3. Flip back buffer to front — the frame appears on screen.
    D3DContext::Get().EndFrame();
}
```

### What `BeginFrame()` does

```cpp
void D3DContext::BeginFrame(float r, float g, float b) {
    // Re-bind render target every frame — another renderer may have unbound it.
    ID3D11RenderTargetView* rtv = mRenderTarget.Get();
    mContext->OMSetRenderTargets(1, &rtv, mDepthStencilView.Get());

    float clearColor[4] = { r, g, b, 1.0f };
    mContext->ClearRenderTargetView(mRenderTarget.Get(), clearColor);
    mContext->ClearDepthStencilView(mDepthStencilView.Get(),
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}
```

Re-binding the RTV every `BeginFrame()` is intentional: some overlay states
(like `BattleState`) re-call `BeginFrame()` with a different clear color to
paint over the previous state's background, without touching `EndFrame()`.

### What `EndFrame()` does

```cpp
void D3DContext::EndFrame() {
    mSwapChain->Present(0, 0);  // 0 = no VSync; use 1 to lock to 60 Hz
}
```

`Present()` is the only GPU-synchronizing call in the entire frame.
Everything before it is queued in the command buffer and submitted here.

---

## 4. Render Ownership Rule

> **`GameApp::Render()` is the sole owner of `BeginFrame()` and `EndFrame()`.**
> States issue draw calls only. They never call `Present()`.

| Who | What they call | What they NEVER call |
|---|---|---|
| `GameApp::Render()` | `BeginFrame()` + `EndFrame()` | — |
| Any `IGameState::Render()` | `SpriteBatch::Begin/Draw/End` | `EndFrame()` / `Present()` |
| `BattleState::Render()` | `BeginFrame()` again *(re-clear only)* | `EndFrame()` |

**Why can a state call `BeginFrame()` again?**
`BeginFrame()` is not destructive to the swap chain — it just clears the back buffer
and re-binds the RTV. Calling it twice in a single frame is safe: the second call
simply overwrites the previous background with a different color.

**The bug that taught us this:**
During the Battle system integration, `BattleState::Render()` was calling both
`BeginFrame()` AND `EndFrame()`. This meant `Present()` fired **twice** per frame:
once inside `BattleState::Render()`, and again inside `GameApp::Render()`.
The result was a black flash / flickering screen because the monitor was receiving
two presents in rapid succession with nothing drawn between them.
**Fix:** Remove `EndFrame()` from every state. Only `GameApp::Render()` calls it.

---

## 5. SpriteBatch

`SpriteBatch` (from **DirectXTK**) is the workhorse 2D renderer.
It batches many `Draw()` calls into as few GPU draw calls as possible.

### The Begin / Draw / End Pattern

```cpp
// Begin: set GPU state (blend, depth, sampler, rasterizer, transform matrix)
spriteBatch->Begin(
    SpriteSortMode_Deferred,   // buffer all calls; sort + flush on End()
    mAlphaBlend.Get(),          // blend state: src_alpha / inv_src_alpha
    nullptr,                    // sampler: SpriteBatch default (linear wrap)
    mDepthNone.Get(),           // depth state: OFF for 2D
    nullptr,                    // rasterizer: SpriteBatch default
    nullptr,                    // custom effect: none
    transformMatrix             // WORLD→PIXEL matrix (see §6 for why)
);

// Draw: queue one sprite for rendering
spriteBatch->Draw(
    textureSRV,     // GPU texture
    position,       // XMFLOAT2 — where to draw (world pos or screen pos)
    &sourceRect,    // RECT into the atlas (UV rectangle in pixel units)
    Colors::White,  // tint (white = no tint)
    0.0f,           // rotation in radians
    origin,         // pivot point in source-rect-local pixels
    scale,          // uniform scale
    SpriteEffects_FlipHorizontally  // or SpriteEffects_None
);

// End: sort sprites by texture (reduces GPU state changes) and submit draw calls
spriteBatch->End();
```

### The Transform Matrix — The Most Critical Parameter

SpriteBatch's internal constant buffer is computed in `PrepareForRendering()`:

```
CB0 = mTransformMatrix × GetViewportTransform(deviceContext, mRotation)
```

`GetViewportTransform()` maps **pixel space → NDC**:
```
scale( 2/W, -2/H ) + translate( -1, +1 )
```

This transform is **always applied** because `mRotation` is initialized to
`DXGI_MODE_ROTATION_IDENTITY` (not `DXGI_MODE_ROTATION_UNSPECIFIED`), meaning
SpriteBatch never skips the multiply.

**Therefore, the matrix you supply must map coordinates → pixel space:**

| Use Case | Matrix to Pass | Reasoning |
|---|---|---|
| **World-space sprite** (character, projectile) | `camera.GetViewMatrix()` | world→pixel; SpriteBatch adds pixel→NDC ✓ |
| **Screen-space UI sprite** (HUD, menu icon) | Identity (default) | pixel→pixel; SpriteBatch adds pixel→NDC ✓ |
| ❌ WRONG | `camera.GetViewProjectionMatrix()` | world→NDC already; SpriteBatch adds pixel→NDC again = double projection → sprite invisible |

---

## 6. Camera2D

`Camera2D` (`src/Renderer/Camera2D.h/.cpp`) converts world coordinates → screen pixels.

### GetViewMatrix()

```
ViewMatrix = scale(zoom) × translate(-position)
```

- `position` — the camera's center in world space (follows the player).
- `zoom` — scales the world; values > 1 zoom in, < 1 zoom out.

A world point `(wx, wy)` becomes screen pixel `(sx, sy)` via:

```
sx = (wx - camera.x) * zoom + screenWidth  / 2
sy = (wy - camera.y) * zoom + screenHeight / 2
```

This is exactly what `camera.GetViewMatrix()` encodes as an `XMMATRIX`.

### Why not GetViewProjectionMatrix()?

`GetViewProjectionMatrix()` includes the NDC projection step.
As shown in §5, SpriteBatch **always** appends its own `pixel→NDC` step.
Supplying a matrix that already includes `→NDC` causes a double projection,
mapping a world point at `(400, 200)` to an NDC coordinate far outside `[-1, +1]`.
The hardware clip stage discards it and the sprite is invisible.

---

## 7. WorldSpriteRenderer

`WorldSpriteRenderer` (`src/Renderer/WorldSpriteRenderer.h/.cpp`) draws an
animated sprite sheet character at a **world-space position**.

### Initialization (once, in state `OnEnter`)

1. `CreateWICTextureFromFile()` — decodes the PNG and uploads it to GPU VRAM as an `ID3D11ShaderResourceView`.
2. `std::make_unique<SpriteBatch>(context)` — creates the batch renderer.
3. `CreateDepthStencilState()` — creates a depth-OFF state object.
4. `CreateBlendState()` — creates an alpha-blend state (`SRC_ALPHA / INV_SRC_ALPHA`).

State objects are created **once** at init time because `CreateXxxState()` is an
allocating GPU call. Creating them every frame would stall the pipeline and
produce unbounded live objects that the DX debug layer would report as leaks.

### Draw (each frame, per character)

```
RSGetViewports() → SetViewport(vp)     — bypass SpriteBatch's own RSGetViewports
OMSetDepthStencilState(mDepthNone)     — force depth OFF before Begin
SpriteBatch::Begin(camera.GetViewMatrix())
SpriteBatch::Draw(SRV, worldPos, srcRect, origin=pivot, scale, flipX?)
SpriteBatch::End()
```

**Pivot (origin):**
The pivot is loaded from the sprite sheet JSON (`pivotX`, `pivotY`).
A pivot of `[64, 128]` on a `128×128` frame means the **bottom-center** of
the frame lands exactly at `(worldX, worldY)`.
This is the JRPG convention: characters are positioned by their feet.

**Horizontal flip:**
```cpp
flipX ? SpriteEffects_FlipHorizontally : SpriteEffects_None
```
The GPU mirrors the UV in U-space (`U' = 1 - U`). Zero CPU cost.
The default sprite faces right; `flipX=true` faces left.

---

## 8. UIRenderer

`UIRenderer` (`src/Renderer/UIRenderer.h/.cpp`) draws sprites at **screen-space positions**.
No camera is involved — the sprite is fixed to the screen regardless of the camera position.

Key constraints:
- **Single-row sprite sheets only.** The atlas height must equal the frame height.
  An `assert` enforces this in `Initialize()`.
- **No camera matrix.** `SpriteBatch::Begin()` is called with the default identity transform.
- **Named draw methods** (`DrawBottomCenter`, `DrawTopLeft`, `DrawCentered`) encode
  placement intent at the call site. No magic enum arguments.

Usage:
```cpp
mPortrait.Initialize(device, context, L"assets/ui/portrait.png", sheet);
mPortrait.SetScreenSize(1280, 720);
mPortrait.PlayClip("talk");
// Each frame:
mPortrait.Update(dt);
mPortrait.DrawBottomCenter(ctx, 2.0f);  // scale = 2x, bottom-center of screen
```

---

## 9. Sprite Sheet UV Math

All sprites are packed into a single **atlas texture** — one PNG per character,
with each row of frames belonging to one animation clip.

```
Atlas layout (128×128 frames, 4 frames wide):

Row 0 (idle):  [0,0]─[128,0]  [128,0]─[256,0]  [256,0]─[384,0]  [384,0]─[512,0]
Row 1 (walk):  [0,128]─...    [128,128]─...     ...
Row 2 (attack): ...
```

Frame index to source RECT:

```cpp
const int fpr      = sheetWidth / frameWidth;      // frames per row (e.g. 4)
const int col      = mFrameIndex % fpr;             // column in the atlas
const int atlasRow = clip.startRow + (mFrameIndex / fpr);  // row in the atlas

RECT srcRect = {
    col      * frameWidth,              // left
    atlasRow * frameHeight,             // top
    col      * frameWidth  + frameWidth,  // right
    atlasRow * frameHeight + frameHeight  // bottom
};
```

`clip.startRow` is set by `JsonLoader` from the clip's index in the `animations[]`
array in the JSON file. **No pixel offsets are hardcoded in C++.**

### Spanning Multiple Rows

For an animation clip with more frames than `framesPerRow`, the `(mFrameIndex / fpr)`
term automatically advances to the next row. This is handled transparently — the
caller never needs to know how the atlas is laid out.

---

## 10. Depth State

2D sprites draw at `z=0` in clip space. If depth testing is left ON from a previous
3D or SDF render pass (e.g. `CircleRenderer` fills the depth buffer at various depths),
the sprite's z=0 will **fail the depth test** against the stored values and be discarded —
making the sprite completely invisible.

**Solution:** `WorldSpriteRenderer` creates and binds a depth-OFF state before every `Begin()`:

```cpp
D3D11_DEPTH_STENCIL_DESC dsDesc = {};
dsDesc.DepthEnable    = FALSE;   // no depth comparison
dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;  // no depth writes
dsDesc.StencilEnable  = FALSE;
device->CreateDepthStencilState(&dsDesc, mDepthNone.GetAddressOf());
```

This state is passed explicitly to `SpriteBatch::Begin()` AND set directly on
the context before `Begin()`. Layer ordering for 2D sprites is handled entirely
by draw-call order (managed by `SceneGraph::SortByLayer()`), not by depth values.

---

## 11. Common Mistakes Reference

| Mistake | Symptom | Fix |
|---|---|---|
| Passing `GetViewProjectionMatrix()` to SpriteBatch | All sprites invisible | Pass `camera.GetViewMatrix()` (world→pixel only) |
| Calling `EndFrame()` inside a state | Screen flickers / black flash | Only `GameApp::Render()` calls `EndFrame()` |
| Forgetting `SetViewport()` before `Begin()` | `SpriteBatch::End()` throws std::exception | Call `mSpriteBatch->SetViewport(vp)` before every `Begin()` |
| Creating D3D state objects every frame | GPU stall + unbounded live objects | Create once in `Initialize()`, reuse every frame |
| Raw `->Release()` on a ComPtr | Double-free / crash on next valid access | Use `ComPtr<T>` — call `.Reset()` or let scope handle it |
| Resizing window without releasing RTV | `DXGI_ERROR_INVALID_CALL` | Call `ReleaseRenderTargetAndDepth()` before `ResizeBuffers()` |
| Not calling `OMSetRenderTargets()` after RTV recreation | Nothing renders | `CreateRenderTargetAndDepth()` always calls `OMSetRenderTargets()` at the end |
| Forgetting to bind depth-OFF before 2D draw | Sprites invisible (depth culled) | `context->OMSetDepthStencilState(mDepthNone.Get(), 0)` before `Begin()` |
| Using world-space pixel coordinates in `UIRenderer` | UI drifts with camera | `UIRenderer` uses screen-space pixel coordinates; no camera matrix |
| `LOG()` with string concatenation | Compiler error | `LOG()` is printf-style: use `LOG("%s", str.c_str())` |
