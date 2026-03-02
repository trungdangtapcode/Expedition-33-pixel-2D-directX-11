# WorldSpriteRenderer

**Files:** `src/Renderer/WorldSpriteRenderer.h`, `src/Renderer/WorldSpriteRenderer.cpp`

---

## Purpose

`WorldSpriteRenderer` draws an animated sprite at a **world-space** coordinate.
The camera's view matrix is forwarded to the GPU so the sprite moves, zooms, and
scrolls with the camera without any CPU coordinate conversion per object.

It is a pure **rendering component** — it owns GPU resources and knows how to
draw.  It has no concept of HP, faction, velocity, or player input.

---

## Key distinction: world-space vs screen-space

| | `WorldSpriteRenderer` | `UIRenderer` |
|---|---|---|
| Coordinate system | World pixels | Screen pixels |
| Camera effect | Sprite scrolls and zooms with camera | Sprite is fixed on screen |
| Use case | Characters, enemies, props | HUD, dialogue boxes, menus |
| SpriteBatch matrix | `camera.GetViewMatrix()` | Identity (default) |

---

## Owned GPU resources

| Member | Type | Created in | Released in |
|---|---|---|---|
| `mTextureSRV` | `ComPtr<ID3D11ShaderResourceView>` | `Initialize()` | `Shutdown()` |
| `mSpriteBatch` | `unique_ptr<SpriteBatch>` | `Initialize()` | `Shutdown()` |
| `mDepthNone` | `ComPtr<ID3D11DepthStencilState>` | `Initialize()` | `Shutdown()` |
| `mAlphaBlend` | `ComPtr<ID3D11BlendState>` | `Initialize()` | `Shutdown()` |

All four are created exactly once and reused for the lifetime of the renderer.
Never create D3D11 state objects per frame — that stalls the GPU pipeline.

---

## Initialization

```cpp
WorldSpriteRenderer renderer;

SpriteSheet sheet;
JsonLoader::LoadSpriteSheet("assets/animations/verso.json", sheet);

bool ok = renderer.Initialize(device, context,
                               L"assets/animations/verso.png", sheet);
// ok == false → texture file not found or GPU ran out of memory
```

`Initialize()` performs four steps:

1. **Texture upload** — `CreateWICTextureFromFile()` decodes the PNG via WIC
   (Windows Imaging Component) and uploads it to a `D3D11_USAGE_DEFAULT` texture
   in VRAM.  Only the `ID3D11ShaderResourceView` is kept; the underlying
   `ID3D11Texture2D` resource reference is discarded.

2. **SpriteBatch** — `new SpriteBatch(context)` stores the device context
   internally.  All `Begin/Draw/End` calls must use the same context.

