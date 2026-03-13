// ============================================================
// File: MenuState.cpp
// Responsibility: Main menu — the entry state of the game.
//
// Transitions:
//   ENTER key → ChangeState(OverworldState)
//
// Lifetime:
//   OnEnter() called when pushed onto StateManager's stack.
//   OnExit()  called before the state is popped or replaced.
// ============================================================
#include "MenuState.h"
#include "StateManager.h"
#include "OverworldState.h"
// #include "BattleState.h"
#include "../Events/EventManager.h"
#include "../Utils/Log.h"
#include <windows.h>  // GetAsyncKeyState

void MenuState::OnEnter() {
    // TODO: Load font, create UI buttons, start menu background music.
    LOG("[MenuState] OnEnter");
}

void MenuState::OnExit() {
    // TODO: Unload menu-specific resources (fonts, textures, audio).
    LOG("[MenuState] OnExit");
}

void MenuState::Update(float dt) {
    // Press ENTER to transition to OverworldState.
    // TODO: Replace GetAsyncKeyState with a proper InputManager later.
    if (GetAsyncKeyState(VK_RETURN) & 0x8000) {
        StateManager::Get().ChangeState(std::make_unique<OverworldState>());
        // StateManager::Get().ChangeState(std::make_unique<BattleState>(D3DContext::Get()));
    }
}

void MenuState::Render() {
    // TODO: Draw background, logo, and button list using SpriteBatch / SpriteFont.
    // For now the dark blue clear color from D3DContext::BeginFrame() is sufficient.
}
