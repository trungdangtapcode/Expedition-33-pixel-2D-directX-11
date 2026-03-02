# ControllableCharacter

**Files:** `src/Entities/ControllableCharacter.h`, `src/Entities/ControllableCharacter.cpp`

---

## Purpose

`ControllableCharacter` is the first concrete `IGameObject`.  It represents the
player character in world space: reads WASD input, runs velocity physics, and
draws itself via a `WorldSpriteRenderer` component.

`PlayState` spawns it, then interacts with it through exactly two interfaces:
- `IGameObject*` — via `SceneGraph` (`Update` / `Render`)
- `GetX()` / `GetY()` — for camera follow

Everything else is private implementation.

---

## Class design

### Composition over inheritance for rendering

`ControllableCharacter` **owns** a `WorldSpriteRenderer` as a value member, not
a base class:

```cpp
class ControllableCharacter : public IGameObject {
private:
    WorldSpriteRenderer mRenderer;   // composition
    ...
};
```

Why composition?
- `WorldSpriteRenderer` is a rendering **service** with a specific interface.
  Inheriting it would expose `PlayClip()`, `Shutdown()`, `mTextureSRV`, etc. as
  part of the entity's public surface — none of which callers should see.
- Multiple entities can own a `WorldSpriteRenderer` independently without
  sharing state or creating diamond inheritance.
- The renderer can be replaced or swapped without changing the entity hierarchy.

### WASD input inside `Update()`, not in `PlayState`

```cpp
void ControllableCharacter::Update(float dt)
{
    if (GetAsyncKeyState('W') & 0x8000) mVelY -= kAccel * dt;
    if (GetAsyncKeyState('S') & 0x8000) mVelY += kAccel * dt;
    if (GetAsyncKeyState('A') & 0x8000) mVelX -= kAccel * dt;
    if (GetAsyncKeyState('D') & 0x8000) mVelX += kAccel * dt;
    // ...
}
```

`PlayState` polls zero keys.  If you add a second character, a second
`Spawn<ControllableCharacter>(...)` call gives it full, independent input — no
`PlayState` changes required.

---

## Movement physics

All three physics constants are `constexpr` — no magic numbers in the
implementation:

```cpp
static constexpr float kMaxSpeed = 400.0f;  // world units per second
static constexpr float kAccel    = 600.0f;  // world units per second^2
static constexpr float kFriction = 8.0f;    // exponential decay coefficient
```

### Acceleration

Each held key adds a velocity impulse scaled by `dt`:

```cpp
mVelX += kAccel * dt;   // at 60 fps: +10 units/frame per key held
```

### Friction — exponential decay

```cpp
const float friction = 1.0f - kFriction * dt;
mVelX *= friction;
mVelY *= friction;
```

Why not `mVelX -= kFriction * dt`?  Additive friction can drive velocity past
zero and cause oscillation (left-right jitter at rest).  Multiplicative
(exponential) decay approaches zero asymptotically — smoother, and guaranteed
to never overshoot.

At 60 fps (`dt ≈ 0.016 s`):
```
decay factor = 1 - 8 × 0.016 = 0.872
velocity halves after ≈ 5 frames (0.08 s) — snappy slide-to-stop
```

### Speed cap — diagonal equalization

```cpp
const float speed = std::sqrt(mVelX * mVelX + mVelY * mVelY);
if (speed > kMaxSpeed) {
    const float inv = kMaxSpeed / speed;
    mVelX *= inv;
    mVelY *= inv;
}
```

Holding W+D simultaneously produces velocity `≈ kMaxSpeed × √2 ≈ 1.41×`.  The
speed cap normalizes the velocity vector to `kMaxSpeed` when diagonal input is
held, so every direction feels equally fast.

### Position integration

```cpp
mPosX += mVelX * dt;
mPosY += mVelY * dt;
```

Euler integration — sufficient for character movement at typical game speeds.
For physics-critical objects (projectiles, platformer collisions) use a more
accurate integrator.

---

## Render delegation

```cpp
void ControllableCharacter::Render(ID3D11DeviceContext* ctx)
{
    if (!mReady || !mCamera) return;

    // Rebind RTV — CircleRenderer's SDF shader clears pipeline state as a
    // side effect; SpriteBatch requires a valid render target before Begin().
    ID3D11RenderTargetView* rtv = D3DContext::Get().GetRTV();
    ID3D11DepthStencilView* dsv = D3DContext::Get().GetDSV();
    ctx->OMSetRenderTargets(1, &rtv, dsv);

    mRenderer.Draw(ctx, *mCamera, mPosX, mPosY);
}
```

The RTV rebind is a safety guard.  `CircleRenderer` uses a custom SDF pixel
shader that resets `OMSetRenderTargets` as part of its state cleanup.  If
`Render()` is called after any `CircleRenderer::Draw()`, the render target is
unbound and `SpriteBatch::End()` draws to nothing.  Rebinding costs one DX call
per entity per frame — negligible.

---

## Narrow public interface

```cpp
float GetX() const { return mPosX; }
float GetY() const { return mPosY; }
void  Kill()       { mAlive = false; }
```

`PlayState` uses only `GetX()` and `GetY()` for camera follow.  The velocity,
the animation clip, the renderer state — none of it leaks to the caller.

---

## Lifecycle

```
PlayState::OnEnter()
  └── mScene.Spawn<ControllableCharacter>(device, ctx, path, sheet, "idle", 0, 0, camera)
        └── ControllableCharacter constructor
              └── mRenderer.Initialize(device, ctx, path, sheet)   ← GPU upload
              └── mRenderer.PlayClip("idle")

[every frame]
  SceneGraph::Update(dt) → ControllableCharacter::Update(dt)   ← WASD + physics
  SceneGraph::Render(ctx) → ControllableCharacter::Render(ctx)  ← sprite draw

PlayState::OnExit()
  └── mPlayer = nullptr
  └── mScene.Clear()
        └── unique_ptr<ControllableCharacter> destroyed
              └── ~ControllableCharacter()
                    └── mRenderer.Shutdown()   ← GPU resources released
```

---

## Adding a second character

```cpp
// Spawn at a different world position — fully independent:
auto* npc = mScene.Spawn<ControllableCharacter>(
    device, ctx,
    L"assets/animations/maelle.png",
    maelleSheet, std::string("idle"),
    300.0f, 0.0f,   // 300 world units to the right
    mCamera.get()
);
// npc has its own velocity, its own animation state, its own GPU texture.
// PlayState changes: 0 lines.
```

---

## Common mistakes

| Mistake | Consequence | Fix |
|---|---|---|
| Calling `mWorldSprite.Draw()` from `PlayState` directly | Breaks encapsulation; duplicates rendering logic | All drawing goes through `ControllableCharacter::Render()` |
| Not calling `mRenderer.PlayClip()` after `Initialize()` | `mActiveClip == nullptr` → crash on first `Draw()` | Always call `PlayClip(startClip)` in the constructor |
| Storing `mCamera` as an owned `unique_ptr` | Double-free when `PlayState` also owns the camera | `Camera2D* mCamera` is a **non-owning** observer pointer |
| Forgetting the RTV rebind before `mRenderer.Draw()` | Sprite invisible after CircleRenderer runs | Always call `OMSetRenderTargets` before `SpriteBatch::Begin()` |
