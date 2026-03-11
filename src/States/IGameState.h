#pragma once

// ============================================================
// File: IGameState.h
// Responsibility: Pure virtual interface for all game states.
//
// Every state (MenuState, BattleState, CutsceneState …) inherits this.
// GameApp and StateManager have no knowledge of what a state does internally —
// they only call Update(dt) and Render().  This is the Dependency Inversion
// Principle: high-level policy (the game loop) depends on an abstraction,
// not on concrete state implementations.
//
// State lifecycle:
//   OnEnter()  — called once when the state is pushed onto the stack.
//   Update(dt) — called every frame while the state is the top of the stack.
//   Render()   — called every frame to draw.
//   OnExit()   — called once when the state is popped off the stack.
//
// Stack semantics (StateManager):
//   PushState(new)   — OnEnter() on new state; old state paused underneath.
//   PopState()       — OnExit() on top state; state below resumes WITHOUT
//                      OnEnter() being called again.
//   ChangeState(new) — OnExit() on top, then OnEnter() on new state.
//
// Because OnEnter() is NOT called on resume after a pop, any resource that
// must be restored when the state regains focus (e.g., BGM) must be restored
// by the state that triggered the pop (in its OnExit()).
// ============================================================
class IGameState {
public:
    virtual ~IGameState() = default;

    // Called once when the state becomes active (pushed onto the stack).
    // Allocate GPU resources, subscribe to events, start BGM here.
    virtual void OnEnter() = 0;

    // Called once when the state is removed from the stack.
    // Release GPU resources, unsubscribe events, stop/restore BGM here.
    virtual void OnExit() = 0;

    // Advance all game logic by one frame.
    // dt = delta time in seconds, always scaled by GameTimer.
    // Never use raw wall-clock time inside a state.
    virtual void Update(float dt) = 0;

    // Submit all draw calls for this frame.
    virtual void Render() = 0;

    // Human-readable name used for debug logging.
    virtual const char* GetName() const = 0;
};
