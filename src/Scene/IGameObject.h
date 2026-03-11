// ============================================================
// File: IGameObject.h
// Responsibility: Pure virtual base for every entity that lives in a scene.
//
// Why this interface exists:
//   SceneGraph, PlayState, and BattleState MUST NOT know the concrete type,
//   position, velocity, or draw details of any individual object.
//   They only know IGameObject*.  This enforces:
//     - Single Responsibility  — each entity encapsulates its own logic.
//     - Open/Closed Principle  — new entities are added without touching the loop.
//     - Liskov Substitution    — any IGameObject can be swapped in the scene.
//
// Contract:
//   Update(dt)   — advance all logic for one frame (physics, AI, animation).
//   Render(ctx)  — draw the object; the caller does NOT know what is drawn.
//   GetLayer()   — controls draw order (lower = drawn first).
//   IsAlive()    — false triggers removal from SceneGraph at end of frame.
//
// Suggested layer ranges:
//   0..49   = background tiles / terrain
//   50..79  = world characters and enemies
//   80..99  = particle effects and VFX
//   100+    = screen-space UI
//
// Common mistakes:
//   1. Putting game logic in PlayState::Update() instead of IGameObject::Update()
//      — breaks Open/Closed; adding a new enemy type requires editing PlayState.
//   2. Reading obj->mPosition from outside the object — access must go through
//      a narrow getter (e.g. GetPosition()) so the field can be refactored.
//   3. Forgetting IsAlive() check — SceneGraph::PurgeDead() handles removal;
//      callers should never manually erase from the object list.
// ============================================================
#pragma once
#include <d3d11.h>

// ============================================================
// Interface: IGameObject
// ============================================================
class IGameObject
{
public:
    virtual ~IGameObject() = default;

    // ------------------------------------------------------------
    // Function: Update
    // Purpose:
    //   Advance this object's state by one frame (dt seconds).
    //   Handles: input, physics, animation timers, AI decisions,
    //            cooldown countdowns, lifetime timers.
    // Why:
    //   All per-frame logic is encapsulated here.  SceneGraph calls
    //   this once per object — the game loop needs no knowledge of what
    //   the object IS or what it does internally.
    // Parameters:
    //   dt — delta time in seconds, always scaled by GameTimer.
    //        Never use raw wall time inside an Update() implementation.
    // ------------------------------------------------------------
    virtual void Update(float dt) = 0;

    // ------------------------------------------------------------
    // Function: Render
    // Purpose:
    //   Issue all draw calls for this object for the current frame.
    //   The caller (SceneGraph::Render) does NOT know how many draw calls
    //   are made, what textures are used, or where on screen they appear.
    // Why:
    //   A Character might need one sprite draw + one shadow draw + one
    //   health bar draw.  All three are private implementation details;
    //   the caller just calls Render(ctx) once.
    // Parameters:
    //   ctx — the D3D11 device context for this frame.
    //         Must have a valid RTV bound (see D3DContext::BeginFrame).
    // ------------------------------------------------------------
    virtual void Render(ID3D11DeviceContext* ctx) = 0;

    // ------------------------------------------------------------
    // Function: GetLayer
    // Purpose:
    //   Return the draw-order layer for this object.
    //   SceneGraph::Render() sorts all live objects by layer before
    //   issuing draw calls — lower values are drawn first (background).
    // Why:
    //   Centralising sort order here means no caller switch/if logic
    //   and no hardcoded draw sequences in PlayState::Render().
    // ------------------------------------------------------------
    virtual int GetLayer() const = 0;

    // ------------------------------------------------------------
    // Function: GetSortY
    // Purpose:
    //   Return the world-space Y coordinate used as a secondary sort key
    //   within objects that share the same GetLayer() value.
    //   Objects with a LOWER Y are drawn first (they are further "up" on
    //   screen, i.e. further away from the viewer in a top-down scene).
    //   Objects with a HIGHER Y are drawn last (closer to the viewer,
    //   visually in front).
    //
    //   This implements the painter's Y-sort algorithm standard in 2-D
    //   JRPGs and top-down games so that characters "behind" others do not
    //   incorrectly overlap characters "in front" of them.
    //
    // Default:
    //   Returns 0.0f — objects that do not need Y-sorting (e.g. background
    //   tiles, screen-space UI) keep the default and are sorted only by layer.
    // ------------------------------------------------------------
    virtual float GetSortY() const { return 0.0f; }

    // ------------------------------------------------------------
    // Function: IsAlive
    // Purpose:
    //   Return false when this object should be removed from the scene.
    //   SceneGraph::PurgeDead() erases all dead objects after the full
    //   Update pass completes — never mid-iteration.
    // Why defer removal?
    //   Erasing from a vector while iterating it causes undefined behaviour.
    //   The deferred purge is the safest, simplest solution.
    // ------------------------------------------------------------
    virtual bool IsAlive() const = 0;
};
