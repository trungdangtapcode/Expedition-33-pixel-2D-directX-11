# 03 — First Game Objects

This chapter covers the entity architecture: how every in-game object — player,
enemy, VFX, projectile — is represented by the same interface so the game loop
never needs to know what it is actually rendering or updating.

---

## Files in this chapter

| Document | What it covers |
|---|---|
| [igameobject.md](igameobject.md) | `IGameObject` interface — the universal entity contract |
| [scenegraph.md](scenegraph.md) | `SceneGraph` — owns and drives all entities |
| [controllable-character.md](controllable-character.md) | `ControllableCharacter` — first concrete entity |

---

## The core idea

Every object that participates in a scene implements two methods:

```cpp
virtual void Update(float dt) = 0;   // advance your own logic
virtual void Render(ID3D11DeviceContext* ctx) = 0;  // draw yourself
```

`SceneGraph` calls `Update(dt)` on all objects, then `Render(ctx)` on all
objects (sorted by layer).  It never asks "what are you?" — it only asks
"what do you need to do this frame?"

Adding a new entity type (enemy, coin, door, VFX burst) means:
- Write one new class that implements `IGameObject`
- Call `mScene.Spawn<MyNewThing>(...)` in the state that owns it
- **Zero changes** to the game loop, `PlayState::Update()`, or `SceneGraph`

---

## Responsibility boundaries

```
PlayState
  │
  │  mScene.Update(dt)        ← one line
  │  mScene.Render(ctx)       ← one line
  │
  ▼
SceneGraph
  │
  │  for each object:
  │    obj->Update(dt)        ← object drives its own logic
  │    obj->Render(ctx)       ← object drives its own drawing
  │
  ▼
ControllableCharacter  /  Enemy  /  VFXSprite  /  Projectile  / ...
  │
  │  [input, physics, animation, AI — all internal]
  │
  ▼
WorldSpriteRenderer  (rendering component — owned, not inherited)
```

`PlayState` has **zero knowledge** of velocities, textures, clip names, or draw
call counts.  Each entity is fully self-contained.

---

## Class hierarchy

```
IGameObject  (pure virtual: Update / Render / GetLayer / IsAlive)
  └── ControllableCharacter   (WASD player — owns WorldSpriteRenderer)
  └── Enemy                   (AI-driven — future)
  └── Projectile              (velocity + lifetime — future)
  └── VFXSprite               (one-shot animation — future)
  └── UIWidget                (screen-space — future)
```

Every new entity class **must** inherit from `IGameObject`.  Any behavior that
needs to work across multiple entity types becomes a component or a system —
never copy-pasted between subclasses.
