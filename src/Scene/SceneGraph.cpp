// ============================================================
// File: SceneGraph.cpp
// Responsibility: Implement update/render loops and lifetime management
//                 for all IGameObject instances in a scene.
//
// Owns: nothing directly — all ownership is via the unique_ptr vector
//       declared in SceneGraph.h.
//
// Lifetime:
//   Created in  → PlayState::OnEnter() (value member — no allocation needed)
//   Destroyed in → PlayState::OnExit() calls Clear() before D3D teardown
//
// Common mistakes:
//   1. Calling Render() before Update() — sprites will show the previous
//      frame's animation state.
//   2. Storing the raw pointer from Spawn<T>() in a container that outlives
//      the scene — the pointer becomes dangling after PurgeDead() destroys
//      the object.
//   3. Spawning or erasing objects inside the Render() loop — safe here
//      because Render() builds a sorted INDEX vector and iterates that,
//      not mObjects directly.  New objects appended during Render() are
//      skipped this frame.
// ============================================================
#include "SceneGraph.h"

// ------------------------------------------------------------
// Function: Update
// ------------------------------------------------------------
void SceneGraph::Update(float dt)
{
    // Call Update on every object.
    // DO NOT erase here — an object that sets IsAlive()=false during its own
    // Update() must complete that call fully (e.g. to queue death VFX).
    for (auto& obj : mObjects)
    {
        obj->Update(dt);
    }

    // Safe removal: now that all Update() calls are done, purge the dead.
    PurgeDead();
}

// ------------------------------------------------------------
// Function: Render
// Purpose:
//   Sort objects by (layer ASC, sortY ASC), then draw in that order.
//   This implements the painter's algorithm: objects with lower Y are
//   drawn first (they appear further away from the viewer in a top-down
//   scene) and are therefore occluded by objects with higher Y positions.
//
// Why compound sort?
//   Layer alone is sufficient for well-separated render groups (background
//   vs. characters vs. UI).  Within the characters layer (50-79), entities
//   that are lower on screen (higher Y) should visually overlap those that
//   are higher on screen (lower Y).  Y-sort achieves this without requiring
//   each entity to manage a unique layer value.
// ------------------------------------------------------------
void SceneGraph::Render(ID3D11DeviceContext* ctx)
{
    // Build a lightweight sorted index list so mObjects insertion order
    // stays stable and Spawn() during Render() is safe.
    const int n = static_cast<int>(mObjects.size());
    std::vector<int> indices(n);
    for (int i = 0; i < n; ++i) indices[i] = i;

    // Compound sort:
    //   Primary   — GetLayer()  ascending (lower layer = background = drawn first)
    //   Secondary — GetSortY()  ascending (lower world Y = further back = drawn first)
    // stable_sort guarantees deterministic ordering for ties.
    std::stable_sort(indices.begin(), indices.end(),
        [this](int a, int b)
        {
            const int layerA = mObjects[a]->GetLayer();
            const int layerB = mObjects[b]->GetLayer();
            if (layerA != layerB) return layerA < layerB;
            // Same layer: sort by world Y so "lower on screen" = drawn on top.
            return mObjects[a]->GetSortY() < mObjects[b]->GetSortY();
        });

    // Issue draw calls in sorted order.
    for (int i : indices)
    {
        mObjects[i]->Render(ctx);
    }
}

// ------------------------------------------------------------
// Function: Clear
// ------------------------------------------------------------
void SceneGraph::Clear()
{
    // Destroy all unique_ptrs — each object's destructor releases its GPU resources.
    // Called from the owning state's OnExit() to ensure deterministic teardown
    // before D3DContext is destroyed.
    mObjects.clear();
}

// ------------------------------------------------------------
// Function: PurgeDead
// ------------------------------------------------------------
void SceneGraph::PurgeDead()
{
    // C++17 erase_if: single-pass O(n), no iterator invalidation, idiomatic.
    // The unique_ptr destructor is called for each erased element, so GPU
    // resources owned by the object are released immediately.
    mObjects.erase(
        std::remove_if(mObjects.begin(), mObjects.end(),
            [](const std::unique_ptr<IGameObject>& obj)
            {
                return !obj->IsAlive();
            }),
        mObjects.end()
    );
}
