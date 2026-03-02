# SceneGraph

**Files:** `src/Scene/SceneGraph.h`, `src/Scene/SceneGraph.cpp`

---

## Purpose

`SceneGraph` owns every `IGameObject` in a scene and drives the `Update` and
`Render` loops.  It is the **single point of contact** between the active state
and all entities — the state never iterates objects directly, never checks
counts, and never knows what concrete types are stored.

---

## Ownership model

```
SceneGraph
  mObjects: vector<unique_ptr<IGameObject>>
     ├── unique_ptr<ControllableCharacter>   ← sole owner
     ├── unique_ptr<Enemy>                   ← sole owner
     └── unique_ptr<VFXSprite>               ← sole owner

PlayState
  mScene: SceneGraph                         ← value member (no heap alloc)
  mPlayer: ControllableCharacter*            ← non-owning observer (from Spawn)
```

`SceneGraph` holds sole ownership of every entity via `unique_ptr`.  `Spawn<T>`
returns a raw non-owning pointer for immediate post-spawn setup only — it is
never stored past the end of the frame except for narrow use cases (camera
follow position).

---

## API

### `Spawn<T>(args...)` — add an entity

```cpp
template<typename T, typename... Args>
T* SceneGraph::Spawn(Args&&... args)
{
    auto obj = std::make_unique<T>(std::forward<Args>(args)...);
    T* raw = obj.get();           // raw observer, non-owning
    mObjects.push_back(std::move(obj));
    return raw;
}
```

**Usage:**
```cpp
// In PlayState::OnEnter():
mPlayer = mScene.Spawn<ControllableCharacter>(
    device, context,
    L"assets/animations/verso.png",
    sheet, std::string("idle"),
    0.0f, 0.0f,
    mCamera.get()
);
// mPlayer is a non-owning ControllableCharacter* — valid until SceneGraph::Clear()
```

The raw pointer returned by `Spawn` is valid until the object's `IsAlive()`
returns `false` and `PurgeDead()` runs.  Use it only for:
- Immediate one-time setup after spawn (`PlayClip("run")`)
- Persistent narrow reads that outlive the spawn frame (`mPlayer->GetX()`)

Never store it in a container or pass it to another system beyond these uses.

### `Update(float dt)` — advance all entities

```cpp
void SceneGraph::Update(float dt)
{
    for (auto& obj : mObjects)
        obj->Update(dt);

    PurgeDead();   // safe: runs after all Update() calls complete
}
```

**Two-phase design:** an object that sets `IsAlive() = false` inside its own
`Update()` (e.g. an explosion that runs out of lifetime) completes its Update
call fully before being removed.  This allows the dying object to queue death
VFX, broadcast events, or finalize state before its destructor fires.

### `Render(ID3D11DeviceContext* ctx)` — draw all entities in layer order

```cpp
void SceneGraph::Render(ID3D11DeviceContext* ctx)
{
    // Build a sorted index list — never touches mObjects insertion order.
    std::vector<int> indices(mObjects.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::stable_sort(indices.begin(), indices.end(),
        [this](int a, int b) {
            return mObjects[a]->GetLayer() < mObjects[b]->GetLayer();
        });

    for (int i : indices)
        mObjects[i]->Render(ctx);
}
```

Why sort an **index vector** instead of sorting `mObjects` directly?
- `mObjects` insertion order stays stable — `Spawn()` order is preserved for
  equal-layer objects.
- Sorting `unique_ptr`s is expensive (move semantics) and changes ownership
  indices permanently.
- The index vector is cheap stack/heap allocation; elements are `int`.

`std::stable_sort` guarantees that objects with identical `GetLayer()` values
draw in insertion order — predictable and matching artist expectations.

### `Clear()` — destroy all entities

```cpp
void SceneGraph::Clear()
{
    mObjects.clear();   // unique_ptr destructors fire → GPU resources released
}
```

Called from the owning state's `OnExit()` to ensure deterministic teardown
**before** `D3DContext` is destroyed.  Do not rely on the `SceneGraph`
destructor for this — explicit call order matters when GPU resources are involved.

### `PurgeDead()` — remove dead entities

Called automatically at the end of every `Update()` call.  Uses the
erase-remove idiom:

```cpp
mObjects.erase(
    std::remove_if(mObjects.begin(), mObjects.end(),
        [](const auto& obj) { return !obj->IsAlive(); }),
    mObjects.end()
);
```

Single-pass O(n), no iterator invalidation, `unique_ptr` destructor fires for
each removed element — GPU resources freed immediately.

---

## Integration with PlayState

```cpp
// PlayState.h
class PlayState : public IGameState {
    SceneGraph mScene;                    // owns all entities
    ControllableCharacter* mPlayer;       // non-owning, camera follow only
    std::unique_ptr<Camera2D> mCamera;
};

// PlayState.cpp — OnEnter
mPlayer = mScene.Spawn<ControllableCharacter>(device, ctx, L"verso.png",
                                               sheet, "idle", 0.f, 0.f,
                                               mCamera.get());

// PlayState.cpp — Update
mScene.Update(dt);
if (mPlayer && mCamera)
    mCamera->Follow(mPlayer->GetX(), mPlayer->GetY(), kSmoothing, dt);

// PlayState.cpp — Render
mScene.Render(ctx);   // one line — all entities drawn, in layer order

// PlayState.cpp — OnExit
mPlayer = nullptr;    // null before Clear() — prevents stale pointer access
mScene.Clear();
```

`PlayState::Update()` is 4 lines.  `PlayState::Render()` is 2 lines (plus the
static circle).  The complexity lives in the entities.

---

## Common mistakes

| Mistake | Consequence | Fix |
|---|---|---|
| Storing the raw `Spawn<T>()` pointer in a `std::vector` | Dangling pointer after `PurgeDead()` destroys the object | Keep only `mPlayer*` for camera follow; never build a registry from Spawn returns |
| Calling `Render()` before `Update()` | Sprites render last frame's animation state | Always call `mScene.Update(dt)` then `mScene.Render(ctx)` |
| Calling `mScene.Clear()` after `D3DContext` is destroyed | GPU resources released on an invalid device → crash | Call `Clear()` in `OnExit()` while the device is still alive |
| Forgetting to null `mPlayer` before `Clear()` | `mPlayer` points to freed memory | `mPlayer = nullptr` immediately before `mScene.Clear()` |
| Spawning inside `Render()` | New object appended while index vector iterates — safe but skipped this frame | Acceptable; expected behavior. Spawn only from `Update()` for same-frame rendering |
