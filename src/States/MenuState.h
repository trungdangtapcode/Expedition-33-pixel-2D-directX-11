// ============================================================
// File: MenuState.h
// Responsibility: Entry state that routes New Game and Continue.
//
// Lifetime:
//   Pushed by GameApp after DirectX and audio are initialized.
//   Replaced by OverworldState when the player starts or loads a game.
//
// Save/load:
//   MenuState does not serialize data itself. It delegates save-slot work
//   to SaveManager and only owns the high-level state transition decision.
// ============================================================
#pragma once

#include "IGameState.h"

class MenuState : public IGameState
{
public:
    void OnEnter() override;
    void OnExit() override;
    void Update(float dt) override;
    void Render() override;
    const char* GetName() const override { return "MenuState"; }

private:
    bool mEnterWasDown = false;
    bool mContinueWasDown = false;
    bool mSlotWasDown[9] = {};
};
