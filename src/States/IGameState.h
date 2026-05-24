#pragma once

// ============================================================
// File: IGameState.h
// Responsibility: Pure virtual interface for all game states.
//
// Every state (MenuState, BattleState, CutsceneState, and overlays) inherits
// this interface. GameApp and StateManager know only this abstraction, so
// concrete screen logic stays isolated inside each state.
//
// State lifecycle:
//   OnEnter()  - called once when the state is pushed onto the stack.
//   Update(dt) - called every frame while the state is the top of the stack.
//   Render()   - called when the state should submit draw calls.
//   OnExit()   - called once when the state is removed from the stack.
//
// Stack semantics:
//   PushState(new)   - enters a new top state; the previous state remains alive.
//   PopState()       - exits the top state and resumes the state below.
//   ChangeState(new) - clears the active stack and enters a replacement state.
//
// Render overlays:
//   Opaque states keep ShouldRenderBelow() false. Overlay states, such as the
//   pause menu, return true so StateManager renders the preserved gameplay
//   state behind them before drawing the overlay.
// ============================================================
class IGameState
{
public:
    virtual ~IGameState() = default;

    // Called once when the state becomes active.
    virtual void OnEnter() = 0;

    // Called once when the state is removed from the stack.
    virtual void OnExit() = 0;

    // Advance state logic by one frame.
    virtual void Update(float dt) = 0;

    // Submit all draw calls for this frame.
    virtual void Render() = 0;

    // Overlay states return true when the state below should remain visible.
    virtual bool ShouldRenderBelow() const { return false; }

    // Human-readable name used for debug logging.
    virtual const char* GetName() const = 0;
};
