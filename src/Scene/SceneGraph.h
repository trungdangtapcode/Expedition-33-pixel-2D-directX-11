// ============================================================
// File: SceneGraph.h
// Responsibility: Own and drive all IGameObject instances in a scene.
//
// SceneGraph is the single point through which all entity Update() and
// Render() calls flow.  The owning state (e.g. PlayState) calls:
//
//   mScene.Update(dt);         <- advances all objects
//   mScene.Render(ctx);        <- draws all objects, sorted by layer
//
// The state never iterates objects itself, never checks counts, and
// never knows the concrete type of any object inside the graph.
//
// Ownership model:
//   SceneGraph owns every object as a std::unique_ptr<IGameObject>.
//   Spawn<T>() creates the object and transfers ownership to the graph;
//   it returns a raw observer pointer for one-time post-spawn setup only.
//   When IsAlive() returns false, PurgeDead() destroys the unique_ptr.
//
// Thread safety:
//   NOT thread-safe.  All calls must originate from the game thread.
//
// Common mistakes:
//   1. Holding the raw pointer returned by Spawn<T>() past the end of the
//      frame it was returned — the object may have been purged by PurgeDead().
//      Use the raw pointer only for immediate post-spawn setup.
//   2. Calling Render() before Update() — animation state will be one frame stale.
//   3. Spawning inside a Render() call — the new object is appended while the
//      iterator is live; it is skipped this frame and rendered next frame.
//      This is intentional and safe.
// ============================================================
#pragma once
#include "IGameObject.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <d3d11.h>
#include <utility>    // std::forward

// ============================================================
// Class: SceneGraph
// ============================================================
class SceneGraph
{
public:
    // ------------------------------------------------------------
    // Function: Spawn
    // Purpose:
    //   Construct a new entity of type T in-place and add it to the scene.
    //   SceneGraph takes sole ownership via unique_ptr.
    //   The returned raw pointer is a non-owning observer — valid until
    //   the object's IsAlive() returns false and PurgeDead() runs.
    //
    // Why template factory instead of accepting unique_ptr?
    //   Centralises ownership semantics — callers can never forget to
    //   std::move the unique_ptr, and the graph controls allocation order.
    //
    // Usage:
    //   auto* c = mScene.Spawn<ControllableCharacter>(device, context, sheet);
    //   c->PlayClip("idle");   // one-time post-spawn setup only
    // ------------------------------------------------------------
    template<typename T, typename... Args>
    T* Spawn(Args&&... args)
    {
        // Construct the object — forward all constructor arguments unchanged.
        auto obj = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw   = obj.get();   // stash raw pointer before ownership transfer
        mObjects.push_back(std::move(obj));
        return raw;
    }

    // ------------------------------------------------------------
    // Function: Update
    // Purpose:
    //   Call Update(dt) on every live object, then remove dead ones.
    //
    // Why Update-then-purge instead of purge-then-update?
    //   An object that dies during Update() (e.g. health reaches 0) must
    //   still complete its own Update() call so it can queue a death VFX,
    //   broadcast a "died" event, or trigger a combo.  Purging mid-loop
    //   would cut that logic short and invalidate iterators.
    //
    // Parameters:
    //   dt — delta time in seconds from GameTimer.
    // ------------------------------------------------------------
    void Update(float dt);

    // ------------------------------------------------------------
    // Function: Render
    // Purpose:
    //   Sort live objects by layer (ascending), then by world Y within the
    //   same layer (ascending — lower Y drawn first, higher Y drawn on top),
    //   and call Render(ctx) on each in that order.
    //
    // Sort criteria (painter's algorithm):
    //   Primary   : GetLayer()  ascending — lower layer = background
    //   Secondary : GetSortY()  ascending — lower Y = further up on screen
    //                                       = further from viewer = drawn first
    //
    // Why sort here instead of in Spawn()?
    //   Objects may change their layer or Y position at runtime (e.g. a
    //   character moves across the scene).  Sorting at render time is the
    //   only correct approach.
    //   std::stable_sort preserves insertion order for objects with identical
    //   layer AND identical Y — deterministic for static scenes.
    //
    // Parameters:
    //   ctx — D3D11 device context for this frame.
    // ------------------------------------------------------------
    void Render(ID3D11DeviceContext* ctx);

    // ------------------------------------------------------------
    // Function: Clear
    // Purpose:
    //   Destroy all objects immediately.
    //   Called from the owning state's OnExit() to release GPU resources
    //   before the D3D device is destroyed.
    // ------------------------------------------------------------
    void Clear();

    // Returns the number of live objects currently in the graph.
    // Intended for debug overlays only — game logic must NOT branch on count.
    int Count() const { return static_cast<int>(mObjects.size()); }

private:
    // ------------------------------------------------------------
    // Function: PurgeDead
    // Purpose:
    //   Erase all objects whose IsAlive() returns false.
    //   Called at the end of Update() after all objects have had their turn.
    //
    // Why erase_if instead of a hand-written loop?
    //   std::erase_if(vector, pred) is O(n) with a single pass and
    //   no iterator invalidation risk — it is the idiomatic C++17 approach.
    // ------------------------------------------------------------
    void PurgeDead();

    // All entities owned by this scene.
    // Objects are stored in insertion order; Render() sorts a temporary
    // index array by layer rather than sorting mObjects in-place, so that
    // Spawn() insertion order is stable and predictable.
    std::vector<std::unique_ptr<IGameObject>> mObjects;
};
