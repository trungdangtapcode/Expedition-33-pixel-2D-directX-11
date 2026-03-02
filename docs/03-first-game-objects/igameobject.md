# IGameObject

**File:** `src/Scene/IGameObject.h`

---

## Purpose

`IGameObject` is the **universal entity contract**.  Every object that lives in
a scene — character, enemy, projectile, VFX burst, UI widget — implements this
interface.

`SceneGraph`, `PlayState`, and `BattleState` hold only `IGameObject*`.  They
never know the concrete type.

---

## The interface

```cpp
class IGameObject {
public:
    virtual ~IGameObject() = default;

    virtual void Update(float dt) = 0;
    virtual void Render(ID3D11DeviceContext* ctx) = 0;
    virtual int  GetLayer() const = 0;
    virtual bool IsAlive() const = 0;
};
```

Four methods.  That is the entire contract between the game loop and every
entity that will ever exist in this codebase.

---

## Method responsibilities

### `Update(float dt)`

Advance this object's state by one frame.

Everything time-dependent belongs here:
- WASD input polling and velocity integration
- AI decision-making and pathfinding
- Animation frame timer advancement
- Cooldown and lifetime countdown timers
- Collision response

`dt` is **delta time in seconds**, always sourced from `GameTimer`.  Every
value that changes over time must be multiplied by `dt` — this is what makes the
game frame-rate independent.

```cpp
// Correct — frame-rate independent
mPosX += mVelX * dt;

// Wrong — moves 3× faster at 180 fps than at 60 fps
mPosX += mVelX;
```

### `Render(ID3D11DeviceContext* ctx)`

Issue all draw calls for this frame.

The caller (`SceneGraph::Render`) does **not** know:
- How many draw calls this object needs
- What textures or shaders it uses
- Where on screen it appears

A `Character` might draw a sprite + a shadow + a health bar — that is three draw
calls, all internal, all invisible to the caller.  The contract is one function
call in, all necessary drawing out.

### `GetLayer() const`

Controls draw order.  Lower values are drawn first (rendered behind higher
values).

| Range | Use |
|---|---|
| `0 – 49` | Background tiles, terrain, floor shadows |
| `50 – 79` | World characters and enemies |
| `80 – 99` | Particle effects, hit sparks, VFX |
| `100+` | Screen-space UI overlays |

`SceneGraph::Render()` uses `std::stable_sort` on layer values before drawing,
so objects with equal layers draw in insertion order — deterministic and
predictable.

### `IsAlive() const`

Return `false` when this object should be removed from the scene.

`SceneGraph::PurgeDead()` calls this after every `Update()` pass and destroys
all dead objects.  The object's destructor releases its GPU resources
immediately.

```
Lifetime state machine:
  mAlive = true   ← default on construction
       │
       │  Kill() called (HP reaches 0, lifetime expired, etc.)
       ▼
  mAlive = false
       │
       │  SceneGraph::PurgeDead() runs at end of the frame
       ▼
  unique_ptr destroyed → destructor called → GPU resources released
```

---

## Design principles this interface enforces

### Open/Closed Principle

Adding a new enemy type, a new VFX, or a new interactable object requires:
- One new `.h/.cpp` file with the concrete class
- One `mScene.Spawn<NewThing>(...)` call at the right moment

Zero changes to `SceneGraph`, `PlayState`, or any other existing file.

### Single Responsibility Principle

Each entity class is responsible for exactly its own logic and rendering.
`PlayState` is responsible for state lifecycle and camera follow.
`SceneGraph` is responsible for ownership and iteration order.
None of these responsibilities overlap.

### Liskov Substitution Principle

Any `IGameObject` can be placed in the `SceneGraph` and driven by the same
`Update(dt)` / `Render(ctx)` calls.  A `ControllableCharacter`, an `Enemy`,
and a `VFXSprite` all behave correctly when iterated by the same loop.

---

## Implementing a new entity

```cpp
// MyThing.h
#pragma once
#include "../Scene/IGameObject.h"

class MyThing : public IGameObject {
public:
    MyThing(/* constructor args */);
    ~MyThing() override;

    void Update(float dt)              override;
    void Render(ID3D11DeviceContext*)   override;
    int  GetLayer() const              override { return 50; }
    bool IsAlive()  const              override { return mAlive; }

private:
    bool mAlive = true;
    // ... your members
};
```

```cpp
// Spawn it from a state's OnEnter():
mScene.Spawn<MyThing>(/* constructor args */);
```

That is all.  The game loop picks it up automatically.

---

## Common mistakes

| Mistake | Consequence | Fix |
|---|---|---|
| Putting entity logic in `PlayState::Update()` | Adding a second entity requires editing PlayState | Move all per-entity logic into the entity's own `Update()` |
| Reading `obj->mPosition` from outside | Breaks encapsulation; field rename cascades everywhere | Expose only via a getter: `GetX()`, `GetTransform()` |
| Never returning `false` from `IsAlive()` | Dead entities accumulate in the scene forever | Call `Kill()` on HP=0, lifetime expired, or any terminal condition |
| Calling `obj->Update(dt)` manually from a state | Double-updates physics; breaks animation timing | Only `SceneGraph::Update(dt)` drives the loop |