3. **Depth-stencil state OFF** — 2-D sprites are drawn at z=0 in clip space.
   If depth test is left ON, any depth written by a preceding 3-D draw (e.g.
   `CircleRenderer`'s SDF shader) will occlude the sprite, making it invisible.

4. **Alpha-blend state** — standard one-minus-source-alpha blending so PNG
   transparency is respected.  Passed explicitly to `SpriteBatch::Begin()` so
   the renderer's blend mode is self-contained and never leaks pipeline state.

---

## Per-frame usage

```cpp
// In ControllableCharacter::Update():
renderer.Update(dt);            // advance animation frame timer

// In ControllableCharacter::Render():
renderer.Draw(ctx, camera, worldX, worldY);
```

### `Update(dt)`

Advances `mFrameTimer` by `dt` seconds.  When the timer exceeds the current
frame's duration (`1.0f / clip.frameRate`), the timer is **subtracted** rather
than reset to zero:

```
mFrameTimer -= (1.0f / mActiveClip->frameRate);
```

Why subtract instead of reset?  If a frame takes 0.12 s and `dt` is 0.05 s,
the timer might fire 2 ms late.  Carrying the 2 ms remainder forward keeps the
animation at the correct average speed regardless of frame-rate variance.

### `Draw(ctx, camera, worldX, worldY, scale)`

```
1. Compute srcRect from mFrameIndex + SpriteSheet (UV slicing)
2. Build XMFLOAT2 pos = { worldX, worldY }
   (world pixels — GPU converts to screen via the camera matrix)
3. Build XMFLOAT2 origin = { pivotX, pivotY }
   (source-rect-local pixels — where worldX/worldY lands within the frame)
4. ctx->OMSetRenderTargets(...)   ← rebind in case another renderer cleared it
5. mSpriteBatch->SetViewport(vp)  ← required; bypasses RSGetViewports() in Begin()
6. mSpriteBatch->Begin(SpriteSortMode_Deferred,
                        mAlphaBlend.Get(),
                        nullptr, mDepthNone.Get(),
                        nullptr, nullptr,
                        camera.GetViewMatrix())   ← world→pixel transform
7. mSpriteBatch->Draw(mTextureSRV.Get(), pos, &srcRect,
                       Colors::White, 0.0f, origin, scale)
8. mSpriteBatch->End()   ← flushes all queued draw calls to the GPU
```

---

## The critical camera matrix rule

```
SpriteBatch internal pipeline:
  CB0 = userMatrix × GetViewportTransform()

GetViewportTransform() maps:  pixel space → NDC  (pixel→NDC)
```

| What you pass | Combined transform | Result |
|---|---|---|
| `camera.GetViewMatrix()` | world→pixel → pixel→NDC | ✅ Correct |
| `camera.GetViewProjectionMatrix()` | world→NDC → pixel→NDC | ❌ Double-projected — sprite invisible |
| Identity | pixel→pixel → pixel→NDC | Screen-space only (use for UI) |

**Always pass `camera.GetViewMatrix()`**, not `GetViewProjectionMatrix()`.
This is the single most common mistake with SpriteBatch in world-space rendering.

---

## Depth state — why it must be OFF

```
Frame render order:
  CircleRenderer::Draw()     ← writes depth buffer at z = f(SDF gradient)
  WorldSpriteRenderer::Draw() ← sprites at z = 0 in clip space

With depth ON:   z_sprite(0) < z_circle(varies) → depth test FAILS → sprite invisible
With depth OFF:  no depth test → sprite always drawn on top → correct for 2-D
```

The `mDepthNone` state (depth test OFF, depth write OFF) is created once and
passed as the 4th argument to `SpriteBatch::Begin()`.

---

## Shutdown

```cpp
renderer.Shutdown();
```

Releases all four GPU resources in order.  Must be called before
`D3DContext` is destroyed.  `ControllableCharacter`'s destructor calls this.

If you forget `Shutdown()`, the DirectX debug layer reports at program exit:

```
D3D11 WARNING: Live ID3D11ShaderResourceView  refcount=1
D3D11 WARNING: Live ID3D11DepthStencilState   refcount=1
D3D11 WARNING: Live ID3D11BlendState          refcount=1
D3D11 WARNING: Live SpriteBatch internal VB/IB/CB ...
```

---

## Common mistakes

| Mistake | Symptom | Fix |
|---|---|---|
| `GetViewProjectionMatrix()` instead of `GetViewMatrix()` | Sprite not visible, no error | Pass `camera.GetViewMatrix()` to `SpriteBatch::Begin()` |
| Forgetting `camera.Update()` before `Draw()` | Sprite lags 1 frame behind position | Call `mCamera->Update()` in `PlayState::Update()` after `Follow()` |
| Skipping `SetViewport()` | `std::exception` thrown inside `SpriteBatch::End()` | Always call `mSpriteBatch->SetViewport(vp)` before `Begin()` |
| Passing screen pixels as `worldX/worldY` | Sprite appears thousands of pixels off-screen | Use world coordinates; camera matrix handles the conversion |
| Calling `Draw()` without `PlayClip()` | `mActiveClip == nullptr` → crash | Always call `PlayClip("idle")` after `Initialize()` |
